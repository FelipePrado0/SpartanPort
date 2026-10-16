# Exit to Desktop: confirmation dialog and main menu row (feasibility)

Builds on `docs/plans/2026-10-15-native-menu-feasibility.md` and `scripts/pause_menu.py`.

## What the game does today (CUSA01623 01.02, SMI_R_UI)

Restart and Quit Game confirm through one native dialog. Confirm (trigger 0xe9):

```
16 "SND_MM_SELECT"  13 005c          ; play the select sound
16 ""  01 75  01 1  01 <action>  01 4  13 005e   ; send message 75 to the engine ("" target)
1a  13 0069                          ; builtin 0x69(false): pause input off while the dialog is up
```

- Builtin `0x5e` is a generic "send message" call: `(target, message, args…)`. The shell uses it
  as `("PauseMenu", 76, 1, 3)` to open Options and `("mainShell", 76, 0, 3)` to return to the shell.
  With target `""` and message 75 it opens the engine's yes/no dialog.
- `<action>` across all scripts: 0 = Restart checkpoint, 1 = Quit Game, 2 = Quit (challenge),
  3 = Restart (challenge) and fail-safe, 4 = fail-safe yes. The native side picks the text and
  carries out the action on "yes".
- The reply comes back to the active state as trigger **75 (0x4b)**. Its handler is:
  `01 0  13 0047  01 0  3f  17 +5  19 13 0069` — if `builtin 0x47(0) == 0` (the answer), turn
  input back on with `0x69(true)`. "Yes" has nothing in the script: the engine acts on it.

The opcode length table (5 bytes: `00 01 47 4a`; 3: `02`–`18`; 1: the rest) decodes all 1,012
code blocks of SMI_R_UI and SMI_Shell exactly to their boundaries. The decoder is reliable, so
relocating inserted records is now possible.

## Implemented: the exit rows mark Quit Game's question; FailSafeYes acts on the mark

The dialog is not native. It is the `FailSafeDisplay` machine of the same script (decoded 2026-10-16):

- A message to the empty target (`16 "" 01 75 01 <show> 01 <action> 01 <mode> 13 005e`) reaches the
  states that have a handler for trigger 0x4b (75). `FailSafeDisplay`'s shows the dialog when the
  first parameter is 1 (action in the second) and keeps the action in `failSafeID`; two arrays set
  at its entry give each action's text (`failSafeMsgID`: 0 Restart 4470, 1 and 2 Quit 4471, 3 Easy
  Mode 4472, 4 Easy Mode 4476) and kind (`failSafeExec`: 1 restart, 2 quit, 3 ask again, 4 restart
  after Easy Mode). Yes/No are messages 4003/4004.
- `FailSafeYes`'s confirm block branches on the kind: restart is builtin 0x64, quit is builtin 0x63.
  `FailSafeNoXfer` hides the dialog with `16 "" 01 75 01 0 01 3 13 005e` (first parameter 0).
- Every state with a 0x4b handler sees both messages, including the sender: `HLQuitGame`'s handler is
  "if the first parameter is 0, turn the input back on", so it ignores its own request (parameter 1)
  and reacts to the hide. A first attempt put the exit in the row's own 0x4b handler and ended the
  process the moment the request was sent (the log showed no dialog).

So each Exit row sends Quit Game's request with a third parameter of 5 (nothing reads parameter 2 of
this message; a missing one reads as 0). Builtin 0x5e pops the argument count (the trailing `01 4`),
then up to 3 parameters, the message and the target: the first mark attempt (`ff4cbea`) raised the count
to 5 without pushing a value and crashed the interpreter's value stack (read fault at image 0x21702). `FailSafeDisplay`'s block now also stores that parameter in its local
22 (`failSafeMenu`, unused), and `FailSafeYes`'s confirm block starts with "if local 22 is 5: builtin
0xfff, return", which `src/menu_hook.h` turns into the end of the process. Both blocks are copies
with those instructions added, written over WadChoose's dead code, with the states' items pointing at
the copies (gotos in the copies follow the states that move). The rows' own 0x4b block is Quit Game's.

Not yet checked in game after this change. If the dialog does not show, check that the mark did not
break `FailSafeOn`'s visuals (it keys on the action, which stays 1).

## Option B: a native action code

Extend the engine's dialog with action 0x7e (Exit): its own text, and on "yes" the port exits.

- Find provider 0's handler for function 0x5e and its message-75 path: the vtable `+0x18` call
  in the interpreter's opcode 0x13 handler (image 0x1d5f2b) leads there. Find the switch over
  the action (cases 0–4) and the table mapping action to message text.
- Hook the switch's default case (signature hook like `src/menu_hook.h`) so action 0x7e calls the
  port's exit. Add the text: either point it at an existing message, or add a `*NNNN*` entry to
  each `R_LANG_*.WAD` `MSGS_TXT` (plain text; the WAD is resized, which the test showed is
  accepted).
- Effort: medium (native RE of the dialog path, 2–4 days). It is needed only if A fails, or if
  the Quit text is not acceptable.

## Main menu row

`SMI_Shell` (SHELL.WAD) has `MainMenu` -> `NewGame`, `Load`, `OptionsShell`, `Treasures`. Unlike
the pause menu, its UI object `gouimainmenuag` has **exactly four rows**, so a fifth is a scene
edit, not a script edit. What is known (decoded 2026-10-15, nothing built yet):

- **Row geometry** `MG_b2a8c32e_0` (0x520 bytes): an offset table (`0xb4 0x17c 0x244 0x30c`) of four
  0xc8-byte draw blocks that differ in 7 words each (indices 3/5/7/9, a slot 0x2c9d7d03/5/7/9 and
  two values, 0x3a8/0x1f0/0x178/0x100, whose rule is not decoded), then a vertex stream.
- **Materials** `MAT_uimenuitemmat0..42` all exist in SHELL.WAD; the model entry `MDL_b2a8c32e`
  references `mat0..3`.
- **Text** `SCP_menuItem0..3` (0xa0 bytes each, `[msg_A[8001..8004]]`); a fifth needs message 8005
  in every `R_LANG_*.WAD` (plain text).
- **Animation** `ANM_uimainmenuag` (0x1710 bytes): a clip table with room for 10 entries (offsets
  `0x80 0x3c0 0x700 0xa40`, 0x340 apart, relative to 0x410) and a clip count of 4 at 0x420. Each
  clip is 0x340 bytes: a header, a 4-byte **name hash**, the name (`menuItemNSelect`) and tracks.
  The hash is the same for a name in every animation (`menuItem4Select` = `7f21ec50` wherever it
  appears), so it can be copied from another animation without knowing the function.
- **Scene nodes**: after the clips, node tables (about 0x30 bytes of scale/rotation/translation per
  node) and index tables (0..9). Rows seem to use two nodes each, so a fifth row means inserting
  nodes into several parallel tables. This is the undecoded, risky part.

A fifth row is therefore a multi-structure edit with a visual layout to get right (and room under
the fourth row). Cheaper ways, in order:

1. **Exit row in Options (implemented).** Options opened from the main menu runs the same
   `PauseOptions` state of the `PauseMenu` machine (`fromShell`), so the row added there is one
   step from the main menu and from the pause menu.
2. **Back button on the main menu.** `MainMenu` has no back action. Adding trigger 0xeb to a
   confirm dialog would work, but nothing on screen tells the player.

## Inserting records (implemented)

`scripts/pause_menu.py:add_options_row` inserts a state record into the middle of the table and
appends its code after the literal pool, then fixes: the header (+0x08, +0x0c, +0x10), the sizes of
the new record's ancestors, the `parent` of records after it, every `47` operand to a later state
(found by decoding all code blocks; the opcode length table decodes both scripts exactly), and the
WAD entry size and padding. Code refs, literal operands and symbol refs are relative and do not
move. `tests/test_pause_menu.py` compares every record and every goto before and after: exactly the
expected five records and four gotos differ.

Still unverified in game: that the engine accepts a *resized* R_UI.WAD (the first test only changed
bytes in place).
