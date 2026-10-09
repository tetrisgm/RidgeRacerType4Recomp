# Modern controls: response curves

The Modern scheme (`r4.modern-controls`, on by default) presents a NeGcon in
races: RT is gas (I), LT is brake (II), the left stick is twist. This page
covers how the triggers and stick are shaped before they reach R4, and the
hidden tuning options.

## Why the curves are needed

R4's own NeGcon handling was measured from a race savestate (headless debug
build, `tools/test_modern_controls_runtime.py`):

- **Gas has a step at half pressure (I = 53 of 106).** From rest the car does
  not move at all below it, and only launches while I >= 53 is *held*: a
  frame below resets the launch, so dithering cannot start the car. Rolling,
  I 0..52 is a graded feather (R4 roughly holds speed near 32), the step jumps
  to about 70% of the available acceleration, and 53..106 rises to full.
- **Brake is graded** from the first unit; it needs only shaping.
- **Twist is linear** past R4's own dead zone (6) up to its range (38).

## Mapping

| Input | Shaping |
|---|---|
| Triggers | 2% inner dead zone, 95% counts as full, ease-in gamma 1.3 |
| Gas from rest | below `launch_speed + request x launch_span` any press holds the step, scaled 53..106 with the request: a light press rolls away and settles at a gentle cruise |
| Gas rolling | request 0..40% -> feather 32..52; 40..70% -> 52 / 53 dithered on R4's 30 Hz frame (16-frame ordered pattern, keyed to a guest VBlank counter); 70..100% -> 53..106 |
| Brake | floor 6 for any press, then the curve up to 106 |
| Stick | 6% radial dead zone (both axes, so vertical drift does not steer), linear with a 20% centre ease, spread over R4's twist range |
| Speed-sensitive lock | P1 only: lock reduced linearly by up to 10% between R4 speed 450 and 900 |

Everything is a pure function of the host pad and guest RAM read at pad time
(P1 speed at `0x800AC0B0 + 0x1D8`, VBlank counter `0x800A6548`). The
transform runs before netplay staging, so peers receive and simulate the same
NeGcon bytes; rollback resimulation never re-runs it.

### Speed-sensitive steering

Three A/B runs each at R4 speed 450 and 650 (medians): a 20% cut from 300 to
800 steadied full-lock yaw at 650 (yaw-rate std 12.7 -> 10.2) but cut drift
hold slip by 30-40%, making drifts harder. The shipped 10% from 450 to 900
keeps drift hold slip at or above no-cut (657 / 700 vs 619 / 626) and still
steadies full lock at 650 (11.5).

## Tuning options

Hidden feature `controls-tuning` in the `r4.modern-controls` package
(`game.toml hide_hidden_mod_features` keeps it out of the launcher). Set
values in `mods/state.toml`:

```toml
[[feature]]
package_id = "r4.modern-controls"
id = "controls-tuning"
enabled = true
[feature.values]
steer_speed_cut = 0
```

| Option | Default | Meaning |
|---|---|---|
| `trigger_deadzone` | 2 | inner dead zone, % |
| `trigger_full` | 95 | travel that counts as full, % |
| `trigger_gamma` | 130 | ease-in exponent x100 (100 = linear) |
| `throttle_low_share` | 40 | request share fed through the feather band, % |
| `throttle_step_share` | 70 | request share where R4's step lands, % |
| `launch_speed` | 20 | launch zone base, R4 speed units |
| `launch_span` | 300 | launch zone growth with the request |
| `feather_min` | 32 | rolling gas floor, NeGcon pressure |
| `brake_min` | 6 | brake floor, NeGcon pressure |
| `stick_deadzone` | 6 | radial dead zone, % |
| `stick_ease` | 20 | centre ease, % quadratic blend (0 = linear) |
| `steer_speed_cut` | 10 | lock removed at high speed, % (0 = off) |
| `steer_speed_low` | 450 | speed where the cut starts |
| `steer_speed_high` | 900 | speed where the cut is complete |

Classic is untouched: no transform is registered.
