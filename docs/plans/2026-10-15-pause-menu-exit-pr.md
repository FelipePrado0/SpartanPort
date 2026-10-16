## Feat(Menu): Exit to Desktop in the pause menu and the Options screen

Adds **Exit to Desktop** rows to the game's own menus: a sixth row in the pause menu and a fifth row
in the Options screen, which the main menu's Options opens too. Selecting it closes the game to the
desktop. They are the game's own menu rows, not the ImGui overlay: same font, highlight animation,
select sound and controller navigation as the other rows.

### How it works

- All menus are one compiled state-machine script (`SMI_R_UI` in `R_UI.WAD`). `scripts/pause_menu.py`
  turns its cheat-only `WadChoose` state, which sits right after `PauseMain`'s subtree, into the sixth
  pause row in place. `PauseMain` already draws `line5` as an empty row; the patch fills it.
- The Options row is a new state record inserted after `HLDisplay`, so the state table grows by one
  record and the script and the WAD entry grow by 56 bytes. The patcher corrects the header, the
  sizes of the record's ancestors, the parent of later records and every goto to a later state (all
  code blocks are decoded; the opcode length table reads both scripts exactly). Both rows' code and
  label go over `WadChoose`'s dead code.
- Confirming a row calls a builtin id (0xfff) that no provider has. `src/menu_hook.h` hooks the script
  interpreter's builtin call (found by signature, unique in 01.00 and 01.02) and turns that id into
  the end of the process, as the in-game restart does.
- `run.py` writes the patched `R_UI.WAD` into `out/` (on top of the controller-glyph copy when glyphs
  are on) and passes it as `--pause-menu`. The probe mounts it only once the hook is installed, so a
  build where the hook fails never shows a row that does nothing. The game folder is never written.
- The patcher refuses any script it does not recognise (and a copy already patched), and the game
  runs unmodified.

### Testing

- `tests/test_menu_hook.cpp`: runs the stub: other functions replay the replaced instructions,
  0xfff reaches quit.
- `tests/test_pause_menu.py`: refusals, and the patch against the real `R_UI.WAD` when
  `GOW3_GAME_DIR` is set (skipped otherwise): exactly five records and four gotos differ from the
  original.
- In game (CUSA01623 01.02, Windows): the pause row shows, highlights, is reachable down from Quit
  Game and up from Continue, and exits cleanly (`Pause menu: Exit to Desktop` in the log). The Options
  row shows under Display in pause → Options and main menu → Options and works.

### Limitations

- The label is English in every game language.
- It exits immediately, without Quit Game's confirmation dialog. A plan for confirming with the
  engine's own dialog is in `docs/plans/2026-10-15-exit-confirmation.md`.
- Not a row of the main menu itself: it has exactly four rows built into its scene. The plan records
  what a fifth would need (row geometry, a clip and scene nodes).

### To be published as a mod

This is a data change to `R_UI.WAD` plus one small hook. It is waiting for the loose-file mods fix
(commit `7a69cab` on branch `claude/hungry-noyce-5a540a`, "mods laid out like the game folder
(EXEC/...) now load"; no PR is open for it yet). Once that is merged, the WAD half is meant to be
published as a mod that is turned on by default, instead of patching at run time. The hook would stay
in the loader.

Format notes: `docs/plans/2026-10-15-native-menu-feasibility.md`.

🤖 Generated with [Claude Code](https://claude.com/claude-code)
