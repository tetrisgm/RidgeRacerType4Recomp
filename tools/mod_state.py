#!/usr/bin/env python3
"""mod_state.py - enable a mod feature for --no-launcher test runs.

The launcher persists mod choices in <build>/mods/state.toml (format 2,
psxrecomp docs/MOD_PACKAGES.md "State and migration"); --no-launcher runs
commit whatever that file says. This writes it so a scripted run can switch a
bundled package on or off without clicking through the launcher.

  tools/mod_state.py build --enable r4.enhancement.widescreen/widescreen aspect=Fit
  tools/mod_state.py build --enable r4.enhancement.widescreen/widescreen aspect=21:9
  tools/mod_state.py build --clear          # every feature back to its default

The package version is read from build/mods/bundled/<package>/. Local only:
state.toml is machine state and never ships.
"""
import argparse
import os
import sys


def bundled_version(build, package):
    root = os.path.join(build, 'mods', 'bundled', package)
    versions = sorted(d for d in os.listdir(root)) if os.path.isdir(root) else []
    if not versions:
        raise SystemExit(f'{package} is not staged under {root}; build first')
    return versions[-1]


def quote(v):
    return '"' + v.replace('\\', '\\\\').replace('"', '\\"') + '"'


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('build', help='build directory holding the runtime')
    ap.add_argument('--enable', metavar='PACKAGE/FEATURE')
    ap.add_argument('--clear', action='store_true')
    ap.add_argument('values', nargs='*', metavar='option=value')
    args = ap.parse_args()
    path = os.path.join(args.build, 'mods', 'state.toml')
    if args.clear:
        if os.path.exists(path):
            os.remove(path)
        print(f'removed {path}')
        return 0
    if not args.enable or '/' not in args.enable:
        ap.error('--enable PACKAGE/FEATURE is required')
    package, feature = args.enable.split('/', 1)
    lines = ['format_version = 2', '',
             '[[package]]', f'id = {quote(package)}',
             f'version = {quote(bundled_version(args.build, package))}', '',
             '[[feature]]', f'package_id = {quote(package)}', f'id = {quote(feature)}',
             'enabled = true']
    if args.values:
        lines += ['', '[feature.values]']
        for kv in args.values:
            k, _, v = kv.partition('=')
            lines.append(f'{k} = {quote(v)}')
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'w') as fh:
        fh.write('\n'.join(lines) + '\n')
    print(f'wrote {path}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
