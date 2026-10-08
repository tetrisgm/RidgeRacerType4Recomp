#!/usr/bin/env python3
"""Build the US (SLUS-00797) HD HUD pack from Kuid0us/T4HDHUD.

T4HDHUD names its PNGs for the NTSC-J game with Beetle-style keys
(TTTTTTTT-PPPPPPPP.png: CRC-32 of the uploaded texture words, CRC-32 of the
16-entry CLUT). Most HUD uploads are byte-identical in the US game, so those
files are used as they are. The rest is derived here:

* The US text/digit atlas (upload 32203df6) differs from the JP one
  (c41643fd): "mph" replaces "km/h". Every other glyph sits at the same place,
  so the US atlas is the JP art per palette; glyphs the art leaves empty in one
  palette are taken from another palette's art, recoloured; "mph", and any
  glyph whose shape the art does not cover, is an edge-directed 8x upscale
  (Scale2x three times) of the US native glyph.
* Palette variants the US game draws that the pack lacks (gauge ticks lit and
  unlit, course-map shadow, logo fades) are the pack art recoloured from one
  palette to the other.

Recolouring uses native reference images (the US upload decoded with each
CLUT, named like pack keys, at native size): a native pixel's colour under the
source palette maps to its colour under the target palette.

Usage:
  tools/r4_hd_hud_pack.py --upstream T4HDHUD/RidgeRacerType4-texture-replacements \
      --native DIR_OF_NATIVE_REFS --out mods/preloaded/packages/r4.enhancement.ui-fonts/1.0.0/pack
"""
import argparse
import hashlib
import json
import pathlib
import sys

import numpy as np
from PIL import Image

# Upstream files whose upload and CLUT CRCs are identical in the US game.
COPY = [
    "1477dbf9-38bfad7b", "185689d1-601f4eec", "275d0836-5c0dfd2f",
    "2edf11dc-cf63f4a4", "5fec75f-601f4eec", "61e37be4-601f4eec",
    "635e2d0a-c7ee5386", "6d2ed651-601f4eec", "75c13c85-53ec37ad",
    "75c13c85-73b584a", "799a8ca3-601f4eec", "8c0241a2-601f4eec",
    "b18b2859-231036a2", "cb4bbc22-601f4eec", "d57328c2-601f4eec",
]
# US text atlas: target upload, JP art upload, palettes, native-only boxes
# (x, y, w, h in native texels).
ATLAS = ("32203df6", "c41643fd", ["bf9158c6", "3cba29b4", "db4fb359"],
         [(40, 19, 24, 13)])  # "mph"
# Palette variants drawn by the US game: target key <- source key.
RECOLOR = {
    "1477dbf9-2cbaa84c": "1477dbf9-38bfad7b", "1477dbf9-3cba29b4": "1477dbf9-38bfad7b",
    "1477dbf9-601f4eec": "1477dbf9-38bfad7b", "1477dbf9-cf63f4a4": "1477dbf9-38bfad7b",
    "185689d1-38bfad7b": "185689d1-601f4eec", "185689d1-cf63f4a4": "185689d1-601f4eec",
    "275d0836-bf9158c6": "275d0836-5c0dfd2f", "275d0836-c7ee5386": "275d0836-5c0dfd2f",
    "2edf11dc-38bfad7b": "2edf11dc-cf63f4a4", "2edf11dc-3cba29b4": "2edf11dc-cf63f4a4",
    "2edf11dc-601f4eec": "2edf11dc-cf63f4a4", "2edf11dc-bf9158c6": "2edf11dc-cf63f4a4",
    "b18b2859-0cf3265e": "b18b2859-231036a2", "b18b2859-22bc9fbd": "b18b2859-231036a2",
    "b18b2859-33644e53": "b18b2859-231036a2", "b18b2859-6d2763ce": "b18b2859-231036a2",
    "b18b2859-8c4dcb46": "b18b2859-231036a2", "b18b2859-eb85a939": "b18b2859-231036a2",
}


def load(path):
    return np.asarray(Image.open(path).convert("RGBA"))


def color_map(src_native, dst_native):
    """Native colour under the source palette -> colour under the target."""
    pairs = {}
    s = src_native.reshape(-1, 4)
    d = dst_native.reshape(-1, 4)
    for a, b in zip(map(tuple, s), map(tuple, d)):
        if a[3] and a not in pairs:
            pairs[a] = b
    return pairs


def recolor(hd, pairs):
    keys = np.array([k[:3] for k in pairs], np.int32)
    vals = np.array(list(pairs.values()), np.uint8)
    rgb = hd[..., :3].astype(np.int32).reshape(-1, 3)
    near = np.empty(len(rgb), np.int64)
    for i in range(0, len(rgb), 1 << 16):  # bounded memory
        d = ((rgb[i:i + (1 << 16), None, :] - keys[None]) ** 2).sum(-1)
        near[i:i + (1 << 16)] = d.argmin(-1)
    out = vals[near].reshape(hd.shape).copy()
    out[..., 3] = np.where(out[..., 3] > 0, hd[..., 3], 0)
    return out


def scale2x(a):
    h, w = a.shape
    p = np.pad(a, 1, mode="edge")
    B, D, E = p[0:h, 1:w + 1], p[1:h + 1, 0:w], p[1:h + 1, 1:w + 1]
    F, H = p[1:h + 1, 2:w + 2], p[2:h + 2, 1:w + 1]
    o = np.empty((h * 2, w * 2), a.dtype)
    c = (B != H) & (D != F)
    o[0::2, 0::2] = np.where(c & (D == B), D, E)
    o[0::2, 1::2] = np.where(c & (B == F), F, E)
    o[1::2, 0::2] = np.where(c & (D == H), D, E)
    o[1::2, 1::2] = np.where(c & (H == F), F, E)
    return o


def upscale8(native):
    flat = native.reshape(-1, 4)
    colors, inv = np.unique(flat, axis=0, return_inverse=True)
    idx = inv.reshape(native.shape[:2])
    for _ in range(3):
        idx = scale2x(idx)
    return colors[idx]


def dilate(m):
    p = np.pad(m, 1)
    return p[1:-1, 1:-1] | p[:-2, 1:-1] | p[2:, 1:-1] | p[1:-1, :-2] | p[1:-1, 2:]


def components(m):
    lab = np.zeros(m.shape, np.int32)
    n = 0
    for y, x in zip(*np.nonzero(m)):
        if lab[y, x]:
            continue
        n += 1
        lab[y, x] = n
        stack = [(y, x)]
        while stack:
            cy, cx = stack.pop()
            for ny, nx in ((cy + 1, cx), (cy - 1, cx), (cy, cx + 1), (cy, cx - 1)):
                if 0 <= ny < m.shape[0] and 0 <= nx < m.shape[1] and m[ny, nx] and not lab[ny, nx]:
                    lab[ny, nx] = n
                    stack.append((ny, nx))
    return lab, n


def build_atlas(upstream, native_dir, out_dir, log):
    us, jp, palettes, native_boxes = ATLAS
    natives = {p: load(native_dir / f"{us}-{p}.png") for p in palettes}
    hds = {p: load(upstream / f"{jp}-{p}.png") for p in palettes}
    h, w = natives[palettes[0]].shape[:2]
    S = hds[palettes[0]].shape[0] // h
    block = np.ones((S, S), bool)
    occ_hd = {p: hds[p][..., 3].reshape(h, S, w, S).mean(axis=(1, 3)) > 127 for p in palettes}
    for p in palettes:
        occ_us = natives[p][..., 3] > 0
        out = hds[p].copy()
        up = upscale8(natives[p])
        lab, n = components(occ_us)
        claimed = np.zeros((h, w), bool)
        for c in range(1, n + 1):
            m = lab == c
            region = dilate(dilate(m))
            claimed |= region

            def score(q):
                return (m & occ_hd[q]).sum() / max(1, (m | (occ_hd[q] & region)).sum())
            if score(p) >= 0.75:
                continue
            big = np.kron(region, block)
            best = max((q for q in palettes if q != p), key=score)
            if score(best) >= 0.75:
                out[big] = recolor(hds[best], color_map(natives[best], natives[p]))[big]
                log.append(f"{us}-{p}: glyph at {np.argwhere(m).min(0)[::-1].tolist()} from palette {best}")
            else:
                out[big] = up[big]
                log.append(f"{us}-{p}: glyph at {np.argwhere(m).min(0)[::-1].tolist()} upscaled from native")
        for bx, by, bw, bh in native_boxes:
            region = np.zeros((h, w), bool)
            region[by:by + bh, bx:bx + bw] = True
            big = np.kron(region, block)
            out[big] = up[big]
            claimed |= region
            log.append(f"{us}-{p}: US-only region {bx},{by} {bw}x{bh} upscaled from native")
        out[np.kron(~claimed, block)] = 0  # JP art with no US glyph under it
        Image.fromarray(out).save(out_dir / f"{us}-{p}.png")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--upstream", type=pathlib.Path, required=True)
    ap.add_argument("--native", type=pathlib.Path, required=True)
    ap.add_argument("--out", type=pathlib.Path, required=True)
    args = ap.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    log = []
    for key in COPY:
        (args.out / f"{key}.png").write_bytes((args.upstream / f"{key}.png").read_bytes())
    build_atlas(args.upstream, args.native, args.out, log)
    for target, source in RECOLOR.items():
        hd = load(args.upstream / f"{source}.png")
        pairs = color_map(load(args.native / f"{source}.png"), load(args.native / f"{target}.png"))
        Image.fromarray(recolor(hd, pairs)).save(args.out / f"{target}.png")
    files = sorted(p.name for p in args.out.glob("*.png"))
    manifest = {name: hashlib.sha256((args.out / name).read_bytes()).hexdigest() for name in files}
    print(json.dumps({"files": len(files), "notes": log}, indent=1))
    return manifest


if __name__ == "__main__":
    sys.exit(0 if main() else 1)
