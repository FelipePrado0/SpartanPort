**English** | [Português (Brasil)](CONTROLS.pt-BR.md) | [Español](CONTROLS.es.md)

# Controls

The default keys are in the [README](../README.md#controls). This page explains the options on the
launcher's **Controls** page.

## Keyboard and mouse

**Rage of Sparta on one input** (on by default) leaves Z and X unused. **Joystick prompts** answer
the stick-rotation quick-time events without a stick: type the turn (half turns D S A, A S D, A W D
or D W A; full turns D S A W or A S D W) or, with *Adaptive mouse*, draw it. While you play, the
mouse stays in the game window; the in-game menu, the pause key (Esc) and other windows get it back,
and Esc again or a click in the window takes it back. With L2 held (R), the mouse aims with the
right stick, for the Head of Helios or the Bow of Apollo: *Relative* follows the movement,
*Absolute* points from where the mouse was when L2 was pressed, and *Right stick sensitivity* sets
how much mouse it takes. Without the launcher the game keeps its own keys: E and Q for Square and
Triangle, 1 and 3 for L1 and R1, F for R2, C for R3, I, K, J, L for the d-pad, Enter for Options,
Tab and Backspace for the touchpad.

## Alternative keys

Every button can have a second and a third key or mouse input, in the second and third boxes of its
row on the Controls page (the x button next to a box clears it). All of them press the button, so
you can keep Square on the left click for combat and also press it with J, or with Keypad 4, in the
minigames. On the Muses' lyre, for example, keep Space, Left Shift and the two mouse buttons and add
K, L, J and I, laid out like the four buttons (I up, J left, L right, K down). A key already used by
another button swaps with it, as for the main keys. The in-game keyboard icons show the main key;
*Keys on the icons* (Controls page, with the Keyboard or Match my controller style) can show the
alternative instead, or the alternative on the lyre only (the other icons keep the main key). The
icons draw the first alternative that is set: the second box, or the third when the second is
empty. Without the launcher, join the names with `|` in `SPARTAN_KEY_MAP`, for example
`cross=Space|K|Keypad 2,square=Mouse Left|J`.

## Controllers

On a controller without a touchpad (Xbox and others), View opens the pause menu (the right
touchpad click) and L1 + View opens photo mode (the left click). On a DualSense the touchpad
works as on the PS4, and Share/Create works like View. DualSense details: [DUALSENSE.md](DUALSENSE.md).

In the port's menu: L1/R1 or Q/E switch tabs. In the save list: arrows or the d-pad choose, Enter
or Cross confirm, Esc or Circle cancel.

### Generic USB controllers

In **Controls → Set up a controller**, select the joystick, then choose **Configure** next to any
button, d-pad direction or stick axis. Joysticks without an existing SDL gamepad mapping are listed
too. Each detected input is saved immediately; you can start with any row and correct just one
control later. Existing bindings are shown when reopening the editor. **Clear** removes only that
row, and **Cancel capture** leaves the pending edit unchanged while keeping previous saves. The live
preview lets you check the result at any time. Close the window when finished.

Profiles are saved per SDL device GUID and operating system in `gamecontrollerdb.txt` in the port's
data folder. They load on the next game start, including `--play` and shortcuts. Explicit
`SDL_GAMECONTROLLERCONFIG` and `SDL_GAMECONTROLLERCONFIG_FILE` mappings keep priority. The game's
button remapping still applies; use **Reset to defaults** if you want the buttons to match the
guided setup. SDL must be able to read the joystick; the mapper does not install device drivers.

## Switch attacks

**Switch attacks on L1 + d-pad and L1 + Cross** (off by default, Controls page): L1 + a d-pad
direction switches to that weapon with its switch attack, and the weapon in Kratos' hands does its
attack again; L1 + Cross does the switch attack of the weapon in his hands instead of cycling to
the next weapon. The same weapon's switch attack again waits 0.8 seconds after the last one
(another weapon's does not). The d-pad alone switches as before. The Weapon Switching tutorials say
so in every language (on copies of the language files in the port's data folder). Without the
launcher, set `SPARTAN_SWITCH_ATTACKS=1`. How the game picks Kratos' moves, for further changes:
[moves.md](moves.md).

## Free camera (experimental)

Off by default. Turn it on with *Free camera (experimental)* on the launcher's Controls page, under
*Controller* in the port's menu (Insert) or in the game's Options → Controls. The right stick, or
the mouse while the game holds it, then turns the camera all the way around Kratos and up over
him, and the camera keeps that angle while he moves; the mouse follows *Right stick sensitivity*,
and with L2 held it aims as before. Kratos runs the way the camera looks, and L1 + right stick
dodges.

In play, G (remappable) or a tap of R3 alone switches it off and on: off is the game's own camera,
for boss fights and set pieces. The game takes the camera back by itself for its camera cuts,
quick-time events, the title screen and the pause menu. The camera stops short of walls and rocks
(the level's own collision, not the invisible walls at cliff edges), rises over Kratos when he
stands against a wall and does not go below his feet. Many places were built for the game's camera
only, so other angles can show unfinished backs of the scenery. Without the launcher, set
`SPARTAN_FREE_CAMERA=1`; `SPARTAN_FREE_CAMERA_LIMITS=yaw,pitch` (degrees) limits how far it turns.

## In-game button icons

Off by default. Choose them on the launcher's Controls page, under *In-game button icons*: *Match
my controller*, *Xbox*, *Nintendo* or *Keyboard*. They replace the PlayStation icons in the
tutorial, move-list and quick-time prompts and on the Muses' lyre in the Chamber of the Flame; the
pause-menu controller drawing stays a DualShock 4. *Keyboard* shows the key that presses each
button, on the lyre too, following your keyboard mapping (a changed key shows on the next start);
*Match my controller* shows the keys too when no gamepad is connected.

*Key colors* (same place) paints the key labels of Cross, Circle, Square and Triangle, on the lyre
and on the other keyboard icons: *PlayStation colors*, or *Custom* with one color of a short
palette for each button; the other icons and the default *Gray* are unchanged.

The launcher writes a patched copy of up to three game files (about 410 MB) into the port's data
folder and never changes your game folder. A new controller is picked up after a restart. Without
the launcher, set `SPARTAN_GLYPH_STYLE` to `auto`, `xbox`, `nintendo` or `keyboard` before starting
`run.sh`.
