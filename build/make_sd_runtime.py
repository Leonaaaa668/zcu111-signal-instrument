"""Package newly compiled runtime and pin its own archive + BOOT.BIN hashes."""
from pathlib import Path
import argparse
import hashlib
import tarfile
import json

p = argparse.ArgumentParser()
p.add_argument('--runtime', type=Path, required=True)
p.add_argument('--boot-bin', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
root = Path(__file__).resolve().parents[1]
release = json.loads((root / 'board/releases/v17.json').read_text(encoding='utf-8'))
assert a.runtime.name == release['runtime_directory'], 'Use newly built V17 runtime'
assert json.loads((a.runtime / 'release.json').read_text(encoding='utf-8')) == release, 'V17 release identity missing or mismatched'
assert a.boot_bin.is_file()
a.output.mkdir(parents=True, exist_ok=True)
rt = a.runtime
sha = lambda x: hashlib.sha256(x.read_bytes()).hexdigest()
required = ['zcu111-instrument', 'zcu111-clock', 'zcu111-tx', 'zcu111-rfdc-init', 'zcu111-rfdc-rx-init', 'zcu111-rx-tuned', 'zcu111-rx-prepare']
for name in required:
    assert (rt / 'usr/bin' / name).is_file(), name
for name in ['zcu111_tx.ko', 'zcu111_sa.ko']:
    assert (rt / 'modules' / name).is_file(), name
(rt / 'SHA256SUMS.txt').write_text(''.join(sha(x) + '  ' + x.relative_to(rt).as_posix() + '\n' for x in sorted(rt.rglob('*')) if x.is_file() and x.name != 'SHA256SUMS.txt'), newline='\n')
archive = a.output / (rt.name + '.tar.gz')
assert not archive.exists(), 'Preserve old packages; choose a fresh output'
def mode(m):
    m.uid = m.gid = 0
    m.uname = m.gname = 'root'
    return m
with tarfile.open(archive, 'w:gz') as z:
    z.add(rt, arcname=rt.name, filter=mode)
# Reuse the accepted startup logic, retaining the V16 reference file unchanged.
script = (root / 'board/reference/start_v16.sh').read_text()
script = script.replace('20261101_16', '20261103_17').replace('release16', 'release17').replace('v16', 'v17').replace('V16', 'V17')
script = script.replace('18bc65c83ccd07cd9017e3d64b619ac8c7536744af105fa1c66bc0a7df955474', sha(archive))
script = script.replace('dbec5bb2c132297faeb3467b17467566e61d0d555e186171c93757efec4b054b', sha(a.boot_bin))
(a.output / release['startup_script']).write_text(script, newline='\n')
print('NEW_RUNTIME_PACKAGED', archive)
print('BOOT.BIN sha256', sha(a.boot_bin))
print('Copy matching BOOT.BIN/image.ub/boot.scr plus this archive/start_v17.sh to FAT partition.')
