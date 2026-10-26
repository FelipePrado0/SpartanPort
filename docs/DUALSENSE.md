# DualSense extras

The game is a PS4 title and only knows the PS4 pad, so what a DualSense can do beyond that is
added by the port (`src/dualsense.h`, `src/runtime_pad.c`).

## Always on

- **Light bar.** `scePadSetLightBar` is forwarded to the pad's LED, and the colour is applied
  again if the pad reconnects. `scePadResetLightBar` returns to the PS4 player 1 blue. Pads
  without an RGB LED ignore it.
- **Touchpad click**, as before (photo mode, pause menu).

## Opt-in: `SPARTAN_DUALSENSE`

A comma-separated list in the environment (`SPARTAN_DUALSENSE=triggers,hitflash`), or `all`.
Everything here needs a DualSense (SDL's PS5 gamepad type) and is off by default.

| Name | What it does |
|---|---|
| `triggers` | R2 gets a click: a wall that gives way near the end of the travel (a DualSense "weapon" trigger effect). The triggers are set free again when the game exits normally. |
| `hitflash` | The light bar flashes red for 180 ms when the player loses health, then returns to the colour the game set. Needs the health hook (`Health display: damage hook ready` in the log). |
| `qte` | While a QTE prompt is up, the light bar blinks in the colour of its button (triangle green, circle red, cross blue, square pink; stick rotations white) and the pad gives a short rumble tap at each new prompt. Mash and hold prompts are not told apart yet. |
| `motion` | The pad's gyro (rad/s) and accelerometer (g) are put in the PS4 pad data. |
| `gyroaim` | Includes `motion`. While L2 is held, the gyro also moves the right stick (turn the pad to turn the view, tilt it to raise or lower it). |

## Status

The effect encodings, the trigger packet layout and the light bar path are covered by
`tests/test_dualsense.c` and `tests/test_pad.c`. The hit flash, the trigger feel and the gyro
signs and gain (`GYRO_STICK_PER_RAD`) have not been tried on a real DualSense yet; the constants
at the top of each block in `runtime_pad.c` are the places to tune them.

## Why SDL_SendGamepadEffect

SDL has no trigger API. `SDL_SendGamepadEffect` passes the 47-byte effects block of the
DualSense output report to SDL's PS5 driver unchanged: right trigger at byte 10, left at byte
21, enable bits `0x04` and `0x08` in byte 0. Rumble and LED fields stay zero with their enable
bits clear, so SDL's own rumble and light bar writes are not disturbed.
