#!/usr/bin/env python3
"""r4_ws_scan.py - R4 (SLUS-00797) widescreen cull-site scanner.

Static scan of the US boot EXE and the R4.BIN code overlays (entries 659-672,
linked at 0x801149A8) for every screen-space X test that native-wide rendering
must widen, mapped to psxrecomp [widescreen.cull] site kinds. Its output is the
[widescreen.cull] block in game.toml; --check keeps the two in step.

What it finds
  W   slti/sltiu rt, rs, {0x13F,0x140,0x141}   right-edge per-vertex test
  L0  slti rt, rs, 0 on a register a W test also reads (no write between)
  LB  bltz/bgez/blez/bgtz rs on a register a W test also reads (the compiled
      renderers' "all vertices left of 0" chains)
  Y   slti/sltiu with a height immediate {0xEF,0xF0,0xF1,0x1DF,0x1E0,0x1E1};
      sign branches paired with Y tests are classified Y (never widened)
  CL  loads/stores of the scratchpad viewport clip rectangle 1F80006C..72
      (X = 6C/70, Y = 6E/72) inside the hand-written asm renderers

Mapping (psxrecomp [widescreen.cull])
  right-edge slti            -> slti_sites
  last left reject  bltz     -> bltz_sites
  left keeps        bgez     -> bgez_sites
  clip-rect X loads (lh)     -> clip_edge_x_load_sites
  the two subdivided-course paths (a vanilla `sra v1,v1,16` x4 typo makes
  their X operand sign-only): the final left-reject branch -> branch_keep_sites
  (their right-edge slti can never reject in vanilla, so it is not listed)

Usage (from the repo root)
  tools/r4_ws_scan.py --emit-toml          print the [widescreen.cull] block
  tools/r4_ws_scan.py --check game.toml    exit 1 if game.toml's lists drift
  tools/r4_ws_scan.py --report             full per-function report

Inputs default to the local disc (see game.toml): disc/SLUS_007.97 (read from
the bin's root directory when it was never extracted, as in a fresh clone),
the R4.BIN entries read straight from the cue/bin, and the committed
generated/SLUS_007.97_full.ranges (function starts). Read-only.
"""
import argparse
import bisect
import glob
import hashlib
import os
import struct
import sys
from collections import defaultdict

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))

EXE_BASE = 0x80010000
EXE_CODE_END = 0x8009E150          # end of code in the EXE text (data follows)
OVL_BASE = 0x801149A8
OVL_FIRST, OVL_LAST = 659, 672
# Hand-written asm renderers that address the scratchpad through $a0.
# Evidence: 0x8002D5DC 'lui a0,0x1f80' before 'jal 0x8005F3F4' and
# 0x8006F0B4 'lui a0,0x1f80' before 'jal 0x80060F94'; a0+0x28 is the camera
# matrix 1F800028 and a0+0x64 the mirror flag the viewport setter writes.
ASM_RENDERERS = [(0x80058420, 0x80058AEC), (0x8005F000, 0x8006E000)]
SCRATCH = 0x1F800000

W_IMMS = {0x13F, 0x140, 0x141}
H_IMMS = {0xEF, 0xF0, 0xF1, 0x1DF, 0x1E0, 0x1E1}
CLIP_X = {0x6C: 'clipL', 0x70: 'clipR'}
CLIP_Y = {0x6E: 'clipT', 0x72: 'clipB'}
WINDOW = 16
TYPO_WORD = 0x00031C03             # sra v1, v1, 16

REG = ['zero', 'at', 'v0', 'v1', 'a0', 'a1', 'a2', 'a3', 't0', 't1', 't2', 't3',
       't4', 't5', 't6', 't7', 's0', 's1', 's2', 's3', 's4', 's5', 's6', 's7',
       't8', 't9', 'k0', 'k1', 'gp', 'sp', 'fp', 'ra']

# Keys this tool owns in game.toml's [widescreen.cull], in emit order.
KEYS = ['slti_sites', 'bltz_sites', 'bgez_sites', 'clip_edge_x_load_sites',
        'branch_keep_sites']


def sx16(v):
    return v - 0x10000 if v & 0x8000 else v


def fields(w):
    return w >> 26, (w >> 21) & 31, (w >> 16) & 31, (w >> 11) & 31, w & 0xFFFF, w & 0x3F


def mnemonic(addr, w):
    """Just enough MIPS to comment the listed sites (no capstone needed, so the
    emitted TOML is identical on every machine)."""
    op, rs, rt, rd, imm, fn = fields(w)
    r = REG
    if op == 0x0A:
        return f'slti ${r[rt]}, ${r[rs]}, {sx16(imm):#x}' if sx16(imm) >= 0 else \
            f'slti ${r[rt]}, ${r[rs]}, {sx16(imm)}'
    if op == 0x0B:
        return f'sltiu ${r[rt]}, ${r[rs]}, {imm:#x}'
    if op == 1 and rt in (0, 1):
        return f'{"bltz" if rt == 0 else "bgez"} ${r[rs]}, {addr + 4 + 4 * sx16(imm):#010x}'
    if op in (4, 5):
        name = ('beq', 'bne')[op - 4]
        tgt = addr + 4 + 4 * sx16(imm)
        if rt == 0:
            return f'{name}z ${r[rs]}, {tgt:#010x}'
        return f'{name} ${r[rs]}, ${r[rt]}, {tgt:#010x}'
    if op in (0x21, 0x25, 0x23):
        name = {0x21: 'lh', 0x25: 'lhu', 0x23: 'lw'}[op]
        return f'{name} ${r[rt]}, {imm:#x}(${r[rs]})'
    return f'.word 0x{w:08X}'


def writes(w):
    """GPR written by w, or None."""
    op, rs, rt, rd, imm, fn = fields(w)
    if op == 0:
        if fn in (0x08, 0x0C, 0x0D, 0x11, 0x13, 0x18, 0x19, 0x1A, 0x1B):
            return None
        return rd or None
    if op == 3:
        return 31
    if op == 1:
        return 31 if rt in (0x10, 0x11) else None
    if op in (0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F) or 0x20 <= op <= 0x26:
        return rt or None
    if op == 0x12 and rs in (0, 2):                      # mfc2 / cfc2
        return rt or None
    return None


def is_branch(w):
    op, rs, rt, rd, imm, fn = fields(w)
    return op in (1, 4, 5, 6, 7) or (op == 0 and fn in (8, 9)) or op in (2, 3)


def sign_branch(w):
    """(mnemonic, reg) for bltz/bgez/blez/bgtz, else None."""
    op, rs, rt, rd, imm, fn = fields(w)
    if op == 1 and rt in (0, 1):
        return ('bltz' if rt == 0 else 'bgez', rs)
    if op == 6 and rt == 0:
        return ('blez', rs)
    if op == 7 and rt == 0:
        return ('bgtz', rs)
    return None


def is_gte(w):
    op = w >> 26
    return op == 0x12 or op in (0x32, 0x3A)


class Seg:
    def __init__(self, name, base, data, kind):
        self.name, self.base, self.kind = name, base, kind
        self.aliases = []
        self.W = list(struct.unpack('<%dI' % (len(data) // 4), data[:len(data) // 4 * 4]))
        self.end = base + 4 * len(self.W)

    def word(self, a):
        return self.W[(a - self.base) >> 2]


# --- inputs ------------------------------------------------------------------

EXE_NAME = 'SLUS_007.97'
DEFAULT_EXE = os.path.join(ROOT, 'disc', EXE_NAME)


def load_exe(path, bin_image=None):
    """The boot EXE from `path`, or, when `path` is None, disc/SLUS_007.97 if
    it was extracted and otherwise the copy in the disc image's root
    directory (identical bytes; a fresh clone has no extracted EXE because
    generated/ is committed and nothing ran prepare_disc)."""
    if path is None and os.path.exists(DEFAULT_EXE):
        path = DEFAULT_EXE
    if path is not None:
        b = open(path, 'rb').read()
    elif bin_image:
        b = DiscImage(bin_image).read_root_file(EXE_NAME)
    else:
        raise SystemExit(f'no {DEFAULT_EXE} and no disc/*.bin to read {EXE_NAME} from')
    tsize = struct.unpack_from('<I', b, 0x1C)[0]
    text = b[0x800:0x800 + tsize]
    return Seg('EXE', EXE_BASE, text[:EXE_CODE_END - EXE_BASE], 'exe'), \
        hashlib.md5(b).hexdigest()


class DiscImage:
    """Minimal ISO9660 reader over a raw 2352-byte-sector Mode 2 image."""

    def __init__(self, path):
        self.f = open(path, 'rb')

    def sector(self, lba):
        self.f.seek(lba * 2352)
        raw = self.f.read(2352)
        return raw[24:24 + 2048] if raw[15] == 2 else raw[16:16 + 2048]

    def find_root_file(self, name):
        pvd = self.sector(16)
        if pvd[1:6] != b'CD001':
            raise SystemExit('not an ISO9660 image (no PVD at LBA 16)')
        root = pvd[156:156 + 34]
        lba, size = struct.unpack_from('<I', root, 2)[0], struct.unpack_from('<I', root, 10)[0]
        data = b''.join(self.sector(lba + i) for i in range((size + 2047) // 2048))
        off = 0
        while off < len(data):
            n = data[off]
            if n == 0:
                off = (off // 2048 + 1) * 2048
                continue
            nl = data[off + 32]
            fname = data[off + 33:off + 33 + nl].decode('ascii', 'replace').split(';')[0]
            if fname.upper() == name.upper():
                return struct.unpack_from('<I', data, off + 2)[0], \
                    struct.unpack_from('<I', data, off + 10)[0]
            off += n
        raise SystemExit(f'{name} not found in the disc root directory')

    def read_root_file(self, name):
        lba, size = self.find_root_file(name)
        data = b''.join(self.sector(lba + i) for i in range((size + 2047) // 2048))
        return data[:size]


def load_overlays(bin_image):
    disc = DiscImage(bin_image)
    lba, _size = disc.find_root_file('R4.BIN')
    head = disc.sector(lba)
    n = struct.unpack_from('<I', head, 0)[0]
    need = 4 + 4 * (n + 1)
    table = b''.join(disc.sector(lba + i) for i in range((need + 2047) // 2048))
    offs = struct.unpack_from('<%dI' % (n + 1), table, 4)
    segs, seen = [], {}
    for k in range(OVL_FIRST, OVL_LAST + 1):
        b = b''.join(disc.sector(lba + s) for s in range(offs[k], offs[k + 1]))
        h = hashlib.md5(b).hexdigest()
        words = struct.unpack('<%dI' % (len(b) // 4), b)
        if 0x03E00008 not in words:          # entry 664: a Shift-JIS text table
            continue
        if h in seen:
            seen[h].aliases.append(k)
            continue
        s = Seg('OVL%d' % k, OVL_BASE, b, 'ovl')
        seen[h] = s
        segs.append(s)
    return segs


def default_bin_image():
    hits = sorted(glob.glob(os.path.join(ROOT, 'disc', '*.bin')))
    return hits[0] if hits else None


def exe_functions(ranges_path):
    if not os.path.exists(ranges_path):
        raise SystemExit(f'{ranges_path} missing: run tools/regen.sh first '
                         '(the scan groups sites by recompiled function)')
    return sorted(int(l.split()[1], 16) for l in open(ranges_path) if l.startswith('F '))


def ovl_functions(seg):
    """Heuristic starts: the first prologue, then every stack-allocating addiu
    that follows a jr ra + delay slot (or a j + delay slot)."""
    starts = []
    W = seg.W
    for i, w in enumerate(W):
        if (w & 0xFFFF0000) == 0x27BD0000 and (w & 0x8000):
            prev = W[i - 2] if i >= 2 else 0
            if not starts or prev == 0x03E00008 or (prev >> 26) == 2:
                starts.append(seg.base + 4 * i)
    return starts


# --- scan --------------------------------------------------------------------

def scan(seg, fs):
    W, base = seg.W, seg.base
    fs = fs or [base]

    def fn_of(a):
        i = bisect.bisect_right(fs, a) - 1
        return fs[i] if i >= 0 else base

    const = {}                        # reg -> value (lui/ori/addiu tracking)
    starts = set(fs)
    out = defaultdict(list)
    gte_funcs = set()
    for i, w in enumerate(W):
        a = base + 4 * i
        if a in starts:
            const = {}
            if seg.kind == 'exe' and any(lo <= a < hi for lo, hi in ASM_RENDERERS):
                const[4] = SCRATCH    # asm renderer: a0 = scratchpad
        op, rs, rt, rd, imm, fn = fields(w)
        if is_gte(w):
            gte_funcs.add(fn_of(a))
        if op in (0x0A, 0x0B) and imm in W_IMMS:
            out['W'].append(dict(addr=a, word=w, op='slti' if op == 0x0A else 'sltiu',
                                 rs=rs, rt=rt, imm=imm))
        if op in (0x0A, 0x0B) and imm in H_IMMS:
            out['Y'].append(dict(addr=a, word=w, rs=rs, imm=imm))
        if op == 0x0A and imm == 0:
            out['Z'].append(dict(addr=a, word=w, rs=rs, rt=rt))
        sb = sign_branch(w)
        if sb:
            out['S'].append(dict(addr=a, word=w, mn=sb[0], rs=sb[1]))
        if (0x20 <= op <= 0x26 or 0x28 <= op <= 0x2B) and const.get(rs) == SCRATCH \
                and imm in (0x6C, 0x6E, 0x70, 0x72):
            out['CL'].append(dict(addr=a, word=w, kind='store' if op >= 0x28 else 'load',
                                  field=CLIP_X.get(imm) or CLIP_Y.get(imm),
                                  axis='x' if imm in CLIP_X else 'y'))
        r = writes(w)
        if op == 0x0F:
            const[rt] = imm << 16
        elif op in (0x09, 0x0D) and rs in const and rt:
            const[rt] = (const[rs] | imm) if op == 0x0D else (const[rs] + sx16(imm)) & 0xFFFFFFFF
        elif r is not None:
            const.pop(r, None)
    return out, fn_of, gte_funcs


def dist_ok(seg, a0, a1, reg):
    """True if no instruction strictly between a0 and a1 writes reg."""
    lo, hi = sorted((a0, a1))
    return all(writes(seg.word(a)) != reg for a in range(lo + 4, hi, 4))


def src_of(seg, a, reg, back=6):
    """Where the value of reg read at a came from: ('mem', base, off) for a
    load within `back` words, else ('reg', reg, write_addr or None)."""
    for b in range(a - 4, a - 4 * (back + 1), -4):
        try:
            w = seg.word(b)
        except IndexError:
            break
        if writes(w) == reg:
            op, rs, rt, rd, imm, fn = fields(w)
            if 0x20 <= op <= 0x26:
                return ('mem', rs, imm, op)
            return ('reg', reg, b)
    return ('reg', reg, None)


def same_source(seg, a0, r0, a1, r1):
    if r0 == r1 and dist_ok(seg, a0, a1, r0):
        return True
    s0, s1 = src_of(seg, a0, r0), src_of(seg, a1, r1)
    return s0[0] == 'mem' and s0[:3] == s1[:3]


def classify(seg, out, fn_of, gte_funcs):
    Ws = sorted(out['W'], key=lambda x: x['addr'])
    Ys = sorted(out['Y'], key=lambda x: x['addr'])
    groups = []
    for w in Ws:
        f = fn_of(w['addr'])
        if groups and groups[-1]['func'] == f and \
                w['addr'] - groups[-1]['W'][-1]['addr'] <= 4 * WINDOW:
            groups[-1]['W'].append(w)
        else:
            groups.append(dict(func=f, W=[w], L=[]))
    for g in groups:
        regs = {w['rs'] for w in g['W']}
        lo = g['W'][0]['addr'] - 4 * 2 * WINDOW
        hi = g['W'][-1]['addr'] + 4 * 2 * WINDOW
        for z in out['Z']:
            if lo <= z['addr'] <= hi and fn_of(z['addr']) == g['func'] and z['rs'] in regs:
                near = min(g['W'], key=lambda w: abs(w['addr'] - z['addr'])
                           if w['rs'] == z['rs'] else 1 << 30)
                if near['rs'] == z['rs'] and dist_ok(seg, near['addr'], z['addr'], z['rs']):
                    g['L'].append(dict(z, kind='L0', mn='slti'))
        for s in out['S']:
            if not (lo <= s['addr'] <= hi and fn_of(s['addr']) == g['func']):
                continue
            # Pair with the nearest compare fed by the same value: the same
            # register with no write in between, or the same memory source
            # (the compiled renderers reload SX/SY with lh before every test).
            cands = [(abs(w['addr'] - s['addr']), 'W') for w in g['W']
                     if same_source(seg, w['addr'], w['rs'], s['addr'], s['rs'])]
            cands += [(abs(y['addr'] - s['addr']), 'Y') for y in Ys
                      if abs(y['addr'] - s['addr']) <= 4 * 2 * WINDOW and
                      same_source(seg, y['addr'], y['rs'], s['addr'], s['rs'])]
            if cands and min(cands)[1] == 'W':
                g['L'].append(dict(s, kind='LB'))
        g['L'].sort(key=lambda x: x['addr'])
        g['gte'] = g['func'] in gte_funcs
        g['isolated'] = len(g['W']) == 1 and not g['L'] and not g['gte']
    return groups


def typo_group(seg, g):
    """The subdivided-course paths shift X0 with `sra v1,v1,16` four times:
    v1 ends as sign(X0), s5..s7 as X<<16. Their right chain never rejects."""
    a0 = g['W'][0]['addr']
    return sum(1 for a in range(a0 - 0x30, a0, 4) if seg.word(a) == TYPO_WORD) >= 4


def collect(args):
    exe, exe_md5 = load_exe(args.exe, args.bin_image or default_bin_image())
    segs = [exe] + (load_overlays(args.bin_image) if args.bin_image else [])
    sites = defaultdict(list)      # key -> [(addr, word, seg, func, note)]
    report = [f'# r4_ws_scan.py report  EXE md5 {exe_md5}']
    for seg in segs:
        fs = exe_functions(args.ranges) if seg.kind == 'exe' else ovl_functions(seg)
        out, fn_of, gte = scan(seg, fs)
        groups = classify(seg, out, fn_of, gte)
        report.append(f'==== {seg.name} 0x{seg.base:08X}-0x{seg.end:08X}'
                      + (f' (identical: {seg.aliases})' if seg.aliases else '')
                      + f': {len(out["W"])} W tests in {len(groups)} groups, '
                      f'{len(out["Y"])} Y tests (left alone)')
        for g in groups:
            typo = typo_group(seg, g)
            tag = 'ISOLATED (not a screen cull)' if g['isolated'] else \
                ('typo path' if typo else 'vertex group')
            report.append(f'  func 0x{g["func"]:08X}: {tag}')
            for w in g['W']:
                report.append(f'    W  {w["addr"]:08X} {mnemonic(w["addr"], w["word"])}')
            for l in g['L']:
                report.append(f'    {l["kind"]:2s} {l["addr"]:08X} {mnemonic(l["addr"], l["word"])}')
            if g['isolated']:
                continue
            for w in g['W']:
                if typo:
                    sites['noop'].append((w['addr'], w['word'], seg.name, g['func']))
                elif w['op'] == 'slti':
                    sites['slti_sites'].append((w['addr'], w['word'], seg.name, g['func']))
                else:
                    sites['screen_x_sites'].append((w['addr'], w['word'], seg.name, g['func']))
            if typo and g['L']:
                last = max(l['addr'] for l in g['L'])
                for a in (last + 4, last + 8):
                    if is_branch(seg.word(a)):
                        sites['branch_keep_sites'].append((a, seg.word(a), seg.name, g['func']))
                        break
            for l in g['L']:
                if l['kind'] == 'L0':
                    if not typo:
                        sites['slti_lower_sites'].append((l['addr'], l['word'], seg.name, g['func']))
                elif l['mn'] == 'bltz':
                    sites['bltz_sites'].append((l['addr'], l['word'], seg.name, g['func']))
                elif l['mn'] == 'bgez':
                    sites['bgez_sites'].append((l['addr'], l['word'], seg.name, g['func']))
                else:
                    sites['unmapped_%s' % l['mn']].append((l['addr'], l['word'], seg.name, g['func']))
        for c in sorted(out['CL'], key=lambda x: x['addr']):
            if c['kind'] == 'load' and c['axis'] == 'x':
                sites['clip_edge_x_load_sites'].append(
                    (c['addr'], c['word'], seg.name, fn_of(c['addr']), c['field']))
    if any(sn != 'EXE' for k in KEYS for (_a, _w, sn, *_r) in sites.get(k, [])):
        report.append('WARNING: overlay sites found; they need overlay-aware config')
    for k in sorted(sites):
        report.append(f'== {k}: {len(sites[k])}')
    return sites, report


def emit_toml(sites):
    t = ['[widescreen.cull]',
         '# Generated by tools/r4_ws_scan.py --emit-toml; tools/r4_ws_scan.py --check',
         '# game.toml keeps it in step. Every site is main-EXE code (no R4.BIN code',
         '# overlay holds a screen-X test). Identity at 4:3 (margin 0).',
         'guard_pixels = 0',
         '# R4 compares Y against 0xF0/0x1E0 and tests left edges with bgez/bltz',
         '# chains, so the auto detector would find nothing: list sites explicitly.',
         'auto_screen_x = false']
    notes = {
        'slti_sites': 'right edge: slti SX,0x140 (keep while SX < 320+m)',
        'bltz_sites': 'left edge, last vertex: bltz SX,reject (reject while SX < -m)',
        'bgez_sites': 'left edge, other vertices: bgez SX,keep (keep while SX >= -m)',
        'clip_edge_x_load_sites': 'asm renderers: lh of the scratchpad clip rect X '
                                  '(0 -> -m, 320 -> 320+m; mirror 98..222 unchanged)',
        'branch_keep_sites': 'subdivided-course paths: vanilla `sra v1,v1,16` x4 typo '
                             'leaves a sign-only X, so the left reject is off while wide',
    }
    for k in KEYS:
        rows = sorted(set(sites.get(k, [])))
        if not rows:
            continue
        t.append(f'# {notes[k]}')
        t.append(f'{k} = [')
        for a, w, sn, f, *extra in rows:
            t.append(f'  "0x{a:08X}",  # fn {f:08X} {mnemonic(a, w)}')
        t.append(']')
    return '\n'.join(t) + '\n'


def check(sites, game_toml):
    import tomllib
    with open(game_toml, 'rb') as fh:
        cfg = tomllib.load(fh)
    cull = cfg.get('widescreen', {}).get('cull', {})
    bad = 0
    for k in KEYS:
        want = {a for a, *_ in sites.get(k, [])}
        have = {int(v, 16) for v in cull.get(k, [])}
        if want != have:
            bad += 1
            miss, extra = sorted(want - have), sorted(have - want)
            print(f'{k}: game.toml drifts from the scan '
                  f'(missing {[f"0x{a:08X}" for a in miss]}, extra {[f"0x{a:08X}" for a in extra]})')
        else:
            print(f'{k}: {len(have)} sites match')
    unmapped = [k for k in sites if k.startswith('unmapped_') or k in
                ('screen_x_sites', 'slti_lower_sites')]
    for k in unmapped:
        bad += 1
        print(f'{k}: {len(sites[k])} scanned sites have no configured kind')
    return 1 if bad else 0


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--exe', default=None,
                    help='boot EXE (default: disc/SLUS_007.97, else read from the disc image)')
    ap.add_argument('--bin-image', default=None,
                    help='raw .bin of the disc (default: disc/*.bin); R4.BIN is read from it')
    ap.add_argument('--no-overlays', action='store_true', help='scan the EXE only')
    ap.add_argument('--ranges', default=os.path.join(ROOT, 'generated', 'SLUS_007.97_full.ranges'))
    mode = ap.add_mutually_exclusive_group(required=True)
    mode.add_argument('--emit-toml', action='store_true')
    mode.add_argument('--check', metavar='GAME_TOML')
    mode.add_argument('--report', action='store_true')
    args = ap.parse_args()
    if args.no_overlays:
        args.bin_image = None
    elif args.bin_image is None:
        args.bin_image = default_bin_image()
        if args.bin_image is None:
            raise SystemExit('no disc/*.bin: pass --bin-image or --no-overlays')
    sites, report = collect(args)
    if args.report:
        print('\n'.join(report))
        return 0
    if args.emit_toml:
        sys.stdout.write(emit_toml(sites))
        return 0
    return check(sites, args.check)


if __name__ == '__main__':
    sys.exit(main())
