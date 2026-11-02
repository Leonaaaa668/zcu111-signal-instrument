"""Generate (ONLY) a CW binary at a calibrated requested power.

No JTAG/network/serial access, no playback. Reference plane is the unchanged
2026-10-26 J7 cable end into FSVA3030 /101148 at1.790GHz. Not QAM power control.
"""
import argparse
import json
from pathlib import Path
from tx_power_cal import recommend_cw, cw_words, save_json, sha256

ROOT = Path(__file__).resolve().parents[2]
PROFILE = ROOT/"data/calibration/historical_fsva/calibration.json"
VERIFICATION = ROOT/"data/calibration/historical_fsva/independent_verification.json"


def generate(dbm, output):
    if output.exists() or output.with_suffix(".json").exists():
        raise ValueError("Use a new output file; previous requests are preserved")
    profile = json.loads(PROFILE.read_text())
    verification = json.loads(VERIFICATION.read_text())
    if (verification["status"] != "INDEPENDENT_RELATIVE_POWER_CHECK_PASS" or
        verification["frozen_profile_sha256"] != sha256(PROFILE)):
        raise ValueError("Measured model/independent verification hash mismatch")
    if not -50 <= dbm <= -10:
        raise ValueError("Initial independent verification covers -50..-10dBm only")
    result = recommend_cw(profile, 1.79e9, dbm, profile["context"]["setup_id"])
    words, rms, offset = cw_words(1.79e9, result["nominal_amplitude_counts"], profile["max_amplitude_counts"])
    output.parent.mkdir(parents=True, exist_ok=True)
    words.tofile(output)
    result.update(file=str(output), bytes=words.nbytes, rms_counts=rms, iq_offset_hz=offset,
                  sha256=sha256(output), profile_sha256=sha256(PROFILE),
                  verification_sha256=sha256(VERIFICATION),
                  reference_plane=profile["context"]["reference_plane"],
                  setup_id=profile["context"]["setup_id"],
                  independent_check="Nine setpoints, ten readings each, one same-analyzer session",
                  absolute_accuracy_below_0p1_db="NOT_CLAIMED",
                  note="GENERATED ONLY. Changing cable/frequency/setup invalidates this calibration.")
    save_json(output.with_suffix(".json"), result)
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--dbm", type=float, required=True)
    p.add_argument("--output", type=Path, required=True)
    a = p.parse_args()
    generate(a.dbm, a.output.resolve())
