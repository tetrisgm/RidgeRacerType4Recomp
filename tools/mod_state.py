#!/usr/bin/env python3
"""mod_state.py - set mod features for --no-launcher test runs.

The launcher persists mod choices in <build>/mods/state.toml (format 2,
psxrecomp docs/MOD_PACKAGES.md "State and migration"); --no-launcher runs
commit whatever that file says, and a feature the file does not name runs as
its manifest's default_enabled says. R4's widescreen package is on by
default, so an empty file (or none) is the shipped experience.

  tools/mod_state.py build --defaults       # no state.toml: every feature at its default
  tools/mod_state.py build --stock          # every bundled feature explicitly off (stock R4)
  tools/mod_state.py build --enable r4.enhancement.widescreen/widescreen aspect=21:9
  tools/mod_state.py build --disable psx.enhancement.pgxp/pgxp   # a default-on feature off
  tools/mod_state.py build --disable r4.enhancement.widescreen/widescreen
  tools/mod_state.py build --stock --enable r4.enhancement.widescreen/widescreen

--enable and --disable may repeat; option=value pairs after an --enable apply
to that feature. Features not named keep their manifest default, except under
--stock. --clear is the old name of --defaults. The package version is read
from build/mods/bundled/<package>/. Local only: state.toml is machine state
and never ships.
"""
import argparse
import os
import re
import sys


def bundled_versions(build):
    root = os.path.join(build, 'mods', 'bundled')
    if not os.path.isdir(root):
        raise SystemExit(f'{root} does not exist; build first')
    out = {}
    for package in sorted(os.listdir(root)):
        versions = sorted(d for d in os.listdir(os.path.join(root, package))
                          if os.path.isfile(os.path.join(root, package, d, 'manifest.toml')))
        if versions:
            out[package] = versions[-1]
    return out


def feature_ids(build, package, version):
    """The [[feature]] ids of a staged manifest (feature-style packages)."""
    path = os.path.join(build, 'mods', 'bundled', package, version, 'manifest.toml')
    ids, in_feature = [], False
    with open(path, encoding='utf-8') as fh:
        for line in fh:
            s = line.strip()
            if s.startswith('[['):
                in_feature = s == '[[feature]]'
                continue
            if s.startswith('['):
                in_feature = False
                continue
            m = re.match(r'id\s*=\s*"([^"]+)"', s)
            if in_feature and m:
                ids.append(m.group(1))
                in_feature = False
    return ids


def quote(v):
    return '"' + v.replace('\\', '\\\\').replace('"', '\\"') + '"'


def parse_actions(argv):
    """[(enabled, 'pkg/feature', {opt: val}), ...] in command-line order."""
    actions, cur = [], None
    for arg in argv:
        if arg in ('--enable', '--disable'):
            cur = [arg == '--enable', None, {}]
            actions.append(cur)
        elif cur is not None and cur[1] is None:
            if '/' not in arg:
                raise SystemExit(f'expected PACKAGE/FEATURE, got {arg!r}')
            cur[1] = arg
        elif cur is not None and '=' in arg:
            if not cur[0]:
                raise SystemExit(f'option {arg!r} given for a disabled feature')
            k, _, v = arg.partition('=')
            cur[2][k] = v
        else:
            raise SystemExit(f'unexpected argument {arg!r}')
    for enabled, name, _ in actions:
        if name is None:
            raise SystemExit('--enable/--disable needs PACKAGE/FEATURE')
    return actions


def main(argv):
    ap = argparse.ArgumentParser(
        description=__doc__.split('\n')[0],
        epilog='Remaining arguments: --enable/--disable PACKAGE/FEATURE [option=value ...]')
    ap.add_argument('build', help='build directory holding the runtime')
    ap.add_argument('--defaults', '--clear', dest='defaults', action='store_true',
                    help='remove state.toml: every feature at its manifest default')
    ap.add_argument('--stock', action='store_true',
                    help='write every bundled feature as explicitly disabled')
    args, rest = ap.parse_known_args(argv)
    path = os.path.join(args.build, 'mods', 'state.toml')
    if args.defaults:
        if rest or args.stock:
            ap.error('--defaults takes no other arguments')
        if os.path.exists(path):
            os.remove(path)
        print(f'removed {path}')
        return 0
    actions = parse_actions(rest)
    if not actions and not args.stock:
        ap.error('nothing to do: give --defaults, --stock, --enable or --disable')
    versions = bundled_versions(args.build)
    features = {}   # (package, feature) -> (enabled, values); insertion order kept
    if args.stock:
        for package, version in versions.items():
            for fid in feature_ids(args.build, package, version):
                features[(package, fid)] = (False, {})
    for enabled, name, values in actions:
        package, feature = name.split('/', 1)
        if package not in versions:
            raise SystemExit(f'{package} is not staged under {args.build}/mods/bundled; build first')
        if feature not in feature_ids(args.build, package, versions[package]):
            raise SystemExit(f'{package} has no feature {feature!r}')
        features[(package, feature)] = (enabled, values)
    packages = []
    for package, _ in features:
        if package not in packages:
            packages.append(package)
    lines = ['format_version = 2']
    for package in packages:
        lines += ['', '[[package]]', f'id = {quote(package)}',
                  f'version = {quote(versions[package])}']
    for (package, feature), (enabled, values) in features.items():
        lines += ['', '[[feature]]', f'package_id = {quote(package)}',
                  f'id = {quote(feature)}', f'enabled = {"true" if enabled else "false"}']
        if values:
            lines.append('[feature.values]')
            lines += [f'{k} = {quote(v)}' for k, v in values.items()]
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'w', encoding='utf-8') as fh:
        fh.write('\n'.join(lines) + '\n')
    print(f'wrote {path}')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
