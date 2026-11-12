#!/usr/bin/env python3
"""Stage a clean, optimized native-overlay Apple Silicon build locally."""
import argparse
import json
import plistlib
from pathlib import Path
import re
import shutil
import subprocess
import sys

from check_static_overlays import check as check_static_overlays
from deliver_windows import verify
from stage_windows import digest

ROOT = Path(__file__).resolve().parents[1]


def output(*command):
    return subprocess.check_output(command, text=True).strip()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('version')
    parser.add_argument('--build', required=True, type=Path)
    parser.add_argument('--race-60', action='store_true', help='select fixed 2160-line / 60-FPS race settings')
    parser.add_argument('--evidence', required=True, type=Path,
                        help='verified performance/visual qualification receipt')
    args = parser.parse_args()
    if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9._-]*', args.version):
        parser.error('version must be a simple folder name')
    build = args.build.resolve()
    delivery = ROOT / 'dist/macos' / args.version
    stage = delivery / 'R4.app/Contents/MacOS'
    if delivery.exists():
        parser.error('version exists; never reuse a package directory')
    if output('git', '-C', str(ROOT), 'status', '--porcelain', '--untracked-files=no'):
        parser.error('commit the verified game and submodule work before packaging')
    cache = (build / 'CMakeCache.txt').read_text()
    for required in ('CMAKE_BUILD_TYPE:STRING=Release', 'PSX_DEBUG_TOOLS:BOOL=OFF',
                     'PSX_RUNTIME_IPO:BOOL=ON', 'PSXRECOMP_BIOS_STEMS:STRING=OpenBIOS'):
        if required not in cache.splitlines():
            parser.error(f'unqualified build configuration: requires {required}')
    binary = build / 'r4-runtime'
    if output('lipo', '-archs', str(binary)) != 'arm64':
        parser.error('expected the qualified arm64 Mac binary')
    imports = output('otool', '-L', str(binary))
    for line in imports.splitlines()[1:]:
        if not line.strip().startswith(('/usr/lib/', '/System/Library/')):
            parser.error(f'non-system dependency: {line}')
    symbols = output('nm', '--defined-only', str(binary))
    variants = re.findall(r'\b[tT] _ov_[A-Fa-f0-9_]+_func_[A-Fa-f0-9]+$', symbols, re.M)
    audit = check_static_overlays()
    if len(variants) != audit['published_variants']:
        parser.error(f'linked overlay count mismatch: {len(variants)}')
    evidence = json.loads(args.evidence.read_text())
    if not evidence.get('release_qualified') or not evidence.get('rewind_status', {}).get('enabled'):
        parser.error('release qualification must include enabled rewind and its required playback checks')
    if evidence.get('binary_sha256') != digest(binary):
        parser.error('qualification receipt does not identify this executable')
    stage.mkdir(parents=True)
    (stage.parent / 'Info.plist').write_bytes(plistlib.dumps(dict(
        CFBundleExecutable='r4-runtime', CFBundleIdentifier='net.ramine.r4recomp',
        CFBundleName='R4 Recompiled', CFBundlePackageType='APPL',
        CFBundleVersion='1', CFBundleShortVersionString='0.1',
        NSHighResolutionCapable=True, LSMinimumSystemVersion='11.0')))
    for name in ('r4-runtime', 'game.toml', 'game_options.toml', 'assets', 'postfx'):
        source = build / name
        if source.is_dir():
            shutil.copytree(source, stage / name)
        else:
            shutil.copy2(source, stage / name)
    shutil.copytree(build / 'mods/bundled', stage / 'mods/bundled')
    if args.race_60:
        (stage / 'mods/state.toml').write_text('''format_version = 2
[[package]]
id = "psx.enhancement.internal-resolution"
[[feature]]
package_id = "psx.enhancement.internal-resolution"
id = "internal-resolution"
enabled = true
[feature.values]
resolution = "9x"
[[package]]
id = "r4.enhancement.frame-rate"
[[feature]]
package_id = "r4.enhancement.frame-rate"
id = "frame-rate"
enabled = true
[feature.values]
rate = "60"
[[package]]
id = "psx.feature.rewind"
[[feature]]
package_id = "psx.feature.rewind"
id = "rewind"
enabled = true
''')
    (stage / 'bios').mkdir()
    for name in ('openbios.bin', 'OpenBIOS.LICENSE'):
        shutil.copy2(build / 'bios' / name, stage / 'bios' / name)
    subprocess.run(['codesign', '--force', '--sign', '-', str(stage / 'r4-runtime')], check=True)
    subprocess.run(['codesign', '--verify', '--strict', str(stage / 'r4-runtime')], check=True)
    subprocess.run([sys.executable, str(ROOT / 'psxrecomp/tools/release_stage.py'),
                    'stage-execution', '--binary', str(stage / 'r4-runtime'),
                    '--manifest', str(build / 'r4-runtime.execution.json')], check=True)
    for name in ('LICENSE', 'DISC.md', 'THIRD-PARTY-LICENSES'):
        source = ROOT / name
        if source.is_dir():
            shutil.copytree(source, stage / name)
        else:
            shutil.copy2(source, stage / name)
    notices = {
        ROOT / 'psxrecomp/LICENSE': 'psxrecomp-LICENSE',
        ROOT / 'recomp-ui/LICENSE': 'recomp-ui-LICENSE',
        ROOT / 'recomp-ui/LICENSES/recomp-ui-MIT.txt': 'recomp-ui-upstream-MIT.txt',
        ROOT / 'recomp-ui/third_party/imgui/LICENSE.txt': 'imgui-LICENSE.txt',
        ROOT / 'psxrecomp/lib/recomp-net/LICENSE': 'recomp-net-LICENSE',
        ROOT / 'psxrecomp/lib/retcomm-rbengine/LICENSE': 'rbengine-LICENSE',
        build / '_deps/sdl3-src/LICENSE.txt': 'SDL3-LICENSE.txt',
        build / '_deps/psx_libwebp-src/COPYING': 'libwebp-COPYING',
        build / '_deps/psx_libchdr-src/LICENSE.txt': 'libchdr-LICENSE.txt',
        build / '_deps/libjuice-src/LICENSE': 'libjuice-LICENSE',
    }
    (stage / 'licenses').mkdir()
    for source, name in notices.items():
        shutil.copy2(source, stage / 'licenses' / name)
    revisions = {name: output('git', '-C', str(path), 'rev-parse', 'HEAD')
                 for name, path in [('game', ROOT), ('engine', ROOT / 'psxrecomp'),
                                    ('ui', ROOT / 'recomp-ui')]}
    revisions.update(working_tree_dirty=False, linked_overlay_variants=len(variants),
                     source_binary_sha256=digest(binary), packaged_binary_sha256=digest(stage / 'r4-runtime'))
    (stage / 'BUILD.json').write_text(json.dumps(revisions, indent=2) + '\n')
    overlay_receipt = {key: audit[key] for key in ('game_id', 'recipe_count',
        'published_variants', 'all_guards_match_known_input_bytes', 'full_static_coverage_proven',
        'profile_sha256', 'original_disc_sha256', 'image_coverage', 'files')}
    overlay_receipt['linked_variants'] = len(variants)
    (stage / 'NATIVE-OVERLAYS.json').write_text(json.dumps(overlay_receipt, indent=2) + '\n')
    (stage / 'QUALIFICATION.json').write_text(json.dumps(evidence, indent=2) + '\n')
    (stage / 'MACH-O-IMPORTS.txt').write_text(imports + '\n')
    start = delivery / 'Start R4.command'
    start.write_text('#!/bin/zsh\ncd -- "${0:A:h}/R4.app/Contents/MacOS"\nexec ./r4-runtime "$@"\n')
    start.chmod(0o755)
    (stage / 'README.txt').write_text('''R4 — Apple Silicon Mac local playtest package

Open R4.app (or Start R4.command) and choose your own USA disc. Keep the entire folder
together and writable. No game data, retail BIOS, saves, compiler or SDK is
included. OpenBIOS and all 7,459 native overlay variants are bundled.

The custom game menu opens at startup and with Select/Back during play.
Options contains display, controls, bindings and audio settings. Esc also
opens the menu. Existing saves and settings are not installed or overwritten.

R4.app/Contents/MacOS/QUALIFICATION.json contains measured results and their limits for this build;
it is not a claim about every course, mode or Mac. Physical Steam Controller
behavior still requires that device; synthetic input tests are identified as
such. This local package is ad-hoc signed, not notarized or publicly released.
''')
    if args.race_60:
        with (stage / 'README.txt').open('a') as note:
            note.write('\nThis package selects fixed 9x (2160 lines) and the 60-FPS GP race enhancement.\nAutomatic rewind capture is enabled in this release.\nManual save/load states also remain available.\nChange these in the custom menu if desired.\n')
    shutil.copy2(stage / 'README.txt', delivery / 'README.txt')
    files = sorted(p for p in delivery.rglob('*') if p.is_file())
    (delivery / 'SHA256SUMS').write_text(''.join(
        f'{digest(p)}  {p.relative_to(delivery).as_posix()}\n' for p in files))
    verify(delivery)
    print(delivery)


if __name__ == '__main__':
    main()
