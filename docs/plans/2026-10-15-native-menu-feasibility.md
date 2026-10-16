# Native menu items (Exit, PC Options): feasibility

Goal: add rows to the game's own pause and main menus (not the ImGui overlay).

## Findings (CUSA01623 01.02)

- **WAD container** (`EXEC/WAD/PS4_LE/*.WAD`): a stream of entries, each with a 0x40-byte
  header (`u32 tag, u32 size, char name[56]`, little-endian) followed by `size` bytes, padded
  to 16. Groups are `GroupStart` (tag 2) and `GroupEnd` (tag 3). No external size or offset
  table: neither `eboot.bin` nor `sce_discmap.plt` stores WAD sizes, so a resized WAD is allowed
  in principle.
- **Labels**: `R_LANG_<lang>.WAD` → `MSGS_TXT` (tag 7) is plain text with entries
  `*4012*\r\nQuit Game\r\n`. UI text uses `[MSG:4012]`.
- **All menu logic is one script**: `R_UI.WAD` → `SMI_R_UI` (tag 0x90001, 0x13302 bytes).
  No other WAD has an SMI. Native code only exposes globals to it (`inPauseMenu`,
  `photoMode`, …).
  - Header (LE): `+0x08` total size − 4, `+0x0c` code start (0x1de0), `+0x10` symbol table
    (0x11028).
  - 0x0–0x1de0: state and transition tables (not decoded yet).
  - 0x1de0–0xe739: stack-VM bytecode. Operands are big-endian.
  - 0xe739–0x11028: string literal pool. A literal operand is a BE u16 relative to 0x1de4.
  - Symbol table at 0x1102c: `be16 kind, be16 index, be32 name_offset` (names relative to
    0x1102c). Full debug names are included. Kinds: 0 = state, 2 = global, 3 = local string,
    35/7 = machine, −1 = event.
  - Opcodes seen: `01 <be32>` push int, `00 <bef32>` push float, `16 <be16>` push literal,
    `09 e0NN` local NN of the current machine, `11 XXXX` array variable, `43` store/apply,
    `13 XXXX` builtin call, `17 <be16>` conditional jump, `18 <be16>` jump.
- **Pause menu** (`PauseMenu` machine, 35 locals): `header`, `line0..7`, `option0..7`,
  `optSet0..7`. The states are `PauseMain → HLContinue, HLOptions, HLPhotoMode, HLRestart,
  HLQuitGame`, plus `PauseOptions → HLControls, HLGSettings, HLAudio, HLDisplay`. Row setup
  at SMI+0xb9dc: `16 <lit> 09 e0NN 43` sets `header=[MSG:4005]` and `line0..4` to
  4010/4006/4015/4011/4012. It explicitly sets **`line5..7` to `""`**. `R_UI.WAD` already
  has `SCP_menuItem5..7` bound to `[*PauseMenu:line5..7]`, so three spare rows exist with
  geometry.
- **Mods system**: `scripts/mods.py` only overlays `dvdroot_ps4/`. This dump has `EXEC/` at the
  root (the game opens `/app0/EXEC/...`), so loose-file mods cannot replace these files today.

## In-game test: passed (2026-10-15)

Both edits appeared in the pause menu: the top row was relabeled and a 6th row was drawn. The
game runs an edited SMI with no validation, and the spare rows render.

Overlay `E:\Games\God of War III Remastered\mod-game-asxrbwkq` changes two literal operands
in place:

- `line0` → `[MSG:4011]`: the top row reads "Restart from last checkpoint" and selecting it
  still continues.
- `line5` → `[MSG:4023]`: a 6th row reads "Restart Challenge". The cursor is not expected to
  reach it.

| Top row | 6th row | Meaning |
|---|---|---|
| relabeled | shows | Edited script runs and spare rows render: GO |
| relabeled | missing | Edits work; rows 5–7 need visibility/count logic elsewhere |
| unchanged | missing | The edited WAD is not being used |
| crash/hang | — | Something validates WAD/SMI |

## Implemented: Exit to Desktop (tested in-game 2026-10-15: row shows, highlights, exits cleanly)

- `scripts/pause_menu.py`: `WadChoose` is a cheat-only level loader whose record directly follows
  `PauseMain`'s subtree. Its record becomes `PauseMain`'s sixth child, `HLExit`, with the same
  size. Its code goes over `WadChoose`'s dead code: highlight `menuItem5Select`, up →
  `HLQuitGame`, down → `HLContinue`, confirm → select sound + builtin `13 0fff`. `PauseMain`
  grows by 0x38; `HLQuitGame`↓ and `HLContinue`↑ are retargeted; `line5` = "Exit to Desktop".
  No byte moves.
- `run.py` passes the patched copy (made from the glyphs' copy when there is one) as
  `--pause-menu`; the probe mounts it only once the hook is installed.
- `src/menu_hook.h`: in the interpreter's opcode 0x13 handler (image 0x1d5f0f, function id in
  esi), function 0xfff ends the process.
- Interpreter: 76-way switch at image 0x1d5a4e. Native script variables: a name/id table at image
  0x3d0970 (e.g. `cheat` 0xcc, `challengeMode` 0xf1, `inPauseMenu` 0x112, `photoMode` 0x116).

## Remaining work (earlier plan, for reference)

1. Decode the state table (0x0–0x1de0) to add an `HLExit` state, with transitions from
   `HLQuitGame` and wrapping back to `HLContinue`.
2. Add code without relocating anything: append it after the literal pool, update the
   symtab offset in the header, and reach it through a jump. Inserting code before the pool
   would shift every `16` operand.
3. Native action: SMI cannot exit the process. Add a hook (signature-based, like
   `src/cheat_hook.h`) on the global or builtin the new state sets, then exit from the port.
4. PC Options: reuse the `PauseOptions` row and `optSet` machinery, then have the port read
   and write those SMI variables in guest memory.

## Options row (tested in game 2026-10-15: shows in pause and main menu Options, works)

`PauseOptions` (Controls, Game Settings, Audio, Display) shows four of the eight row slots and is
also what the main menu's Options opens (`fromShell`). `add_options_row` adds `HLExit` as its fifth
child (down from Display, up from Controls, wrapping both ways). It needs a record inserted into the
state table, so the WAD grows by 144 bytes; see `docs/plans/2026-10-15-exit-confirmation.md`.

First attempt (commit bd14fda) appended the row's code and label after the literal pool and the row
did not show in game; the second (74677a0) keeps both rows' code and the label in WadChoose's dead
code, so only the state table grows (one record, +48 bytes), and the row shows. The cause was never
isolated: either the label or the code after the pool is not usable by the engine.
