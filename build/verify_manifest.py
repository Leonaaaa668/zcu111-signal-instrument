"""Generic release manifest verification. No hardware or external dependency."""
import argparse
import hashlib
from pathlib import Path

p = argparse.ArgumentParser()
p.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
a = p.parse_args()
root = a.root.resolve()
count = 0
for line in (root / 'SHA256SUMS.txt').read_text().splitlines():
    digest, name = line.split('  ', 1)
    file = (root / name).resolve()
    if not file.is_relative_to(root):
        raise SystemExit('Unsafe manifest path: ' + name)
    if not file.is_file() or hashlib.sha256(file.read_bytes()).hexdigest() != digest:
        raise SystemExit('MISSING_OR_MODIFIED ' + name)
    count += 1
print('SOURCE_MANIFEST_PASS', count, 'files')
