R4 Frame Rate
=============

Shows R4 at a higher frame rate (the monitor's refresh rate, or 60, 100, 120,
200, 240 or 300 frames per second) without speeding anything up. The game
still runs its race logic, lap timer, AI, input and music at the original
30 Hz; only what reaches the screen changes.

Methods
-------

Interpolated (default)
  Between two game frames the plugin redraws the race with every car and the
  camera placed part of the way from the previous frame to the new one, using
  the game's own drawing code. Each redraw runs in a sandbox: guest time,
  interrupts, sound and CD are frozen, and memory, GPU state and video memory
  are put back exactly as they were, so the game never sees it. Motion is
  really smooth and no latency is added compared with the original.

  Races (Grand Prix, time-limited races with the mirror, VS split screen,
  the attract demo and the after-race replay) are interpolated while the race
  is running. Menus, the intro fly-by, pause, results and movies are shown
  exactly as the original does. The HUD (speed, rev meter, timers) still
  updates 30 times a second, as it does on a PlayStation.

  If the computer cannot draw every in-between frame in time, fewer are drawn
  and the gaps are crossfaded; nothing slows the game down.

Frame blend
  Crossfades the last two finished frames. It is cheaper, but moving objects
  show a faint double image and the picture is one game frame (about 33 ms)
  behind. "Smooth" crossfades everything; "Sharp" switches fast-changing areas
  at the halfway point instead.

Notes
-----

- Requires the OpenGL renderer (the package selects it). Vsync is turned off
  so the presenter can pace itself; on a fixed-refresh monitor choose
  "Display refresh" to avoid tearing.
- 200 FPS and above at 4K or higher internal resolutions need a fast GPU.
- Online (netplay) sessions always run without mods.

Credits
-------

The list of car and camera values to interpolate and the per-mode drawing
sequences were first worked out for the Japanese release by dogewow2048
(the "60 FPS" cheat); this package re-derives them for the US release.
