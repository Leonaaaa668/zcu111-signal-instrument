"""Independent mathematical controls only, not board-level receiver tests."""
import unittest
import numpy as np
from sa_reference import (Request, plan, hann_metrics, bin_power, vbw_parameters,
                          VideoFilter, ScanAverage, HALF_POWER_3DB)


class SpectrumReferenceTests(unittest.TestCase):
    def test_hann_width_enbw_and_gain(self):
        m = hann_metrics(150e6, 65536)
        self.assertAlmostEqual(m["rbw_3db_hz"], 3291.789165, delta=.002)
        self.assertAlmostEqual(m["enbw_hz"], 1.5*150e6/65536)
        self.assertAlmostEqual(m["coherent_gain"], .5)

    def test_tone_power_scale_and_fft_length(self):
        for n in (128, 1024, 32768):
            z = 16384*np.exp(2j*np.pi*7*np.arange(n)/n)
            frequencies, p = bin_power(z, 150e6)
            self.assertAlmostEqual(float(np.max(p)), .25, places=12)
            self.assertAlmostEqual(frequencies[np.argmax(p)], 7*150e6/n)
            # Hann tone-normalized sum is ENBW_bins * signal power, NOT signal power.
            self.assertAlmostEqual(float(np.sum(p)), 1.5*.25, places=12)

    def test_vbw_exact_response_and_time_state(self):
        video = vbw_parameters(150e6/32768, 1000)
        a = video["coefficient_a"]
        omega = 2*np.pi*1000/(150e6/32768)
        ratio = (1-a)**2/(1+a*a-2*a*np.cos(omega))
        self.assertAlmostEqual(ratio, HALF_POWER_3DB, places=12)
        self.assertLessEqual(a**video["establishment_windows"], .01)
        f = VideoFilter(2, a)
        for frame in range(video["establishment_windows"]):
            y = f.update([1, 4], frame)
        np.testing.assert_allclose(y, [1, 4], rtol=.01)
        with self.assertRaises(ValueError):
            f.update([1, 4], 100)
        with self.assertRaises(ValueError):
            f.update([1, 4], frame+1)
        with self.assertRaises(ValueError):
            vbw_parameters(100, 50)

    def test_grid_overlap_and_rbw_candidates(self):
        for span, rbw, minimum_segments in [(100e6, 3300, 1), (1e9, 10e3, 13),
                                             (100e6, 1000, 5), (100e6, 100, 80)]:
            p = plan(Request(span_hz=span, rbw_hz=rbw))
            self.assertGreaterEqual(p["segment_count"], minimum_segments)
            self.assertLessEqual(p["actual"]["rbw_3db_hz"], rbw)
            self.assertEqual(p["actual_sweep_time_s"], None)
            self.assertLess(p["grid"]["endpoint_gap_hz"], p["grid"]["step_hz"])
            half = (p["actual"]["retained_bins"]-1)//2
            left, right = p["grid"]["first_bin"], p["grid"]["last_bin"]
            previous_right = left
            for segment in p["segments"]:
                center = segment["center_grid_bin"]
                self.assertLessEqual(center-half, previous_right)
                previous_right = center+half
                self.assertLess(abs(segment["nco_grid_error_hz"]), .001)
            self.assertGreaterEqual(previous_right, right)
        p = plan(Request(span_hz=100e6, rbw_hz=10e3, vbw_hz=1e3))
        self.assertEqual(p["actual"]["fft_n"], 32768)
        self.assertEqual(p["actual"]["video"]["establishment_windows"], 4)

    def test_bad_requests_rejected(self):
        for request in [Request(center_hz=1.3e9), Request(span_hz=-1), Request(rbw_hz=0),
                        Request(vbw_hz=1e9), Request(sweep_time_s=.01),
                        Request(span_hz=1e9, rbw_hz=1), Request(average_count=0)]:
            with self.assertRaises(ValueError):
                plan(request)

    def test_average_linear_not_db_epoch_and_hold(self):
        average = ScanAverage(epoch=7, grid_id="grid_a", points=2, count=2)
        common = dict(epoch=7, grid_id="grid_a", complete=True, quality_ok=True)
        average.add([1, 1], sweep_id=4, **common)
        with self.assertRaises(ValueError):
            average.add([100, 1], sweep_id=4, **common)
        with self.assertRaises(ValueError):
            average.add([100, 1], sweep_id=5, epoch=8, grid_id="grid_a", complete=True, quality_ok=True)
        result = average.add([100, 1], sweep_id=5, **common)
        np.testing.assert_array_equal(result, [50.5, 1])  # 17.03dB, not mean(0,20)=10dB
        with self.assertRaises(ValueError):
            average.add([100, 1], sweep_id=6, **common)


if __name__ == "__main__":
    unittest.main(verbosity=2)
