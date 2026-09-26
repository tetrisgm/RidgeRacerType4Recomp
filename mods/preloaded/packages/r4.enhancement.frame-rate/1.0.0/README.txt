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

  Grand Prix and Time Attack races, the attract demo and the replay after a
  Time Attack are interpolated while the race is running. VS split screen is
  not interpolated yet (it has not been tested) and, like menus, the intro
  fly-by, pause, results and movies, is shown exactly as the original does.
  The HUD (speed, rev meter, timers) still updates 30 times a second, as it
  does on a PlayStation.

  If the computer cannot draw every in-between frame in time, fewer are drawn
  and the gaps are crossfaded; nothing slows the game down. If the renderer
  cannot draw in-between frames at all (for example in a mode it does not
  support them in), the package shows Frame blend instead, using the Blend
  style below, and says so once in the log; it switches back by itself when
  they are available again.

Frame blend
  Crossfades the last two finished frames. It is cheaper, but moving objects
  show a faint double image and the picture is one game frame (about 33 ms)
  behind. "Smooth" crossfades everything; "Sharp" switches fast-changing areas
  at the halfway point instead.

Notes
-----

- Requires the OpenGL renderer (the package selects it). Vsync is turned off
  so the presenter can pace itself.
- A monitor shows at most its own refresh rate. Rates above it cost more
  (more frames drawn) without showing more motion (with vsync off they can
  show as tearing instead); "Display refresh" is the best choice unless you
  know you want something else.
- Each in-between frame redraws the race, so high rates need a fast
  computer; when it falls behind, fewer in-between frames are drawn.
- Online (netplay) sessions always run without mods.

Credits
-------

Package by Shokunin. The list of car and camera values to interpolate and
the draw-only call lists per race mode come from dogewow2048's "60 FPS"
cheat for the Japanese release; this package re-derives them for the US
release.
