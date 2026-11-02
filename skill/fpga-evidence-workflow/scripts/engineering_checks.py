#!/usr/bin/env python3
"""Read-only, standard-library checks. Never opens JTAG/MMIO/serial/network."""
from __future__ import annotations
import argparse
import csv
from decimal import Decimal, InvalidOperation
import hashlib
import json
from pathlib import Path, PurePosixPath
import re


def number(value):
    try:
        v = Decimal(str(value).replace(',', ''))
    except InvalidOperation as e:
        raise ValueError('Invalid number') from e
    if not v.is_finite():
        raise ValueError('Non-finite value refused')
    return v


def plain(value):
    if isinstance(value, Decimal):
        return int(value) if value == value.to_integral_value() else float(value)
    raise TypeError(type(value).__name__)


def read_json(path):
    return json.loads(Path(path).read_text(encoding='utf-8-sig'))


def read_utilization(path):
    text = Path(path).read_text(encoding='utf-8-sig', errors='replace')
    result = {'source_sha256': hashlib.sha256(Path(path).read_bytes()).hexdigest()}
    for key, label in [('tool', 'Tool Version'), ('device', 'Device'), ('stage', 'Design State'), ('design', 'Design')]:
        m = re.search(r'^\|\s*' + label + r'\s*:\s*([^\r\n|]+)', text, re.M)
        if not m:
            raise ValueError('Missing report header: ' + label)
        result[key] = m.group(1).strip()
    result['resources'] = {}
    for key, site in [('lut', r'(?:CLB|Slice) LUTs'), ('registers', r'(?:CLB|Slice) Registers'),
                      ('bram_tiles', 'Block RAM Tile'), ('dsp', r'DSPs?')]:
        m = re.search(r'^\|\s*' + site + r'\*?\s*\|\s*([\d,.]+)\s*\|', text, re.M)
        if not m:
            raise ValueError('Missing resource row: ' + key)
        result['resources'][key] = number(m.group(1))
    return result


def read_timing(path):
    lines = Path(path).read_text(encoding='utf-8-sig', errors='replace').splitlines()
    for i, line in enumerate(lines):
        if 'WNS(ns)' in line and 'WPWS(ns)' in line:
            for row in lines[i+1:i+8]:
                vals = row.split()
                if len(vals) != 12:
                    continue
                try:
                    values = list(map(number, vals))
                except ValueError:
                    continue
                return {'wns_ns': values[0], 'whs_ns': values[4], 'wpws_ns': values[8],
                        'setup_failing_endpoints': values[2], 'hold_failing_endpoints': values[6],
                        'pulse_failing_endpoints': values[10],
                        'met': all(values[k] >= 0 for k in [0, 4, 8]) and all(values[k] == 0 for k in [2, 6, 10])}
    raise ValueError('No complete native timing summary table')


def compare_reports(before, after, context, before_timing=None, after_timing=None):
    if not isinstance(context, dict) or not isinstance(context.get('before'), dict) or not isinstance(context.get('after'), dict):
        raise ValueError('Context requires before/after objects')
    a, b = read_utilization(before), read_utilization(after)
    reasons = []
    for key in ['device', 'tool', 'stage', 'design']:
        if a[key] != b[key]:
            reasons.append('report.' + key + ' differs')
    required = ['clock_target_hz', 'sample_rate_hz', 'fft_max_n', 'data_width_bits',
                'channel_count', 'feature_set', 'included_blocks', 'constraints_id']
    for key in required:
        va = context.get('before', {}).get(key)
        vb = context.get('after', {}).get(key)
        if va in (None, '', [], {}) or vb in (None, '', [], {}):
            reasons.append('condition.' + key + ' unknown')
        elif va != vb:
            reasons.append('condition.' + key + ' differs')
    if context.get('claim') != 'equivalent_function':
        reasons.append('claim is not equivalent_function')
    result = {'status': 'NOT_COMPARABLE' if reasons else 'PASS', 'reasons': reasons,
              'before': a, 'after': b, 'metadata_is_user_declared': True,
              'scope': 'Report/context comparison only; verify declared conditions against source/config.'}
    for key, path in [('before_timing', before_timing), ('after_timing', after_timing)]:
        if path:
            result[key] = read_timing(path)
    if not reasons:
        result['resource_change'] = {
            key: {'after_minus_before': b['resources'][key] - value,
                  'change_percent': (b['resources'][key] - value) * 100 / value if value else None}
            for key, value in a['resources'].items()}
    else:
        result['resource_change'] = None  # no misleading efficiency percentage
    return result


def read_series(path):
    points = {}
    with Path(path).open(encoding='utf-8-sig', newline='') as f:
        reader = csv.DictReader(f)
        if not {'index', 'value'}.issubset(reader.fieldnames or []):
            raise ValueError('CSV requires index,value')
        for row in reader:
            index = int(row['index'])
            if index < 0 or index in points:
                raise ValueError('Negative/duplicate sample index')
            points[index] = number(row['value'])
    if not points or sorted(points) != list(range(len(points))):
        raise ValueError('Empty, missing or nonzero-origin indices')
    return points


def compare_numeric(reference, actual, mode, domain, atol='0', rtol='0'):
    a, b = read_series(reference), read_series(actual)
    if a.keys() != b.keys():
        return {'status': 'FAIL', 'reason': 'sample count/index set differs', 'domain': domain}
    abs_tol, rel_tol = number(atol), number(rtol)
    if min(abs_tol, rel_tol) < 0:
        raise ValueError('Negative tolerance')
    if mode == 'exact' and (abs_tol or rel_tol):
        raise ValueError('Exact comparison cannot use tolerance')
    failures = []
    maximum = Decimal(0)
    for i in sorted(a):
        if mode == 'exact' and (a[i] != a[i].to_integral_value() or b[i] != b[i].to_integral_value()):
            raise ValueError('Exact mode requires integer samples')
        error = abs(b[i] - a[i])
        maximum = max(maximum, error)
        if error > abs_tol + rel_tol * abs(a[i]):
            failures.append(i)
    return {'status': 'FAIL' if failures else 'PASS', 'samples': len(a), 'mode': mode,
            'domain': domain, 'max_absolute_error': maximum,
            'mismatch_count': len(failures), 'first_mismatch_indices': failures[:8],
            'rf_evm_or_ber_certified': False,
            'scope': 'Aligned supplied samples only; scale/bin order/latency must be established separately.'}


def check_power(context, request, readings_path):
    reasons = []
    for key in ['waveform', 'frequency_hz', 'reference_plane', 'setup_id']:
        if context.get(key) is None or request.get(key) is None:
            reasons.append('missing ' + key)
        elif context[key] != request[key]:
            reasons.append('outside calibrated ' + key)
    if request.get('unit') != 'dBm':
        reasons.append('readings require dBm; convert dBW explicitly before checking')
    target = number(request['target_dbm'])
    limits = context.get('target_range_dbm')
    if not isinstance(limits, list) or len(limits) != 2 or not number(limits[0]) <= target <= number(limits[1]):
        reasons.append('target outside measured range')
    if reasons:
        return {'status': 'NOT_COMPARABLE', 'reasons': reasons, 'absolute_accuracy_certified': False}
    with Path(readings_path).open(encoding='utf-8-sig', newline='') as f:
        reader = csv.DictReader(f)
        if not {'point_id', 'role', 'measured_dbm'}.issubset(reader.fieldnames or []):
            raise ValueError('CSV requires point_id,role,measured_dbm')
        rows = list(reader)
    if len(rows) < 3:
        raise ValueError('At least three repeated readings are required by this spot-check protocol')
    ids = {row['point_id'] for row in rows}
    roles = {row['role'] for row in rows}
    if len(ids) != 1 or roles != {'holdout'}:
        raise ValueError('Use one reserved holdout point, not fitted calibration observations')
    point = next(iter(ids))
    if point in context.get('fit_point_ids', []):
        raise ValueError('Holdout point was used to estimate correction')
    values = [number(row['measured_dbm']) for row in rows]
    mean = sum(values) / len(values)
    tolerance = number(request.get('tolerance_db', '0.1'))
    if tolerance <= 0:
        raise ValueError('Tolerance must be positive')
    mean_error = mean - target
    return {'status': 'PASS' if abs(mean_error) <= tolerance else 'FAIL',
            'target_dbm': target, 'mean_dbm': mean, 'mean_error_db': mean_error,
            'repeat_peak_to_peak_db': max(values) - min(values), 'readings': len(values),
            'tolerance_db': tolerance, 'scope': 'One holdout-point agreement with supplied reference readings only',
            'absolute_accuracy_certified': False,
            'reference_uncertainty_db': context.get('reference_uncertainty_db'),
            'holdout_independence_is_declared': True}


def verify_manifest(root, manifest):
    root = Path(root).resolve()
    seen = set()
    bad = []
    count = 0
    for line in Path(manifest).read_text(encoding='utf-8-sig').splitlines():
        if not line:
            continue
        match = re.fullmatch(r'([a-fA-F0-9]{64})  (.+)', line)
        if not match:
            raise ValueError('Invalid SHA256 manifest line')
        digest, name = match.groups()
        path = PurePosixPath(name)
        normalized = path.as_posix().casefold()
        if path.is_absolute() or '..' in path.parts or '\\' in name or ':' in name or normalized in seen:
            raise ValueError('Unsafe/duplicate manifest path')
        seen.add(normalized)
        target = (root / name).resolve()
        if not target.is_relative_to(root):
            raise ValueError('Manifest escaped release root')
        if not target.is_file() or hashlib.sha256(target.read_bytes()).hexdigest() != digest.lower():
            bad.append(name)
        count += 1
    if not count:
        raise ValueError('Empty manifest')
    return {'status': 'FAIL' if bad else 'PASS', 'files': count, 'missing_or_modified': bad,
            'scope': 'Byte integrity only, not functional or RF acceptance'}


def rate_budget(input_rate_hz, decimation, components, component_bits, fft_n, power_bits, header_bytes, displays_per_second):
    rate = number(input_rate_hz)
    factors = [int(x) for x in decimation.split(',')]
    if rate <= 0 or not factors or any(x < 1 for x in factors):
        raise ValueError('Rates/decimation factors must be positive')
    if min(components, component_bits, fft_n, power_bits) < 1 or header_bytes < 0 or number(displays_per_second) <= 0:
        raise ValueError('Invalid format/frame/display parameters')
    stages = []
    for factor in factors:
        rate /= factor
        stages.append(rate)
    packet = number(header_bytes) + number(fft_n) * power_bits / 8
    return {'status': 'PASS', 'stage_sample_rates_hz': stages,
            'raw_iq_bytes_per_second': rate * components * component_bits / 8,
            'spectrum_packet_bytes': packet,
            'display_payload_bytes_per_second': packet * number(displays_per_second),
            'scope': 'Arithmetic estimate only; not a DMA/DDR benchmark or gapless-capture claim'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    subs = parser.add_subparsers(dest='command', required=True)
    p = subs.add_parser('compare-reports')
    p.add_argument('--before', required=True); p.add_argument('--after', required=True)
    p.add_argument('--context', required=True)
    p.add_argument('--before-timing'); p.add_argument('--after-timing')
    p = subs.add_parser('compare-numeric')
    p.add_argument('--reference', required=True); p.add_argument('--actual', required=True)
    p.add_argument('--mode', choices=['exact', 'tolerance'], required=True)
    p.add_argument('--domain', choices=['rtl_simulation', 'digital_file', 'reference_data'], required=True)
    p.add_argument('--atol', default='0'); p.add_argument('--rtol', default='0')
    p = subs.add_parser('power-check')
    p.add_argument('--context', required=True); p.add_argument('--request', required=True)
    p.add_argument('--readings', required=True)
    p = subs.add_parser('verify-manifest')
    p.add_argument('--root', required=True); p.add_argument('--manifest', required=True)
    p = subs.add_parser('rate-budget')
    p.add_argument('--input-rate-hz', required=True); p.add_argument('--decimation', required=True)
    p.add_argument('--components', type=int, default=2); p.add_argument('--component-bits', type=int, default=16)
    p.add_argument('--fft-n', type=int, required=True); p.add_argument('--power-bits', type=int, default=64)
    p.add_argument('--header-bytes', type=int, default=0); p.add_argument('--displays-per-second', required=True)
    for p in subs.choices.values():
        p.add_argument('--output', type=Path)
    args = parser.parse_args()
    try:
        if args.command == 'compare-reports':
            result = compare_reports(args.before, args.after, read_json(args.context), args.before_timing, args.after_timing)
        elif args.command == 'compare-numeric':
            result = compare_numeric(args.reference, args.actual, args.mode, args.domain, args.atol, args.rtol)
        elif args.command == 'power-check':
            result = check_power(read_json(args.context), read_json(args.request), args.readings)
        elif args.command == 'verify-manifest':
            result = verify_manifest(args.root, args.manifest)
        else:
            result = rate_budget(args.input_rate_hz, args.decimation, args.components, args.component_bits,
                                 args.fft_n, args.power_bits, args.header_bytes, args.displays_per_second)
        rendered = json.dumps(result, ensure_ascii=False, indent=2, default=plain) + '\n'
        if args.output:
            with args.output.open('x', encoding='utf-8', newline='\n') as f:
                f.write(rendered)
        print(rendered, end='')
        return {'PASS': 0, 'FAIL': 1, 'NOT_COMPARABLE': 2}[result['status']]
    except (OSError, ValueError, KeyError, TypeError, csv.Error) as e:
        print(json.dumps({'status': 'INSUFFICIENT_EVIDENCE', 'error': str(e)}, ensure_ascii=False))
        return 2


if __name__ == '__main__':
    raise SystemExit(main())
