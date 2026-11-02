"""Package newly compiled runtime and pin its own archive + BOOT.BIN hashes."""
from pathlib import Path
import argparse
import hashlib
import tarfile

p = argparse.ArgumentParser()
p.add_argument('--runtime', type=Path, required=True)
p.add_argument('--boot-bin', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
root = Path(__file__).resolve().parents[1]
assert a.runtime.name == 'zcu111_instrument_qt5_20261101_16'
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
script = (root / 'board/reference/start_v16.sh').read_text()
script = script.replace('18bc65c83ccd07cd9017e3d64b619ac8c7536744af105fa1c66bc0a7df955474', sha(archive))
script = script.replace('dbec5bb2c132297faeb3467b17467566e61d0d555e186171c93757efec4b054b', sha(a.boot_bin))
(a.output / 'start_v16.sh').write_text(script, newline='\n')
print('NEW_RUNTIME_PACKAGED', archive)
print('BOOT.BIN sha256', sha(a.boot_bin))
print('Copy matching BOOT.BIN/image.ub/boot.scr plus this archive/start_v16.sh to FAT partition.')
