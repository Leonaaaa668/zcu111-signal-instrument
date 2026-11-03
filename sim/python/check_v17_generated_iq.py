"""Check C++ test-generated CW I/Q, not ADC/DAC measurements. No board access."""
import argparse
import hashlib
import json
from pathlib import Path
import numpy as np


def check(run):
    rows = []
    for rf_mhz, expected in [(1300, 500e6), (1800, 0.), (2300, -500e6)]:
        p = run / f"cw_{rf_mhz}MHz.bin"
        meta = json.loads(p.with_suffix('.bin.json').read_text())
        assert p.stat().st_size == 48000000
        samples = np.memmap(p, dtype='<i2', mode='r').reshape(-1, 2)
        z = samples[:65537, 0].astype(float) + 1j * samples[:65537, 1].astype(float)
        frequency = float(np.angle(np.vdot(z[:-1], z[1:])) * 1.2e9 / (2 * np.pi))
        assert abs(frequency - expected) < 1000
        first, last = complex(*samples[0]), complex(*samples[-1])
        error = float(np.angle(first * last.conjugate() * np.exp(-2j * np.pi * expected / 1.2e9)))
        assert abs(error) < .001
        digest = hashlib.sha256(p.read_bytes()).hexdigest()
        assert digest == meta['sha256'] and meta['offset_hz'] == expected
        assert meta['sample_rate_hz'] == 1.2e9 and meta['rf_frequency_hz'] == rf_mhz * 1e6
        assert meta['power_frequency_estimated'] is True
        assert meta['absolute_power_accuracy_certified'] is False
        rows.append({'waveform': p.name, 'bytes': p.stat().st_size, 'sha256': digest,
                     'rf_setting_hz': meta['rf_frequency_hz'], 'expected_iq_frequency_hz': expected,
                     'estimated_iq_frequency_hz_from_generated_samples': frequency,
                     'cyclic_boundary_phase_error_radians': error,
                     'power_frequency_scope': meta['power_frequency_scope']})
    return {'status': 'GENERATED_IQ_SAMPLES_FREQUENCY_HASH_AND_CYCLIC_BOUNDARY_PASS',
            'source': 'Native C++ V17 host test output; not ADC/DAC capture',
            'sample_rate_hz': 1200000000, 'records': rows, 'rf_measurement': False}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run', type=Path, help='One cw-frequency-estimate-evidence/run_* directory')
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    result = check(args.run)
    text = json.dumps(result, indent=2) + '\n'
    if args.output:
        args.output.write_text(text, encoding='utf-8', newline='\n')
    print(text)
