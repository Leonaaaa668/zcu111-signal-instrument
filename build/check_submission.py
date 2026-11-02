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
    identity = json.loads((ROOT/'report/source_identity.json').read_text(encoding='utf-8'))
    for name, expected in identity['files'].items():
        require(name.startswith('src/') and '..' not in Path(name).parts, 'Unsafe source identity path')
        p = ROOT/name
        require(p.is_file() and hashlib.sha256(p.read_bytes()).hexdigest() == expected,
                'Frozen source changed: ' + name)
    freeze = json.loads((ROOT/'build/reports/fpga/source_freeze_narrow09.json').read_text(encoding='utf-8-sig'))
    rtl_count = 0
    for name, expected in freeze['files'].items():
        if name.startswith('rtl/'):
            p = ROOT/'src/fpga'/name
            require(hashlib.sha256(p.read_bytes()).hexdigest() == expected, 'Frozen RTL ' + name)
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
    result = {'status': 'V16_SUBMISSION_STRUCTURE_AND_SOURCE_IDENTITY_PASS',
              'source_files_exact': len(identity['files']), 'self_written_verilog_files': rtl_count,
              'collected_evidence_hashes': len(provenance['added_raw_evidence']),
              'required_entries': len(entries), 'path_format': 'ASCII_NO_SPACES',
              'fixed_name_exceptions': len(fixed_names), 'exceptions_documented': 'report/path_exceptions.csv',
              'new_synthesis_or_board_measurement': False}
    print(json.dumps(result, ensure_ascii=False, indent=2))

if __name__ == '__main__':
    main()
