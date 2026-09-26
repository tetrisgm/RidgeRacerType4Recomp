# Framework pin history

The `psxrecomp` and `recomp-ui` submodule gitlinks are the pins. Bump them
deliberately and record each bump here with the reason and what was verified.

| Date | psxrecomp | recomp-ui | recomp-net (nested) | rbengine (nested) | Why / verified |
|---|---|---|---|---|---|
| 2026-09-25 | `19b5a65f` (master) | `b688ca7` (master) | `c2338c6` (psxrecomp's pin) | `a7b9850` (psxrecomp's pin) | Initial bring-up on latest masters (not MMX6's pins). Verified on macOS arm64: OpenBIOS HLE-boot and full LLE boot to the intro movie, menus, Grand Prix race, audio (SPU + XA), 0 dispatch misses, native overlay shards, rollback self-check 7/7 PASS, two-instance netplay (delay-sync and rollback) VS race with 0 digest mismatches. |
| 2026-09-26 | `4d7311c8` (master) | `b688ca7` | `c2338c6` | `a7b9850` | Setup-host distribution fixes landed upstream: RetroPortingToolKit/psxrecomp#393 (macOS executable path), #394 (macOS toolchain pack + Command Line Tools check, selfcheck `toolchain_ready`), #395 (Windows rebuild helper in folders with parentheses). Tree identical to the `integration/distribution` build the v0.1.0 zips were tested with (macOS arm64 and Windows x64 player flow end to end). Host-only changes; no regen. |

Nested submodules stay at whatever `psxrecomp` pins; do not bump recomp-net on
its own.

The MMX6 adaptive renderer patch (`renderer/adaptive/`) is verified against the
`psxrecomp` pin recorded in `renderer/adaptive/BASE`; re-verify it on every bump.
