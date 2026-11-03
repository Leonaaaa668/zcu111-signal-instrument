"""Optional checker for a manifest supplied by the caller; no project manifest is published."""
import argparse
import hashlib
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
parser.add_argument('--manifest', type=Path, required=True)
args = parser.parse_args()
root = args.root.resolve()
count = 0
for line in args.manifest.read_text().splitlines():
    digest, name = line.split('  ', 1)
    file = (root / name).resolve()
    if not file.is_relative_to(root):
        raise SystemExit('Unsafe manifest path')
    if not file.is_file() or hashlib.sha256(file.read_bytes()).hexdigest() != digest:
        raise SystemExit('MISSING_OR_MODIFIED ' + name)
    count += 1
print('CALLER_MANIFEST_PASS', count, 'files')
