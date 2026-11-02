#!/usr/bin/env python3
"""Offline behavioral checks; all generated fixtures are synthetic and temporary."""
import copy
import argparse
import hashlib
import json
from pathlib import Path
import tempfile
import sys
import shutil
import uuid
import unittest
from engineering_checks import compare_reports, read_timing, compare_numeric, check_power, verify_manifest, rate_budget

ROOT = Path(__file__).resolve().parents[1]
TEMP_ROOT = None


class Checks(unittest.TestCase):
    def setUp(self):
        self.base = Path(TEMP_ROOT or tempfile.gettempdir()).resolve()
        self.p = self.base / ('fpga-skill-test-' + uuid.uuid4().hex)
        # Inherit the caller-approved workspace ACL; do not set a private Windows ACL.
        self.p.mkdir()

    def tearDown(self):
        target = self.p.resolve()
        if not target.is_relative_to(self.base) or target == self.base:
            raise ValueError('Test cleanup escaped its own root')
        shutil.rmtree(target)

    def write(self, name, text):
        p = self.p / name
        p.write_text(text, encoding='utf-8')
        return p

    def report(self, name='report.rpt', stage='Routed', lut=100, tool='Vivado v.2025.2'):
        return self.write(name, f'''| Tool Version : {tool}
| Device : synthetic-device
| Design State : {stage}
| Design : synthetic-design
| CLB LUTs | {lut} | 0 | 0 | 1000 | 10 |
| CLB Registers | 200 | 0 | 0 | 2000 | 10 |
| Block RAM Tile | 2.5 | 0 | 0 | 10 | 25 |
| DSPs | 4 | 0 | 0 | 40 | 10 |
''')

    def context(self):
        x = {'clock_target_hz': {'dsp': 100000000}, 'sample_rate_hz': 10000000,
             'fft_max_n': 1024, 'data_width_bits': 16, 'channel_count': 1,
             'feature_set': {'zoom': [0]}, 'included_blocks': ['synthetic_filter'],
             'constraints_id': 'synthetic_constraints_1'}
        return {'claim': 'equivalent_function', 'before': x, 'after': copy.deepcopy(x)}

    def series(self, name, values):
        return self.write(name, 'index,value\n' + ''.join(f'{i},{v}\n' for i, v in enumerate(values)))

    def power(self, values=(-25.01, -25.01, -25.00)):
        c = {'waveform': 'CW', 'frequency_hz': 1790000000, 'reference_plane': 'synthetic_port',
             'setup_id': 'synthetic_setup', 'target_range_dbm': [-40, -20], 'fit_point_ids': ['fit_1']}
        q = dict(c, target_dbm=-25, unit='dBm', tolerance_db=0.1)
        f = self.write('power.csv', 'point_id,role,measured_dbm\n' + ''.join(f'holdout_1,holdout,{v}\n' for v in values))
        return c, q, f

    def test_same_conditions_allow_resource_delta(self):
        a, b = self.report('a.rpt'), self.report('b.rpt', lut=80)
        r = compare_reports(a, b, self.context())
        self.assertEqual(r['status'], 'PASS')
        self.assertEqual(r['resource_change']['lut']['change_percent'], -20)

    def test_stage_mismatch_not_comparable(self):
        r = compare_reports(self.report('a.rpt'), self.report('b.rpt', stage='Synthesized'), self.context())
        self.assertEqual(r['status'], 'NOT_COMPARABLE')
        self.assertIsNone(r['resource_change'])

    def test_feature_pruning_not_equivalent(self):
        c = self.context(); c['before']['feature_set']['zoom'] = [0, 1, 2]
        self.assertEqual(compare_reports(self.report('a.rpt'), self.report('b.rpt'), c)['status'], 'NOT_COMPARABLE')

    def test_unknown_constraints_not_guessed(self):
        c = self.context(); del c['after']['constraints_id']
        self.assertEqual(compare_reports(self.report('a.rpt'), self.report('b.rpt'), c)['status'], 'NOT_COMPARABLE')

    def test_missing_resource_row_is_not_zero(self):
        a = self.report(); a.write_text(a.read_text().replace('| DSPs | 4 |', '| other | 4 |'))
        with self.assertRaises(ValueError):
            compare_reports(a, self.report('b.rpt'), self.context())

    def test_native_failed_timing_detected(self):
        p = ROOT / 'assets/demo/reports/before_timing.rpt'
        r = read_timing(p)
        self.assertFalse(r['met'])
        self.assertEqual(str(r['wns_ns']), '-0.109')

    def test_actual_historical_pair_rejected(self):
        base = ROOT / 'assets/demo/reports'
        c = json.loads((base / 'contexts.json').read_text())
        r = compare_reports(base / 'before_utilization.rpt', base / 'after_utilization.rpt', c)
        self.assertEqual(r['status'], 'NOT_COMPARABLE')
        self.assertIsNone(r['resource_change'])

    def test_exact_uint64_does_not_round_to_float(self):
        a = self.series('a.csv', [2**63, 2**63+1])
        b = self.series('b.csv', [2**63, 2**63])
        r = compare_numeric(a, b, 'exact', 'rtl_simulation')
        self.assertEqual(r['status'], 'FAIL')
        self.assertEqual(r['first_mismatch_indices'], [1])

    def test_integer_exact_pass(self):
        a = self.series('a.csv', [-32768, 0, 32767])
        self.assertEqual(compare_numeric(a, a, 'exact', 'digital_file')['status'], 'PASS')

    def test_explicit_tolerance(self):
        a = self.series('a.csv', [1, 100]); b = self.series('b.csv', [1.001, 100.001])
        self.assertEqual(compare_numeric(a, b, 'tolerance', 'reference_data', '.002')['status'], 'PASS')

    def test_missing_sample_rejected(self):
        a = self.write('a.csv', 'index,value\n0,1\n2,3\n')
        with self.assertRaises(ValueError):
            compare_numeric(a, a, 'exact', 'rtl_simulation')

    def test_duplicate_sample_rejected(self):
        a = self.write('a.csv', 'index,value\n0,1\n0,2\n')
        with self.assertRaises(ValueError):
            compare_numeric(a, a, 'exact', 'rtl_simulation')

    def test_nan_rejected(self):
        a = self.series('a.csv', ['NaN'])
        with self.assertRaises(ValueError):
            compare_numeric(a, a, 'tolerance', 'reference_data')

    def test_exact_mode_cannot_silently_add_tolerance(self):
        a = self.series('a.csv', [1])
        with self.assertRaises(ValueError):
            compare_numeric(a, a, 'exact', 'rtl_simulation', '1')

    def test_power_spot_agreement_not_certification(self):
        r = check_power(*self.power())
        self.assertEqual(r['status'], 'PASS')
        self.assertFalse(r['absolute_accuracy_certified'])
        self.assertAlmostEqual(float(r['mean_error_db']), -.0066666666667)

    def test_wrong_frequency_rejected(self):
        c, q, f = self.power(); q['frequency_hz'] = 1300000000
        self.assertEqual(check_power(c, q, f)['status'], 'NOT_COMPARABLE')

    def test_cw_correction_cannot_be_used_for_qam(self):
        c, q, f = self.power(); q['waveform'] = '16QAM'
        self.assertEqual(check_power(c, q, f)['status'], 'NOT_COMPARABLE')

    def test_changed_cable_or_setup_rejected(self):
        c, q, f = self.power(); q['setup_id'] = 'different_setup'
        self.assertEqual(check_power(c, q, f)['status'], 'NOT_COMPARABLE')

    def test_holdout_cannot_be_fit_data(self):
        c, q, f = self.power(); c['fit_point_ids'].append('holdout_1')
        with self.assertRaises(ValueError):
            check_power(c, q, f)

    def test_outside_tolerance_fails(self):
        self.assertEqual(check_power(*self.power([-24.5]*3))['status'], 'FAIL')

    def test_manifest_detects_tampering(self):
        f = self.write('file.txt', 'content')
        m = self.write('sha.txt', hashlib.sha256(f.read_bytes()).hexdigest() + '  file.txt\n')
        self.assertEqual(verify_manifest(self.p, m)['status'], 'PASS')
        f.write_text('changed')
        self.assertEqual(verify_manifest(self.p, m)['status'], 'FAIL')

    def test_manifest_path_escape_rejected(self):
        m = self.write('sha.txt', '0'*64 + '  ../outside.txt\n')
        with self.assertRaises(ValueError):
            verify_manifest(self.p, m)

    def test_manifest_duplicate_refused(self):
        line = '0'*64 + '  file.txt\n'
        with self.assertRaises(ValueError):
            verify_manifest(self.p, self.write('sha.txt', line*2))

    def test_project_rate_budget_separates_iq_from_spectrum(self):
        r = rate_budget('2400000000', '2,8', 2, 16, 65536, 64, 256, '10')
        self.assertEqual(r['stage_sample_rates_hz'], [1200000000, 150000000])
        self.assertEqual(r['raw_iq_bytes_per_second'], 600000000)
        self.assertEqual(r['spectrum_packet_bytes'], 524544)
        self.assertEqual(r['display_payload_bytes_per_second'], 5245440)

    def test_other_board_rate_budget_is_parameterized(self):
        r = rate_budget('100000000', '4', 2, 12, 1024, 32, 32, '20')
        self.assertEqual(r['raw_iq_bytes_per_second'], 75000000)
        self.assertEqual(r['spectrum_packet_bytes'], 4128)

    def test_bad_decimator_rejected(self):
        with self.assertRaises(ValueError):
            rate_budget('100', '2,0', 2, 16, 1024, 64, 0, '10')

    def test_empty_feature_table_is_unknown(self):
        c = self.context(); c['before']['feature_set'] = {}; c['after']['feature_set'] = {}
        self.assertEqual(compare_reports(self.report('a.rpt'), self.report('b.rpt'), c)['status'], 'NOT_COMPARABLE')

    def test_manifest_alias_duplicate_refused(self):
        text = '0'*64 + '  file.txt\n' + '0'*64 + '  ./file.txt\n'
        with self.assertRaises(ValueError):
            verify_manifest(self.p, self.write('sha.txt', text))

    def test_two_power_readings_are_insufficient(self):
        with self.assertRaises(ValueError):
            check_power(*self.power([-25, -25]))

    def test_fractional_data_cannot_pass_exact_integer_mode(self):
        a = self.series('a.csv', ['1.5'])
        with self.assertRaises(ValueError):
            compare_numeric(a, a, 'exact', 'digital_file')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument('--work-root', type=Path)
    options, rest = parser.parse_known_args()
    if options.work_root:
        options.work_root.mkdir(parents=True, exist_ok=True)
        TEMP_ROOT = options.work_root
    unittest.main(argv=[sys.argv[0], *rest], verbosity=2)
