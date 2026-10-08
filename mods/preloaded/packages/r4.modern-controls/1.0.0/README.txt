R4 Controls

On by default with the Modern scheme. Set the scheme to Classic, or turn the
feature off, to get R4's stock controls exactly: nothing is registered and the
input path is the stock one.

Modern, in races on a gamepad with both analog trigger axes:

  Right trigger      -> gas (analog)
  Left trigger       -> brake (analog)
  Left stick X       -> steering (analog)
  Square             -> downshift
  Circle             -> upshift
  R1                 -> camera view
  Y / Triangle       -> Rewind (enable Rewind in Settings)

The pad is presented to R4 as its native NeGcon while you drive: the stick is
the twist, RT is analog I and LT analog II, scaled so the whole trigger travel
covers R4's NeGcon pressure range on every poll. Square, Circle and R1 press
the buttons R4's own NeGcon settings use for shift down, shift up and camera
view, so a remap in R4's options is followed. No car physics change. Cross,
L1, L2, R2 and the D-pad do nothing in a race. Menus, the pause menu,
keyboards, pads without both triggers and steering wheels keep the stock
controls. The stick steers in either pad mode (digital or analog).

Y is R4's default Rewind button. In Modern it acts alone (and never reaches the
game); in Classic it means Select + Y. A Rewind button saved in Settings wins.
Netplay always runs with mods cleared, so it uses the stock controls.

Rewind is off in split screen and online. Online, psxrecomp refuses it; in
2-player VS Battle a hidden rule in this package (on by default, applied with
any scheme or with Controls off) blocks it: Rewind does not open, keeps no
history, and Y reaches the game as if Rewind were off.
