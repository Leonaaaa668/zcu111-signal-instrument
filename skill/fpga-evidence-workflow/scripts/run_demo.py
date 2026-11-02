#!/usr/bin/env python3
"""Exercise positive and rejection paths using supplied scoped evidence."""
import argparse
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--output-dir', type=Path, required=True)
a = p.parse_args()
a.output_dir.mkdir(parents=True, exist_ok=False)
cases = [
 ('rate_budget', 'PASS', 0, ['rate-budget', '--input-rate-hz', '2400000000', '--decimation', '2,8', '--fft-n', '65536', '--header-bytes', '256', '--displays-per-second', '10']),
 ('rtl_payload', 'PASS', 0, ['compare-numeric', '--reference', 'assets/demo/numeric/reference.csv', '--actual', 'assets/demo/numeric/actual.csv', '--mode', 'exact', '--domain', 'rtl_simulation']),
 ('synthetic_one_bit_fault', 'FAIL', 1, ['compare-numeric', '--reference', 'assets/demo/numeric/reference.csv', '--actual', 'assets/demo/numeric/actual_corrupted_synthetic.csv', '--mode', 'exact', '--domain', 'rtl_simulation']),
 ('actual_reserved_power_point', 'PASS', 0, ['power-check', '--context', 'assets/demo/power/context.json', '--request', 'assets/demo/power/request.json', '--readings', 'assets/demo/power/holdout.csv']),
 ('wrong_frequency_rejected', 'NOT_COMPARABLE', 2, ['power-check', '--context', 'assets/demo/power/context.json', '--request', 'assets/demo/power/wrong_frequency_request.json', '--readings', 'assets/demo/power/holdout.csv']),
 ('non_equivalent_resources_rejected', 'NOT_COMPARABLE', 2, ['compare-reports', '--before', 'assets/demo/reports/before_utilization.rpt', '--after', 'assets/demo/reports/after_utilization.rpt', '--context', 'assets/demo/reports/contexts.json', '--before-timing', 'assets/demo/reports/before_timing.rpt', '--after-timing', 'assets/demo/reports/after_timing.rpt'])
]
results = []
for name, expected, code, args in cases:
    process = subprocess.run([sys.executable, '-X', 'utf8', str(ROOT / 'scripts/engineering_checks.py'), *args],
                             cwd=ROOT, capture_output=True, text=True, encoding='utf-8')
    try:
        output = json.loads(process.stdout)
    except json.JSONDecodeError:
        output = {'status': 'INVALID_JSON', 'stdout': process.stdout, 'stderr': process.stderr}
    passed = process.returncode == code and output.get('status') == expected
    (a.output_dir / (name + '.json')).write_text(json.dumps(output, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    results.append({'case': name, 'expected_checker_status': expected, 'expected_exit': code,
                    'actual_checker_status': output.get('status'), 'actual_exit': process.returncode,
                    'behavior_test_passed': passed})
report = {'status': 'PASS' if all(x['behavior_test_passed'] for x in results) else 'FAIL',
          'cases': results, 'hardware_actions': 'NONE',
          'scope': 'Offline tool behavior; correct rejection is a successful test. No new RF measurements.'}
(a.output_dir / 'summary.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
print(json.dumps(report, ensure_ascii=False, indent=2))
raise SystemExit(0 if report['status'] == 'PASS' else 1)
