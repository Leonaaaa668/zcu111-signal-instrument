"""Archive source/evidence only; no Vivado/PetaLinux outputs or credentials."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import zipfile

p = argparse.ArgumentParser()
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
root = Path(__file__).resolve().parents[1]
assert not a.output.exists(), 'Never overwrite an existing release ZIP'
assert not a.output.resolve().is_relative_to(root), 'ZIP output must be outside source directory'
excluded_parts = {'generated', '__pycache__', '.git', '.Xil', 'xsim.dir', 'node_modules'}
excluded_suffixes = {'.pyc', '.pyo', '.o', '.obj', '.dcp', '.bit', '.xsa', '.elf', '.ko', '.exe', '.dll', '.so', '.a', '.wdb', '.jou', '.pb', '.lic'}
files = []
def input_files():
    # Prune generated output before traversal, including Windows restricted temp dirs.
    for directory, directories, names in os.walk(root):
        directories[:] = sorted(n for n in directories if n not in excluded_parts)
        for name in sorted(names):
            yield Path(directory)/name

for f in sorted(input_files()):
    if not f.is_file():
        continue
    rel = f.relative_to(root)
    if set(rel.parts) & excluded_parts or f.suffix in excluded_suffixes or rel.as_posix() == 'SHA256SUMS.txt':
        continue
    if any(c.isspace() or ord(c) > 127 for c in rel.as_posix()):
        raise SystemExit('Non-portable filename: ' + rel.as_posix())
    data = f.read_bytes()
    if data.startswith((b'\x7fELF', b'MZ')):
        raise SystemExit('Compiled executable in source package: ' + rel.as_posix())
    if f.stat().st_size > 5 * 1024 * 1024:
        raise SystemExit('Unexpected large file: ' + rel.as_posix())
    files.append((rel.as_posix(), data))
checks = ''.join(hashlib.sha256(data).hexdigest() + '  ' + rel + '\n' for rel, data in files)
(root / 'SHA256SUMS.txt').write_text(checks, newline='\n')
files.append(('SHA256SUMS.txt', checks.encode()))
prefix = root.name + '/'
a.output.parent.mkdir(parents=True, exist_ok=True)
with zipfile.ZipFile(a.output, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as z:
    for rel, data in files:
        info = zipfile.ZipInfo(prefix + rel, (2026, 11, 2, 0, 0, 0))
        info.compress_type = zipfile.ZIP_DEFLATED
        info.external_attr = (0o100755 if rel.endswith('.sh') else 0o100644) << 16
        z.writestr(info, data)
with zipfile.ZipFile(a.output) as z:
    assert z.testzip() is None
    for line in checks.splitlines():
        digest, name = line.split('  ', 1)
        assert hashlib.sha256(z.read(prefix + name)).hexdigest() == digest, name
digest = hashlib.sha256(a.output.read_bytes()).hexdigest()
a.output.with_suffix(a.output.suffix + '.sha256').write_text(digest + '  ' + a.output.name + '\n', newline='\n')
receipt = {'status': 'SOURCE_ZIP_CRC_SHA256_PASS', 'zip': a.output.name, 'sha256': digest,
           'files': len(files), 'bytes': a.output.stat().st_size,
           'uncompressed_bytes': sum(len(b) for _, b in files), 'compiled_artifacts': 0}
a.output.with_suffix(a.output.suffix + '.validation.json').write_text(json.dumps(receipt, indent=2) + '\n')
print(json.dumps(receipt, indent=2))
