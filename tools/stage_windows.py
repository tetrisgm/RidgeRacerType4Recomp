#!/usr/bin/env python3
"""Stage only runtime assets; refuse unexpected PE imports and reused versions."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys

from check_static_overlays import check as check_static_overlays

ROOT = Path(__file__).resolve().parents[1]
SYSTEM_DLLS = set('''advapi32.dll bcrypt.dll comdlg32.dll d2d1.dll dbghelp.dll
    dwrite.dll gdi32.dll imm32.dll iphlpapi.dll kernel32.dll ole32.dll oleaut32.dll
    opengl32.dll setupapi.dll shell32.dll user32.dll version.dll winmm.dll
    ws2_32.dll'''.split())


def digest(path):
    with path.open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('version')
    parser.add_argument('--toolchain', required=True, type=Path)
    args = parser.parse_args()
    if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9._-]*', args.version):
        parser.error('version must be a simple folder name')
    build = ROOT / 'build-windows'
    stage = ROOT / 'dist/windows' / args.version
    if stage.exists():
        parser.error('version already exists; never reuse a delivery folder')
    report = subprocess.check_output([
        str(args.toolchain / 'bin/llvm-readobj'), '--file-headers', '--coff-imports',
        str(build / 'r4-runtime.exe')], text=True)
    if 'IMAGE_FILE_MACHINE_AMD64' not in report:
        raise SystemExit('Expected an x86-64 Windows PE')
    imports = sorted(set(re.findall(r'^  Name: (.+)$', report, re.M)))
    unexpected = [name for name in imports if name.lower() not in SYSTEM_DLLS
                  and not re.fullmatch(r'api-ms-win-crt-[a-z0-9-]+\.dll', name.lower())]
    if unexpected:
        raise SystemExit(f'Unexpected DLL imports: {unexpected}')
    audit = check_static_overlays()
    symbols = subprocess.check_output([str(args.toolchain / 'bin/llvm-nm'),
                                      '--defined-only', str(build / 'r4-runtime.exe')], text=True)
    if not re.search(r'\bT psx_overlay_dispatch$', symbols, re.M):
        raise SystemExit('Windows executable does not link the native overlay dispatcher')
    variants = re.findall(r'\bT ov_[A-Fa-f0-9_]+_func_[A-Fa-f0-9]+$', symbols, re.M)
    if len(variants) != audit['published_variants']:
        raise SystemExit(f'Linked overlay count mismatch: {len(variants)}')
    stage.mkdir(parents=True)  # Atomic refusal if another build reserves this version.
    for name in ('r4-runtime.exe', 'game.toml', 'game_options.toml', 'assets', 'mods', 'postfx'):
        src = build / name
        if src.is_dir():
            shutil.copytree(src, stage / name)
        else:
            shutil.copy2(src, stage / name)
    subprocess.run([sys.executable, str(ROOT / 'psxrecomp/tools/release_stage.py'),
                    'stage-execution', '--binary', str(stage / 'r4-runtime.exe'),
                    '--manifest', str(build / 'r4-runtime.execution.json')], check=True)
    (stage / 'bios').mkdir()
    for name in ('openbios.bin', 'OpenBIOS.LICENSE'):
        shutil.copy2(build / 'bios' / name, stage / 'bios' / name)
    # No cache, disc, extracted PS-X EXE, compiler, SDK, settings or saves are copied.
    for name in ('LICENSE', 'THIRD-PARTY-LICENSES'):
        src = ROOT / name
        if src.is_dir():
            shutil.copytree(src, stage / name)
        elif src.exists():
            shutil.copy2(src, stage / name)
    notices = {
        ROOT / 'psxrecomp/LICENSE': 'psxrecomp-LICENSE',
        build / '_deps/sdl3-src/LICENSE.txt': 'SDL3-LICENSE.txt',
        build / '_deps/psx_zlib-src/LICENSE': 'zlib-LICENSE',
        build / '_deps/psx_libwebp-src/COPYING': 'libwebp-COPYING',
        build / '_deps/psx_libchdr-src/LICENSE.txt': 'libchdr-LICENSE.txt',
        build / '_deps/libjuice-src/LICENSE': 'libjuice-LICENSE',
        ROOT / 'recomp-ui/LICENSE': 'recomp-ui-LICENSE',
        ROOT / 'recomp-ui/third_party/imgui/LICENSE.txt': 'imgui-LICENSE.txt',
        ROOT / 'psxrecomp/lib/recomp-net/LICENSE': 'recomp-net-LICENSE',
        ROOT / 'psxrecomp/lib/retcomm-rbengine/LICENSE': 'rbengine-LICENSE',
        args.toolchain / 'LICENSE.TXT': 'llvm-mingw-LICENSE.txt',
    }
    (stage / 'licenses').mkdir()
    for src, name in notices.items():
        if not src.is_file():
            raise SystemExit(f'Missing license: {src}')
        shutil.copy2(src, stage / 'licenses' / name)
    (stage / 'PE-IMPORTS.txt').write_text(report)
    overlay_receipt = {key: audit[key] for key in ('game_id', 'recipe_count',
        'published_variants', 'all_guards_match_known_input_bytes', 'full_static_coverage_proven',
        'profile_sha256', 'original_disc_sha256', 'image_coverage', 'files')}
    overlay_receipt['linked_variants'] = len(variants)
    (stage / 'NATIVE-OVERLAYS.json').write_text(json.dumps(overlay_receipt, indent=2) + '\n')
    revisions = {name: subprocess.check_output(['git', '-C', str(path), 'rev-parse', 'HEAD'],
                  text=True).strip() for name, path in
                 [('game', ROOT), ('engine', ROOT / 'psxrecomp'), ('ui', ROOT / 'recomp-ui')]}
    revisions['working_tree_dirty'] = bool(subprocess.check_output(
        ['git', '-C', str(ROOT), 'status', '--porcelain', '--untracked-files=no'], text=True))
    revisions['toolchain'] = args.toolchain.name
    (stage / 'BUILD.json').write_text(json.dumps(revisions, indent=2) + '\n')
    (stage / 'README.txt').write_text('''R4 Windows x64 — portable owner playtest build.

Built on the Mac; no Windows execution has been performed. Windows 11 supplies
all imported system/UCRT DLLs; no VC++ redistributable is imported. No installer,
admin rights or system changes are required. Keep this folder writable.

Use your own USA BIN/CUE by path. From this folder:
  r4-runtime.exe --disc "D:\\your-disc\\R4.cue"
The custom game menu opens at startup. Select/Back opens it during play;
Options includes controls, bindings, display and settings. Esc also opens it.
OpenBIOS is bundled; no retail BIOS or game data is included. Do not copy SDKs,
Git, compilers or agents to STEAMBOX. No overlay toolchain is packaged.

All nine distinct R4.BIN code-overlay images (13 archive slots) are compiled
into the executable. No cache preparation, compiler, Python or SDK is needed
on Windows. Original-disc guards protect each native variant. Mac smoke checks
native overlay execution with no interpreted R4 overlay PCs on that route;
this is not an exhaustive proof for every mode or self-modified code path.

In-game settings: Select/Back, Esc or Guide. Arrows/D-pad navigate;
Enter/A selects; Back/B/Esc returns. Resume keeps your current race.

Owner-only STEAMBOX checks:
1. Boot to an active race using your own disc path.
2. Replay the rank-8 crash route at normal pacing with Max Detail / 8 MB RAM.
3. Check crowded-race game/audio speed at 4K/Max Detail, presentation thread off.
4. Check in-game menus and the physical Steam Controller, both direct launch
   and Steam Input launch. Synthetic Mac button events do not qualify these.
Report build version, route and observations. Preserve native saves/settings.
''')
    files = sorted(p for p in stage.rglob('*') if p.is_file())
    manifest = ''.join(f'{digest(p)}  {p.relative_to(stage).as_posix()}\n' for p in files)
    (stage / 'SHA256SUMS').write_text(manifest)
    for line in manifest.splitlines():
        expected, name = line.split('  ', 1)
        if digest(stage / name) != expected:
            raise SystemExit(f'Manifest verification failed: {name}')
    print(f'Local candidate: {stage}\nVerified {len(files)} SHA256 entries; system DLLs only.')
    print('Ready for owner playtest; no Windows execution performed on the Mac.')


if __name__ == '__main__':
    main()
