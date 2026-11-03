"""Offline source/rate/bandplan cross-check; never accesses board or instruments.

RF-DAC interpolation passband fraction is a PG269 v2.6 specification input,
not a measured RF flatness result. FIR responses use the frozen coefficients
as ideal linear filters; fixed-point noise and the analog path are excluded.
"""
import argparse
import cmath
import hashlib
import json
import math
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def require(ok, message):
    if not ok:
        raise SystemExit("BANDPLAN_FAIL: " + message)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    inputs = {}

    def source(name):
        data = (ROOT / name).read_bytes()
        inputs[name] = hashlib.sha256(data).hexdigest()
        return data.decode("utf-8-sig")

    bd = source("src/fpga/bd/adc3_capture.tcl")
    for name, value in {"ADC1_Sampling_Rate": "2.400", "DAC1_Sampling_Rate": "2.400",
                        "ADC_Decimation_Mode12": "2", "DAC_Interpolation_Mode10": "2",
                        "DAC_NCO_Freq10": "1.8"}.items():
        require(re.search(r"CONFIG\." + name + r"\s+\{" + re.escape(value) + r"\}", bd),
                "Unexpected RFDC setting " + name)
    dsp = source("src/fpga/rtl/sa_d8_zoom.v")
    require(all(name in dsp for name in ("d2_1200_to600.mem", "d2_600_to300.mem",
                                         "d2_300_to150.mem", ".TAPS(23)", ".TAPS(63)")),
            "Three-stage RX FIR chain changed")
    waveform = source("src/linux/apps/instrument_cpp/src/waveform.cpp")
    require("r.bandwidth<=40e6" in waveform and "r.symbolRate<=10e6" in waveform,
            "Recheck documented GUI waveform bandwidth/rate bounds")
    require("r.sampleRate==1.2e9" in waveform and "r.duration==.010" in waveform,
            "Recheck the 10 ms I/Q format contract")
    sweep = source("src/linux/apps/instrument_cpp/src/sweep.cpp")
    require(all(s in sweep for s in ("center=start+40e6", "center+50e6>=stop",
                                     "center+80e6,stop-40e6", "m.fs!=150e6")),
            "Recheck sweep geometry/rate before using the documented plan")

    adc_fs, dac_fs, complex_fs = 2.4e9, 2.4e9, 1.2e9
    low, high, nco = 1.3e9, 2.3e9, 1.8e9
    require(2 * high / 2 <= adc_fs <= 2 * low / (2 - 1), "Bandpass sampling interval")
    require(adc_fs / 2 < low < high < adc_fs, "Second Nyquist zone guards")
    require(dac_fs / 2 == complex_fs == 4 * 300e6, "DAC I/Q interface rate")
    require(complex_fs / 8 == 150e6, "PL D8 sample rate")
    spec_fraction = .8
    spec_width = spec_fraction * complex_fs

    centers = []
    center = low + 40e6
    for _ in range(16):
        centers.append(center)
        if center + 50e6 >= high:
            break
        center = min(center + 80e6, high - 40e6)
    require(len(centers) == 13, "Full-span segment count")
    intervals = [(max(low, c - 50e6), min(high, c + 50e6)) for c in centers]
    require(intervals[0][0] == low and intervals[-1][1] == high, "Endpoint coverage")
    require(all(b[0] <= a[1] for a, b in zip(intervals, intervals[1:])), "Coverage gap")

    filters = []
    for name, rate, taps in [("d2_1200_to600.mem", 1.2e9, 15),
                             ("d2_600_to300.mem", 600e6, 23),
                             ("d2_300_to150.mem", 300e6, 63)]:
        values = [int(v, 16) for v in source("src/fpga/coeff/sa/" + name).split()]
        require(len(values) == taps and all(0 <= v < (1 << 18) for v in values), name)
        signed = [v - (1 << 18) if v & (1 << 17) else v for v in values]
        require(signed == signed[::-1] and sum(signed) == (1 << 17), "FIR symmetry/DC")
        filters.append((rate, signed))

    def response_db(f):
        gain = 1.
        for rate, coeff in filters:
            gain *= abs(sum(v * cmath.exp(-2j * math.pi * f * k / rate)
                            for k, v in enumerate(coeff))) / sum(coeff)
        return 20 * math.log10(gain)

    response = {str(int(f)): response_db(f) for f in (0, 50e6, 60e6, 70e6, 75e6)}
    safe_grid = [response_db(k * 1e5) for k in range(501)]
    require(max(abs(v) for v in safe_grid) < .01, "Ideal FIR +/-50 MHz response changed")
    require(response["75000000"] < -5.9, "150 MS/s endpoints are not a flat passband")
    result = {
        "status": "OFFLINE_SOURCE_RATE_BANDPLAN_PASS",
        "adc_sample_rate_hz": adc_fs, "dac_sample_rate_hz": dac_fs,
        "if_working_band_hz": [low, high], "design_working_band_width_hz": high - low,
        "bandpass_sampling_allowed_fs_hz_for_zone2": [2 * high / 2, 2 * low],
        "zone2_hz": [adc_fs / 2, adc_fs],
        "adc_alias_hz_for_rf_1300_1800_2300_mhz": [adc_fs - f for f in (low, nco, high)],
        "tx_complex_sample_rate_hz": complex_fs,
        "tx_complex_samples_per_cycle": 4, "tx_stream_clock_hz": 300e6,
        "tx_i16_q16_payload_bytes_per_second": int(complex_fs * 4),
        "tx_record_duration_s": .010, "tx_record_bytes": int(complex_fs * .010 * 4),
        "dac_interpolation_pg269_passband_fraction": spec_fraction,
        "derived_digital_spec_passband_width_hz": spec_width,
        "derived_digital_spec_passband_at_fixed_nco_hz": [nco - spec_width / 2, nco + spec_width / 2],
        "passband_reference": "https://docs.amd.com/r/en-US/pg269-rf-data-converter/RF-DAC-Interpolation-Filters-Gen-1/Gen-2",
        "rx_complex_sample_rate_hz": 150e6,
        "rx_fir_dc_normalized_ideal_response_db_by_offset_hz": response,
        "rx_fir_max_abs_response_db_on_0_to_50mhz_100khz_grid": max(abs(v) for v in safe_grid),
        "sweep_used_offset_hz": [-50e6, 50e6],
        "full_span_sweep_centers_hz": centers,
        "full_span_sweep_is_simultaneous": False,
        "builtin_lfm_max_bandwidth_hz": 40e6, "builtin_psk_qam_max_symbol_rate_hz": 10e6,
        "source_sha256": inputs,
        "limitations": [
            "No RF measurement or new synthesis was performed by this check.",
            "Interpolation passband is a specification-based calculation, not measured analog flatness.",
            "FIR response excludes quantization, RFDC filters, DAC/ADC, baluns, cables and instruments.",
            "GUI bounds and sequential coverage do not certify arbitrary modulation RF quality.",
        ],
    }
    text = json.dumps(result, indent=2, ensure_ascii=False) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(text, encoding="utf-8", newline="\n")
    print("OFFLINE_SOURCE_RATE_BANDPLAN_PASS: zone2, 13 slices, 48MB/10ms, FIR and TX passband")


if __name__ == "__main__":
    main()
