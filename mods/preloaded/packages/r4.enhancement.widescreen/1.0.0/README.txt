R4 Custom Renderer (r4.enhancement.widescreen 1.0.0)
======================================================

Mods > Display > R4 Custom Renderer. Off by default.

What it does
------------
Races render wider than 4:3. The extra columns are real rendering, not a
stretch: the game draws the road, scenery and cars that sit beyond the 4:3
edges, and the HUD (minimap, speed, tachometer, lap and time) moves to the
edges of the window.

View
  Fit to Window  Follows the window as you resize it, from 4:3 with no upper
                 aspect limit. The window opens at 16:9.
  16:9, 21:9, 32:9
                 Fixed ratios; the picture keeps that aspect whatever the
                 window shape.

What stays 4:3
--------------
Menus, the garage, car and course select, results, replays after the finish
and movies stay 4:3 with side bars, because their 2D art is drawn for 4:3.
Extra Trial and link-battle races are not widened in this version.

Notes
-----
- Best on OpenGL (the default). On Vulkan and the software renderer the
  rear-view mirror is not shown while a race is wide.
- Netplay always plays stock 4:3; mods are cleared for netplay sessions.
- Changing it needs no rebuild; the widening code is in every build and
  does nothing while this package is disabled.

Credits
-------
Custom-renderer pattern after mstan's TombaRecomp and MegaManX6Recomp.
HUD anchoring classes and the race test are derived from dogewow2048's
analysis of their R4 (JP) 60 FPS / 16:9 patch. R4 port: Shokunin.
