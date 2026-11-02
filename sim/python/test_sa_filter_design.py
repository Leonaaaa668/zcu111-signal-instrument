"""Filter coefficient and lane-order controls, NOT FPGA timing tests."""
import unittest
import numpy as np
from sa_filter_design import halfband, characterize, scalar_decimate, parallel4_to2_reference, cascade_alias_check, SCALE
from generate_sa_decim_vectors import quantize


class FilterDesignTests(unittest.TestCase):
    def test_quantized_gain_symmetry_and_bands(self):
        for length, fs, pass_edge, stop_edge in [(15, 1.2e9, 50e6, 550e6),
                                                (23, 600e6, 50e6, 250e6),
                                                (63, 300e6, 50e6, 100e6)]:
            q = halfband(length)
            self.assertEqual(np.sum(q), SCALE)
            np.testing.assert_array_equal(q, q[::-1])
            metrics = characterize(q, fs, pass_edge, stop_edge)
            self.assertLess(metrics["pass_max_abs_db"], .01)
            self.assertLess(metrics["stop_max_db"], -70)

    def test_4lane_is_time_sequence_not_channels(self):
        rng = np.random.default_rng(1853)
        x = rng.normal(size=1024)+1j*rng.normal(size=1024)
        q = halfband(15)
        np.testing.assert_allclose(parallel4_to2_reference(x, q), scalar_decimate(x, q), atol=1e-14)

    def test_cascade_all_input_alias_images(self):
        self.assertLess(cascade_alias_check([halfband(n) for n in (15, 23, 63)]), -70)

    def test_convergent_rounding_and_saturation_reference(self):
        rounded, saturated = quantize(np.array([2,6,-2,-6,10,-10],dtype=np.int64),2)
        np.testing.assert_array_equal(rounded,[0,2,0,-2,2,-2])
        self.assertFalse(np.any(saturated))
        rounded,saturated=quantize(np.array([1<<30,-(1<<30)],dtype=np.int64),2)
        np.testing.assert_array_equal(rounded,[(1<<23)-1,-(1<<23)])
        self.assertTrue(np.all(saturated))


if __name__ == "__main__":
    unittest.main(verbosity=2)
