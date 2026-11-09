#!/usr/bin/env python3
"""Stage only runtime assets; refuse unexpected PE imports and reused versions."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess

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
    stage.mkdir(parents=True)  # Atomic refusal if another build reserves this version.
    for name in ('r4-runtime.exe', 'game.toml', 'game_options.toml', 'assets', 'mods', 'postfx'):
        src = build / name
        if src.is_dir():
            shutil.copytree(src, stage / name)
        else:
            shutil.copy2(src, stage / name)
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
        ROOT / 'recomp-ui/LICENSE': 'recomp-ui-LICENSE',
        ROOT / 'recomp-ui/src/third_party/imgui/LICENSE.txt': 'imgui-LICENSE.txt',
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
    revisions = {name: subprocess.check_output(['git', '-C', str(path), 'rev-parse', 'HEAD'],
                  text=True).strip() for name, path in
                 [('game', ROOT), ('engine', ROOT / 'psxrecomp'), ('ui', ROOT / 'recomp-ui')]}
    revisions['working_tree_dirty'] = bool(subprocess.check_output(
        ['git', '-C', str(ROOT), 'status', '--porcelain', '--untracked-files=no'], text=True))
    revisions['toolchain'] = args.toolchain.name
    (stage / 'BUILD.json').write_text(json.dumps(revisions, indent=2) + '\n')
    (stage / 'README.txt').write_text('''R4 Windows x64 — LOCAL VALIDATION CANDIDATE, not a qualified delivery.

Built on the Mac; no Windows execution has been performed. Windows 11 supplies
all imported system/UCRT DLLs; no VC++ redistributable is imported. No installer,
admin rights or system changes are required. Keep this folder writable.

Use your own USA BIN/CUE by path. From this folder:
  r4-runtime.exe --disc "D:\\your-disc\\R4.cue" --no-launcher
OpenBIOS is bundled; no retail BIOS or game data is included. Do not copy SDKs,
Git, compilers or agents to STEAMBOX. No overlay toolchain is packaged.

BLOCKER: complete native R4.BIN overlay cache coverage has not been prepared
and validated on the Mac. Without it, the existing runtime uses its dirty-RAM
fallback for uncached menu overlays. This candidate does not satisfy the final
fully native, compiler-free delivery goal. Do not promote it as a finished build.

After that blocker is resolved, owner-only STEAMBOX checks:
1. Boot to an active race using your own disc path.
2. Replay the rank-8 crash route at normal pacing with Max Detail / 8 MB RAM.
3. Check crowded-race game/audio speed at 4K/Max Detail, presentation thread off.
Report build version, route and observations. Preserve native saves/settings.
''')
    (stage / 'DELIVERY_BLOCKED.txt').write_text(
        'Native overlay coverage is unqualified. See README.txt and docs/WINDOWS_BUILD.md.\n')
    files = sorted(p for p in stage.rglob('*') if p.is_file())
    manifest = ''.join(f'{digest(p)}  {p.relative_to(stage).as_posix()}\n' for p in files)
    (stage / 'SHA256SUMS').write_text(manifest)
    for line in manifest.splitlines():
        expected, name = line.split('  ', 1)
        if digest(stage / name) != expected:
            raise SystemExit(f'Manifest verification failed: {name}')
    print(f'Local candidate: {stage}\nVerified {len(files)} SHA256 entries; system DLLs only.')
    print('Delivery remains blocked; no NAS writes or Windows execution performed.')


if __name__ == '__main__':
    main()
