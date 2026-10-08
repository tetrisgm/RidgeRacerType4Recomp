#!/usr/bin/env python3
"""r4_detail_scan.py - R4 Max Detail's guest sites, checked against the disc.

R4's course renderers (0x80060F94 .. 0x8006E354) drop a polygon whose
ordering-table index (OTZ >> 5, in $v0) is past the end of the table:

  sltiu t2, v0, 0x1C0 ; beqz t2, reject                    (16 renderers)
  addiu at, v0, -1 ; sltiu at, at, 0x1BF ; beqz at, reject  (2 renderers)

and the code after the guard indexes the table with $v0 (plus a per-polygon
bias), so clamping $v0 to 0x1BF keeps a far polygon in the farthest slot
with the same worst-case slot as stock. game.toml lists the guards as
psxrecomp [[draw_distance.clamp]] sites; this tool derives that list from
the boot EXE and checks the R4 Max Detail package's car-table patches.

It also ties the plugin's guest constants to the game's own code: the
course visibility lookup at 0x8006F5AC indexes its table as
table[section * 8 + octant] (`sll s0,s0,3; addu s0,s0,v0`), which is
R4_PVS_COLUMNS in src/mods/r4_pvs.h, and the octant and course-list hooks
gate on return addresses that must follow their `jal`s. The same holds
for the car-reflection hook (the env-map car draw reads the page word that
the setter writes) and the rear-view mirror's limit and list hooks.

  tools/r4_detail_scan.py --emit-toml          print the [[draw_distance.clamp]] block
  tools/r4_detail_scan.py --check game.toml    game.toml, the manifest and the
                                               plugin's guest layout match the EXE
  tools/r4_detail_scan.py --check-manifest     the manifest alone (no disc): the
                                               package is on by default, every
                                               option has Stock and the owner's
                                               default (the most detail, except
                                               Mirror scenery: Stock), and the
                                               car patches are exactly the car
                                               tables in r4_max_detail.h

The EXE comes from disc/SLUS_007.97 or, without it, from the disc image's
root directory (tools/r4_ws_scan.py's reader). Exit status 0 = consistent.
"""
import re
import argparse
import os
import sys
import tomllib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import r4_ws_scan as ws  # noqa: E402  (EXE reader)

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MANIFEST = os.path.join(ROOT, 'mods', 'preloaded', 'packages',
                        'r4.enhancement.max-detail', '1.0.0', 'manifest.toml')
MODS = os.path.join(ROOT, 'src', 'mods')
PVS_H = os.path.join(MODS, 'r4_pvs.h')
MD_H = os.path.join(MODS, 'r4_max_detail.h')

RENDERERS = (0x80060F94, 0x8006E354)   # course renderer dispatcher .. matrix helpers
V0, AT, T2 = 2, 1, 10
SLTIU_T2_V0_1C0 = 0x2C4A01C0
ADDIU_AT_V0_M1 = 0x2441FFFF
SLTIU_AT_AT_1BF = 0x2C2101BF
SLL_V0_V0_2 = 0x00021080
CLAMP_MAX = 0x1BF
EXPECTED_SITES = 18

CAR_TABLE = 0x8009F228
CAR_ROWS = 5
CAR_CULL = 8704

# r4_max_detail.h entry hooks game.toml must list (besides the course renderer).
HOOK_DEFINES = (
    ('R4_MD_ENV_RENDER_FN', 'the env-map car part draw (car reflections)'),
    ('R4_MD_MIRROR_LIMIT_FN', "the rear-view mirror's block limit (mirror scenery)"),
    ('R4_MD_MIRROR_LIST_FN', "the mirror list's first consumer (mirror scenery)"),
)


def beqz(w, reg):
    return (w >> 26) == 0x04 and ((w >> 21) & 31) == reg and ((w >> 16) & 31) == 0


def gpr_written(w):
    """The GPR an instruction writes, or None."""
    op, rs, rt, rd = w >> 26, (w >> 21) & 31, (w >> 16) & 31, (w >> 11) & 31
    if op == 0x00:
        f = w & 0x3F
        if f in (0x08, 0x0C, 0x0D, 0x11, 0x13, 0x18, 0x19, 0x1A, 0x1B):
            return None            # jr, syscall, break, mthi, mtlo, mult(u), div(u)
        return rd                  # shifts, jalr, mfhi/mflo, ALU
    if op == 0x01:
        return 31 if rt in (0x10, 0x11) else None   # bltzal/bgezal link
    if op == 0x03:
        return 31                  # jal
    if 0x08 <= op <= 0x0F or 0x20 <= op <= 0x26:
        return rt                  # I-type ALU, loads
    if op in (0x10, 0x12) and rs in (0x00, 0x02):
        return rt                  # mfc0/cfc0, mfc2/cfc2
    return None                    # j, branches, stores, lwc2/swc2, cop ops


def index_flows_from_v0(seg, site):
    """From the guard to the slot computation `sll v0, v0, 2`, the only
    write to $v0 is the bias add `addu v0, v0, rX`."""
    for a in range(site + 4, site + 4 * 200, 4):
        w = seg.word(a)
        if w == SLL_V0_V0_2:
            return True
        if gpr_written(w) == V0:
            is_bias = (w >> 26) == 0 and (w & 0x3F) == 0x21 and \
                ((w >> 11) & 31) == V0 and ((w >> 21) & 31) == V0
            if not is_bias:
                return False
    return False


def scan(seg):
    sites = []
    for a in range(RENDERERS[0], RENDERERS[1], 4):
        w = seg.word(a)
        if w == SLTIU_T2_V0_1C0 and beqz(seg.word(a + 4), T2):
            form = 'sltiu t2, v0, 0x1C0'
        elif (w == ADDIU_AT_V0_M1 and seg.word(a + 4) == SLTIU_AT_AT_1BF
              and beqz(seg.word(a + 8), AT)):
            form = 'addiu at, v0, -1 (then sltiu at, at, 0x1BF)'
        else:
            continue
        if not index_flows_from_v0(seg, a):
            raise SystemExit(f'0x{a:08X}: the OT index after this guard is not $v0')
        sites.append((a, w, form))
    return sites


def emit(sites):
    out = ['# Course renderer far-clip guards (tools/r4_detail_scan.py --emit-toml;',
           '# --check keeps them in step with the disc). Inert unless R4 Max Detail',
           '# switches the clamp on: a polygon past the end of the ordering table is',
           '# then kept in the farthest slot (index 0x1BF) instead of dropped.']
    for a, w, form in sites:
        out += ['[[draw_distance.clamp]]',
                f'address = "0x{a:08X}"',
                f'expected = "0x{w:08X}"  # {form}',
                f'reg = {V0}',
                f'max = 0x{CLAMP_MAX:X}',
                '']
    return '\n'.join(out).rstrip('\n') + '\n'


def check_game_toml(sites, path):
    cfg = tomllib.load(open(path, 'rb'))
    listed = cfg.get('draw_distance', {}).get('clamp', [])
    want = [(a, w, V0, CLAMP_MAX) for a, w, _ in sites]
    have = [(int(c['address'], 16), int(c['expected'], 16), c['reg'], c['max'])
            for c in listed]
    errors = []
    if have != want:
        errors.append(f'{path}: [[draw_distance.clamp]] does not match the EXE scan '
                      f'({len(have)} listed, {len(want)} found); '
                      'regenerate with tools/r4_detail_scan.py --emit-toml')
    hooks = [int(x, 16) for x in cfg.get('recompiler', {}).get('mod_function_entry_funcs', [])]
    need = [(RENDERERS[0], 'the course renderer (course hook)')]
    need += [(c_define(MD_H, name), what) for name, what in HOOK_DEFINES]
    for a, what in need:
        if a not in hooks:
            errors.append(f'{path}: mod_function_entry_funcs lacks 0x{a:08X}, '
                          f'{what} (R4 Max Detail)')
    return errors


def exe_image(path):
    """The boot EXE's whole load image (code and data), as the BIOS loads it."""
    import struct
    if path is None and os.path.exists(ws.DEFAULT_EXE):
        path = ws.DEFAULT_EXE
    if path is not None:
        b = open(path, 'rb').read()
    else:
        b = ws.DiscImage(ws.default_bin_image()).read_root_file(ws.EXE_NAME)
    tsize = struct.unpack_from('<I', b, 0x1C)[0]
    return b[0x800:0x800 + tsize]


def check_manifest(seg_bytes, path):
    """Every [[patch]] guard matches the EXE; the replacements keep the car
    draw distance (T2) at the stock forward cull and keep each row ordered."""
    m = tomllib.load(open(path, 'rb'))
    errors = []
    patches = m.get('patch', [])
    if not patches:
        errors.append(f'{path}: no [[patch]] entries')
    for p in patches:
        addr = p['address']
        exp = bytes.fromhex(p['expected'])
        rep = bytes.fromhex(p['replace'])
        if not (CAR_TABLE <= addr and addr + len(exp) <= CAR_TABLE + 6 * CAR_ROWS
                and (addr - CAR_TABLE) % 6 == 0 and len(exp) == 6 == len(rep)):
            errors.append(f'{path}: patch at 0x{addr:08X} is not one car-table row')
            continue
        stock = seg_bytes(addr, 6)
        if stock != exp:
            errors.append(f'{path}: patch at 0x{addr:08X} expects {exp.hex(" ")}, '
                          f'EXE holds {stock.hex(" ")}')
        t = [int.from_bytes(rep[i:i + 2], 'little', signed=True) for i in (0, 2, 4)]
        if t[2] > CAR_CULL or not t[0] <= t[1] <= t[2]:
            errors.append(f'{path}: patch at 0x{addr:08X} writes {t}: T2 must stay '
                          f'<= {CAR_CULL} and T0 <= T1 <= T2')
    return errors


# ---- the plugin's guest constants against the game's code --------------------

def c_define(path, name):
    m = re.search(r'#define\s+' + name + r'\s+(0x[0-9A-Fa-f]+|\d+)u?\b', open(path).read())
    if not m:
        raise SystemExit(f'{path}: no #define {name}')
    return int(m.group(1), 0)


def c_table(path, name):
    """A `static const int16_t name[rows][3] = { { a, b, c }, ... };` table."""
    m = re.search(r'\b' + name + r'\s*\[[^]]*\]\s*\[3\]\s*=\s*\{(.*?)\};',
                  open(path).read(), re.S)
    if not m:
        raise SystemExit(f'{path}: no table {name}')
    return [tuple(int(v) for v in row.split(','))
            for row in re.findall(r'\{([^{}]*)\}', m.group(1))]


def jal(target):
    return 0x0C000000 | ((target & 0x0FFFFFFF) >> 2)


def lui_lo(addr):
    """(%hi, %lo) as a lui / signed-offset pair reaches `addr`."""
    return ((addr + 0x8000) >> 16) & 0xFFFF, addr & 0xFFFF


def md_layout():
    """r4_max_detail.h's reflection and mirror constants, as instruction words
    the game must hold at those addresses."""
    env_fn = c_define(MD_H, 'R4_MD_ENV_RENDER_FN')
    page = c_define(MD_H, 'R4_MD_ENV_TPAGE_ADDR')
    race_page = c_define(MD_H, 'R4_MD_ENV_TPAGE_RACE')
    lim_fn = c_define(MD_H, 'R4_MD_MIRROR_LIMIT_FN')
    lim_ra = c_define(MD_H, 'R4_MD_MIRROR_LIMIT_RA')
    list_fn = c_define(MD_H, 'R4_MD_MIRROR_LIST_FN')
    list_ra = c_define(MD_H, 'R4_MD_MIRROR_LIST_RA')
    lst = c_define(MD_H, 'R4_MD_LIST_ADDR')
    if lst != c_define(PVS_H, 'R4_PVS_LIST_ADDR'):
        raise SystemExit(f'{MD_H}: R4_MD_LIST_ADDR is not r4_pvs.h R4_PVS_LIST_ADDR')
    ph, pl = lui_lo(page)
    lh, ll = lui_lo(lst)
    setter = env_fn - 0x0C
    return [
        # setter(a0): lui v0,%hi(page); jr ra; sw a0,%lo(page)(v0)
        (setter + 0x00, 0x3C020000 | ph, f'lui v0,%hi(0x{page:08X}) (R4_MD_ENV_TPAGE_ADDR)'),
        (setter + 0x08, 0xAC440000 | pl, f'sw a0,%lo(0x{page:08X})(v0)'),
        # the env-map draw reads the page twice: bgez at entry+0x34, the tpage at +0x58
        (env_fn + 0x28, 0x3C110000 | ph, f'lui s1,%hi(0x{page:08X})'),
        (env_fn + 0x2C, 0x8E220000 | pl, f'lw v0,%lo(0x{page:08X})(s1)'),
        (env_fn + 0x34, 0x04410003, 'bgez v0 (draw only with a page >= 0)'),
        (env_fn + 0x58, 0x8E240000 | pl, f'lw a0,%lo(0x{page:08X})(s1)'),
        # race init, the after-goal run and replays set the race page themselves
        (0x8003D3E0, jal(setter), 'jal setter (race init)'),
        (0x8003D3E4, 0x24040000 | race_page, f'addiu a0,zero,{race_page} (R4_MD_ENV_TPAGE_RACE)'),
        (0x8005EB78, jal(setter), 'jal setter (replay / attract frame)'),
        (0x8005EB7C, 0x24040000 | race_page, f'addiu a0,zero,{race_page}'),
        # the mirror draw: list built, limit, count lowered, GTE set-up, consumer
        (lim_ra - 0x10, jal(0x8006EB58), 'jal 0x8006EB58 (mirror list build)'),
        (lim_ra - 0x08, jal(lim_fn), f'jal 0x{lim_fn:08X} (R4_MD_MIRROR_LIMIT_RA - 8)'),
        (lim_ra + 0x00, 0x3C040000 | lh, f'lui a0,%hi(0x{lst:08X})'),
        (lim_ra + 0x04, 0x8C830000 | ll, f'lw v1,%lo(0x{lst:08X})(a0) (the count)'),
        (lim_ra + 0x0C, 0x0043182B, 'sltu v1,v0,v1'),
        (lim_ra + 0x18, 0xAC820000 | ll, f'sw v0,%lo(0x{lst:08X})(a0) (only the count)'),
        (lim_ra + 0x1C, jal(0x8006EF88), 'jal 0x8006EF88 (GTE set-up)'),
        (list_ra - 0x08, jal(list_fn), f'jal 0x{list_fn:08X} (R4_MD_MIRROR_LIST_RA - 8)'),
    ]


def far_layout():
    """r4_max_detail.h's far-object, far-car and frame-budget constants, and
    r4_pvs.h's centreline pointer, as the instruction words the game holds."""
    xf = c_define(MD_H, 'R4_MD_XF_FN')
    st_fn = c_define(MD_H, 'R4_MD_XF_SETTRANS_FN')
    st_ra = c_define(MD_H, 'R4_MD_XF_SETTRANS_RA')
    vec = c_define(MD_H, 'R4_MD_XF_VECTOR_OFF')
    mat = c_define(MD_H, 'R4_MD_XF_MATRIX_OFF')
    cam = c_define(MD_H, 'R4_MD_CAMERA_POS_ADDR')
    cmat = c_define(MD_H, 'R4_MD_CAMERA_MATRIX_ADDR')
    car_fn = c_define(MD_H, 'R4_MD_CAR_DRAW_FN')
    after = c_define(MD_H, 'R4_MD_CAR_AFTER_LOD_FN')
    far = c_define(MD_H, 'R4_MD_CAR_FAR')
    fs_fn = c_define(MD_H, 'R4_MD_FRAME_START_FN')
    fs_ra = c_define(MD_H, 'R4_MD_FRAME_START_RA')
    vs_fn = c_define(MD_H, 'R4_MD_VSYNC_FN')
    vs_ra = c_define(MD_H, 'R4_MD_VSYNC_WAIT_RA')
    segtab = c_define(PVS_H, 'R4_TRACK_SEGMENT_TABLE_ADDR')
    ras = [int(v, 0) for v in re.findall(
        r'(0x[0-9A-Fa-f]+)u', re.search(r'r4_md_car_after_lod_ra\[4\]\s*=\s*\{(.*?)\};',
                                         open(MD_H).read(), re.S).group(1))]
    if cam != 0x1F800008 or cmat != 0x1F800028:
        raise SystemExit(f'{MD_H}: the camera is at 0x1F800008, its matrix at 0x1F800028')
    th, tl = lui_lo(CAR_TABLE)
    sh, sl = lui_lo(segtab)
    want = [
        # 0x8006F160: pos - camera stored as 16 bits, rotated (MAC, 32 bits), SetTransMatrix
        (xf + 0x14, 0x3C061F80, 'lui a2,0x1f80 (the camera)'),
        (xf + 0x1C, 0x94A20000, 'lhu v0,0(a1) (pos.x, low 16 bits)'),
        (xf + 0x20, 0x94C30000 | (cam & 0xFFFF), 'lhu v1,8(a2) (camera x)'),
        (xf + 0x2C, 0xA6020000, 'sh v0,0(s0) (the 16-bit delta)'),
        (xf + 0x38, 0x34840000 | (cmat & 0xFFFF), 'ori a0,a0,0x28 (the camera matrix)'),
        (xf + 0x58, jal(0x800910A0), 'jal 0x800910A0 (rotate: MVMVA)'),
        (xf + 0x60, 0x8E020000 | vec, 'lw v0,8(s0) (R4_MD_XF_VECTOR_OFF)'),
        (xf + 0x70, 0xAE020000 | (mat + 0x14), 'sw v0,0x2C(s0) (the MATRIX t[] at R4_MD_XF_MATRIX_OFF)'),
        (st_ra - 0x08, jal(st_fn), f'jal 0x{st_fn:08X} (R4_MD_XF_SETTRANS_RA - 8)'),
        (st_ra - 0x04, 0x26040000 | mat, 'addiu a0,s0,0x18 (R4_MD_XF_MATRIX_OFF)'),
        (0x800910D4, 0x4A486012, 'MVMVA sf=1, rotation x V0, no translation'),
        (0x800910D8, 0xE8D90000, 'swc2 MAC1 (a 32-bit result)'),
        (st_fn + 0x00, 0x8C880014, 'lw t0,0x14(a0) (SetTransMatrix reads t[])'),
        # 0x8002DC00(car, row): the three row reads, then 0x80015F60 on every path
        (car_fn + 0x21C, 0x3C030000 | th, f'lui v1,%hi(0x{CAR_TABLE:08X})'),
        (car_fn + 0x220, 0x24630000 | tl, f'addiu v1,v1,%lo(0x{CAR_TABLE:08X})'),
        (0x8002DE34, 0x84620000, 'lh v0,0(v1) (T0)'),
        (0x8002E29C, 0x84620002, 'lh v0,2(v1) (T1)'),
        (0x8002E44C, 0x84620004, 'lh v0,4(v1) (T2)'),
        # the car renderer's ordering-table guard bounds the far cull distance
        (0x8005F6BC, 0x2C4201BF, 'sltiu v0,v0,447 (car OT guard)'),
        # the main loop: DrawSync, the VSync(1) floor wait, VSync(0), frame start
        (vs_ra - 0x18, jal(0x80092F2C), 'jal DrawSync (0x80092F2C)'),
        (vs_ra - 0x08, jal(vs_fn), f'jal 0x{vs_fn:08X} (R4_MD_VSYNC_WAIT_RA - 8)'),
        (vs_ra - 0x04, 0x24040001, 'addiu a0,zero,1 (VSync(1))'),
        (fs_ra - 0x08, jal(fs_fn), f'jal 0x{fs_fn:08X} (R4_MD_FRAME_START_RA - 8)'),
        # the segment lookup reads the centreline table pointer
        (0x800269E4, 0x3C070000 | sh, f'lui a3,%hi(0x{segtab:08X}) (R4_TRACK_SEGMENT_TABLE_ADDR)'),
        (0x800269F8, 0x8CE30000 | sl, f'lw v1,%lo(0x{segtab:08X})(a3)'),
    ]
    want += [(ra - 0x08, jal(after), f'jal 0x{after:08X} (r4_md_car_after_lod_ra)') for ra in ras]
    if not (far * 4 < 0x10000 and far < 447 * 32):
        raise SystemExit(f'{MD_H}: R4_MD_CAR_FAR {far} is past SZ or the car OT guard')
    return want


def poly_kinds():
    """r4_max_detail.h's r4_md_poly_kinds as (field, stride, table)."""
    m = re.search(r'r4_md_poly_kinds\[\]\s*=\s*\{(.*?)\};', open(MD_H).read(), re.S)
    return {tuple(int(v.strip().rstrip('u'), 0) for v in row.split(','))
            for row in re.findall(r'\{([^{}]*)\}', m.group(1))}


def check_poly_kinds(seg, seg_bytes):
    """Each renderer of the 1P course chain (0x800A2370) loads its block field,
    record size and table pointer: lw t1,F(t2); addiu t2,zero,S; ... lui t2 /
    ori t2 (the table)."""
    found = set()
    for i in range(11):
        r = int.from_bytes(seg_bytes(0x800A2370 + 4 * i, 4), 'little')
        words = [seg.word(r + 4 * k) for k in range(40)]
        for k, w in enumerate(words[:-6]):
            if (w >> 16) != 0x8D49 or (words[k + 1] >> 16) != 0x240A:
                continue
            hi = next((x for x in words[k:] if (x >> 16) == 0x3C0A), None)
            lo = next((x for x in words[k:] if (x >> 16) == 0x354A), None)
            if hi is not None and lo is not None:
                found.add((w & 0xFFFF, words[k + 1] & 0xFFFF, ((hi & 0xFFFF) << 16) | (lo & 0xFFFF)))
            break
    have = poly_kinds()
    errors = []
    if found != have:
        errors.append(f'{MD_H}: r4_md_poly_kinds {sorted(have)} differs from the course '
                      f'renderers {sorted(found)}')
    return errors


def check_hook_list(game_toml):
    """Every address the plugin registers is a mod entry hook in game.toml."""
    funcs = {int(a, 16) for a in tomllib.load(open(game_toml, 'rb'))['recompiler']
             .get('mod_function_entry_funcs', [])}
    src = open(os.path.join(MODS, 'r4_max_detail_plugin.c')).read()
    names = re.findall(r'R4_MD_REGISTER_ENTRY\((R4_\w+),', src)
    errors = []
    for n in names:
        try:
            a = c_define(MD_H, n)
        except SystemExit:
            try:
                a = c_define(PVS_H, n)
            except SystemExit:
                a = c_define(os.path.join(MODS, 'r4_max_detail_plugin.c'), n)
        if a not in funcs:
            errors.append(f'{game_toml}: mod_function_entry_funcs lacks 0x{a:08X} ({n}), '
                          'which the Max Detail plugin hooks')
    count = c_define(os.path.join(MODS, 'r4_max_detail_plugin.c'), 'R4_MD_HOOK_COUNT')
    if count != len(names):
        errors.append(f'R4_MD_HOOK_COUNT {count} != {len(names)} registrations')
    return errors


def check_guest_layout(seg):
    """The course visibility lookup at 0x8006F5AC and the hooks' return-address
    gates, as the plugin and r4_pvs.h encode them."""
    errors = []
    cols = c_define(PVS_H, 'R4_PVS_COLUMNS')
    table_ptr = c_define(PVS_H, 'R4_PVS_TABLE_PTR_ADDR')
    oct_fn = c_define(PVS_H, 'R4_PVS_OCTANT_FN')
    oct_ra = c_define(PVS_H, 'R4_PVS_OCTANT_RA')
    merge_fn = c_define(PVS_H, 'R4_PVS_MERGE_FN')
    merge_ras = (c_define(PVS_H, 'R4_PVS_MERGE_RA1'), c_define(PVS_H, 'R4_PVS_MERGE_RA2'))
    if cols <= 0 or cols & (cols - 1):
        return [f'{PVS_H}: R4_PVS_COLUMNS = {cols} is not a power of two']
    shift = cols.bit_length() - 1
    want = [
        # octant(a0): ((yaw & 0xFFF) + 0x100) >> 9 & 7, as r4_pvs_octant() computes it
        (oct_fn + 0x00, 0x8C820014, 'lw v0,0x14(a0)'),
        (oct_fn + 0x08, 0x30430FFF, 'andi v1,v0,0xfff'),
        (oct_fn + 0x0C, 0x24620100, 'addiu v0,v1,0x100'),
        (oct_fn + 0x1C, 0x00021243, 'sra v0,v0,9'),
        (oct_fn + 0x24, 0x30420007, 'andi v0,v0,7'),
        # the lookup: s0 = section, v0 = octant; entry = table[s0 * COLUMNS + v0]
        (oct_ra - 0x08, jal(oct_fn), f'jal 0x{oct_fn:08X} (R4_PVS_OCTANT_RA - 8)'),
        (oct_ra + 0x00, 0x00108000 | (shift << 6), f'sll s0,s0,{shift} (R4_PVS_COLUMNS = {cols})'),
        (oct_ra + 0x04, 0x02028021, 'addu s0,s0,v0'),
        (oct_ra + 0x08, 0x3C020000 | (((table_ptr + 0x8000) >> 16) & 0xFFFF),
         f'lui v0,%hi(0x{table_ptr:08X}) (R4_PVS_TABLE_PTR_ADDR)'),
        (oct_ra + 0x0C, 0x00108080, 'sll s0,s0,2'),
        (oct_ra + 0x10, 0x8C420000 | (table_ptr & 0xFFFF),
         f'lw v0,%lo(0x{table_ptr:08X})(v0) (R4_PVS_TABLE_PTR_ADDR)'),
    ]
    want += [(ra - 0x08, jal(merge_fn), f'jal 0x{merge_fn:08X} (R4_PVS_MERGE_RA - 8)')
             for ra in merge_ras]
    want += md_layout()
    want += far_layout()
    for a, w, what in want:
        got = seg.word(a)
        if got != w:
            errors.append(f'0x{a:08X}: expected `{what}` = 0x{w:08X}, the EXE holds '
                          f'0x{got:08X}; r4_pvs.h or r4_max_detail.h disagrees with the game')
    return errors


# ---- the package's policy (no disc needed) -----------------------------------

FEATURE = 'max-detail'
PLUGIN_ID = 'r4.maxdetail'
# Owner decisions (2026-10-01): on by default with every option at its most
# detail, Car reflections on ("On in races"), Mirror scenery off ("Separate
# option, off"); every option can be set back to stock.
DEFAULTS = {'draw_distance': 'maximum', 'course': 'full', 'cars': 'full',
            'split_screen': 'same', 'reflections': 'on', 'mirror': 'stock'}


def row_bytes(row):
    return b''.join(int(v).to_bytes(2, 'little', signed=True) for v in row)


def expected_patches():
    """The car-table [[patch]] set r4_max_detail.h's tables imply: Car detail =
    full writes every full row; Split screen = same with stock cars copies the
    1P row into both 2P rows."""
    stock = c_table(MD_H, 'r4_md_car_lod_stock')
    full = c_table(MD_H, 'r4_md_car_lod_full')
    if len(stock) != CAR_ROWS or len(full) != CAR_ROWS:
        raise SystemExit(f'{MD_H}: car tables are not {CAR_ROWS} rows')
    want = {(CAR_TABLE + 6 * r, row_bytes(stock[r]), row_bytes(full[r]),
             (('cars', 'full'),)) for r in range(CAR_ROWS)}
    want |= {(CAR_TABLE + 6 * r, row_bytes(stock[r]), row_bytes(stock[0]),
              (('cars', 'stock'), ('split_screen', 'same'))) for r in (3, 4)}
    return want, stock


def check_manifest_policy(path):
    m = tomllib.load(open(path, 'rb'))
    errors = []
    feats = [f for f in m.get('feature', []) if f.get('id') == FEATURE]
    if len(feats) != 1:
        errors.append(f'{path}: expected one [[feature]] id = "{FEATURE}"')
    elif feats[0].get('default_enabled') is not True:
        errors.append(f'{path}: feature {FEATURE} must be default_enabled = true '
                      '(owner decision: Max Detail is on by default)')
    opts = {o.get('id'): o for o in m.get('option', []) if o.get('feature') == FEATURE}
    if set(opts) != set(DEFAULTS):
        errors.append(f'{path}: options are {sorted(opts)}, expected {sorted(DEFAULTS)}')
    for oid, want in DEFAULTS.items():
        o = opts.get(oid)
        if not o:
            continue
        values = [c.get('value') for c in o.get('choice', [])]
        if o.get('default') != want:
            errors.append(f'{path}: option {oid} defaults to {o.get("default")!r}, '
                          f'expected {want!r} (owner decision)')
        if not values or values[0] != want or 'stock' not in values:
            errors.append(f'{path}: option {oid} choices {values} must list {want!r} '
                          "first and offer 'stock'")
    plugins = [p.get('id') for p in m.get('plugin', []) if p.get('feature') == FEATURE]
    if plugins != [PLUGIN_ID]:
        errors.append(f'{path}: feature {FEATURE} plugins {plugins}, expected [{PLUGIN_ID!r}]')
    want, _stock = expected_patches()
    have = set()
    for p in m.get('patch', []):
        if p.get('feature') != FEATURE or p.get('target') != 'main_exe':
            errors.append(f'{path}: patch at {p.get("address")} is not a {FEATURE} main_exe patch')
            continue
        have.add((p['address'], bytes.fromhex(p['expected']), bytes.fromhex(p['replace']),
                  tuple(sorted(p.get('when', {}).items()))))
    for a, e, r, w in sorted(want - have):
        errors.append(f'{path}: missing patch 0x{a:08X} {e.hex(" ")} -> {r.hex(" ")} when {dict(w)}')
    for a, e, r, w in sorted(have - want):
        errors.append(f'{path}: unexpected patch 0x{a:08X} {e.hex(" ")} -> {r.hex(" ")} '
                      f'when {dict(w)} (r4_max_detail.h car tables disagree)')
    return errors


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--exe', help='boot EXE (default: disc/SLUS_007.97 or the disc image)')
    ap.add_argument('--emit-toml', action='store_true')
    ap.add_argument('--check', metavar='GAME_TOML')
    ap.add_argument('--check-manifest', action='store_true',
                    help='check the manifest against the owner policy and r4_max_detail.h only')
    ap.add_argument('--manifest', default=MANIFEST)
    args = ap.parse_args()
    if args.check_manifest:
        errors = check_manifest_policy(args.manifest)
        for e in errors:
            print('FAIL:', e, file=sys.stderr)
        if errors:
            return 1
        print('r4_detail_scan: the manifest is on by default with the owner\'s '
              'option defaults, and its car patches match r4_max_detail.h')
        return 0
    seg, _md5 = ws.load_exe(args.exe, ws.default_bin_image())
    sites = scan(seg)
    if len(sites) != EXPECTED_SITES:
        raise SystemExit(f'found {len(sites)} course far-clip guards, expected '
                         f'{EXPECTED_SITES}: not the US boot EXE?')
    if args.emit_toml:
        sys.stdout.write(emit(sites))
        return 0
    if not args.check:
        ap.error('give --emit-toml or --check GAME_TOML')

    image = exe_image(args.exe)

    def seg_bytes(addr, n):
        off = addr - ws.EXE_BASE
        return image[off:off + n]

    errors = (check_game_toml(sites, args.check) + check_manifest(seg_bytes, args.manifest)
              + check_manifest_policy(args.manifest) + check_guest_layout(seg)
              + check_poly_kinds(seg, seg_bytes) + check_hook_list(args.check))
    _want, stock = expected_patches()
    exe_rows = seg_bytes(CAR_TABLE, 6 * CAR_ROWS)
    if b''.join(row_bytes(r) for r in stock) != exe_rows:
        errors.append(f'{MD_H}: r4_md_car_lod_stock is not the EXE car table '
                      f'({exe_rows.hex(" ")})')
    for e in errors:
        print('FAIL:', e, file=sys.stderr)
    if errors:
        return 1
    print(f'r4_detail_scan: {len(sites)} clamp sites, the car-table patches, the '
          'course-list lookup, the reflection and mirror hooks, the far object and car '
          'hooks, the frame-budget anchors and the course poly kinds match the EXE')
    return 0


if __name__ == '__main__':
    sys.exit(main())
