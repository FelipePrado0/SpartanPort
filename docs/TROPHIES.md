# Trophies

**English** | [Português](TROPHIES.pt-BR.md) | [Español](TROPHIES.es.md)

The game calls the PlayStation trophy library (`sceNpTrophy*`). This port answers those calls, keeps
the unlocked trophies on disk and shows them to the player: a notification when a trophy is
unlocked, a trophy window inside the game and a **Trophies** page in the launcher.

Everything is read from the player's own game folder (`sce_sys/trophy/trophy00.trp`); no game data
is part of the port.

**Contents:** [What the player sees](#what-the-player-sees) · [Order and rarity](#order-and-rarity) ·
[Notifications](#notifications) · [Language](#language) · [Saved state](#saved-state) ·
[Trophy ids](#trophy-ids) · [What is in this repository](#what-is-in-this-repository-and-what-is-not) ·
[How it works](#how-it-works) · [Limits](#limits)

## What the player sees

| Where | What |
|-------|------|
| In the game, when a trophy unlocks | A notification in the theme chosen in the trophy window (below). The default, **Original**, is a card at the top-right (below the FPS counter when it is on) with the trophy's icon, name and description; it slides in, stays 5 seconds and fades. |
| In the game, **F8** or **R3+R2** | The trophy window, in two pages. **Trophies**: every trophy with its icon, a cup in its grade's colour, its name, what to do to unlock it, its state and how many players earned it; an unlocked trophy is framed in its grade's colour (bronze, silver, gold, and a platinum that shines). Above the list: the count, a filter by state (all / unlocked / locked), one by grade (every grade / platinum / gold / silver / bronze, with how many of each are unlocked; the two combine) and the order. **Notifications**: the language, and the look and sound of the notification, with previews. |
| Launcher, **Trophies** page | The same list, with the category (platinum, gold, silver, bronze) and a filter. |

Keys of the **Trophies** page: Up/Down, W/S, PageUp/PageDown, Home/End, mouse wheel or D-pad scroll;
Left/Right, A/D or D-pad Left/Right change the state filter; Square or G the grade filter; Triangle or
Tab changes the order; L1/R1 or Q/E go to the **Notifications** page; F8, Esc, Backspace, Enter,
Circle/Cross or R3+R2 close it.
The **Notifications** page is used like the Insert menu (D-pad or arrows, Cross or Enter, the
mouse); L1/R1 or Q/E go back to the list; F8, Esc or R3+R2 close it. While the window is open the
game gets no input (like the Insert menu), and the game stops until it closes, as it does with
the Insert menu: Game tab > **Pause the game** (on by default) turns that off. R3 held with R2 or
L2 never reaches the game, so opening the window or the menu does not use the item or the magic.

The platinum (id 0) is unlocked with the last other trophy, as the PlayStation does.

## Order and rarity

Each trophy shows the share of players who earned it and its tier, as a PlayStation shows them:
**Common** (over 50%), **Rare** (over 15%), **Very rare** (over 5%) and **Ultra rare**. The order
is the game's list, **Most earned first** or **Rarest first**.

The port has no PSN connection, so the numbers are fixed: the share of the players tracked by
[Exophase](https://www.exophase.com/game/god-of-war-iii-remastered-ps4/trophies/) who earned each
trophy, as listed in October 2026 (`gpu/shim/spartanport_trophy_rarity.h`). They are only shown for
this game's list of 35 trophies; another package keeps the list's order and shows no percentage.

## Notifications

The **Notifications** page of the trophy window (F8 or R3+R2, then L1/R1) chooses how an unlock is
shown and heard. Nothing changes for a player who does not open it: the **Original** notification
stays the default, as it was, and no sound plays without a sound file.

| Option | What it does |
|--------|--------------|
| Theme | **Original** (the port's card, above), **Card** (a dark card at the top-right with the trophy's art, the grade's cup and an animated badge; the platinum gets a short drawn animation), **Card, cinematic platinum** (the platinum stays longer, with light around it) or **Corner popup** (a rectangle that rises in the bottom-right corner with the art framed in the grade's colour, the name and what the trophy asks). |
| Mute the sound, Volume | The notification's sound only; the game's own sound is not touched. |
| Time on screen | 0 to 20 seconds; "Theme default" keeps the theme's own time. |
| Platinum light | Card, cinematic platinum only: how bright its light is. |
| Animate the platinum | Off: the platinum shows its own picture, like the other trophies. |
| Write the grade | Writes the grade's name (Bronze, Silver...) before the second line. |
| Preview | Bronze, Silver, Gold, Platinum, all four in a row or three at once, through the same queue, drawing and sound as a real unlock. Nothing is unlocked or saved. |

The grade of each trophy is told by the colour of its icon, as the launcher does (the grades in the
package are encrypted); for this game's 35 trophies it matches the package's own list. The page is
in the menu's language (below).

### Your own files

Put them in `user/custom_trophy` (inside your saves folder, so updates keep them). None is part of
the port: without them the drawn art, the built-in fonts and no sound are used.

| File | Effect |
|------|--------|
| `bronze`, `silver`, `gold`, `platinum` `.wav .mp3 .ogg .flac .m4a .opus` | The sound of that grade (at most 20 seconds). `trophy.*` is used for every grade without its own. The audio device is only opened when there is a sound file, and never with `SPARTAN_AUDIO=none`. **Reload sounds** reads them again while playing. |
| `<grade>_anim.gif`, `.apng`, `.png`, `.webp`, `.jpg` | Replaces the art of that grade's notification (Card and Corner popup). A GIF or an animated PNG plays once with its own timing; a still picture is shown as it is. `trophy_anim.*` covers every grade without its own. |
| `<grade>_anim.png` + `<grade>_anim.ini` | A sheet of frames: `frames=`, `columns=`, `fps=`, `loop=1` to repeat, `delay_ms=` to start later. |
| `badge.png` | The disc on the right of the Card. |
| `font.ttf`, `font-light.ttf` | The Card's fonts (the name, the second line). |
| `notifications.ini` | The options above, written by the page. It also holds the texts, in any language: `text_unlocked=` (the Card's second line), `text_banner=` (the first line of the Original card), `name_bronze=`, `name_silver=`, `name_gold=`, `name_platinum=`. |

Pictures and animations are read when the game starts. Larger than 8192 pixels on a side, longer
than 20 seconds or 600 frames, or more than 64 MB of decoded frames are refused or cut. Files are
opened as local files only: a "sound" that is really a playlist or a link is not followed. A broken
or missing file is skipped and written to the log; the game never stops for it.

## Language

The launcher writes `user/trophy_texts.txt` every time it starts the game, in the launcher's
language, and the same in every language in `user/trophy_texts/<language>.txt`. The game reads the
first at start. **Language**, at the top of the Insert menu's **Game** tab and of the trophy
window's **Notifications** page, switches the menu, the trophy window and the trophy names at once,
reading the second; the choice is kept in `spartanport.ini` (`ui_language`), and **Same as the
launcher** follows the launcher again.

The menu and the trophy window are drawn in English, Portuguese, Spanish, French, German, Italian,
Polish, Turkish, Indonesian, Russian and Chinese (Chinese with an installed CJK font, see the
README). Arabic, Japanese and Korean keep them in English, because the overlay's font does not draw
those scripts; the trophy names still come in those languages in the launcher.

Languages: English, Russian, Arabic, Spanish, Portuguese, French, German, Italian, Polish, Turkish,
Chinese, Japanese and Korean (`launcher-rust/assets/i18n.json`, made from
`launcher/spartanport_trophies.py` by `tools/launcher_i18n.py`). The names and descriptions are
the game's own, as the PS4 trophy list shows them in each language, so a trophy can be looked up by
the name the game gives it: English, Portuguese (Brazil), Spanish (the Latin American list), French,
German, Italian, Polish, Turkish, Russian, Japanese and Korean. **Arabic and Simplified Chinese are
translations**: the game has no list in those languages. A language without a list uses English.

Without the launcher, put the same file by hand in the saves folder (`user/` or `SPARTAN_USER_DIR`):
`trophy_texts.txt`, or `trophy_names.txt` (same format, used when there is no `trophy_texts.txt`).

### File format

```
# comment
@heading=Trophies
@notice=TROPHY UNLOCKED
@unlocked=UNLOCKED
@locked=LOCKED
@all=All trophies
@only_unlocked=Unlocked
@only_locked=Locked
@counter={} of {} unlocked
@hint=Up/Down: scroll   Left/Right: filter   F8 / R3+R2 / Esc: close
@notifications=Notifications
@order_list=List order
@order_common=Most earned first
@order_rare=Rarest first
@rarity={}% of players
@common=Common
@rare=Rare
@very_rare=Very rare
@ultra_rare=Ultra rare
@hint_order=Triangle/Tab: order   L1/R1: notifications
@hint_notifications=L1/R1: trophies   F8 / R3+R2 / Esc: close
0=King of the Hill | Unlock all trophies
1=Releasing the Floodgates | Kill Poseidon
```

`ID=Name | Description` (the description is optional), UTF-8, one trophy per line. A name takes up to
63 bytes and a description up to 159. A trophy without a line is shown as `Trophy ID N`. Missing
`@` lines use the English labels.

## Saved state

`user/trophies.dat`: 16 bytes, four little-endian 32-bit words. Trophy `id` is bit `id % 32` of
word `id / 32`. Delete the file to reset the trophies. It is separate from the game's own saves.

## Trophy ids

The ids are the numbers in the icon names of the package (`TROP000.PNG` ... `TROP034.PNG`): 35
trophies, id 0 the platinum, 1-18 bronze, 19-29 silver, 30-34 gold (by the colour of the icons).
The texts are listed in the order of the PlayStation list, which matches this order.

## What is in this repository, and what is not

Players often ask what can be shared. The trophy features are built so that nothing in this
repository belongs to Sony or to the game's makers:

- **No game files.** The icons, the trophy package and the game are read from the player's own
  copy at run time; none of them is copied into the port.
- **No PlayStation logos, sounds or pictures.** The notification's art (cups, badge, the platinum's
  animation) is drawn with shapes in code. The look of the Card is the port's own; it only follows
  the usual layout of a notification (art on the left, two lines of text, a badge).
- **The texts** are the names and descriptions of the game's public trophy list, the short lines
  every trophy site shows and that the port already carried in English and Portuguese. They are what
  lets a player find a trophy by its name; nothing else of the game's text is included.
- **The rarity numbers** are statistics (a share of players), with their source named above. A
  number about how many people did something is a fact, not a work.
- **Your own files** in `user/custom_trophy` (a sound you like, a picture, a logo) stay on your
  computer. They are not part of the port, and you should not upload them to this repository or
  share them if they belong to someone else.

This is not legal advice; it is how the port keeps its own repository clean.

## How it works

- `src/runtime_services.c`: the `sceNpTrophy*` functions. On `RegisterContext` it reads
  `trophy00.trp` (header big endian: magic `DC A2 4D 00`, file count at `0x10`, entry size at
  `0x14`, entries from `0x60`: name, offset, size) and the texts file. `UnlockTrophy` sets the bit,
  saves the state and shows the notification; the platinum is granted when every other trophy is set.
  `GetTrophyUnlockState` returns the saved bits and the trophy count.
- `gpu/spartanportgpu.h`: `spartanportgpu_toast_trophy` (the notification, with the trophy's id) and
  `spartanportgpu_trophy_setup` (the window: texts, icons and a pointer to the live state), handed
  over once by the runtime.
- `gpu/shim/spartanport_overlay.cpp`: draws the Original notification and the trophy window with Dear
  ImGui, decodes the icons with `SpartanPng` (on a worker thread), tells each trophy's grade by its
  icon and handles the keys and buttons of the window.
- `gpu/shim/spartanport_trophy_toast.{h,cpp}`: the other themes: the queue, the drawing, and a worker
  thread for the sounds (an SDL audio stream of its own beside the game's).
- `gpu/shim/spartanport_trophy_toast_model.{h,cpp}`: `notifications.ini`, the animation settings,
  the timeline and the grade by colour. No window, GPU or audio, so it is unit-tested.
- `gpu/shim/spartanport_trophy_media.{h,cpp}`: the player's sounds and pictures decoded with FFmpeg,
  with the limits above.
- `gpu/shim/spartanport_trophy_rarity.h`: the share of players for each trophy.
- `gpu/shim/spartanport_trophy_texts.{h,cpp}`: reads `user/trophy_texts/<language>.txt` for the language
  chosen in the game.
- `gpu/shim/spartanport_ui_language.h`, `gpu/shim/ui_language/`: the menu's and the trophy window's
  texts in every language, keyed by the English text.
- `gpu/tools/overlay_preview.cpp` (`overlay-preview`): draws the menu, the trophy window and the
  notifications offscreen with made-up trophies, and the game's save list with made-up saves,
  without the game, to work on their look.
- `launcher-rust/assets/i18n.json`: the texts and labels in every language, made from
  `launcher/spartanport_trophies.py` and `launcher/spartanport_lang.py` by `tools/launcher_i18n.py`.
- `launcher-rust/src/core/trophies.rs`: the reader of the `.trp` and of `trophies.dat`, and the share
  of players (the same table as the game's).
- `launcher-rust/src/core/env.rs`: `write_trophy_texts`, the files the game reads.
- `launcher-rust/src/ui/pages/trophies.rs`: the launcher's Trophies page, with the same order and
  rarity as the game's window.
- `tests/test_trophies.py`, `tests/test_trophy_toast.cpp`, `tests/test_ui_language.cpp`,
  `tests/test_services.c`: unit tests. They build their own files and need no game files;
  `trophy-toast-test SCRATCH_DIR trophy00.trp` also checks the grades of the game's own icons.

## Limits

- Only trophies unlocked through this port are known. Trophies earned on a PlayStation are not
  imported, and with an already advanced save the game usually does not unlock again what it did before.
- The names and descriptions of the package are encrypted (`TROP*.ESFM`) and are not read: the
  texts above are built into the launcher.
- Trophy synchronisation with PSN is not implemented, and there is no online profile. The rarity is
  a fixed snapshot and does not change while you play.
- R3+R2 can be pressed by accident in combat; the Insert menu uses R3+L2 for the same reason.
