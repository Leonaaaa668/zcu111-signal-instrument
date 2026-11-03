"""Check submission structure and recorded source identity; no hardware access."""
from pathlib import Path
import hashlib
import json
import re

ROOT = Path(__file__).resolve().parents[1]

def require(condition, message):
    if not condition:
        raise SystemExit('SUBMISSION_CHECK_FAIL: ' + message)

def main():
    for name in ('src', 'sim', 'build', 'board', 'data', 'skill', 'report'):
        require((ROOT/name).is_dir(), 'Missing directory ' + name)
    entries = ('README.md', 'build/reproduce.md', 'build/fpga/create_project.tcl',
               'build/fpga/build_bitstream.tcl', 'build/linux/prepare_petalinux.sh',
               'build/linux/build_all.sh', 'build/make_sd_runtime.py',
               'sim/verification_catalog.md', 'sim/run_rtl_tests.ps1',
               'board/bringup.md', 'board/measurements.md', 'board/reference/start_v16.sh',
               'skill/README.md', 'skill/fpga-evidence-workflow/SKILL.md',
               'report/hardware_performance.md', 'report/source_identity.json')
    for name in entries:
        require((ROOT/name).is_file(), 'Missing entry ' + name)
    baseline = json.loads((ROOT/'report/source_identity.json').read_text(encoding='utf-8'))
    release = json.loads((ROOT/'board/releases/v17.json').read_text(encoding='utf-8'))
    require(release['release'] == 'V17' and release['application_version'] == '0.11.0', 'Active version')
    require('VERSION 0.11.0' in (ROOT/'src/linux/apps/instrument_cpp/CMakeLists.txt').read_text(), 'CMake version')
    actual_src = {p.relative_to(ROOT).as_posix() for p in (ROOT/'src').rglob('*') if p.is_file()}
    new_test = 'src/linux/apps/instrument_cpp/tests/cw_frequency_estimate_tests.cpp'
    require(actual_src == set(baseline['files']) | {new_test} and len(actual_src) == 224, 'V17 source set')
    application_changes = {'src/linux/apps/instrument_cpp/' + n for n in (
        'CMakeLists.txt', 'src/core.hpp', 'src/gui.cpp', 'src/tx_power.cpp', 'src/tx_power.hpp', 'src/waveform.cpp')}
    comment_change = 'src/fpga/rtl/sa_halfband_decim2.v'
    delta = {n for n, old_hash in baseline['files'].items()
             if hashlib.sha256((ROOT/n).read_bytes()).hexdigest() != old_hash}
    require(delta == application_changes | {comment_change}, 'Expected application changes and exact RTL comment edit')
    freeze = json.loads((ROOT/'build/reports/fpga/source_freeze_narrow09.json').read_text(encoding='utf-8-sig'))
    rtl_count = 0
    for name, expected in freeze['files'].items():
        if name.startswith('rtl/'):
            p = ROOT/'src/fpga'/name
            data = p.read_bytes()
            if name == 'rtl/sa_halfband_decim2.v':
                current = b'// Integrated by sa_d8_zoom as RX D8 stages d0/d1/d2 in the accepted design.'
                original = b'// This module is NOT yet connected to the accepted ADC/DAC block design.'
                require(data.count(current) == 1, 'Expected exact documented RTL comment edit')
                data = data.replace(current, original, 1)
            require(hashlib.sha256(data).hexdigest() == expected, 'Frozen RTL implementation ' + name)
            rtl_count += 1
    require(rtl_count == 18, 'Expected 18 self-written Verilog files/headers')
    util = (ROOT/'build/reports/fpga/utilization.rpt').read_text()
    for label, expected in [('CLB LUTs', '72273'), ('CLB Registers', '113196'),
                            ('Block RAM Tile', '602.5'), ('DSPs', '183')]:
        match = re.search(r'\|\s*' + re.escape(label) + r'\s*\|\s*([\d.]+)\s*\|', util)
        require(match is not None and match[1] == expected, 'Resource value ' + label)
    timing = (ROOT/'build/reports/fpga/timing_summary.rpt').read_text()
    require('All user specified timing constraints are met.' in timing, 'Recorded timing signoff')
    provenance = json.loads((ROOT/'report/materials_provenance.json').read_text(encoding='utf-8'))
    for entry in provenance['added_raw_evidence']:
        p = ROOT/entry['file']
        require(p.is_file() and hashlib.sha256(p.read_bytes()).hexdigest() == entry['sha256'],
                'Collected evidence changed: ' + entry['file'])
    fixed_names = []
    for p in ROOT.rglob('*'):
        if not p.is_file() or '__pycache__' in p.parts:
            continue
        name = p.relative_to(ROOT).as_posix()
        require(name.isascii() and not any(c.isspace() for c in name), 'Nonportable path: ' + name)
        require(p.suffix not in ('.lic', '.dcp', '.bit', '.xsa', '.ko', '.elf', '.pyc'),
                'Generated or private artifact: ' + name)
        if not re.fullmatch(r'[a-z0-9_./-]+', name):
            fixed_names.append(name)
    result = {'status': 'V17_STRUCTURE_AND_BASELINE_REGRESSION_PASS',
              'release': 'V17', 'application_version': '0.11.0',
              'application_files_modified_from_v16': 6, 'rtl_comment_only_files': 1,
              'new_source_files': 1, 'unchanged_from_v16': 216,
              'local_sdk_rebuild': 'NOT_RUN_ON_THIS_COMPUTER',
              'v17_board_acceptance': release['board_validation'],
              'source_files': len(actual_src), 'self_written_verilog_files': rtl_count,
              'collected_evidence_hashes': len(provenance['added_raw_evidence']),
              'required_entries': len(entries), 'path_format': 'ASCII_NO_SPACES',
              'fixed_name_exceptions': len(fixed_names),
              'new_synthesis_or_board_measurement': False}
    print(json.dumps(result, ensure_ascii=False, indent=2))

if __name__ == '__main__':
    main()
