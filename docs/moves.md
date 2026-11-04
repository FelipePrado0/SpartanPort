# Kratos' moves: how the game picks the next one

Notes from the switch attacks (`src/switch_hook.h`, `src/runtime_pad.c`, `scripts/switch_text.py`),
for anyone changing what a button does in combat. Addresses are image offsets in the v01.02
eboot (CUSA01715); the hooks find their sites by signature. Data offsets are confirmed in game
unless marked *(unverified)*.

## How this was found

Two blind attempts (an input macro, then hooks built on guessed branch rules) failed in game. What
worked was a **record-only build**: hooks that copy what the game does into a ring buffer, the pad
thread writing it to `last_run.log` next to the buttons as they change, and one short session of
the moves in question. That build hooked seven places (the weapon check, the branch test's exit,
the input queue, the d-pad select, weapon applied and `SCR_EnableWeapon`) with stubs in the style
of `tools/switch_stubs.s`, in about 200 lines; built again the same way, it is the quickest way to
answer the next question about the move system. Pitfalls: log only
changes (the weapon select and apply functions run every frame), keep the line cap high, and note
that `0xe3910` queues its winner inline, not through `0xec660`.

## Player stats

The object the weapon functions take in `rdi` (also reachable from the hero at `[+0x20] -> +0x238`):

| Offset | Meaning |
| --- | --- |
| `+0x168` u64 | flags: bit 4 a weapon change is pending (set by the d-pad); bit 33 the pending change may be applied now (cleared while L1 is held, which is why a d-pad switch waits for L1's release); any of bits 29–31 makes the branches' weapon tests see no weapon in hand |
| `+0x1c4` u32 | items owned: bit `item - 1` (Cestus is bit 15) |
| `+0x20a` u8 | weapon selected (the d-pad's choice) |
| `+0x20b` u8 | weapon in his hands |
| `+0x20c` u8 | non-zero: the d-pad does not select |

Weapons are items `0x10` Nemean Cestus, `0x11` Nemesis Whip, `0x12` Blades of Exile, `0x13` Claws of
Hades.

- `0x6b240` the d-pad weapon select, every frame: an edge of the game's input bits `0x400000` (Down)
  selects the Cestus, `0x100000` (Left) the Whip, `0x200000` (Up) the Blades, the fourth (Right) the
  Claws, if owned; it plays `SND_SELECT_*`, writes `+0x20a` and sets `+0x168` bit 4.
- `0x6ffe0` applies a pending change (writes `+0x20b`) when bit 33 allows it, then revalidates the
  input queue (`0xec5d0`, below).
- `0x6b5b0` selects a weapon for `SCR_EnableWeapon` (`0x141cdb`): what a switch attack's script calls.
- `0x6ff50` the weapon in his hands; `0x6b100` that weapon's level.

## Move data

Kratos' moves live in `R_HERO00.WAD`, loaded as one block: every reference inside is a rel32 from
the field that holds it, so the block can sit anywhere. Move names (`MOV_CestusTransition`, ...)
are hashed; scripts refer to moves by hash.

A **move** starts with a header: `+0x10` rel32 to its branch array (from `+0x10`), `+0x14` the
number of branches. The array holds rel32s (from each slot) to the branches.

A **branch** (0x38 bytes) is one way out of a move:

| Offset | Meaning |
| --- | --- |
| `+0x00` rel32 | the move it goes to |
| `+0x04` rel32 | a target some callers match against (0 in the branches below) |
| `+0x08` u32 | flags: `0x20000000` fires once per move; `0x200000`/`0x400000` its priority class in the queue; `0x100000`, `0x1000000`, `0x2000000` extra conditions *(unverified)* |
| `+0x0c` u32 | mask matched against the caller's state *(unverified)* |
| `+0x10` u16 | weapon flags, two bits per weapon (Cestus `0x80`/`0x40`, Whip `0x200`/`0x100`, Blades `0x800`/`0x400`, Claws `0x2000`/`0x1000`): the high bit "any", else the low bit "only with" / "never with"; `0xaaaa` is any weapon |
| `+0x12` u16 | buttons that must be held |
| `+0x14`, `+0x16` half | the window in the move's normalized time when the branch can be taken |
| `+0x2c` i16 | button for the generic button test (-1: none) |
| `+0x2f` u8 | the item it is for (0 none); ownership is checked (`0xeb460`) |
| `+0x30` u8 | input kind: 1–11 pick a mask from the table at `0x2eb940` (`0x40 0x80 0x10 0x20 0x4 0x1 0x200 0x8 0x2 0x400 0x600`); 1 is Cross; 12 and 47 are special |
| `+0x31` u8 | how: 1 just pressed, 2 just released, 3/4 held, 5 not held |
| `+0x33`, `+0x34` i8 | the weapon level range |
| `+0x35` u8 | 3: handled by `0xeb6c0` instead |

## Picking a branch

Each frame, `0xe3910` takes a move state (the current move at `[state+0xa8]`) and for every branch
of the current move, in order:

1. skips it if it already fired (bit per branch at `state+0x24`) or the move's time is outside
   `+0x14..+0x16`;
2. `0xeb290`, the input condition: compares the mask of `+0x30` with the input state the caller
   gives (`+0x94` current, `+0x98` previous) as `+0x31` says, then tail-calls `0xeaa90` for the
   rest;
3. `0xec0e0`, the general conditions: item owned, weapon level and flags (`0xec000` -> `0xeb5b0`),
   held buttons, ranges, ...

Of the branches that pass, the best priority class wins; equal ones are picked at random. The
winner goes into the **input queue** of the move state: a ring of 16-byte slots (branch, time) at
`[state+0xc8]`, size `state+0xd0`, head `+0xd1`, tail `+0xd2` (`0xec660` pushes the same way). The
move system takes it at the next point the current move allows, which is what buffers an input
pressed during an attack. When the weapon changes, `0xec5d0` drops a queued branch whose weapon
flags no longer pass. `0xe9a70` runs the same input check over another list (`0xe9e86`), and
`0xec6c0` is a branch loop that matches a target move. The switch attacks feature answers the
switch branches only for `0xe3910` and never takes them from `0xe9a70`, so neither can cycle.

## Switch attacks

The hero data has 42 branches to switch attacks, all alike: input kind 1 (Cross), just pressed
(36) or held (6, in moves that lead to the rarer switch attacks, from 0.15 of the move on),
item the weapon, weapon flags `0xaaaa`, button -1. They sit in the block moves (L1 held is a block
move) and a few others, and list only the weapons that follow in the game's cycle (Blades' block:
Claws; Claws' block: Cestus, then Blades if the Cestus is not owned; ...): the first owned one wins,
which is the vanilla L1 + Cross cycle. Each weapon has two switch attack moves, one most branches
go to and a rarer one (a few moves use it); the move's script selects the weapon with
`SCR_EnableWeapon`.

The switch attacks feature keeps all of that and changes two things: the input check of those
branches answers the pad's request (L1 + d-pad or L1 + Cross) instead of Cross, and the branch the
move chose is swapped, at the queue, for one of the game's own branches to the requested weapon's
switch attack, of the same kind and timing. The same weapon's switch attack again waits 0.8 seconds
(`SWITCH_REPEAT_US`): in the game every L1 + Cross also changed the weapon, so the attacks' short
recovery could not be repeated. The branches are found once by scanning the hero data
around the first one the game asks about (`switch_scan`).

## Hooks that call C

The stubs (`tools/switch_stubs.s`, assembled into the arrays of `src/switch_hook.h`) call C
functions marked `ABI` (`sysv_abi`): the game's code is System V. A stub saves the System V
caller-saved registers that are live at its site (arguments at a function entry; everything but
`rax` mid-function, `xmm0`–`xmm15` included), steps over the red zone, aligns the stack to 16,
calls, restores, and replays the instructions its jump replaced. `tests/test_switch_hook.cpp` runs
each stub from frames laid out like the game's.

## Text

`R_LANG_<code>.WAD` (EN, UK, DE, FR, IT, ES, LA, PT, BR, NL, PL, RU, TR, JA, KO, ZH) holds the
language's text in its `MSGS_TXT` entry: UTF-8, CRLF lines, `*<id>*` before each text (`*<id>*T`
for a tutorial card), button tags like `[L1Button]`. The entry can grow (size field, padding to
16, everything after it moves). Each language's font holds only the characters its text uses, so
new text must stick to those (`scripts/switch_text.py` checks).
