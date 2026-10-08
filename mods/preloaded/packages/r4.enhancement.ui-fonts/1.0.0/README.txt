R4 HD HUD (r4.enhancement.ui-fonts)
===================================

HD HUD by Kuid0us (github.com/Kuid0us/T4HDHUD).

Replaces the race HUD's low-resolution textures with high-resolution
artwork: RANK / RECORD / SECTION / REPLAY / LAPS / TIME LIMIT text, the lap,
time, rank and speed digits, the RPM gauge ticks and labels, the course maps
and the pause-menu labels. It is on by default and needs the OpenGL renderer;
software and Vulkan keep the original HUD.

The pack is a folder of PNGs named TTTTTTTT-PPPPPPPP.png (CRC-32 of the
uploaded texture, CRC-32 of its palette), the Beetle PSX HW / T4HDHUD
naming. It is drawn at the game's internal resolution. The game's own video
memory, timing, saves and netplay are unchanged.

Mods > Visual > HD HUD:
  - Load replacements: off shows the original HUD without disabling the mod.
  - HUD pack folder: the bundled pack (this package's pack/ folder). Change
    folder selects your own folder of PNGs with the same naming (or a
    DuckStation texupload/texpage pack); the selection overrides the bundled
    pack until you clear it.

The upstream PNGs are named for the Japanese game. The bundled pack is keyed
for the US disc (SLUS-00797); tools/r4_hd_hud_pack.py in the source repository
rebuilds it and docs/HD_HUD.md lists what it covers.
