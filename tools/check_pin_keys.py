#!/usr/bin/env python3
"""check_pin_keys.py - does the pinned psxrecomp read the keys R4 sets?

psxrecomp's config loader skips [video] keys it does not know, so a key R4
sets ahead of its framework pin (one whose psxrecomp PR has not landed or not
been pinned yet) is inert: the game runs as if it were absent. R4's docs
describe what the keys do, so a release built in that state ships behaviour
its docs do not match.

    tools/check_pin_keys.py [--root DIR]     release gate: exit 1 if any key
                                             is unknown to the pin
    tools/check_pin_keys.py --ctest          the same check as a CTest: exit 77
                                             (skipped, with the list) instead,
                                             so a branch that sets a key ahead of
                                             its pin still tests clean

REQUIRED_VIDEO_KEYS names the keys R4's on-by-default display settings rely
on; game.toml must set each one (checked in both forms) and the pin must read
it.

A key counts as known when psxrecomp/recompiler/src/config_loader.cpp names it
in quotes. Run the strict form before building a release.
"""
import argparse
import os
import sys

try:
    import tomllib
except ImportError:  # Python < 3.11
    tomllib = None


def unknown_video_keys(root):
    with open(os.path.join(root, 'game.toml'), 'rb') as fh:
        video = tomllib.load(fh).get('video', {})
    loader = os.path.join(root, 'psxrecomp', 'recompiler', 'src',
                          'config_loader.cpp')
    with open(loader, encoding='utf-8') as fh:
        src = fh.read()
    return [k for k in set(video) | set(REQUIRED_VIDEO_KEYS)
            if f'"{k}"' not in src]


# [video] keys R4's on-by-default display settings rely on (README "On by
# default"). Checked against the pin even if game.toml stops setting one, and
# game.toml must set each (a dropped key silently changes the default).
REQUIRED_VIDEO_KEYS = (
    'internal_resolution',      # "display" (Match display)
    'dynamic_resolution',       # psxrecomp #508 / #537
    'dynamic_resolution_min',   # "display" floor (psxrecomp #583)
    'supersample',              # 1.5 (psxrecomp #583)
    'antialiasing_mode',        # "off" (psxrecomp #583)
    'render_thread',            # psxrecomp #536
    'frame_generation',         # psxrecomp #539
    'frame_generation_method',  # "reprojection" (psxrecomp #583)
    'present_thread',           # psxrecomp #540
    'pgxp_depth_buffer',        # PGXP extras (psxrecomp #583)
    'pgxp_color_correction',
    'pgxp_seam',                # "fine"
)


def unset_required_keys(root):
    with open(os.path.join(root, 'game.toml'), 'rb') as fh:
        video = tomllib.load(fh).get('video', {})
    return [k for k in REQUIRED_VIDEO_KEYS if k not in video]


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('--root', default=os.path.dirname(
        os.path.dirname(os.path.abspath(__file__))))
    ap.add_argument('--ctest', action='store_true',
                    help='exit 77 (CTest skip) instead of 1 when a key is unknown')
    args = ap.parse_args()
    if tomllib is None:
        print('SKIP: needs Python 3.11+ (tomllib)' if args.ctest
              else 'error: needs Python 3.11+ (tomllib)')
        return 77 if args.ctest else 2
    missing = sorted(unknown_video_keys(args.root))
    unset = unset_required_keys(args.root)
    if unset:
        # A game.toml mistake, not a pin lag: fail under --ctest as well.
        print('error: game.toml [video] does not set ' + ', '.join(unset) +
              ', which R4\'s on-by-default display settings rely on')
        return 1
    if not missing:
        print('ok: the pinned psxrecomp reads every game.toml [video] key')
        return 0
    head = 'SKIP' if args.ctest else 'error'
    print(f'{head}: the pinned psxrecomp does not read game.toml [video] '
          + ', '.join(missing) + '. Those keys are inert until R4 pins a '
          'psxrecomp that has them; a release must not be built before then.')
    return 77 if args.ctest else 1


if __name__ == '__main__':
    sys.exit(main())
