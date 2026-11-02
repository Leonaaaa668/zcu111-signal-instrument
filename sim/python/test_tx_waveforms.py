"""Offline unit checks only. No JTAG, UART, instrument or network access."""
import unittest
import numpy as np
from tx_waveforms import periodic_interpolate, iq_words, rrc_taps, synthesize, ROOT


class WaveformTests(unittest.TestCase):
    def test_interpolation_gain_and_sign(self):
        n, m = 80, 16
        for bin_index in (0, 1, -1, 3, -3):
            low = np.exp(2j*np.pi*bin_index*np.arange(m)/m)
            high = periodic_interpolate(low, n)
            np.testing.assert_allclose(high, np.exp(2j*np.pi*bin_index*np.arange(n)/n), atol=1e-12)
            np.testing.assert_allclose(high[::5], low, atol=1e-12)
        # An even-length real Nyquist sequence must interpolate to a real cosine.
        high = periodic_interpolate((-1.)**np.arange(m), n)
        np.testing.assert_allclose(high, np.cos(np.pi*np.arange(n)/5), atol=1e-12)

    def test_binary_layout(self):
        words = iq_words(np.array([1-2j, -32768+32767j, 2048-2048j]))
        self.assertEqual(words.tobytes().hex(), '0100feff0080ff7f000800f8')

    def test_rrc_and_all_syntheses(self):
        h = rrc_taps()
        self.assertAlmostEqual(float(np.sum(h*h)), 1.)
        np.testing.assert_array_equal(h, h[::-1])
        for k in range(12):
            x, symbols, _, _, _, meta = synthesize(k)
            self.assertEqual(len(x), 600000)
            self.assertTrue(np.all(np.isfinite(x)))
            if meta['type'].startswith(('PSK', 'QAM')):
                self.assertEqual(len(np.unique(symbols)), meta['modulation_order'])
                self.assertEqual(meta['symbol_rate_hz'], 5e6)

    def test_contract_constants(self):
        header = (ROOT.parent/'src/baremetal_reference/tx_memory_map.h').read_text()
        host = (ROOT.parent/'src/baremetal_reference/tx_command.tcl').read_text()
        for value in ('0x24000000', '0x28000000', '0x54585757'):
            self.assertIn(value, header)
        for value in ('0x24000000', '0x54585757', '67108864', '1200000000'):
            self.assertIn(value, host)
        self.assertIn('64u*1024u*1024u', header)
        self.assertIn('4u*1024u*1024u', header)


if __name__ == '__main__':
    unittest.main(verbosity=2)
