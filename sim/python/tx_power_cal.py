"""Offline CW power calibration preparation/fit; NEVER accesses hardware.

No instrument private tables, copied firmware, or assumed voltage conversion.
Measured values must be supplied by the operator in the generated CSV.
"""
from __future__ import annotations

import argparse
import csv
import hashlib
import json
import math
from pathlib import Path
import numpy as np

FS = 1_200_000_000
N = 61_440  # 51.2 us, coherent +/-10 MHz; 256-byte aligned for TX v2
NCO = 1_800_000_000
MAX_AMPLITUDE = 2048
FIELDS = ["point_id", "status", "measured_frequency_hz", "measured_dbm",
          "overload", "evidence", "notes"]


def save_json(path, obj):
    Path(path).write_text(json.dumps(obj, indent=2, ensure_ascii=False,
                                    allow_nan=False) + "\n", encoding="utf-8")


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def cw_words(frequency_hz, amplitude, ceiling=MAX_AMPLITUDE):
    if not math.isfinite(frequency_hz) or not 1.3e9 <= frequency_hz <= 2.3e9:
        raise ValueError("RF frequency must be within the TARGET 1.3-2.3 GHz range")
    if not isinstance(ceiling, int) or not 1 <= ceiling <= 32700:
        raise ValueError("Explicit ceiling must be in 1..32700 (int16 headroom preserved)")
    if not isinstance(amplitude, int) or not 1 <= amplitude <= ceiling:
        raise ValueError(f"Nominal CW amplitude must be an integer in 1..{ceiling} counts")
    offset = NCO - frequency_hz  # Preserve the measured DAC mirror convention.
    cycles = offset * N / FS
    if abs(cycles - round(cycles)) > 1e-7:
        raise ValueError("Frequency is not coherent with the 61440-IQ cyclic buffer")
    if offset == 0:
        raise ValueError("Initial CW calibration excludes DC/LO leakage; use +/-10 MHz")
    z = amplitude * np.exp(2j * np.pi * round(cycles) * np.arange(N) / N)
    words = np.column_stack((np.rint(z.real), np.rint(z.imag))).astype("<i2")
    rms = float(np.sqrt(np.mean(np.sum(words.astype(float)**2, axis=1))))
    return words, rms, offset


def prepare(output, frequencies=(1_790_000_000,), amplitudes=(512, 724, 1024, 1448, 2048),
            ceiling=MAX_AMPLITUDE, repeats=3, strict_error_db=None):
    output = Path(output)
    if output.exists():
        raise ValueError("Use a NEW campaign directory; existing evidence is never overwritten")
    # Validate all requests before creating any output.
    frequencies = tuple(frequencies)
    if not frequencies or len(set(frequencies)) != len(frequencies):
        raise ValueError("Need unique frequency points")
    amplitudes = tuple(amplitudes)
    if len(amplitudes) < 5 or list(amplitudes) != sorted(set(amplitudes)):
        raise ValueError("Need >=5 strictly increasing unique amplitudes")
    if not isinstance(repeats, int) or not 3 <= repeats <= 20:
        raise ValueError("Need 3..20 repeated measurements")
    if strict_error_db is not None and (not math.isfinite(strict_error_db) or not 0 < strict_error_db <= .1):
        raise ValueError("Strict error goal must be >0 and <=0.1dB")
    waves = [(f, a, *cw_words(f, a, ceiling)) for f in frequencies for a in amplitudes]
    output.mkdir(parents=True)
    points, manifest = [], []
    for frequency, amplitude, words, rms, offset in waves:
        name = f"cw_{int(frequency)}hz_a{amplitude}.bin"
        path = output / name
        words.tofile(path)
        role = "holdout" if amplitudes.index(amplitude) % 2 and amplitude != amplitudes[-1] else "fit"
        digest = sha256(path)
        manifest.append(dict(file=name, sha256=digest, bytes=path.stat().st_size))
        for repeat in range(repeats):
            points.append(dict(point_id=f"f{int(frequency)}_a{amplitude}_r{repeat+1}",
                               file=name, sha256=digest, frequency_hz=frequency,
                               iq_offset_hz=offset, nominal_amplitude_counts=amplitude,
                               rms_complex_counts=rms, repeat=repeat+1, role=role))
    campaign = dict(format_version=1, kind="CW_ONLY", iq_fs_hz=FS, iq_pairs=N,
                    bytes_per_iq=4, iq_layout="little-endian int16 I0 Q0 I1 Q1 ...",
                    dac_nco_request_hz=NCO, spectral_sign=-1, max_amplitude_counts=ceiling,
                    evidence_status="UNMEASURED", hardware_actions="NONE", points=points,
                    thresholds=dict(max_slope_error=.02, max_fit_residual_db=.25,
                                    max_holdout_residual_db=.30, max_repeat_span_db=.20,
                                    max_frequency_error_hz=50_000))
    if strict_error_db is not None:
        for key in ("max_fit_residual_db", "max_holdout_residual_db", "max_repeat_span_db"):
            campaign["thresholds"][key] = strict_error_db
        campaign["error_goal_semantics"] = "Relative screening vs this analyzer, NOT absolute uncertainty"
    save_json(output / "campaign.json", campaign)
    save_json(output / "sha256.json", dict(files=manifest))
    save_json(output / "context.json", dict(
        confirmed=False, reference_plane="analyzer_input_including_current_cable",
        analyzer_model="", analyzer_serial="", analyzer_settings_evidence="",
        setup_id="", bit_sha256="", elf_sha256="", connection_confirmed=False,
        calibration_date="", instrument_calibration_status="unknown",
        measurement_method="CW tone power in dBm, NOT a modulated spectral-bin peak",
        note="Do not claim the J7 connector power without independently measured cable loss."))
    with (output / "measurements.csv").open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=FIELDS)
        writer.writeheader()
        for p in points:
            writer.writerow(dict(point_id=p["point_id"], status="pending"))
    return campaign


def fit_campaign(directory):
    directory = Path(directory)
    campaign = json.loads((directory / "campaign.json").read_text(encoding="utf-8"))
    context = json.loads((directory / "context.json").read_text(encoding="utf-8"))
    if campaign.get("format_version") != 1 or campaign.get("kind") != "CW_ONLY":
        raise ValueError("Unsupported campaign")
    ceiling = campaign["max_amplitude_counts"]
    if not isinstance(ceiling, int) or not 1 <= ceiling <= 32700:
        raise ValueError("Invalid campaign ceiling")
    if context.get("confirmed") is not True or context.get("connection_confirmed") is not True:
        raise ValueError("Operator must confirm reference plane, setup and connection in context.json")
    for key in ("reference_plane", "analyzer_model", "analyzer_serial", "setup_id",
                "analyzer_settings_evidence", "calibration_date"):
        if not isinstance(context.get(key), str) or not context[key].strip():
            raise ValueError(f"Missing context: {key}")
    for key in ("bit_sha256", "elf_sha256"):
        digest = context.get(key, "")
        if len(digest) != 64 or any(c not in "0123456789abcdefABCDEF" for c in digest):
            raise ValueError(f"Missing 64-digit deployed artifact hash: {key}")
    for p in campaign["points"]:
        if Path(p["file"]).name != p["file"] or sha256(directory / p["file"]) != p["sha256"]:
            raise ValueError("Waveform provenance mismatch")
        raw = np.fromfile(directory / p["file"], dtype="<i2")
        if len(raw) != N*2 or int(np.max(np.abs(raw.astype(np.int32)))) > ceiling:
            raise ValueError("Waveform size/amplitude contract mismatch")
        rms = float(np.sqrt(np.mean(np.sum(raw.reshape(-1, 2).astype(float)**2, axis=1))))
        if not math.isclose(rms, p["rms_complex_counts"], rel_tol=1e-12):
            raise ValueError("Waveform RMS metadata mismatch")
    with (directory / "measurements.csv").open(encoding="utf-8-sig", newline="") as stream:
        rows = list(csv.DictReader(stream))
    by_id = {r["point_id"]: r for r in rows}
    if len(by_id) != len(rows) or set(by_id) != {p["point_id"] for p in campaign["points"]}:
        raise ValueError("Missing/duplicate/unknown measurement IDs")
    thresholds = campaign["thresholds"]
    samples = []
    for point in campaign["points"]:
        row = by_id[point["point_id"]]
        if row["status"] != "measured" or row["overload"].lower() != "false" or not row["evidence"].strip():
            raise ValueError(f"Missing valid measurement / overload check / evidence: {point['point_id']}")
        f, power = float(row["measured_frequency_hz"]), float(row["measured_dbm"])
        if not math.isfinite(f) or not math.isfinite(power):
            raise ValueError("Nonfinite measured value")
        if abs(f-point["frequency_hz"]) > thresholds["max_frequency_error_hz"]:
            raise ValueError("Frequency does not match expected tone; check mirror/NCO/marker")
        samples.append(dict(**point, measured_dbm=power))
    models = []
    for frequency in sorted({p["frequency_hz"] for p in samples}):
        group = [p for p in samples if p["frequency_hz"] == frequency]
        training = [p for p in group if p["role"] == "fit"]
        holdout = [p for p in group if p["role"] == "holdout"]
        if len({p["nominal_amplitude_counts"] for p in training}) < 3 or not holdout:
            raise ValueError("Need >=3 fit amplitudes and independent holdout amplitudes")
        x = np.array([20*np.log10(p["rms_complex_counts"]/2048) for p in training])
        y = np.array([p["measured_dbm"] for p in training])
        slope, intercept = np.polyfit(x, y, 1)
        residual = max(abs(p["measured_dbm"] - intercept - slope*20*np.log10(p["rms_complex_counts"]/2048))
                       for p in training)
        holdout_error = max(abs(p["measured_dbm"] - intercept - slope*20*np.log10(p["rms_complex_counts"]/2048))
                            for p in holdout)
        spans = []
        for amplitude in {p["nominal_amplitude_counts"] for p in group}:
            values = [p["measured_dbm"] for p in group if p["nominal_amplitude_counts"] == amplitude]
            if len(values) < 3:
                raise ValueError("At least three repeated readings per amplitude are required")
            spans.append(max(values)-min(values))
        if (abs(slope-1) > thresholds["max_slope_error"] or
            residual > thresholds["max_fit_residual_db"] or
            holdout_error > thresholds["max_holdout_residual_db"] or
            max(spans) > thresholds["max_repeat_span_db"]):
            raise ValueError("Calibration screening FAILED: nonlinearity / repeatability / holdout error")
        models.append(dict(frequency_hz=frequency, db_per_db=float(slope),
                           dbm_at_2048_rms=float(intercept), fit_max_error_db=float(residual),
                           holdout_max_error_db=float(holdout_error), repeat_span_db=max(spans),
                           min_rms_counts=min(p["rms_complex_counts"] for p in training),
                           max_rms_counts=max(p["rms_complex_counts"] for p in training)))
    return dict(format_version=1, kind="CW_ONLY", status="MEASURED_PROJECT_SCREENING_PASS",
                context=context, models=models, extrapolation="FORBIDDEN",
                source_csv_sha256=sha256(directory / "measurements.csv"),
                campaign_sha256=sha256(directory / "campaign.json"),
                thresholds=thresholds, max_amplitude_counts=ceiling, hardware_actions="NONE",
                limitations=["Not traceable absolute accuracy certification",
                             "Valid only at measured frequencies, reference plane and setup",
                             "Not a wideband QAM/LFM/pulse total-power calibration",
                             "An analyzer amplitude bias is not removed by fitting its readings"])


def recommend_cw(profile, frequency, target, setup_id):
    if profile.get("status") != "MEASURED_PROJECT_SCREENING_PASS" or profile.get("kind") != "CW_ONLY":
        raise ValueError("Not an accepted CW calibration profile")
    if profile["context"]["setup_id"] != setup_id:
        raise ValueError("Setup/reference-plane mismatch")
    if not math.isfinite(frequency) or not math.isfinite(target):
        raise ValueError("Nonfinite request")
    models = [m for m in profile["models"] if m["frequency_hz"] == frequency]
    if len(models) != 1:
        raise ValueError("Frequency not measured; frequency interpolation/extrapolation is disabled")
    m = models[0]
    exponent = (target-m["dbm_at_2048_rms"])/m["db_per_db"]/20
    lower = math.log10(m["min_rms_counts"]/2048)
    upper = math.log10(m["max_rms_counts"]/2048)
    if not lower <= exponent <= upper:
        raise ValueError("Requested power outside measured digital-amplitude range")
    requested_rms = 2048*10**exponent
    amplitude = round(requested_rms)
    ceiling = profile.get("max_amplitude_counts", MAX_AMPLITUDE)
    if not 1 <= amplitude <= ceiling:
        raise ValueError("Requested amplitude exceeds the profile ceiling")
    _, rms, _ = cw_words(frequency, amplitude, ceiling)
    if not m["min_rms_counts"] <= rms <= m["max_rms_counts"]:
        raise ValueError("Quantized amplitude outside measured range")
    predicted = m["dbm_at_2048_rms"] + m["db_per_db"]*20*math.log10(rms/2048)
    return dict(nominal_amplitude_counts=amplitude, predicted_cw_dbm=predicted,
                target_cw_dbm=target, frequency_hz=frequency, hardware_actions="NONE",
                note="Recommendation only. No waveform is loaded or played.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    p = sub.add_parser("prepare")
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--frequencies", type=float, nargs="+", default=[1.79e9])
    p.add_argument("--amplitudes", type=int, nargs="+", default=[512, 724, 1024, 1448, 2048])
    p.add_argument("--ceiling", type=int, default=2048)
    p.add_argument("--repeats", type=int, default=3)
    p.add_argument("--strict-error-db", type=float)
    p = sub.add_parser("fit")
    p.add_argument("--campaign", type=Path, required=True)
    p = sub.add_parser("recommend")
    p.add_argument("--profile", type=Path, required=True)
    p.add_argument("--frequency", type=float, required=True)
    p.add_argument("--dbm", type=float, required=True)
    p.add_argument("--setup-id", required=True)
    args = parser.parse_args()
    if args.command == "prepare":
        campaign = prepare(args.output, args.frequencies, args.amplitudes, args.ceiling,
                           args.repeats, args.strict_error_db)
        print(f"TX_CAL_PREPARED_UNMEASURED points={len(campaign['points'])}; no hardware access")
    elif args.command == "fit":
        profile = fit_campaign(args.campaign)
        path = args.campaign / "calibration.json"
        if path.exists():
            raise ValueError("Calibration output already exists; preserve prior results")
        save_json(path, profile)
        print("TX_CW_CAL_SCREENING_PASS; only measured setup/frequencies are covered")
    else:
        print(json.dumps(recommend_cw(json.loads(args.profile.read_text(encoding="utf-8")),
                                     args.frequency, args.dbm, args.setup_id), indent=2))


if __name__ == "__main__":
    main()
