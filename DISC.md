# Disc identity — R4: Ridge Racer Type 4 (USA)

Redump-verified clean dump ([redump.info/disc/11608](https://redump.info/disc/11608),
status **Verified**). Format: **bin/cue, single track, MODE2/2352, NTSC-U**.
R4 is genuinely a one-track disc — its music is XA-ADPCM streamed from the data
track, there are no Red Book audio tracks. Do **not** convert to ISO: a
2048-byte "cooked" ISO discards the Mode-2 Form-2 XA sectors the game streams
music and FMV from.

| Field | Value |
|-------|-------|
| Title | R4: Ridge Racer Type 4 (USA) |
| Serial | SLUS-00797 |
| Volume label | `R4USA` |
| EXE date | 1999-01-08 |
| Track | 01, MODE2/2352, data, 264,210 sectors (58:42:60) |
| Size (.bin) | 621,421,920 bytes |
| CRC32 (.bin) | `1FFB6C27` |
| MD5 (.bin) | `BF1D92037B1B2BE83B459C11F0B89EB6` |
| SHA-1 (.bin) | `75F0DAB5C1F71BA80F3D1E2FEFD043F3F2B9EE2E` |
| MD5 (.cue) | `1DA9034808DBB6DE132B69429B051FB4` |
| SHA-1 (.cue) | `7FB365C31E0A1F315479AC9EEF1D6FDE1A84BD1D` |
| TOC fingerprint (`psxrecomp-toc-v1`) | `b76ecb8e9556e320af49a52ef1a2db484e54e361b5d4b624011ca06c552ea5d2` |

Verified 2026-09-25: locally computed CRC32/MD5/SHA-1 of both files match the
Redump entry.

Boot EXE: `SLUS_007.97` — load `0x80010000`, entry `0x8007D8F4`, text
`0x9C000`, SHA-256 `078e3a95185cfa0cc79936dce37cc2c1a90e17aa209b42eef342c970a97f3323`.

The disc image and extracted EXE are local-only (gitignored); recreate from the
source dump if missing. `game.toml [prepare_disc]` carries the same digests so
`psxrecomp_cli.py generate` / `verify-disc` reject a different dump, and
`[netplay] required_disc_fp` keeps a mismatched dump from going online.
