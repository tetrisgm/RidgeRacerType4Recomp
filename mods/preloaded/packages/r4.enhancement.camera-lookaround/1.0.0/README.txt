R4 Camera Look-Around

Enable Camera Look-Around in the mod catalog to look around during a
single-player race with the right stick. It is off by default.

Controls
  Right stick left/right: turn the view toward either side.
  Right stick down: look 180 degrees behind the car; diagonals aim between
    the side and rear. Down changes yaw only, not pitch.
  Right stick up: tilt the view upward by up to 12 degrees.
  Center/release: smoothly return to the game's normal camera.

The right stick is read from the local controller even when the game uses a
digital pad, and never from netplay peers. The default deadzone is
18%; the mod options also offer 12%, 25% and 32%. Sensitivity can be set to
70%, 100% or 130%. Movement follows simulated VBlank time, so additional
presentation redraws do not accelerate the camera.

The original camera switch remains game-controlled. Cockpit movement turns
the view in place while keeping the driver's camera anchor fixed. Holding down
turns the cockpit view toward the road behind. Chase movement keeps the camera's
distance and height while orbiting around the car, keeping the car framed;
holding down moves the view to the car's forward side so the car front and road
behind it are visible. Small horizontal drift around straight down is snapped
to the rear view, and yaw follows the shortest continuous path through the
180-degree wrap. The short guest camera-input patch is restored immediately
after the main camera matrix is built so the game's rear-view mirror keeps its
stock view.

The effect applies to the two single-player race handlers only. It clears when
the game leaves those race views and stays inactive in menus, attract/replay/
cutscene camera paths, split-screen and netplay. No steering input is written.

Package by Shokunin. The guest camera-builder addresses are for the USA
SLUS-00797 boot executable listed in DISC.md.
