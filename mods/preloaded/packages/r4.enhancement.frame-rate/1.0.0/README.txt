R4 Frame Rate
=============

Adds frames between the game's own in races, as many as your computer has
time for, without ever slowing the game down. The game still runs its race
logic, lap timer, AI, input and music at the original 30 Hz; only what
reaches the screen changes.

Off by default: tick it under Mods -> Frame Rate (Display refresh,
Interpolated unless you choose otherwise); your choice is saved.

How it never slows the game
---------------------------

The game's own frames always come first. An in-between frame is drawn only
in the time left over once the game's frame is done and before it is due
on screen. One that would not finish in time is not started, and one that
runs late is stopped, so the game's frame is never shown later than without
the package. Wherever no in-between frame fits, the game's own frame is
shown as is: nothing blended, nothing delayed. Nothing is drawn just to
find out how long it takes: the cost of an in-between frame is learnt from
the ones that fit.

So how many you get depends on the computer and the settings, not the
other way round. On an Apple M4 a race at Native or 720p gets one or two
in-between frames per game frame; at Match display on a Retina or 4K screen
there is usually no time left, and the game shows its own 30 FPS frames, at
full speed. Lower Settings -> Display -> Internal resolution for more
in-between frames, or leave it high for the sharpest picture: resolution and
full speed come first.

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

Frame blend (only if you choose it)
  Crossfades the last two finished frames at every frame. It costs little,
  but moving objects show a faint double image and the picture is one game
  frame (about 33 ms) behind. "Smooth" crossfades everything; "Sharp"
  switches fast-changing areas at the halfway point instead. Interpolated
  never falls back to it.

Presentation rate
-----------------

Display refresh (default)  follows the monitor: the most even motion.
60 ... 300 FPS             fixed rates, for high-refresh or variable-refresh
                           monitors.
Unlimited                  as many in-between frames as fit, each shown at
                           its own time, even beyond the monitor's refresh
                           rate (vsync off). With Frame blend it follows the
                           display refresh.

Every rate is a ceiling, not a target: only frames that fit the time the
game leaves free are drawn, and a picture that has not changed is not sent
to the screen again.

Notes
-----

- Requires the OpenGL renderer (the package selects it). Vsync is turned off
  so the presenter can pace itself.
- A monitor shows at most its own refresh rate. Above it, under a desktop
  compositor (macOS, or a window or borderless fullscreen on Windows) you
  see the newest frame at each refresh; without one (exclusive fullscreen on
  Windows, some Linux setups) frames can tear. "Display refresh" is the best
  choice unless you have a variable-refresh monitor or want to measure.
- Online (netplay) sessions always run without mods.

Credits
-------

Package by Shokunin. The list of car and camera values to interpolate and
the draw-only call lists per race mode come from dogewow2048's "60 FPS"
cheat for the Japanese release; this package re-derives them for the US
release.
