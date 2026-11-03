"""10ms vector waveform library and actual ILA validation; NumPy only for DSP.

No hardware/instrument access. Run from VS Code or Python terminal:
    python python/tx_waveforms.py generate
    python python/tx_waveforms.py verify
    python python/tx_waveforms.py capture --csv ... --reference ... --output ...
"""
from __future__ import annotations
import argparse
import csv
import json
from pathlib import Path
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
DATA = ROOT / "data" / "tx_10ms_py"
FS = 1_200_000_000
LOW_FS = 60_000_000
DURATION = .010
N = 12_000_000
NL = 600_000
PEAK = 2048
NAMES = ["tone_p10mhz_10ms", "pulse_10us_10ms", "lfm_20mhz_10ms",
         "lfm_pulse_20mhz_10ms", "barker13_10ms", "bpsk_5msps_10ms",
         "qpsk_5msps_10ms", "psk8_5msps_10ms", "qam16_5msps_10ms",
         "qam64_5msps_10ms", "qam256_5msps_10ms", "cpfsk2_1msps_10ms"]


def write_json(path, obj):
    Path(path).write_text(json.dumps(obj, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def periodic_interpolate(x, length=N):
    """Bandlimited periodic interpolation, including even-length Nyquist split.

    This is NOT repeating each sample. DC gain and complex sign are preserved.
    """
    m = len(x)
    if m % 2 or length <= m:
        raise ValueError("Expected even input length and upsampling")
    spectrum = np.fft.fft(x)
    out = np.zeros(length, dtype=np.complex128)
    half = m // 2
    out[:half] = spectrum[:half]
    out[-half + 1:] = spectrum[half + 1:]
    out[half] = out[-half] = spectrum[half] * .5
    return np.fft.ifft(out) * (length / m)


def pulse_envelope(width, period, edge):
    pw, pri, rise = [round(v * LOW_FS) for v in (width, period, edge)]
    pos = np.arange(NL) % pri
    y = (pos < pw).astype(float)
    left, right = pos < rise, (pos >= pw-rise) & (pos < pw)
    y[left] = .5 - .5*np.cos(np.pi*pos[left]/rise)
    y[right] = .5 - .5*np.cos(np.pi*(pw-1-pos[right])/rise)
    return y


def rrc_taps(sps=12, beta=.35, span=12):
    t = np.arange(-span*sps//2, span*sps//2+1) / sps
    h = np.empty(len(t))
    for k, v in enumerate(t):
        if abs(v) < 1e-12:
            h[k] = 1+beta*(4/np.pi-1)
        elif abs(abs(4*beta*v)-1) < 1e-10:
            h[k] = beta/np.sqrt(2)*((1+2/np.pi)*np.sin(np.pi/(4*beta)) +
                                  (1-2/np.pi)*np.cos(np.pi/(4*beta)))
        else:
            h[k] = (np.sin(np.pi*v*(1-beta)) + 4*beta*v*np.cos(np.pi*v*(1+beta))) / (
                np.pi*v*(1-(4*beta*v)**2))
    return h / np.linalg.norm(h)


def centered_kernel(h, length):
    kernel = np.zeros(length)
    kernel[:len(h)] = h
    return np.roll(kernel, -(len(h)//2))


def synthesize(k):
    rng = np.random.default_rng(1701+k)
    t = np.arange(NL)/LOW_FS
    meta = dict(name=NAMES[k], complex_sample_rate_hz=FS, complex_samples=N,
                duration_s=DURATION, bytes=N*4, bytes_per_iq=4, peak_target_counts=PEAK,
                layout="little-endian signed16 I0 Q0 I1 Q1 ...", low_sample_rate_hz=LOW_FS,
                dac_nco_hz=1.8e9, symbol_rate_hz=0, rrc_rolloff=0, modulation_order=0,
                nominal_bandwidth_hz=0, pulse_width_s=0, pulse_period_s=0,
                chirp_bandwidth_hz=0, chip_period_s=0, seed=1701+k,
                rf_orientation="UNCHANGED: approximately RF=1.8GHz-fIQ, sideband correction deferred")
    symbols, h, labels, sps = np.array([]), np.array([]), np.array([]), 0
    if k == 0:
        x = np.exp(2j*np.pi*10e6*t)
        meta.update(type="CW", offset_hz=10e6)
    elif k == 1:
        x = pulse_envelope(10e-6, 100e-6, .2e-6)
        meta.update(type="PULSE", pulse_width_s=10e-6, pulse_period_s=100e-6)
    elif k == 2:
        bw = 20e6
        x = np.exp(2j*np.pi*(-bw/2*t+bw/(2*DURATION)*t*t))
        meta.update(type="LFM", chirp_bandwidth_hz=bw, nominal_bandwidth_hz=bw)
    elif k == 3:
        width, pri, bw = 100e-6, 1e-3, 20e6
        u = (np.arange(NL) % round(pri*LOW_FS))/LOW_FS
        x = pulse_envelope(width, pri, 1e-6)*np.exp(2j*np.pi*(-bw/2*u+bw/(2*width)*u*u))
        meta.update(type="PULSED_LFM", chirp_bandwidth_hz=bw, nominal_bandwidth_hz=bw,
                    pulse_width_s=width, pulse_period_s=pri)
    elif k == 4:
        code = np.array([1, 1, 1, 1, 1, -1, -1, 1, 1, -1, 1, -1, 1])
        ci = (np.arange(NL) % 6000)//60
        x = np.zeros(NL)
        on = ci < 13
        x[on] = code[ci[on]]
        x *= pulse_envelope(13e-6, 100e-6, .1e-6)
        meta.update(type="BARKER13", code=code.tolist(), chip_period_s=1e-6,
                    pulse_width_s=13e-6, pulse_period_s=100e-6)
    elif 5 <= k <= 10:
        order = [2, 4, 8, 16, 64, 256][k-5]
        sps = 12
        labels = rng.integers(0, order, NL//sps)
        if order <= 8:
            vals = np.arange(order)
            inverse_gray = np.argsort(vals ^ (vals >> 1))
            offset = 0 if order == 2 else np.pi/order
            symbols = np.exp(1j*(2*np.pi*inverse_gray[labels]/order+offset))
            meta.update(type=f"PSK{order}", mapping="inverse reflected-Gray phase labels")
        else:
            side = round(np.sqrt(order))
            vals = np.arange(side)
            inv = np.argsort(vals ^ (vals >> 1))
            symbols = ((2*inv[labels//side]-(side-1)) +
                       1j*(2*inv[labels % side]-(side-1)))/np.sqrt(2*(order-1)/3)
            meta.update(type=f"QAM{order}", mapping="square QAM, reflected Gray labels per I/Q axis")
        h = rrc_taps(sps)
        impulses = np.zeros(NL, dtype=complex)
        impulses[::sps] = symbols
        x = np.fft.ifft(np.fft.fft(impulses)*np.fft.fft(centered_kernel(h, NL)))
        meta.update(symbol_rate_hz=5e6, rrc_rolloff=.35, rrc_span_symbols=12,
                    modulation_order=order, nominal_bandwidth_hz=6.75e6)
    else:
        sps = 60
        symbols = 2*rng.integers(0, 2, NL//sps)-1
        freq = np.repeat(symbols, sps)*2e6
        phase = np.cumsum(np.r_[0, freq[:-1]])/LOW_FS
        x = np.exp(2j*np.pi*phase)
        meta.update(type="CPFSK2", symbol_rate_hz=1e6, deviation_hz=2e6,
                    modulation_order=2, nominal_bandwidth_hz=5e6)
    return x, symbols, h, labels, sps, meta


def iq_words(x):
    words = np.empty((len(x), 2), dtype="<i2")
    words[:, 0] = np.rint(x.real).astype("<i2")
    words[:, 1] = np.rint(x.imag).astype("<i2")
    return words


def generate(output=DATA):
    output.mkdir(parents=True, exist_ok=True)
    entries = []
    for k, name in enumerate(NAMES):
        xlow, symbols, h, labels, sps, meta = synthesize(k)
        high = periodic_interpolate(xlow)
        gain = PEAK/np.max(np.abs(high))
        high *= gain
        meta.update(file=name+".bin", reference_file=name+"_reference.npz",
                    synthesis_scale_counts=float(gain),
                    rms_complex_counts=float(np.sqrt(np.mean(np.abs(high)**2))))
        meta["papr_db"] = float(20*np.log10(PEAK/meta["rms_complex_counts"]))
        with (output/meta["file"]).open("wb") as stream:
            for first in range(0, N, 262144):
                iq_words(high[first:first+262144]).tofile(stream)
        np.savez_compressed(output/meta["reference_file"], xlow=xlow, symbols=symbols,
                            h=h, labels=labels, sps=sps)
        write_json(output/(name+".json"), meta)
        entries.append(meta)
        print(f"GENERATED {name}: {N} IQ / {4*N} bytes / 10ms", flush=True)
        del high, xlow
    write_json(output/"catalog.json", dict(format_version=2, python_only=True,
               complex_sample_rate_hz=FS, waveform_duration_s=DURATION,
               software_limit_bytes=67108864, waveforms=entries, hardware_actions="NONE"))
    print("TX_LONG_PYTHON_GENERATION_PASS", flush=True)


def verify(output=DATA):
    catalog = json.loads((output/"catalog.json").read_text(encoding="utf-8"))
    results = []
    for meta in catalog["waveforms"]:
        path = output/meta["file"]
        assert path.stat().st_size == 48_000_000
        raw = np.memmap(path, mode="r", dtype="<i2", shape=(N, 2))
        assert int(np.max(np.abs(raw.astype(np.int32)))) <= PEAK
        with np.load(output/meta["reference_file"]) as ref:
            low, symbols, h, sps = ref["xlow"], ref["symbols"], ref["h"], int(ref["sps"])
        high = periodic_interpolate(low)
        high *= meta["synthesis_scale_counts"]
        for first in range(0, N, 262144):
            end = min(first+262144, N)
            assert np.array_equal(iq_words(high[first:end]), raw[first:end]), "Full-rate binary differs"
        del high
        # Spectrum of ALL 12M actual quantized IQ pairs, not decimated reference.
        actual = raw[:, 0].astype(float)+1j*raw[:, 1].astype(float)
        spec = np.abs(np.fft.fft(actual))**2
        cutoff = round(20e6*N/FS)
        fraction = float((np.sum(spec[:cutoff+1])+np.sum(spec[-cutoff:]))/np.sum(spec))
        assert fraction > .995, "Too much power outside +/-20MHz"
        r = actual[::20].copy()
        result = dict(name=meta["name"], verdict="PASS", bytes=path.stat().st_size,
                      duration_ms=10, power_fraction_within_40mhz=fraction,
                      reference_evm_percent=None, rf_evm_measured=False)
        if meta["type"].startswith(("PSK", "QAM")):
            kernel = centered_kernel(h, len(r))
            z = np.fft.ifft(np.fft.fft(r)*np.conj(np.fft.fft(kernel)))[::sps]
            a = np.vdot(symbols, z)/np.vdot(symbols, symbols)
            z /= a
            evm = float(100*np.sqrt(np.mean(np.abs(z-symbols)**2)/np.mean(np.abs(symbols)**2)))
            assert evm < 1, "Ideal digital EVM too large"
            result["reference_evm_percent"] = evm
        if meta["type"] == "LFM":
            freq = np.angle(r[1:]*np.conj(r[:-1]))*LOW_FS/(2*np.pi)
            expected = -10e6+20e6*(np.arange(len(freq))+.5)/LOW_FS/DURATION
            assert np.sqrt(np.mean((freq-expected)**2)) < 10_000
            result.update(chirp_start_hz=float(freq[0]), chirp_end_hz=float(freq[-1]))
        if meta["type"] in ("PULSE", "PULSED_LFM", "BARKER13"):
            mask = np.abs(r) > np.max(np.abs(r))*.5
            diff = np.diff(np.r_[False, mask, False].astype(np.int8))
            starts, ends = np.flatnonzero(diff == 1), np.flatnonzero(diff == -1)
            assert len(starts) == round(DURATION/meta["pulse_period_s"])
            assert np.max(np.abs(np.diff(starts)/LOW_FS-meta["pulse_period_s"])) < 1/LOW_FS
            result.update(pulse_count=len(starts), median_half_height_width_s=float(np.median(ends-starts)/LOW_FS))
        results.append(result)
        print(f"VERIFIED {meta['name']}: 10ms / 48MB, 40MHz fraction={fraction:.8f}, "
              f"ideal EVM={result['reference_evm_percent']}", flush=True)
        del raw, actual, spec, r
    write_json(output/"verification.json", dict(verdict="TX_LONG_BINARY_SPECTRAL_MODEM_REFERENCE_PASS",
               implementation="Python/NumPy", waveforms=results, board_tested=False, rf_tested=False,
               note="Full-rate exact binary + full spectrum + ideal matched-filter EVM; NOT RF EVM"))
    print("TX_LONG_BINARY_SPECTRAL_MODEM_REFERENCE_PASS", flush=True)


def verify_capture(csv_path, reference_path, output):
    with csv_path.open(encoding="utf-8-sig", newline="") as stream:
        reader = csv.reader(stream)
        names = [s.lower() for s in next(reader)]
        def column(net, probe):
            found = [i for i, n in enumerate(names) if net in n]
            if not found and probe == 2:
                found = [i for i, n in enumerate(names) if n.endswith(("/net", "/net[0:0]"))]
            if not found:
                import re
                found = [i for i, n in enumerate(names) if re.search(rf"(^|[^a-z0-9])probe{probe}([^a-z0-9]|$)", n)]
            if len(found) != 1:
                raise ValueError(f"Cannot uniquely find {net}/probe{probe}")
            return found[0]
        cols = [column("tx_guard_m_axis_tdata", 0), column("tx_guard_m_axis_tvalid", 1),
                column("rfdc_s10_axis_tready", 2), column("tx_guard_underrun_count", 3)]
        frame_cols = [i for i, n in enumerate(names) if 'tx_guard_frame_count' in n]
        words, controls, frames, data_started = [], [], [], False
        for row in reader:
            value = row[cols[0]].strip().removeprefix("0x")
            try:
                payload = int(value, 16).to_bytes(16, "little")
                control = [int(row[c].strip(), 16) for c in cols[1:]]
            except (ValueError, OverflowError):
                if data_started:
                    raise ValueError("Invalid/gapped ILA payload after numeric rows")
                continue
            words.append(np.frombuffer(payload, dtype="<i2").copy())
            controls.append(control)
            if len(frame_cols) == 1:
                frames.append(int(row[frame_cols[0]].strip(), 16))
            data_started = True
    control = np.array(controls)
    assert len(words) >= 64 and np.all(control[:, :2] == 1), "Not continuous DAC valid/ready"
    assert np.all(control[:, 2] == 0), "Underrun occurred"
    samples = np.array(words).reshape(-1, 2)
    ref = np.memmap(reference_path, mode="r", dtype="<i2").reshape(-1, 2)
    starts = np.flatnonzero(np.all(ref == samples[0], axis=1))
    # Spread anchors across snapshot; prevents millions of pulse-zero candidates.
    for anchor in np.linspace(1, len(samples)-1, 8, dtype=int):
        starts = starts[np.all(ref[(starts+anchor) % len(ref)] == samples[anchor], axis=1)]
        if not len(starts):
            raise AssertionError("ILA words do not match cyclic binary")
    match = None
    for first in starts[:1000]:
        if np.array_equal(samples, ref[(first+np.arange(len(samples))) % len(ref)]):
            match = int(first)
            break
    assert match is not None, "No exact cyclic match"
    result = dict(verdict="TX_ILA_EXACT_BINARY_SNAPSHOT_PASS", implementation="Python/NumPy",
                  iq_samples=len(samples), sample_rate_hz=FS, reference_cyclic_offset_samples=match,
                  reference_bytes=reference_path.stat().st_size, underrun_count=0, physical_rf_verified=False)
    if frames:
        result.update(completed_descriptor_packets_first=frames[0],
                      completed_descriptor_packets_last=frames[-1])
    output.mkdir(parents=True, exist_ok=True)
    write_json(output/"tx_capture_verification.json", result)
    np.savez_compressed(output/"tx_capture.npz", iq=samples)
    print(json.dumps(result, indent=2), flush=True)
    return result


def selftest_capture():
    # Reuse recorded offline positive/negative fixtures, never connect to hardware.
    base = ROOT/"reports/tx/capture_parser_selftest"
    verify_capture(base/"synthetic.csv", ROOT/"data/tx/tone_p10mhz.bin", base/"python")
    verify_capture(base/"synthetic_named.csv", ROOT/"data/tx/tone_p10mhz.bin", base/"python_named")
    for name in ("synthetic_underrun.csv", "synthetic_corrupt.csv"):
        try:
            verify_capture(base/name, ROOT/"data/tx/tone_p10mhz.bin", base/"python_rejected")
        except (AssertionError, ValueError):
            pass
        else:
            raise AssertionError(f"Negative parser fixture accepted: {name}")
    print("TX_CAPTURE_PARSER_PYTHON_SELFTEST_PASS", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=["generate", "verify", "capture", "selftest"])
    parser.add_argument("--output", type=Path)
    parser.add_argument("--csv", type=Path)
    parser.add_argument("--reference", type=Path)
    args = parser.parse_args()
    if args.action == "generate":
        generate(args.output or DATA)
    elif args.action == "verify":
        verify(args.output or DATA)
    elif args.action == "selftest":
        selftest_capture()
    else:
        if not all((args.csv, args.reference, args.output)):
            parser.error("capture needs --csv --reference --output")
        verify_capture(args.csv, args.reference, args.output)


if __name__ == "__main__":
    main()
