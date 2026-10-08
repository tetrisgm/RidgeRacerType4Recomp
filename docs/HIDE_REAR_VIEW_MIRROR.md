# Hide Rear-view Mirror

The `r4.enhancement.hide-rear-view-mirror` feature is enabled by default. It
skips Ridge Racer Type 4's dedicated rear-view camera render pass. Turn off
**Hide Rear-view Mirror** in the Mods page to restore the game's original
rendering. The setting uses the normal persisted mod selection.

The hook uses the stock function's own early-return path: it supplies an
argument below the function's `361` threshold and clears the mirror-pass gate.
The function then branches to its ordinary epilogue before the mirror draw.
The gate is the word at `0x8010E860` (`lui v1,0x8011; sw v0,-0x17a0(v1)` at
`0x80070A6C`/`0x80070A74`); the race-setup routine at `0x80070764` clears it
and `0x80070788` reads it. Once the argument passes 360 the stock code latches
it to 1 for the rest of the race.
The rest of the guest draw sequence and ordering
tables continue normally, so the main camera, HUD, and vehicle simulation are
not rewritten. The plugin is presentation-only and follows the framework's
normal mod activation/netplay policy.

Visual verification used the local USA cue with the software renderer at
native 4:3. A fresh profile with no saved selection hides the inset while
leaving the scenery behind it and the HUD intact. A profile with the feature
saved off restores the stock inset; it remains restored after restarting that
same profile. In the enabled race, injected Select/Triangle inputs changed the
camera view, and Start paused and resumed the race with the inset still hidden.
All three boot-to-race smoke runs reported zero dispatch misses. Inputs were
injected through the local debug pad path; no physical controller was tested.

The gate address was re-proven with Ghidra and the instruction words of the
retail EXE (an earlier draft cleared `0x8011E860`, an unrelated word). From a
race savestate: with the feature on, the gate stays 0 and the inset is hidden;
with it saved off, the gate latches to 1 and the stock inset is drawn, across
two restarts of the same profile. `0x8011E860` keeps its guest value in both.
All runs reported zero dispatch misses.
