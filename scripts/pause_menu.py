# SPDX-License-Identifier: GPL-2.0-or-later
""""Exit to Desktop" as a sixth row of the game's own pause menu (docs/plans/2026-10-15-native-menu-feasibility.md).

All menus are one compiled state-machine script, the SMI_R_UI entry of R_UI.WAD. Its header (LE)
gives the code start (+0x0c) and the symbol table (+0x10); fields inside are big-endian:
- state records, a preorder tree up to the code: `04 <subtree size>`, parent and symbol words,
  then items `01 <entry code>`, `02 <update code>` and transitions `03 <code>` each followed by
  its trigger words (0x80 in the first byte: more follow). Code refs and literal operands count
  from code start + 4; state refs from SMI + 4; symbol refs from the symbol table + 4.
- code: `16 <u16 literal>`, `01 <i32>`, `09 e0NN` (local NN), `13 <provider:4 function:12>`
  (builtin call), `43` (pop), `49` (return), `4a <rel32>` (call), `47 <state>` (go to state).

PauseMain shows line0..4 (Continue .. Quit Game) and clears line5..7, whose rows the UI already
has. WadChoose, a cheat-only level loader right after PauseMain's subtree, becomes its sixth
child HLExit, with its code written over WadChoose's: no byte moves. Confirming the row calls
builtin 0xfff, which src/menu_hook.h turns into the end of the process."""
import json
from pathlib import Path
import shutil
import struct

from glyphs import entries

VERSION = 1  # of the patch: a change rebuilds the cached copy
WAD = 'EXEC/WAD/PS4_LE/R_UI.WAD'
LABEL = b'Exit to Desktop'
EXIT_CALL = b'\x13\x0f\xff\x43'  # builtin 0xfff (src/menu_hook.h), then pop
UP, DOWN, CONFIRM = [0xe0, 0xec], [0xe4, 0xf0], [0xe9]  # trigger ids of the pad events


def be(value, size=4):
    return value.to_bytes(size, 'big')


class Script:
    def __init__(self, data, base):
        self.data, self.base = data, base
        self.code, self.symtab = struct.unpack_from('<II', data, base + 0x0c)
        self.lit = self.code + 4  # code refs and literal operands count from here
        names, i, pool = {}, self.symtab + 8, None
        while pool is None or i < pool:
            _kind, _index, name = struct.unpack_from('>hhI', data, base + i)
            at = self.symtab + 4 + name
            pool = at if pool is None else min(pool, at)
            names[i - self.symtab - 4] = self.string(at)
            i += 8
        self.records = self.parse_records()
        self.by_name = {}
        for r in self.records:
            self.by_name.setdefault(names.get(r['id']), []).append(r)

    def string(self, at):
        end = self.data.index(b'\0', self.base + at)
        return bytes(self.data[self.base + at:end]).decode('latin1')

    def parse_records(self):
        # The tree starts after a header table of variable length: the first offset that parses
        # exactly up to the code is it.
        for start in range(0x18, 0x400, 4):
            try:
                return self.records_from(start)
            except ValueError:
                continue
        raise ValueError('state table not found')

    def records_from(self, at):
        d, b, out = self.data, self.base, []
        while at < self.code:
            if d[b + at] != 4:
                raise ValueError(f'no state record at {at:#x}')
            size = int.from_bytes(d[b + at + 1:b + at + 4], 'big')
            parent, sid = struct.unpack_from('>II', d, b + at + 4)
            j, items = at + 12, []
            while j < self.code and d[b + j] != 4:
                items.append((d[b + j], int.from_bytes(d[b + j + 1:b + j + 4], 'big')))
                j += 4
            out.append(dict(at=at, size=size, parent=parent, id=sid, items=items))
            at = j
        if at != self.code:
            raise ValueError('state table overruns the code')
        return out

    def one(self, name):
        found = self.by_name.get(name, [])
        if len(found) != 1:
            raise ValueError(f'{name}: {len(found)} states')
        return found[0]

    @staticmethod
    def blocks(record):
        """(kind, code ref, triggers) of each code block of a state."""
        out = []
        for kind, value in record['items']:
            if kind in (1, 2, 3):
                out.append([kind, value, []])
            elif out and out[-1][0] == 3:
                out[-1][2].append(value)
        return out

    def code_refs(self):
        return sorted(v for r in self.records for k, v in r['items'] if k in (1, 2, 3))

    def block_bytes(self, ref):
        refs = self.code_refs()
        end = next(r for r in refs if r > ref)
        return bytes(self.data[self.base + self.lit + ref:self.base + self.lit + end])

    def literal(self, text):
        at = self.data.find(text + b'\0', self.base + self.lit, self.base + self.symtab)
        if at < 0:
            raise ValueError(f'literal {text!r} not found')
        return at - self.base - self.lit


def patch(data):
    """Adds the row to the R_UI.WAD in `data` (a bytearray); ValueError when this is not the
    script the patch was made for (nothing is changed then)."""
    base = next((offset + 0x40 for offset, _size, name in entries(data) if name == 'SMI_R_UI'), None)
    if base is None:
        raise ValueError('no SMI_R_UI in this WAD')
    s = Script(data, base)
    main, cont, quit_, debug = (s.one(n) for n in ('PauseMain', 'HLContinue', 'HLQuitGame', 'WadChoose'))
    if main['at'] + main['size'] != debug['at'] or debug['parent'] != main['parent'] or debug['size'] != 0x38:
        raise ValueError('WadChoose is not where the patch expects it')
    ref = lambda record: record['at'] - 4  # noqa: E731
    lit = lambda text: be(s.literal(text), 2)  # noqa: E731

    def transition(record, triggers):
        found = [b for b in s.blocks(record) if b[0] == 3 and b[2] == triggers]
        if len(found) != 1:
            raise ValueError(f'transition {triggers} not found')
        return found[0][1]

    def retarget(record, triggers, old, new):
        code = s.lit + transition(record, triggers)
        at = data.find(b'\x47' + be(ref(old)), base + code, base + code + 0x20)
        if at < 0:
            raise ValueError('transition target not found')
        edits.append((at + 1, be(ref(new))))

    edits = []
    # Rows: Continue wraps up to Exit, Quit Game goes down to it.
    retarget(cont, UP, quit_, debug)
    retarget(quit_, DOWN, cont, debug)
    # PauseMain clears line5 (local 6): show the label there instead.
    entry = s.block_bytes(next(v for k, v, _t in s.blocks(main) if k == 1))
    clear = [i for i in range(len(entry) - 6) if entry[i] == 0x16 and entry[i + 3:i + 7] == b'\x09\xe0\x06\x43']
    if len(clear) != 1 or s.string(s.lit + int.from_bytes(entry[clear[0] + 1:clear[0] + 3], 'big')) != '':
        raise ValueError('line5 assignment not found')
    main_entry = s.lit + next(v for k, v, _t in s.blocks(main) if k == 1)

    # HLExit's code over WadChoose's: none of it is reached once the record is replaced.
    refs = s.code_refs()
    own = [v for k, v in debug['items'] if k in (1, 2, 3)]
    start, end = min(own), next(r for r in refs if r > max(own))
    if any(start <= r < end for r in refs if r not in own):
        raise ValueError('WadChoose code is shared')
    # No call into it from elsewhere (a 0x4a byte that is not an opcode can only refuse too much).
    for i in range(base + s.lit, base + s.symtab - 5):
        if data[i] == 0x4a and not start <= i - base - s.lit < end:
            target = i - base - s.lit + 5 + struct.unpack_from('>i', data, i + 1)[0]
            if start <= target < end:
                raise ValueError('WadChoose code is called from elsewhere')
    blocks = {k: v for k, v, _t in s.blocks(quit_) if k in (1, 2)}
    highlight = (s.block_bytes(blocks[1]), s.block_bytes(blocks[2]))
    old, new = b'\x16' + lit(b'menuItem4Select'), b'\x16' + lit(b'menuItem5Select')
    if any(code.count(old) != 1 for code in highlight):
        raise ValueError('Quit Game highlight not found')
    up = s.block_bytes(transition(quit_, UP))
    select = s.block_bytes(transition(quit_, CONFIRM))[:7]
    if up[0] != 0x4a or select[0] != 0x16 or select[3:] != b'\x13\x00\x5c\x43':
        raise ValueError('Quit Game transitions not as expected')
    sound = transition(quit_, UP) + 5 + struct.unpack('>i', up[1:5])[0]  # the menu move sound

    code, label = bytearray(LABEL + b'\0'), start
    pieces = {}
    for kind, body in (('entry', highlight[0].replace(old, new)), ('update', highlight[1].replace(old, new))):
        pieces[kind] = start + len(code)
        code += body
    for kind, target in (('up', quit_), ('down', cont)):
        pieces[kind] = start + len(code)
        code += b'\x4a' + struct.pack('>i', sound - (pieces[kind] + 5)) + b'\x47' + be(ref(target)) + b'\x49'
    pieces['confirm'] = start + len(code)
    code += select + EXIT_CALL + b'\x49'
    if start + len(code) > end:
        raise ValueError('no room for the new row')
    record = (be(0x04000000 | debug['size']) + be(ref(main)) + be(debug['id'])
              + be(0x01000000 | pieces['entry']) + be(0x02000000 | pieces['update'])
              + be(0x03000000 | pieces['up']) + b''.join(be(t | 0x80000000) for t in UP[:-1]) + be(UP[-1])
              + be(0x03000000 | pieces['down']) + b''.join(be(t | 0x80000000) for t in DOWN[:-1]) + be(DOWN[-1])
              # the trigger repeated: the record keeps WadChoose's size
              + be(0x03000000 | pieces['confirm']) + be(CONFIRM[0] | 0x80000000) + be(CONFIRM[0]))
    if len(record) != debug['size']:
        raise ValueError('record size mismatch')

    for at_, value in edits:
        data[at_:at_ + len(value)] = value
    data[base + main_entry + clear[0] + 1:base + main_entry + clear[0] + 3] = be(label, 2)
    data[base + s.lit + start:base + s.lit + start + len(code)] = code
    data[base + debug['at']:base + debug['at'] + len(record)] = record
    struct.pack_into('>I', data, base + main['at'], 0x04000000 | (main['size'] + debug['size']))


def build(game, out, source=None):
    """{guest path: patched copy} for run.py to mount, from `source` (another patched copy) or the
    game's own R_UI.WAD; {} when the script is not the one this patch knows. The copy in
    out/pause_menu is kept until the source changes."""
    source = Path(source or Path(game) / WAD)
    target = Path(out) / 'pause_menu'
    copy = target / Path(WAD).name
    if not source.is_file():
        return {}
    stamp = json.dumps([VERSION, str(source), source.stat().st_size, source.stat().st_mtime_ns])
    stamp_file = target / 'stamp.json'
    if not (stamp_file.is_file() and stamp_file.read_text() == stamp):
        shutil.rmtree(target, ignore_errors=True)
        target.mkdir(parents=True)
        data = bytearray(source.read_bytes())
        try:
            patch(data)
        except ValueError as error:
            print(f'Pause menu: {error}; no Exit to Desktop row', flush=True)
            return {}
        copy.write_bytes(data)
        stamp_file.write_text(stamp)  # last: an interrupted build is redone
    if not copy.is_file():
        return {}
    return {'/app0/' + WAD: copy}
