"""Collect source-only V16 submission inputs from frozen releases, no board I/O."""
from pathlib import Path
import hashlib
import io
import json
import re
import shutil
import tarfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
DEST = ROOT / 'delivery/zcu111_v16_source'
assert not DEST.exists(), 'Use a new submission directory; never overwrite a release'
DEST.mkdir()
records = []

def write(name, data, origin='submission integration'):
    p = DEST / name
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_bytes(data if isinstance(data, bytes) else data.encode('utf-8'))
    records.append({'path': name, 'origin': origin, 'sha256': hashlib.sha256(p.read_bytes()).hexdigest()})

def copy(p, name):
    write(name, p.read_bytes(), str(p.relative_to(ROOT)))

with zipfile.ZipFile(ROOT / 'delivery/zcu111_vivado_full_narrow09_20261029_01.zip') as z:
    for folder in ['rtl', 'coeff', 'clock']:
        for p in sorted((ROOT / folder).rglob('*')):
            if p.is_file():
                rel = p.relative_to(ROOT).as_posix()
                if folder == 'rtl':
                    assert z.read('zcu111_n09/source/' + rel) == p.read_bytes(), rel
                copy(p, 'src/fpga/' + rel)
    for name in ['post_route_tx.tcl', 'post_route_rx_dma.tcl']:
        write('src/fpga/scripts/' + name, z.read('zcu111_n09/source/scripts/' + name), 'frozen narrow09 source archive')
    write('src/fpga/constraints/tx_cdc.xdc', z.read('zcu111_n09/source/constraints/tx_cdc.xdc'), 'frozen narrow09 source archive')
    pin = [n for n in z.namelist() if n.endswith('/ddr_cons.xdc')]
    assert len(pin) == 1
    write('src/fpga/constraints/ddr_cons.xdc', z.read(pin[0]), 'frozen narrow09 imported board constraints')
    write('src/fpga/bd/adc3_capture.tcl', z.read('zcu111_n09/evidence/portable_contract/adc_dac_ddr_rebuild.tcl'), 'frozen narrow09 write_bd_tcl export')

with zipfile.ZipFile(ROOT / 'delivery/zcu111_instrument_cpp_20261101_16.zip') as z:
    prefix = 'zcu111_instrument_cpp_20261101_16/'
    keep = ['linux/apps/instrument_cpp/src/', 'linux/apps/instrument_cpp/include/',
            'linux/apps/instrument_cpp/tests/', 'linux/apps/instrument_cpp/resources/',
            'linux/apps/instrument_cpp/deploy/', 'linux/tx_20261030/admin/',
            'linux/tx_20261030/kernel/', 'linux/tx_20261030/include/',
            'linux/tx_20261030/third_party/', 'linux/rx_20261030/admin/',
            'linux/rx_20261030/kernel/']
    for n in z.namelist():
        if n.endswith('/'):
            continue
        rel = n[len(prefix):]
        if any(rel.startswith(k) for k in keep) or rel in ['linux/apps/instrument_cpp/CMakeLists.txt', 'linux/apps/instrument_cpp/LICENSE']:
            assert not rel.endswith(('.ko', '.o', '.a', '.pyc'))
            write('src/' + rel, z.read(n), 'frozen V16 source archive')

# Exact source integration used by the SDK computer. Credentials are removed.
private = ROOT / 'reports/submission_v16_20261101/petalinux_sources_private.tar.gz'
with tarfile.open(private) as a:
    for m in a.getmembers():
        if not m.isfile():
            continue
        name = m.name
        if name == '.petalinux/metadata':
            continue  # recreated with the recipient's SDT path by prepare_petalinux.sh
        b = a.extractfile(m).read()
        if name.endswith('/petalinux-image-minimal.bbappend'):
            t = b.decode()
            lines = t.splitlines()
            lines = [x for x in lines if not ('sed -i' in x and '/etc/shadow' in x)]
            # Each builder provisions their own root password; no shared password/hash.
            t = '\n'.join(lines) + '\n'
            t += '''
# Submission provisioning: set INSTRUMENT_ROOT_PASSWORD_HASH in local.conf.
# Generate with: openssl passwd -6  (interactive, no plaintext in this repository).
instrument_set_root_password() {
    if [ -n "${INSTRUMENT_ROOT_PASSWORD_HASH}" ]; then
        sed -i 's|^root:[^:]*:|root:${INSTRUMENT_ROOT_PASSWORD_HASH}:|' ${IMAGE_ROOTFS}/etc/shadow
    fi
}
INSTRUMENT_ROOT_PASSWORD_HASH ?= ""
ROOTFS_POSTPROCESS_COMMAND:append = " instrument_set_root_password; "
'''
            b = t.encode()
        # Local hostname is descriptive; private source/build paths must not be dependencies.
        if name.endswith('/config'):
            t = b.decode()
            t = re.sub(r'(?m)^(CONFIG_SUBSYSTEM_.*(?:PASSWORD|PASSWD).*)$', '# Removed local credential setting', t)
            b = t.encode()
        write('src/linux/petalinux_project/' + name, b, 'SDK VM actual project-spec snapshot; credentials sanitized')
copy(ROOT / 'reports/submission_v16_20261101/kernel.config', 'src/linux/petalinux_project/reference_kernel.config')

# Captured SDT is readable reconstruction input, not a BIT/XSA or generated work tree.
sdt = ROOT / 'linux/handoff_20261029_01/hardware/sdt'
for p in sdt.rglob('*'):
    if p.is_file() and p.suffix in ['.dts', '.dtsi', '.h', '.c']:
        if 'extracted' not in p.relative_to(sdt).parts:
            copy(p, 'src/linux/sdt/' + p.relative_to(sdt).as_posix())

runtime = ROOT / 'delivery/runtime_qt5_20261101_16'
for p in runtime.iterdir():
    if p.is_file() and p.suffix == '.sh':
        copy(p, 'board/runtime_layout/' + p.name)
for p in (runtime / 'display-fix').rglob('*'):
    if p.is_file():
        copy(p, 'board/runtime_layout/display-fix/' + p.relative_to(runtime / 'display-fix').as_posix())
copy(ROOT / 'delivery/startup_v16_20261101/start_v16.sh', 'board/reference/start_v16.sh')
for name in ['zcu111_instrument_20261101_16_frozen.json', 'zcu111_instrument_20261101_16_sha256.txt']:
    copy(ROOT / 'delivery' / name, 'board/reference/' + name)

for p in (ROOT / 'sw').rglob('*'):
    if p.is_file():
        copy(p, 'src/baremetal_reference/' + p.relative_to(ROOT / 'sw').as_posix())
for p in (ROOT / 'sim').rglob('*'):
    if p.is_file():
        copy(p, 'sim/rtl/' + p.relative_to(ROOT / 'sim').as_posix())
python_names = ['generate_sa_decim_vectors.py', 'generate_sa_rx_vectors.py', 'sa_filter_design.py',
                'sa_reference.py', 'verify_sa_fft.py', 'verify_sa_snapshot_sim.py', 'decode_rx_dma.py',
                'tx_waveforms.py', 'tx_power_cal.py', 'tx_cw_power.py', 'test_tx_waveforms.py',
                'test_sa_filter_design.py', 'test_sa_reference.py', 'test_tx_power_cal.py',
                'requirements.txt', 'requirements_analysis.txt']
for name in python_names:
    copy(ROOT / 'python' / name, 'sim/python/' + name)

hw = ROOT / 'reports/rx_dma_20261027_01'
for name in ['timing_summary.rpt', 'utilization.rpt', 'route_status.rpt', 'postroute_signoff.rpt',
             'bus_skew.rpt', 'cdc_details.rpt', 'drc.rpt', 'methodology.rpt',
             'narrow09_report_review.json', 'artifact_verification.json', 'source_freeze_narrow09.json']:
    copy(hw / name, 'build/reports/fpga/' + name.lower())
for folder, names in {
    'v16_20261101': ['build_v16.log', 'board_test.log', 'startup_check.log', 'sd_archive_verification.log',
                     'status.json', 'sdk_source_verification.json', 'source_delta_from_frozen15.diff'],
    'marker_main_20261031_15': ['board_test.log'],
}.items():
    for name in names:
        copy(ROOT / 'reports' / folder / name, 'report/validation/' + folder + '/' + name.lower())

numeric = ROOT / 'reports/sa_integration/fft_all_20261028_01'
for name in ['status.txt', 'sources.json', 'numeric_verification.json']:
    if (numeric / name).exists():
        copy(numeric / name, 'sim/results/fft/' + name)
large = ROOT / 'data/rx_dma_narrow09_large_20261029_01'
if not large.exists():
    matches = list((ROOT / 'data').glob('*large*'))
    if matches:
        large = matches[0]
for p in (ROOT / 'linux/apps/instrument_cpp/validation').glob('arm_*.json'):
    copy(p, 'sim/results/cpp/' + p.name)
for p in (ROOT / 'linux/rx_20261030/validation/board_samples/loop65536').rglob('*'):
    if p.is_file() and p.stat().st_size < 2 * 1024 * 1024:
        copy(p, 'data/rx_loop65536/' + p.relative_to(ROOT / 'linux/rx_20261030/validation/board_samples/loop65536').as_posix())
for folder in [ROOT / 'delivery/runtime_qt5_20261101_16/calibration_evidence', ROOT / 'reports/tx/modulation_qt_20261031']:
    for p in folder.rglob('*'):
        if p.is_file() and p.suffix in ['.json', '.csv', '.jpg'] and 'cleaned' not in p.name and p.stat().st_size < 3 * 1024 * 1024:
            copy(p, 'data/' + ('calibration/' if 'calibration_evidence' in str(folder) else 'modulation/') + p.relative_to(folder).as_posix())

copy(ROOT / 'scripts/prepare_submission_v16.py', 'report/collection/prepare_submission_v16.py')
write('report/collection/source_provenance.json', json.dumps(records, ensure_ascii=False, indent=2) + '\n')
print('V16_SOURCE_INPUTS_COLLECTED', len(records), str(DEST))
