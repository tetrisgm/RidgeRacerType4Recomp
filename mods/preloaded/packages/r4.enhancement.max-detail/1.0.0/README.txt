R4 Max Detail (r4.enhancement.max-detail 1.0.0)
=================================================

Mods > Detail > Max Detail. On by default.

What it does
------------
The PlayStation could not draw everything at full detail, so R4 lowers
detail with distance. On a modern machine none of that is needed. Each part
below can be set back to Stock on its own; with the whole package off the
game draws exactly as on a PS1.

Draw distance
  Maximum    Far course polygons the game would drop are kept, so distant
             bridges, buildings and road no longer pop in late, and the road
             and scenery of the two track sections ahead and behind are
             loaded too, filling distant stretches the stock game leaves as
             sky. (Recommended, the default.) From about 30:9 wide the extra
             sections are left out, as Extended: there they made the PS1
             game drop frames with a full grid of cars ahead.
  Extended   Far polygons kept, stock visibility lists.
  Stock      As on a PS1.
  Far polygons are drawn behind everything nearer. The rear-view mirror
  keeps the stock amount of scenery unless Mirror scenery is Full.

Course detail
  Always full keeps full-resolution textures and smooth shading on the whole
  course at every distance and in the rear-view mirror. Stock switches
  distant polygons (and everything in the mirror) to a half-resolution copy
  of their texture with flat shading - the blur in the distance.

Car detail
  Always full draws every car with its full model (3D wheels, full-resolution
  texture) out to the normal car draw distance, and the mirror shows cars as
  far back as you see them ahead. Stock swaps in simpler models a short way
  off. The car draw distance itself is not raised: past it the game's car
  transform overflows and draws distant cars huge and in the wrong place.

Split screen
  Same as 1P gives each half of a VS race the 1P course subdivision and car
  models. Split screen already runs at full speed and at the chosen internal
  resolution; this was the one detail it gave up.

Car reflections
  On (the default) gives the cars their reflective bodies - the glass and
  paint that mirror the sky and scenery - during the race too. Stock R4
  shows them in the fly-by before the start, after the finish, in replays
  and in the attract demo, and turns them off from the start signal to the
  finish line. For now this applies to 4:3 views only: in a widened view
  (R4 Custom Renderer with a margin) the widescreen renderer draws the
  reflective parts many times slower than in 4:3 - at 4K on an Apple M4
  the frame took ten times longer - and the wider view's extra scenery
  plus reflections also leaves the emulated PS1 short of time at the race
  start, so wide views keep Stock. The stock attract demo, which shows
  reflections, is slow in wide views for the same reason.
  The reflections take the emulated PS1 a little longer each frame: on one
  of the four Grand Prix grids measured (seven full car models ahead) the
  game dropped one or two frames in the first seconds. Set Stock if a
  perfectly steady start matters more.

Mirror scenery
  Off (Stock) by default. Full draws all the scenery behind you in the
  rear-view mirror; Stock draws only the nearest few track blocks there, as
  on a PS1. Full makes the emulated PlayStation work harder than anything
  else in this package, and where the PS1 runs short of time the game
  drops below its 30 FPS race rate, so it stays off unless you want it.

Notes
-----
- Game logic is unchanged: lap times, AI and handling are the stock game's.
  The extra drawing does take the emulated PS1 longer. In the races measured
  (Helter Skelter, the two attract-demo courses, 4:3 to 32:9, 1P and 2P) it
  still fit the game's 30 FPS race rate; other courses are untested.
- Netplay always plays stock; mods are cleared for netplay sessions.
- Save states keep the car models they were made with. Loaded with this
  package on and Car detail = Stock, the stock models come back at once.
  Loaded with the package off, a state made with Car detail = Always full
  keeps full car models until the game is restarted.
- Composes with R4 Custom Renderer (widescreen) and Smooth motion: wide
  views also load the wide octants of the extra track sections, and
  in-between frames show the same detail.
- Needs a build whose game.toml lists R4's [[draw_distance.clamp]] sites
  (every build of this version does); without them Draw distance stays
  stock and the rest still applies.

Credits
-------
R4 port and research: Shokunin. Uses psxrecomp's opt-in draw-distance clamp
([[draw_distance.clamp]], docs/config_schema.md).
