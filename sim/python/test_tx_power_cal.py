"""Synthetic calibration controls, NOT RF measurements. No device access."""
import csv
import json
import tempfile
import unittest
from pathlib import Path
import numpy as np
from tx_power_cal import (prepare, fit_campaign, recommend_cw, save_json, cw_words, FIELDS)

TEST_PARENT = Path(__file__).resolve().parents[2] / "generated/python_tests"
TEST_PARENT.mkdir(parents=True, exist_ok=True)


class CalibrationTests(unittest.TestCase):
    def make_measured(self, root, nonlinear=False, wrong_frequency=False, overload=False):
        directory = root / "synthetic_campaign"
        c = prepare(directory)
        context = json.loads((directory / "context.json").read_text())
        context.update(confirmed=True, connection_confirmed=True, analyzer_model="SYNTHETIC_ONLY",
                       analyzer_serial="NOT_HARDWARE", analyzer_settings_evidence="synthetic test",
                       setup_id="synthetic", calibration_date="synthetic",
                       bit_sha256="0"*64, elf_sha256="0"*64)
        save_json(directory / "context.json", context)
        with (directory / "measurements.csv").open("w", newline="", encoding="utf-8") as stream:
            writer = csv.DictWriter(stream, fieldnames=FIELDS)
            writer.writeheader()
            for p in c["points"]:
                power = -30 + 20*np.log10(p["rms_complex_counts"]/2048)
                power += (p["repeat"]-2)*.01
                if nonlinear and p["nominal_amplitude_counts"] == 1448:
                    power += 1
                writer.writerow(dict(point_id=p["point_id"], status="measured",
                    measured_frequency_hz=p["frequency_hz"] + (1e6 if wrong_frequency else 0),
                    measured_dbm=power, overload=str(overload).lower(), evidence="synthetic"))
        return directory

    def test_pending_cannot_become_calibration(self):
        with tempfile.TemporaryDirectory(prefix="synthetic_cal_", dir=TEST_PARENT) as root:
            directory = Path(root) / "pending"
            prepare(directory)
            with self.assertRaises(ValueError):
                fit_campaign(directory)
            with self.assertRaises(ValueError):
                prepare(directory)

    def test_fit_holdout_and_recommendation(self):
        with tempfile.TemporaryDirectory(prefix="synthetic_cal_", dir=TEST_PARENT) as root:
            directory = self.make_measured(Path(root))
            profile = fit_campaign(directory)
            self.assertAlmostEqual(profile["models"][0]["db_per_db"], 1, places=9)
            request = recommend_cw(profile, 1.79e9, -36.0206, "synthetic")
            self.assertEqual(request["nominal_amplitude_counts"], 1024)
            for f, p, setup in [(1.81e9, -36, "synthetic"), (1.79e9, -20, "synthetic"),
                                (1.79e9, -80, "synthetic"), (1.79e9, -36, "different")]:
                with self.assertRaises(ValueError):
                    recommend_cw(profile, f, p, setup)

    def test_bad_measurements_rejected(self):
        for kwargs in (dict(nonlinear=True), dict(wrong_frequency=True), dict(overload=True)):
            with tempfile.TemporaryDirectory(prefix="synthetic_cal_", dir=TEST_PARENT) as root:
                directory = self.make_measured(Path(root), **kwargs)
                with self.assertRaises(ValueError):
                    fit_campaign(directory)

    def test_binary_tampering_rejected(self):
        with tempfile.TemporaryDirectory(prefix="synthetic_cal_", dir=TEST_PARENT) as root:
            directory = self.make_measured(Path(root))
            waveform = next(directory.glob("*.bin"))
            with waveform.open("r+b") as stream:
                stream.write(b"bad")
            with self.assertRaises(ValueError):
                fit_campaign(directory)

    def test_frequency_sign_and_limits(self):
        for frequency, expected in [(1.79e9, 10e6), (1.81e9, -10e6)]:
            words, _, offset = cw_words(frequency, 1024)
            z = words[:, 0].astype(float) + 1j*words[:, 1]
            peak = np.fft.fftfreq(len(z), 1/1.2e9)[np.argmax(abs(np.fft.fft(z)))]
            self.assertEqual(offset, expected)
            self.assertAlmostEqual(peak, expected)
        for frequency, amplitude in [(1.8e9, 1024), (1.79e9, 4096), (1.790000001e9, 1024)]:
            with self.assertRaises(ValueError):
                cw_words(frequency, amplitude)

    def test_high_ceiling_is_explicit_and_default_stays_low(self):
        with self.assertRaises(ValueError):
            cw_words(1.79e9, 32700)
        words, _, _ = cw_words(1.79e9, 32700, ceiling=32700)
        self.assertLessEqual(int(np.max(abs(words.astype(np.int32)))), 32700)
        for ceiling in (32768, 0):
            with self.assertRaises(ValueError):
                cw_words(1.79e9, 2048, ceiling=ceiling)


if __name__ == "__main__":
    unittest.main(verbosity=2)
