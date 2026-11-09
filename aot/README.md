# R4 native overlays

The USA archive has 934 sector-indexed members. The existing `tools/r4_ws_scan.py` and `game.toml` identify code slots 659–672 loaded at `0x801149A8`; slot 664 is the text table. Duplicate slots share exact bytes. The profile pins the verified disc SHA-1, table words, extent bounds and individual image SHA-256 hashes.

| Slot | File offset | Bytes | Classification |
|---|---:|---:|---|
| 659 | `0x261C000` | 8192 | Distinct code image |
| 660 | `0x261E000` | 14336 | Distinct code image |
| 661 | `0x2621800` | 16384 | Distinct code image |
| 662 | `0x2625800` | 77824 | Distinct code image |
| 663 | `0x2638800` | 10240 | Distinct code image |
| 664 | `0x263B000` | 16384 | Text table; not code |
| 665 | `0x263F000` | 20480 | Distinct code image |
| 666 | `0x2644000` | 10240 | Distinct code image |
| 667 | `0x2646800` | 18432 | Distinct code image |
| 668 | `0x264B000` | 79872 | Distinct code image |
| 669 | `0x265E800` | 14336 | Same code image as 660 |
| 670 | `0x2662000` | 8192 | Same code image as 659 |
| 671 | `0x2664000` | 16384 | Same code image as 661 |
| 672 | `0x2668000` | 18432 | Same code image as 667 |

`overlays.json` uses the framework’s fixed-address extent extraction and static AOT pipeline. No captured RAM or playthrough-derived bytes enter generation. Native functions are discovered by the framework’s prologue, leaf, call-target and supplemental entry analysis. The generator audits every emitted code guard against its original image. Static outputs are compiled with the runtime’s PGXP definitions; the game and overlays both use CPS.

Regenerate all required outputs on the Mac with `JOBS=2 bash tools/regen.sh`. For overlay-only generation, use:

```sh
python3 psxrecomp/tools/aot_overlay_pipeline.py static \
  --profile aot/overlays.json --game-toml game.toml \
  --recompiler build-recompiler/psxrecomp-game \
  --work-dir .cache/r4-aot --workers 2 --cps
```

Commit the generated native C and `AOT_STATIC_AUDIT.json` together. Raw images and extraction recipes stay in ignored work directories. A clone builds from the committed output without a disc or compiler at runtime. The packaged game reads the owner’s disc by path.

`tools/check_static_overlays.py` checks output/profile/config identities, image coverage and CPS agreement. `tools/verify_native_overlays.py NEW_DIRECTORY` runs the hidden Mac smoke without any external cache/compiler and rejects interpreted PCs in the R4 overlay range.

The audit deliberately retains `full_static_coverage_proven: false`: discovery plus one smoke route cannot prove every indirect target or modified-code state in every game mode. The Mac smoke establishes native execution and no R4-overlay interpreter PCs for its route; Windows gameplay remains an owner check.
