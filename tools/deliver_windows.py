#!/usr/bin/env python3
"""Copy one verified portable build to an existing NAS builds share, once."""
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


def deliver(source, share):
    source, share = source.resolve(), share.resolve()
    if not share.is_dir():
        raise ValueError('NAS builds share is unavailable; keep the build local')
    if not re.fullmatch('[A-Za-z0-9][A-Za-z0-9._-]*', source.name):
        raise ValueError('Invalid version folder name')
    verify(source)
    build = json.loads((source / 'BUILD.json').read_text())
    overlays = json.loads((source / 'NATIVE-OVERLAYS.json').read_text())
    if build['working_tree_dirty'] or overlays['linked_variants'] != overlays['published_variants']:
        raise ValueError('Build is dirty or native overlay link audit is incomplete')
    parent = share / 'r4'
    parent.mkdir(exist_ok=True)
    destination = parent / source.name
    destination.mkdir()  # Exclusive reservation; failed uploads are never reused.
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
    parser.add_argument('--share', required=True, type=Path,
                        help='existing mounted NAS builds share; never created by this tool')
    args = parser.parse_args()
    print(deliver(args.source, args.share))


if __name__ == '__main__':
    main()
