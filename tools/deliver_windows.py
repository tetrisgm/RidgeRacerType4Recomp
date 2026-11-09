#!/usr/bin/env python3
"""Copy one verified portable build to an existing destination directory, once."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil


def verify(folder):
    manifest = folder / 'SHA256SUMS'
    expected = {}
    for line in manifest.read_text().splitlines():
        digest, name = line.split('  ', 1)
        path = Path(name)
        if (not re.fullmatch('[0-9a-f]{64}', digest) or path.is_absolute()
                or '..' in path.parts or name in expected or name == 'SHA256SUMS'):
            raise ValueError('Invalid checksum manifest')
        expected[name] = digest
    actual = set()
    for path in folder.rglob('*'):
        if path.is_symlink():
            raise ValueError(f'Symlinks are not delivery assets: {path}')
        if path.is_file() and path.relative_to(folder).as_posix() != 'SHA256SUMS':
            actual.add(path.relative_to(folder).as_posix())
    if actual != set(expected):
        raise ValueError('Manifest and delivery file set differ')
    for name, wanted in expected.items():
        with (folder / name).open('rb') as stream:
            if hashlib.file_digest(stream, 'sha256').hexdigest() != wanted:
                raise ValueError(f'Checksum mismatch: {name}')
    return expected


def deliver(source, destination_root):
    source, destination_root = source.resolve(), destination_root.resolve()
    if not destination_root.is_dir():
        raise ValueError('Destination directory is unavailable; keep the build local')
    if not re.fullmatch('[A-Za-z0-9][A-Za-z0-9._-]*', source.name):
        raise ValueError('Invalid version folder name')
    verify(source)
    build = json.loads((source / 'BUILD.json').read_text())
    overlays = json.loads((source / 'NATIVE-OVERLAYS.json').read_text())
    if build['working_tree_dirty'] or overlays['linked_variants'] != overlays['published_variants']:
        raise ValueError('Build is dirty or native overlay link audit is incomplete')
    parent = destination_root / 'r4'
    parent.mkdir(exist_ok=True)
    destination = parent / source.name
    destination.mkdir()  # Exclusive reservation; failed copies are never reused.
    for item in source.iterdir():
        if item.is_dir():
            shutil.copytree(item, destination / item.name)
        else:
            shutil.copy2(item, destination / item.name)
    verify(destination)
    return destination


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('--destination-root', required=True, type=Path,
                        help='existing local or removable destination directory; never created by this tool')
    args = parser.parse_args()
    print(deliver(args.source, args.destination_root))


if __name__ == '__main__':
    main()
