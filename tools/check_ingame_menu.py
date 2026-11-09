#!/usr/bin/env python3
"""Verify the custom R4 overlay through actual SDL events in an isolated game."""
import json
from pathlib import Path
import time
from dbg import cmd


def check_menu(output, port):
    output = Path(output)
    output.mkdir(exist_ok=True)
    def call(command, **args):
        result = cmd({'cmd': command, **args}, port=port)
        if result.get('ok') is False:
            raise RuntimeError(result)
        return result
    def until(check, seconds=8):
        end = time.monotonic() + seconds
        while time.monotonic() < end:
            value = check()
            if value:
                return value
            time.sleep(.08)
        raise RuntimeError('Timed out waiting for custom menu operation')
    def state(opened):
        return until(lambda: (s if s['open'] == opened else None)
                     if (s := call('menu_host')) else None)
    def click(x, y):
        call('host_mouse', x=x, y=y)
        time.sleep(.25)
    def back():
        call('host_button', name='back')
        time.sleep(.25)
    def shot(name):
        seq = call('present_shot_seq')['seq']
        call('present_shot', path=str((output / name).resolve()))
        result = until(lambda: (s if s['seq'] != seq else None)
                       if (s := call('present_shot_seq')) else None)
        assert result['wrote'], result
    call('turbo', enabled=0)
    call('clear_input')
    call('window_size', w=1280, h=720)
    before = state(0)
    binding = output.parent / 'keybinds.ini'
    binding_check = None
    if binding.exists() and 'cross = K' in binding.read_text():
        # SDL_PushEvent drives the UI but intentionally does not alter SDL's
        # physical key state. Inspect the runtime's reloaded maps instead.
        assert before['p1_cross_scancode'] == 14, before  # SDL_SCANCODE_K
        assert before['p1_cross_button'] == 2, before    # SDL X / west
        binding_check = {k: before[k] for k in ('p1_cross_scancode', 'p1_cross_button')}
    assert before['attached'] and before['provider'] == 'recomp-launcher', before
    back()
    opened = state(1)
    assert opened['paused'] and opened['pause_depth'] == 1, opened
    frame = call('frame')['frame']
    time.sleep(.3)
    assert call('frame')['frame'] == frame, 'Guest advanced behind the menu'
    shot('main.png')
    click(190, 307)  # Options
    shot('graphics.png')
    back()  # Options -> main
    assert state(1)['pause_depth'] == 1
    shot('back-to-main.png')
    back()  # main -> game
    closed = state(0)
    assert closed['pause_depth'] == 0
    until(lambda: call('frame')['frame'] > frame)
    # Repeated Select presses must release correctly, including across close.
    for _ in range(3):
        back(); state(1)
        back(); state(0)
    call('host_button', name='guide'); state(1)
    call('host_button', name='guide'); state(0)
    back(); state(1)
    click(190, 211)  # Save State
    shot('save-slots.png')
    click(650, 235)  # first slot
    state(0)
    saved = until(lambda: (s if s.get('last_ok') == 1 and s.get('last_op') == 'save' else None)
                  if (s := call('savestate_status')) else None, 20)
    back(); state(1)
    click(190, 259)  # Load State
    time.sleep(1.2)  # slot metadata refresh
    shot('load-slots.png')
    click(650, 235)
    state(0)
    loaded = until(lambda: (s if s.get('last_ok') == 1 and s.get('last_op') == 'load' else None)
                   if (s := call('savestate_status')) else None, 20)
    stats = call('dispatch_stats')
    assert stats['miss_total'] == 0 and stats['segment_miss_total'] == 0, stats
    result = dict(before=before, opened=opened, closed=closed, saved=saved,
                  loaded=loaded, dispatch=stats, rebound_cross=binding_check)
    (output / 'menu-check.json').write_text(json.dumps(result, indent=2) + '\n')
    return result
