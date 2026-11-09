#!/usr/bin/env python3
"""Exercise the real R4 in-game menu through SDL events in a hidden runtime."""
import json
from pathlib import Path
import time
import tomllib
from dbg import cmd


def check_menu(output, port):
    output = Path(output)
    output.mkdir(exist_ok=True)
    def call(command, **args):
        result = cmd({'cmd': command, **args}, port=port)
        if result.get('ok') is False:
            raise RuntimeError(result)
        return result
    def key(name):
        call('host_key', name=name)
        time.sleep(.12)
    def state(opened):
        deadline = time.monotonic() + 5
        while True:
            current = call('menu_host')
            if current['open'] == opened:
                return current
            if time.monotonic() > deadline:
                raise RuntimeError(current)
            time.sleep(.05)
    def shot(name):
        seq = call('present_shot_seq')['seq']
        call('present_shot', path=str((output / name).resolve()))
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            result = call('present_shot_seq')
            if result['seq'] != seq:
                assert result['wrote'], result
                return
            time.sleep(.05)
        raise RuntimeError('Menu was not presented')
    call('turbo', enabled=0)
    call('window_size', w=1280, h=720)
    before = state(0)
    assert before['attached'] and before['provider'] == 'R4 shared UI', before
    key('F10')
    opened = state(1)
    assert opened['paused'] == 1 and opened['pause_depth'] == 1, opened
    frame = call('frame')['frame']
    time.sleep(.3)
    assert call('frame')['frame'] == frame, 'Guest advanced while menu was paused'
    shot('menu.png')
    # Audio is the third section. Change the real row, then restore it.
    key('Down'); key('Down'); key('Return')
    key('Left'); shot('audio.png'); key('Right')
    key('Escape'); key('Down'); key('Return')  # CRT Filter
    key('Right')
    selection = output.parent / 'mods/state.toml'
    def crt_enabled():
        data = tomllib.loads(selection.read_text())
        return next(f['enabled'] for f in data['feature']
                    if f['package_id'] == 'psx.enhancement.crt-filter')
    assert crt_enabled() is True, 'Menu did not persist the feature switch'
    shot('restart-setting.png')
    key('Right')
    assert crt_enabled() is False, 'Menu did not restore the feature switch'
    key('Escape')
    for _ in range(5): key('Down')  # PGXP Precision
    key('Return')
    original = selection.read_bytes()
    key('Right')
    assert selection.read_bytes() == original, 'PGXP enable switch was editable'
    shot('pgxp-locked.png')
    key('Escape')
    for _ in range(8): key('Up')
    key('Return'); key('Return')  # Game / Resume
    closed = state(0)
    assert closed['pause_depth'] == 0, closed
    time.sleep(.3)
    assert call('frame')['frame'] > frame, 'Guest did not resume'
    call('host_button', name='guide')
    pad_opened = state(1)
    call('host_button', name='guide')
    pad_closed = state(0)
    assert pad_opened['paused'] and not pad_closed['pause_depth']
    key('F10')
    state(1)
    shot('mouse-before.png')
    call('host_mouse', x=550, y=145)  # Game / Resume row (footer moves when status is visible)
    mouse_closed = state(0)
    assert not mouse_closed['pause_depth']
    result = dict(before=before, opened=opened, closed=closed,
                  controller_opened=pad_opened, controller_closed=pad_closed, mouse_closed=mouse_closed)
    (output / 'menu-check.json').write_text(json.dumps(result, indent=2) + '\n')
    return result
