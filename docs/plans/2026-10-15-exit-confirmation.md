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

## Option A: script-only, reusing the Quit dialog (recommended first try)

HLExit confirm sends the Quit Game dialog, `("", 75, 1, 1, 4)`, whose text (quit and lose
unsaved progress) also fits Exit to Desktop. A new transition on trigger 0x4b reads the answer:

```
01 0  13 0047  01 0  3f  17 +X   ; answer == "no"?
19  13 0069  43  49              ;   yes: input back on, stay
13 0fff  43  49                  ; "yes": src/menu_hook.h ends the process
```

- Unknown 1: whether trigger 0x4b still reaches HLExit after "yes", or whether the engine
  starts quitting to the shell first and resets the pause machine. Test it: if the player lands
  on the main menu instead of the desktop, use option B.
- Unknown 2: which value of `0x47(0)` means "yes". Read it from the existing handler (0 = the
  input-on path, so presumably "no") and confirm in game.
- Space: the WadChoose slot is 0x38 bytes. A 0x4b transition adds 8, giving 0x3c. Either drop the
  duplicated confirm trigger and the update block (the highlight then does not pulse), or insert
  4 more bytes into the state table with relocation (see "Inserting records").
- No native work beyond the existing hook. Effort: small, about a day including tests.

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
