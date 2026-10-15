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

`SMI_Shell` (SHELL.WAD) has `MainMenu` → `NewGame`, `Load`, `OptionsShell`, `Treasures`. Unlike
the pause menu, its UI object `gouimainmenuag` has **exactly four rows**: model `MDL_b2a8c32e`
with materials `uimenuitemmat0..3`, text objects `SCP_menuItem0..3` (`[msg_A[8001..8004]]`) and
animation clips `menuItem0..3Select` in `ANM_uimainmenuag`. A fifth row needs new geometry, a
material, a text object and an animation clip in formats not decoded yet. That is a large,
open-ended job, so it is not recommended for now.

Cheaper ways to exit from the main menu:

1. **Exit row in Options.** Options from the main menu runs the same `PauseMenu` machine
   (`("PauseMenu", 76, 1, 3)`, `fromShell`), and `PauseOptions` shows 4 of its 8 rows. Adding
   `HLExit` there gives Exit in both main menu → Options and pause → Options. This needs a real
   record insertion in SMI_R_UI. Effort: small to medium.
2. **Back button on the main menu.** `MainMenu` has no back action today. Map trigger 0xeb
   (back) to the confirm dialog and exit. No visible row or hint, so it is hard to discover.

## Inserting records (needed for main menu option 1 and A without trade-offs)

Insert N bytes at the end of the parent's subtree, then:

- Add N to `+0x08`, `+0x0c` and `+0x10` in the header.
- Add N to every ancestor's subtree size (not to siblings that end at the same offset).
- Add N to every parent field and every `47 <state>` operand at or past the insertion point (found
  with the decoder).

Code refs, literal operands and symbol refs are relative and do not change. Put the new code and
literals after the literal pool, before the symbol table, so no code or literal moves. Check
first: whether anything else holds absolute state refs, such as `01 <imm>` pushes equal to a
record ref passed to builtins. Grep both scripts with the decoder before trusting the
relocation.
