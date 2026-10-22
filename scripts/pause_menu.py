# SPDX-License-Identifier: GPL-2.0-or-later
""""Exit to Desktop" rows in the game's own pause menu and in its Options screen (docs/plans/2026-10-15-native-menu-feasibility.md).

All menus are one compiled state-machine script, the SMI_R_UI entry of R_UI.WAD. Its header (LE)
gives the code start (+0x0c) and the symbol table (+0x10); fields inside are big-endian:
- state records, a preorder tree up to the code: `04 <subtree size>`, parent and symbol words,
  then items `01 <entry code>`, `02 <update code>` and transitions `03 <code>` each followed by
  its trigger words (0x80 in the first byte: more follow). Code refs and literal operands count
  from code start + 4; state refs from SMI + 4; symbol refs from the symbol table + 4.
- code: `16 <u16 literal>`, `01 <i32>`, `09 e0NN` (local NN), `13 <provider:4 function:12>`
  (builtin call), `43` (pop), `49` (return), `4a <rel32>` (call), `47 <state>` (go to state).

Pause menu: PauseMain shows line0..4 (Continue .. Quit Game) and clears line5..7, whose rows the
UI already has. WadChoose, a cheat-only level loader right after PauseMain's subtree, becomes its
sixth child HLExit, with its code written over WadChoose's: no byte moves.

Options screen (reached from the pause menu and from the main menu): PauseOptions shows four rows
and clears line4. A fifth state record is inserted after HLDisplay, so the state table and
everything after it move: state refs, parents and subtree sizes are corrected, and the header and
the WAD entry grow. Its code and the label share WadChoose's dead code with the pause row, so the
literal pool and the symbol table keep their places relative to the code.

Confirming a row asks Quit Game's question (the confirmation is the FailSafe machine of the same
script: a message to it shows the dialog, and its FailSafeYes state acts on "yes" by the kind of
action). The row marks its request with a third parameter of 5 (builtin 0x5e pops a count, then up to 3 parameters, the message and the target); FailSafeDisplay keeps
that mark in a local nothing else uses, and FailSafeYes, given the mark, calls builtin 0xfff, which
src/menu_hook.h turns into the end of the process, instead of what Quit Game does. Both states get a
copy of their block with the few instructions added, written over WadChoose's dead code.
"""
import json
import os
from pathlib import Path
import shutil
import struct

from glyphs import entries

VERSION = 14  # of the patch: a change rebuilds the cached copy
WAD = 'EXEC/WAD/PS4_LE/R_UI.WAD'
LABELS = {
    'en': b'Exit to Desktop',
    'ja': b'Exit to Desktop',
    'zh': b'Exit to Desktop',
    'es': 'Salir al escritorio'.encode('utf-8'),
    'fr': 'Quitter vers le bureau'.encode('utf-8'),
    'de': 'Zurück zum Desktop'.encode('utf-8'),
    'it': 'Esci al desktop'.encode('utf-8'),
    'pl': 'Wyjdź do pulpitu'.encode('utf-8'),
    'pt': 'Sair para a área de trabalho'.encode('utf-8'),
    'ru': b'Exit to Desktop',
}
LABEL = LABELS['en']
LANGUAGE_ALIASES = {
    '0': 'ja', 0: 'ja',
    '1': 'en', 1: 'en',
    '2': 'fr', 2: 'fr',
    '3': 'es', 3: 'es',
    '4': 'de', 4: 'de',
    '5': 'it', 5: 'it',
    '7': 'pt', 7: 'pt',
    '8': 'ru', 8: 'ru',
    '10': 'zh', 10: 'zh',
    '16': 'pl', 16: 'pl',
    '17': 'pt', 17: 'pt',  # pt-BR (R_LANG_BR.WAD)
    '18': 'en', 18: 'en',  # en-GB (R_LANG_UK.WAD)
    '20': 'es', 20: 'es',  # es-419 (R_LANG_LA.WAD)
    'br': 'pt', 'pt-br': 'pt', 'pt_br': 'pt',
    'la': 'es', 'es-es': 'es', 'es_es': 'es', 'es-419': 'es', 'es_419': 'es',
    'uk': 'en', 'en-us': 'en', 'en_us': 'en', 'en-gb': 'en', 'en_gb': 'en',
    'zh-tw': 'zh', 'zh_tw': 'zh', 'zh-hant': 'zh', 'zh_hant': 'zh',
}


def resolve_label(language=None):
    """(bytes, str) of (label_bytes, lang_key) for the given language (id or code, or SPARTAN_LANGUAGE)."""
    if language is None:
        language = os.environ.get('SPARTAN_LANGUAGE', '1')
    if isinstance(language, str):
        language = language.strip().lower()
    key = LANGUAGE_ALIASES.get(language, language)
    if isinstance(key, str):
        key = key.lower()
    if key in LABELS:
        return LABELS[key], key
    return LABELS['en'], 'en'


EXIT_CALL = b'\x13\x0f\xff\x43'  # builtin 0xfff (src/menu_hook.h), then pop
UP, DOWN, CONFIRM, REPLY = [0xe0, 0xec], [0xe4, 0xf0], [0xe9], [0x4b]  # trigger ids: pad events and the dialog's answer
ROW_SIZE = 0x3c  # of the row's state record
# Quit Game's reaction to the dialog being hidden by "no" (the message's first parameter is 0): input back on.
REPLY_TEMPLATE = bytes.fromhex('01 00000000 130047 01 00000000 3f 170005 19 130069 43 49')
MARK = 5  # the exit rows' third message parameter (Restart and Quit send two)
# Opening of FailSafeDisplay's block for message 75 ("show" when the first parameter is 1) and of
# FailSafeYes' confirm block (it branches on the action's kind).
SHOW_BLOCK = bytes.fromhex('01 00000000 130047 01 00000001 3f 170084')
YES_BLOCK = bytes.fromhex('03 e001 0b e002 01 00000001 3f 17002f')
# FailSafeDisplay keeps the mark in its local 22 (failSafeMenu, unused); the confirm block ends the
# process when it is MARK. The jump skips the exit call, its pop and the return.
LOCAL = bytes([0xe0, 0x16])
SHOW_PREFIX = bytes.fromhex('01 00000002 130047') + b'\x07' + LOCAL + b'\x43'
YES_PREFIX = b'\x03' + LOCAL + b'\x01' + MARK.to_bytes(4, "big") + bytes.fromhex('3f 170005') + EXIT_CALL + b'\x49'
# Bytes taken by each opcode with its operands (the interpreter's handlers, image 0x1d5a4e).
LENGTH = {op: 5 if op in (0x00, 0x01, 0x47, 0x4a) else 3 if 0x02 <= op <= 0x18 else 1 for op in range(0x4c)}


def be(value, size=4):
    return value.to_bytes(size, 'big')


def decode(data, start, end):
    """[(offset, opcode, operand bytes)] of the code in data[start:end]; ValueError when it does
    not split exactly into instructions."""
    out, i = [], start
    while i < end:
        op = data[i]
        if op not in LENGTH:
            raise ValueError(f'unknown opcode {op:#x}')
        out.append((i, op, bytes(data[i + 1:i + LENGTH[op]])))
        i += LENGTH[op]
    if i != end:
        raise ValueError('code overruns its block')
    return out


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

    def spans(self):
        """[(start, end)] code offsets of every code block of an unpatched script: the last one
        ends where the literal pool begins, at the smallest literal operand."""
        refs = sorted(set(self.code_refs()))
        spans = list(zip(refs, refs[1:]))
        pool = min(int.from_bytes(arg, 'big') for a, b in spans
                   for _at, op, arg in decode(self.data, self.base + self.lit + a, self.base + self.lit + b) if op == 0x16)
        if refs[-1] >= pool:
            raise ValueError('literal pool not found')
        return [*spans, (refs[-1], pool)]


def transition(s, record, triggers):
    found = [b for b in s.blocks(record) if b[0] == 3 and b[2] == triggers]
    if len(found) != 1:
        raise ValueError(f'transition {triggers} not found')
    return found[0][1]


def retarget(s, record, triggers, old, new):
    """(offset, bytes) that make the state `record` go to the state ref `new` instead of `old`
    when `triggers` fire."""
    code = s.base + s.lit + transition(s, record, triggers)
    at = s.data.find(b'\x47' + be(old), code, code + 0x20)
    if at < 0:
        raise ValueError('transition target not found')
    return at + 1, be(new)


def row_code(s, quit_, start, up, down, slot, label=True):
    """(code, pieces) of an Exit row: the label (unless `label` is false), then Quit Game's blocks
    as the template, with `slot` as its highlight, laid from code offset `start`; `up` and `down`
    are the state refs its arrows go to. Confirming asks Quit Game's own question ("Are you sure
    you want to Quit?") with the exit mark; the row's handler for the dialog being hidden is Quit
    Game's. pieces: code offset of each block."""
    lit = lambda text: be(s.literal(text), 2)  # noqa: E731
    blocks = {k: v for k, v, _t in s.blocks(quit_) if k in (1, 2)}
    highlight = (s.block_bytes(blocks[1]), s.block_bytes(blocks[2]))
    old, new = b'\x16' + lit(b'menuItem4Select'), b'\x16' + lit(slot)
    if any(code.count(old) != 1 for code in highlight):
        raise ValueError('Quit Game highlight not found')
    arrow = s.block_bytes(transition(s, quit_, UP))
    ask = s.block_bytes(transition(s, quit_, CONFIRM))
    answer = s.block_bytes(transition(s, quit_, REPLY))
    if arrow[0] != 0x4a or ask[0] != 0x16 or ask[3:7] != b'\x13\x00\x5c\x43' or b'\x13\x00\x5e\x43' not in ask:
        raise ValueError('Quit Game transitions not as expected')
    if answer != REPLY_TEMPLATE:
        raise ValueError("Quit Game's answer handler not as expected")
    request = b'\x01' + be(4) + b'\x13\x00\x5e\x43'  # the argument count (target, message, 2 parameters), then the send
    if ask.count(request) != 1:
        raise ValueError("Quit Game's request not as expected")
    # A third parameter goes in before the count, which becomes 5 (the count alone underflows the stack).
    ask = ask.replace(request, b'\x01' + be(MARK) + b'\x01' + be(5) + b'\x13\x00\x5e\x43')
    sound = transition(s, quit_, UP) + 5 + struct.unpack('>i', arrow[1:5])[0]  # the menu move sound

    if isinstance(label, (bytes, bytearray)):
        prefix = bytes(label) + b'\0'
    elif label:
        prefix = LABEL + b'\0'
    else:
        prefix = b''
    code, pieces = bytearray(prefix), {}
    for kind, body in (('entry', highlight[0]), ('update', highlight[1])):
        pieces[kind] = start + len(code)
        code += body.replace(old, new)
    for kind, target in (('up', up), ('down', down)):
        pieces[kind] = start + len(code)
        code += b'\x4a' + struct.pack('>i', sound - (pieces[kind] + 5)) + b'\x47' + be(target) + b'\x49'
    for kind, body in (('confirm', ask), ('reply', answer)):
        pieces[kind] = start + len(code)
        code += body
    return bytes(code), pieces


def row_record(parent, sid, pieces):
    """The row's state record: highlight, the two arrows, confirm and the dialog's answer."""
    triggers = lambda ids: b''.join(be(t | 0x80000000) for t in ids[:-1]) + be(ids[-1])  # noqa: E731
    record = (be(0x04000000 | ROW_SIZE) + be(parent) + be(sid)
              + be(0x01000000 | pieces['entry']) + be(0x02000000 | pieces['update'])
              + be(0x03000000 | pieces['up']) + triggers(UP) + be(0x03000000 | pieces['down']) + triggers(DOWN)
              + be(0x03000000 | pieces['confirm']) + triggers(CONFIRM)
              + be(0x03000000 | pieces['reply']) + triggers(REPLY))
    if len(record) != ROW_SIZE:
        raise ValueError('record size mismatch')
    return record


def moved_copy(s, ref, at, final):
    """The code block at `ref`, laid at code offset `at`: states that move are followed and calls
    are rebased."""
    end = next(r for r in s.code_refs() if r > ref)
    first = s.base + s.lit + ref
    out = bytearray(s.data[first:s.base + s.lit + end])
    for pos, op, arg in decode(s.data, first, s.base + s.lit + end):
        i = pos - first
        if op == 0x47:
            out[i + 1:i + 5] = be(final(int.from_bytes(arg, 'big') + 4) - 4)
        elif op == 0x4a:
            out[i + 1:i + 5] = struct.pack('>i', ref + i + 5 + struct.unpack('>i', arg)[0] - (at + i + 5))
    return bytes(out)


def failsafe_code(s, start, final):
    """(code, {record: (old block, offset of the new one)}): the FailSafe states' two blocks with
    the exit mark's instructions added, to be laid from code offset `start`."""
    out, code, redirects = bytearray(), bytearray(), {}
    for name, trigger, opening, prefix in (('FailSafeDisplay', REPLY, SHOW_BLOCK, SHOW_PREFIX),
                                           ('FailSafeYes', CONFIRM, YES_BLOCK, YES_PREFIX)):
        record = s.one(name)
        ref = transition(s, record, trigger)
        block = s.block_bytes(ref)
        if not block.startswith(opening):
            raise ValueError(f'{name} not as expected')
        at = start + len(code)
        code += prefix + moved_copy(s, ref, at + len(prefix), final)
        redirects[name] = (record, ref, at)
    return bytes(code), redirects


def blank_line(s, record, local):
    """Offset of the 16-bit literal that `record`'s entry assigns to the local `local` while it
    is empty, to be replaced by the label."""
    entry_ref = next(v for k, v, _t in s.blocks(record) if k == 1)
    entry = s.block_bytes(entry_ref)
    found = [i for i in range(len(entry) - 6)
             if entry[i] == 0x16 and entry[i + 3:i + 7] == b'\x09\xe0' + bytes([local]) + b'\x43'
             and s.string(s.lit + int.from_bytes(entry[i + 1:i + 3], 'big')) == '']
    if len(found) != 1:
        raise ValueError(f'line {local - 1} assignment not found')
    return s.base + s.lit + entry_ref + found[0] + 1


def display_health_bar(data, spans):
    """A fourth Display row, using PauseText's existing line3 and menuItem3Select animation."""
    base = next(offset + 0x40 for offset, _size, name in entries(data) if name == 'SMI_R_UI')
    s = Script(data, base)
    display, y, brightness, template = (s.one(n) for n in
                                       ('PauseDisplay', 'HLScreenLocY', 'HLBrightness', 'HLSubtitleDis'))
    insert = display['at'] + display['size']
    if y['at'] + y['size'] != insert:
        raise ValueError('Display does not end with Screen Location Y')
    row_size = 0x44
    final = lambda at: at + (row_size if at >= insert else 0)
    row_ref = insert - 4
    append = bytearray()
    start = s.symtab - s.lit

    def text(value):
        ref = start + len(append)
        if ref > 0xffff:
            raise ValueError('Display text exceeds the script literal range')
        append.extend(value + b'\0')
        return b'\x16' + be(ref, 2)

    label = text(b'Enemy Health Bar')
    off, on, unavailable = (text(v) for v in (b'Off', b'On', b'Unavailable'))
    selected = text(b'[ArrowSelectionL][PauseMenu:optSet3][ArrowSelectionR]')
    unselected = text(b'[ArrowSpacer][PauseMenu:optSet3][ArrowSpacer]')
    assign = b'\x09\xe0\x14\x43'  # PauseMenu.optSet3
    # A subroutine used on entry and after an edit. Query again so the Insert menu stays in sync.
    refresh = start + len(append)
    enabled = b'\x13\x0f\xfe\x17' + be(len(on + assign + b'\x4b'), 2) + on + assign + b'\x4b' + off + assign + b'\x4b'
    unsupported = unavailable + assign + b'\x4b'
    append += (b'\x13\x0f\xfe\x01' + be(2) + b'\x3f\x17' + be(len(unsupported), 2)
               + unsupported + enabled)

    def call(target, at):
        return b'\x4a' + struct.pack('>i', target - at - 5)

    def add(body):
        ref = start + len(append)
        append.extend(body)
        return ref

    # Display clears its rows whenever it opens, before any individual row gets focus.
    entry_item = next(i for i, (kind, _ref) in enumerate(display['items']) if kind == 1)
    entry_ref = display['items'][entry_item][1]
    at = start + len(append)
    body = moved_copy(s, entry_ref, at + 5, final)
    for local, value in ((4, label), (12, unselected)):
        operand = blank_line(s, display, local)
        empty = b'\x16' + bytes(data[operand:operand + 2])
        setter = b'\x09\xe0' + bytes([local]) + b'\x43'
        body = body.replace(empty + setter, value + setter)
    display_entry = add(call(refresh, at) + body)

    pieces = {}
    for kind, name in ((1, 'entry'), (2, 'update')):
        old = next(v for k, v, _t in s.blocks(template) if k == kind)
        at = start + len(append)
        prefix = call(refresh, at) if kind == 1 else b''
        body = moved_copy(s, old, at + len(prefix), final)
        old_value = b'\x16' + be(s.literal(b'[ArrowSelectionL][PauseMenu:optSet1][ArrowSelectionR]'
                                           if kind == 1 else b'[ArrowSpacer][PauseMenu:optSet1][ArrowSpacer]'), 2)
        body = body.replace(old_value, selected if kind == 1 else unselected)
        body = body.replace(b'\x09\xe0\x0a\x43', b'\x09\xe0\x0c\x43')  # line3 value
        body = body.replace(b'\x16' + be(s.literal(b'menuItem1Select'), 2),
                            b'\x16' + be(s.literal(b'menuItem3Select'), 2))
        pieces[name] = add(prefix + body)
    old = transition(s, template, [0xe6, 0xe2, 0xee, 0xf2])
    at = start + len(append)
    debounce = moved_copy(s, old, at, final)[:5]  # the native left/right repeat gate
    body = debounce + b'\x13\x0f\xfd\x43'
    body += call(refresh, at + len(body)) + selected + b'\x09\xe0\x0c\x43'
    body += b'\x16' + be(s.literal(b'SND_MM_NAV_LR'), 2) + b'\x13\x00\x5c\x43\x49'
    pieces['toggle'] = add(body)
    for name, trigger, target in (('up', UP, y['at'] - 4), ('down', DOWN, brightness['at'] - 4)):
        at = start + len(append)
        old = transition(s, y, trigger)
        body = moved_copy(s, old, at, final)
        body = body[:5] + b'\x47' + be(target) + b'\x49'
        pieces[name] = add(body)

    # Rebase existing states once; code/string offsets do not move relative to the code base.
    edits = []
    for a, b in spans:
        for at, op, arg in decode(data, base + s.lit + a, base + s.lit + b):
            if op == 0x47 and int.from_bytes(arg, 'big') + 4 >= insert:
                edits.append((at + 1, be(final(int.from_bytes(arg, 'big') + 4) - 4)))
    edits += [retarget(s, y, DOWN, brightness['at'] - 4, row_ref),
              retarget(s, brightness, UP, y['at'] - 4, row_ref),
              (blank_line(s, display, 4), label[1:]),
              (base + display['at'] + 12 + 4 * entry_item, be(0x01000000 | display_entry))]
    parents = {r['at'] - 4: r for r in s.records}
    ancestor = display
    while ancestor:
        edits.append((base + ancestor['at'], be(0x04000000 | (ancestor['size'] + row_size))))
        ancestor = parents.get(ancestor['parent'])
    for record in s.records:
        if record['parent'] + 4 >= insert:
            edits.append((base + record['at'] + 4, be(final(record['parent'] + 4) - 4)))
    for at, value in edits:
        data[at:at + len(value)] = value

    def event(name, triggers):
        return (be(0x03000000 | pieces[name])
                + b''.join(be(t | (0x80000000 if i + 1 < len(triggers) else 0)) for i, t in enumerate(triggers)))

    record = (be(0x04000000 | row_size) + be(display['at'] - 4) + be(template['id'])
              + be(0x01000000 | pieces['entry']) + be(0x02000000 | pieces['update'])
              + event('toggle', [0xe6, 0xe2, 0xee, 0xf2, 0xe9]) + event('up', UP) + event('down', DOWN))
    if len(record) != row_size:
        raise ValueError('Display row size mismatch')
    old_size = struct.unpack_from('<I', data, base - 0x40 + 4)[0]
    data[base + s.symtab:base + s.symtab] = append
    data[base + insert:base + insert] = record
    grown = row_size + len(append)
    for offset, delta in ((0x08, grown), (0x0c, row_size), (0x10, grown)):
        struct.pack_into('<I', data, base + offset, struct.unpack_from('<I', data, base + offset)[0] + delta)
    struct.pack_into('<I', data, base - 0x40 + 4, old_size + grown)
    data[base + old_size + grown:base + old_size + grown + -old_size % 16] = bytes(-(old_size + grown) % 16)
    return spans + [(refresh, start + len(append))]


def native_cheats(data, spans):
    """Native Options > Cheats > Multipliers, sharing the port's existing settings."""
    base = next(o + 64 for o, size, name in entries(data) if name == 'SMI_R_UI')
    s = Script(data, base)
    options, game, audio = (s.one(n) for n in ('PauseOptions', 'HLGSettings', 'HLAudio'))
    enter = s.one('EnterPause')
    opt_at, pages_at = options['at'] + options['size'], enter['at'] + enter['size']
    left, right = [0xe6, 0xf2], [0xe2, 0xee]
    toggle = [0xe6, 0xe2, 0xee, 0xf2, 0xe9]
    specs = [('HLNativeCheats', 'options', [UP, DOWN, CONFIRM]),
             ('NativeCheats', 'pause', [[0xeb]])]
    specs += [(f'NativeCheat{i}', 'NativeCheats', [toggle, UP, DOWN]) for i in range(8)]
    specs += [('NativeMultipliersLink', 'NativeCheats', [UP, DOWN, CONFIRM]),
              ('NativeMultipliers', 'pause', [[0xeb]])]
    specs += [(f'NativeMultiplier{i}', 'NativeMultipliers', [left, right, UP, DOWN]) for i in range(6)]
    specs += [('NativeMultiplierReset', 'NativeMultipliers', [UP, DOWN, CONFIRM])]
    sizes = {name: 20 + sum(4 + 4 * len(t) for t in events) for name, parent, events in specs}
    option_size = sizes['HLNativeCheats']
    pages_size = sum(sizes.values()) - option_size
    final = lambda at: at + (option_size if at >= opt_at else 0) + (pages_size if at >= pages_at else 0)
    refs = {'options': final(options['at']) - 4, 'pause': final(enter['at']) - 4}
    refs['HLNativeCheats'] = opt_at - 4
    at = pages_at + option_size
    for name, parent, events in specs[1:]:
        refs[name] = at - 4
        at += sizes[name]

    # New symbol records go before the old name pool; existing symbol ids stay valid.
    old_size = struct.unpack_from('<I', data, base - 60)[0]
    symbols = bytearray(data[base + s.symtab:base + old_size])
    pool, pos = None, 8
    while pool is None or pos < pool:
        offset = struct.unpack_from('>I', symbols, pos + 4)[0] + 4
        pool = offset if pool is None else min(pool, offset)
        pos += 8
    symbol_ids = {name: pool - 4 + i * 8 for i, (name, parent, events) in enumerate(specs)}
    delta = len(specs) * 8
    for pos in range(8, pool, 8):
        struct.pack_into('>I', symbols, pos + 4, struct.unpack_from('>I', symbols, pos + 4)[0] + delta)
    new_symbols, names = bytearray(), bytearray()
    for name, parent, events in specs:
        new_symbols += struct.pack('>HHI', 0, 0, len(symbols) + delta + len(names) - 4)
        names += name.encode('ascii') + b'\0'
    symbols[pool:pool] = new_symbols
    symbols += names

    strings, code, labels, fixups = bytearray(), bytearray(), {}, []
    string_refs = {}
    start = s.symtab - s.lit

    def emit(value):
        code.extend(value)

    def string(value):
        value = value.encode('ascii') if isinstance(value, str) else value
        if value not in string_refs:
            try:
                string_refs[value] = s.literal(value)
            except ValueError:
                ref = start + len(strings)
                if ref > 0xffff:
                    raise ValueError('Native menu text exceeds the script literal range')
                string_refs[value] = ref
                strings.extend(value + b'\0')
        emit(b'\x16' + be(string_refs[value], 2))

    def integer(value):
        emit(b'\x01' + be(value))

    def builtin(function, pop=True):
        emit(b'\x13' + be(function, 2) + (b'\x43' if pop else b''))

    def assign(local):
        emit(b'\x09\xe0' + bytes([local]) + b'\x43')

    def call(target):
        fixups.append((len(code), target))
        emit(b'\x4a' + bytes(4))

    def goto(target):
        emit(b'\x47' + be(refs[target] if isinstance(target, str) else final(target + 4) - 4))

    def sound(name):
        string(name)
        builtin(0x5c)

    def animation(index, layer):
        string('PauseText')
        integer(layer)
        string(f'menuItem{index}Select')
        integer(3)
        builtin(0x5a)

    def value(index, selected=False):
        edge = ('ArrowSelectionL', 'ArrowSelectionR') if selected else ('ArrowSpacer', 'ArrowSpacer')
        string(f'[{edge[0]}][PauseMenu:optSet{index}][{edge[1]}]')
        assign(9 + index)

    def query(index, function, selected=False):
        builtin(function, pop=False)
        assign(0x11 + index)
        value(index, selected)

    def slider(index):
        string(f'sliderBar{index + 4}')
        integer(64)
        string('')
        emit(b'\x00' + struct.pack('>f', 0.0))
        builtin(0xf50 + index, pop=False)
        emit(b'\x2b\x00' + struct.pack('>f', 30.0) + b'\x24')  # timeline seconds = percent / 30
        integer(5)
        builtin(0x5a)

    def bars(show):
        for index in range(6):
            string(f'sliderBar{index + 4}')
        integer(6)
        builtin(0x58 if show else 0x59)

    # Reuse the native repeat gate used by the subtitle toggle and volume sliders.
    template = s.by_name['HLSubtitleDis'][0]
    ref = transition(s, template, [0xe6, 0xe2, 0xee, 0xf2])
    body = s.block_bytes(ref)
    if body[0] != 0x4a:
        raise ValueError('Native repeat gate not found')
    repeat = ref + 5 + struct.unpack('>i', body[1:5])[0]
    cheat_names = ['Infinite Health', 'Infinite Magic', 'Infinite Item Meter', 'Infinite Rage of Sparta',
                   'Max Red Orbs', 'Unlock Very Hard', 'Infinite Double Jumps', 'Automatic QTE', 'Multipliers']
    multiplier_names = ['Red Orbs', 'Green Orbs (Health)', 'Blue Orbs (Magic)', 'Gold Orbs (Rage)',
                        'Damage Dealt', 'Damage Taken', 'Reset All to 1x']
    handlers = {}

    def begin(name, kind):
        key = (name, kind)
        handlers[key] = len(code)
        labels[key] = len(code)

    def end():
        emit(b'\x49')

    def cheat_window(first):
        for slot in range(8):
            row = first + slot if slot < 7 else 8
            string(cheat_names[row])
            assign(1 + slot)
            if row < 8:
                query(slot, 0xf00 + row)
            else:
                string('')
                assign(9 + slot)

    def selection(name, index, function=None, selected=True, cheat=False):
        begin(name, 1 if selected else 2)
        if selected:
            builtin(0x6b)
            if cheat:
                cheat_window(1 if name == 'NativeMultipliersLink' or int(name[-1]) >= 6 else 0)
            if function is not None:
                query(index, function, True)
        elif function is not None:
            value(index)
        animation(index, 8 if selected else 9)
        end()

    def navigation(name, index, rows):
        for kind, target in (('up', rows[(index - 1) % len(rows)]), ('down', rows[(index + 1) % len(rows)])):
            begin(name, kind)
            sound('SND_MM_NAV_UPDN')
            goto(target)
            end()

    selection('HLNativeCheats', 2)
    selection('HLNativeCheats', 2, selected=False)
    for kind, target in (('up', game['at'] - 4), ('down', audio['at'] - 4)):
        begin('HLNativeCheats', kind)
        sound('SND_MM_NAV_UPDN')
        goto(target)
        end()
    begin('HLNativeCheats', 'confirm')
    sound('SND_MM_SELECT')
    goto('NativeCheats')
    end()

    cheat_rows = [f'NativeCheat{i}' for i in range(8)] + ['NativeMultipliersLink']
    multiplier_rows = [f'NativeMultiplier{i}' for i in range(6)] + ['NativeMultiplierReset']
    for page, title, row_names, rows, is_multiplier in (
            ('NativeCheats', 'Cheats', cheat_names, cheat_rows, False),
            ('NativeMultipliers', 'Multipliers', multiplier_names, multiplier_rows, True)):
        begin(page, 1)
        string(title)
        assign(0)
        for index in range(8):
            string(row_names[index] if index < len(row_names) else '')
            assign(1 + index)
            string('')
            assign(9 + index)
        for local in range(0x19, 0x1d):
            string('')
            assign(local)
        for index in range(6 if is_multiplier else 8):
            query(index, (0xf20 if is_multiplier else 0xf00) + index)
            if is_multiplier:
                slider(index)
        if is_multiplier:
            bars(True)
        else:
            cheat_window(0)
        goto(rows[0])
        end()
        begin(page, 2)
        if is_multiplier:
            bars(False)
        end()
        begin(page, 'back')
        sound('SND_MM_BACK')
        goto('NativeCheats' if is_multiplier else options['at'] - 4)
        end()
        for index, name in enumerate(rows):
            function = ((0xf20 if is_multiplier else 0xf00) + index) if index + 1 < len(rows) else None
            slot = index - (not is_multiplier and 6 <= index < 8)
            if not is_multiplier and index == 8:
                slot = 7
            selection(name, slot, function, cheat=not is_multiplier)
            selection(name, slot, function, False)
            navigation(name, index, rows)
            if function is None:
                begin(name, 'confirm')
                sound('SND_MM_SELECT')
                if is_multiplier:
                    builtin(0xf60)
                    for i in range(6):
                        query(i, 0xf20 + i)
                        slider(i)
                else:
                    goto('NativeMultipliers')
                end()
            elif is_multiplier:
                for kind, function in (('left', 0xf30 + index), ('right', 0xf40 + index)):
                    begin(name, kind)
                    call(repeat)
                    query(index, function, True)
                    slider(index)
                    sound('SND_MM_NAV_LR')
                    end()
            else:
                begin(name, 'toggle')
                call(repeat)
                query(slot, 0xf10 + index, True)
                sound('SND_MM_NAV_LR')
                end()

    # Clone Options' entry, adding Cheats at line2 and shifting Audio/Display/Exit down one row.
    old_entry = next(v for k, v, t in s.blocks(options) if k == 1)
    original_entry = s.block_bytes(old_entry)
    replacements = {3: 'Cheats'}
    for old_local in (3, 4, 5):
        setter = b'\x09\xe0' + bytes([old_local]) + b'\x43'
        at = original_entry.index(setter)
        value_ref = int.from_bytes(original_entry[at - 2:at], 'big')
        replacements[old_local + 1] = s.string(s.lit + value_ref).encode('utf-8')
    # Keep existing non-ASCII Exit labels as bytes (their source encoding is UTF-8).
    exit_at = original_entry.index(b'\x09\xe0\x05\x43')
    exit_ref = int.from_bytes(original_entry[exit_at - 2:exit_at], 'big')
    raw = bytes(data[base + s.lit + exit_ref:]).split(b'\0', 1)[0]
    replacements[6] = raw
    labels['options_entry'] = len(code)
    for at, op, arg in decode(data, base + s.lit + old_entry, base + s.lit + old_entry + len(original_entry)):
        relative = at - base - s.lit
        if op == 0x16 and data[at + 3:at + 5] == b'\x09\xe0' and data[at + 5] in replacements:
            string(replacements[data[at + 5]])
        elif op == 0x47:
            goto(int.from_bytes(arg, 'big'))
        elif op == 0x4a:
            call(relative + 5 + struct.unpack('>i', arg)[0])
        else:
            emit(bytes([op]) + arg)

    code_start = start + len(strings)
    for at, target in fixups:
        destination = code_start + labels[target] if target in labels else target
        struct.pack_into('>i', code, at + 1, destination - (code_start + at + 5))
    edits = []
    for a, b in spans:
        for at, op, arg in decode(data, base + s.lit + a, base + s.lit + b):
            if op == 0x47 and int.from_bytes(arg, 'big') + 4 >= opt_at:
                edits.append((at + 1, be(final(int.from_bytes(arg, 'big') + 4) - 4)))
    edits += [retarget(s, game, DOWN, audio['at'] - 4, refs['HLNativeCheats']),
              retarget(s, audio, UP, game['at'] - 4, refs['HLNativeCheats'])]
    entry_item = next(i for i, (k, v) in enumerate(options['items']) if k == 1)
    edits.append((base + options['at'] + 12 + 4 * entry_item, be(0x01000000 | (code_start + labels['options_entry']))))
    for record, old_index in ((audio, 2), (s.by_name['HLDisplay'][0], 3), (s.by_name['HLDisplay'][1], 4)):
        for kind, ref, triggers in s.blocks(record):
            if kind in (1, 2):
                block = s.block_bytes(ref)
                source = b'\x16' + be(s.literal(f'menuItem{old_index}Select'.encode()), 2)
                destination = b'\x16' + be(s.literal(f'menuItem{old_index + 1}Select'.encode()), 2)
                if block.count(source) != 1:
                    raise ValueError('Options selection animation not found')
                edits.append((base + s.lit + ref + block.index(source), destination))
    ancestors = {r['at'] - 4: r for r in s.records}
    growth = {}
    for record, amount in ((options, option_size), (enter, pages_size)):
        while record:
            growth[record['at']] = growth.get(record['at'], 0) + amount
            record = ancestors.get(record['parent'])
    for record in s.records:
        if record['at'] in growth:
            edits.append((base + record['at'], be(0x04000000 | (record['size'] + growth[record['at']]))))
        if record['parent'] + 4 >= opt_at:
            edits.append((base + record['at'] + 4, be(final(record['parent'] + 4) - 4)))
    for at, value in edits:
        data[at:at + len(value)] = value

    records = bytearray()
    for name, parent, events in specs:
        size = sizes[name]
        if name in ('NativeCheats', 'NativeMultipliers'):
            size += sum(sizes[n] for n, p, e in specs if p == name)
        record = bytearray(be(0x04000000 | size) + be(refs[parent]) + be(symbol_ids[name]))
        for kind in (1, 2):
            record += be((kind << 24) | (code_start + handlers[name, kind]))
        event_names = ['back'] if parent == 'pause' else (
            ['left', 'right', 'up', 'down'] if name.startswith('NativeMultiplier') and name[-1:].isdigit()
            else ['toggle', 'up', 'down'] if name.startswith('NativeCheat') and name[-1:].isdigit()
            else ['up', 'down', 'confirm'])
        for event_name, triggers in zip(event_names, events):
            record += be(0x03000000 | (code_start + handlers[name, event_name]))
            record += b''.join(be(t | (0x80000000 if i + 1 < len(triggers) else 0)) for i, t in enumerate(triggers))
        if len(record) != sizes[name]:
            raise ValueError(f'{name}: native record size mismatch')
        records += record
    data[base + s.symtab:base + old_size] = strings + code + symbols
    data[base + pages_at:base + pages_at] = records[option_size:]
    data[base + opt_at:base + opt_at] = records[:option_size]
    grown = option_size + pages_size + len(strings) + len(code) + len(symbols) - (old_size - s.symtab)
    for offset, amount in ((8, grown), (12, option_size + pages_size), (16, option_size + pages_size + len(strings) + len(code))):
        struct.pack_into('<I', data, base + offset, struct.unpack_from('<I', data, base + offset)[0] + amount)
    struct.pack_into('<I', data, base - 60, old_size + grown)
    data[base + old_size + grown:base + old_size + grown + -old_size % 16] = bytes(-(old_size + grown) % 16)
    native_multiplier_bars(data)


def native_multiplier_bars(data):
    """Dedicated sliders left of the values, preserving the original audio/display controls."""
    all_entries = list(entries(data))
    source, size, _ = next(e for e in all_entries if e[2] == 'gosliderbar3' and e[1])
    if size != 148 or struct.unpack_from('<H', data, source + 64 + 0x76)[0] != 3:
        raise ValueError('Native slider template not recognized')
    group = bytes(data[source - 64:source + 64 + 160 + 64])
    reference, _, _ = next(e for e in all_entries if e[2] == 'gosliderbar3' and not e[1])
    instance_ids = [struct.unpack_from('<I', data, o + 64 + 0x3c)[0] for o, n, name in all_entries
                    if n == 148 and struct.unpack_from('<I', data, o)[0] == 0x1f0001]
    last = max(value for value in instance_ids if value >> 16 == 1)
    groups, references = bytearray(), bytearray()
    for index in range(4, 10):
        name = f'gosliderbar{index}'.encode().ljust(56, b'\0')
        copy = bytearray(group)
        copy[72:128] = name
        struct.pack_into('<I', copy, 128 + 0x3c, last + index - 3)
        struct.pack_into('<f', copy, 128 + 0x40, 0.75)
        struct.pack_into('<f', copy, 128 + 0x64, 399.5)
        struct.pack_into('<f', copy, 128 + 0x68, 2.95 - 0.75 * (index - 4))
        struct.pack_into('<H', copy, 128 + 0x76, index)
        groups += copy
        references += bytes(data[reference:reference + 8]) + name
    data[reference + 64:reference + 64] = references
    data[source + 64 + 160 + 64:source + 64 + 160 + 64] = groups


def patch(data, language=None):
    """Adds the rows to the R_UI.WAD in `data` (a bytearray); ValueError when this is not the
    script the patch was made for (nothing is changed then).

    Both rows are new state records. The pause row's replaces WadChoose's, a cheat-only level
    loader right after PauseMain's subtree; the Options row's goes after HLDisplay. Each record is
    a little bigger than what it replaces or follows, so the state table grows and everything after
    it moves: the header, the sizes of the subtrees the records join and leave, the parent of later
    records, every goto to a later state and the WAD entry are corrected. The code of both rows and
    their label go over WadChoose's dead code, which leaves the literal pool and the symbol table
    where they were relative to the code."""
    base = next((offset + 0x40 for offset, _size, name in entries(data) if name == 'SMI_R_UI'), None)
    if base is None:
        raise ValueError('no SMI_R_UI in this WAD')
    work = bytearray(data)
    s = Script(work, base)
    main, cont, quit_, debug = (s.one(n) for n in ('PauseMain', 'HLContinue', 'HLQuitGame', 'WadChoose'))
    opt, ctl, dsp = (s.one(n) for n in ('PauseOptions', 'HLControls', 'HLDisplay'))
    n = ROW_SIZE
    if main['at'] + main['size'] != debug['at'] or debug['parent'] != main['parent'] or debug['size'] > n:
        raise ValueError('WadChoose is not where the patch expects it')
    if dsp['at'] + dsp['size'] != opt['at'] + opt['size'] or dsp['parent'] != opt['at'] - 4:
        raise ValueError('PauseOptions is not laid out as expected')
    starts = {r['at'] for r in s.records}

    # The two splices, in table order: WadChoose's record is replaced, the Options row is added.
    pause_at, options_at = debug['at'], opt['at'] + opt['size']
    deltas = [(debug['at'] + debug['size'], n - debug['size']), (options_at, n)]  # (first offset it moves, bytes)

    def final(at):
        """Where the old record starting at `at` ends up."""
        return at + sum(delta for end, delta in deltas if end <= at)

    pause_ref, options_ref = pause_at - 4, options_at + deltas[0][1] - 4  # state refs of the new records
    fref = lambda record: final(record['at']) - 4  # noqa: E731

    # WadChoose's code is dead once its record is replaced: nothing else may use it.
    refs = s.code_refs()
    own = [v for k, v in debug['items'] if k in (1, 2, 3)]
    start, end = min(own), next(r for r in refs if r > max(own))
    if any(start <= r < end for r in refs if r not in own):
        raise ValueError('WadChoose code is shared')
    # No call into it from elsewhere (a 0x4a byte that is not an opcode can only refuse too much).
    for i in range(base + s.lit, base + s.symtab - 5):
        if work[i] == 0x4a and not start <= i - base - s.lit < end:
            target = i - base - s.lit + 5 + struct.unpack_from('>i', work, i + 1)[0]
            if start <= target < end:
                raise ValueError('WadChoose code is called from elsewhere')
    label_bytes, _ = resolve_label(language)
    pause_code, pause_pieces = row_code(s, quit_, start, up=fref(quit_), down=fref(cont),
                                        slot=b'menuItem5Select', label=label_bytes)
    options_start = start + len(pause_code)
    options_code, options_pieces = row_code(s, quit_, options_start, up=fref(dsp), down=fref(ctl),
                                            slot=b'menuItem4Select', label=False)
    failsafe_start = options_start + len(options_code)
    failsafe, redirects = failsafe_code(s, failsafe_start, final)
    if failsafe_start + len(failsafe) > end:
        raise ValueError('no room for the new rows')

    # Edits in the layout as it is now. First every goto to a state that moves (or to WadChoose,
    # which the pause row replaces), then the rows' own wiring.
    edits = []
    spans = s.spans()
    for a, b in spans:
        if start <= a < end:  # WadChoose's dead code: the new code goes over it
            continue
        for at, op, arg in decode(work, base + s.lit + a, base + s.lit + b):
            if op == 0x47:
                target = int.from_bytes(arg, 'big') + 4
                moved = pause_ref if target == debug['at'] else final(target) - 4
                if moved != target - 4:
                    edits.append((at + 1, be(moved)))
    edits += [retarget(s, cont, UP, quit_['at'] - 4, pause_ref), retarget(s, quit_, DOWN, cont['at'] - 4, pause_ref),
              retarget(s, ctl, UP, dsp['at'] - 4, options_ref), retarget(s, dsp, DOWN, ctl['at'] - 4, options_ref),
              (blank_line(s, main, 6), be(start, 2)),  # PauseMain clears line5 (local 6): the label goes there
              (blank_line(s, opt, 5), be(start, 2)),   # PauseOptions clears line4 (local 5)
              (base + s.lit + start, pause_code + options_code + failsafe)]
    for record, ref, at in redirects.values():  # the FailSafe states run their new blocks
        item = next(i for i, (k, v) in enumerate(record['items']) if k == 3 and v == ref)
        edits.append((base + record['at'] + 12 + 4 * item, be(0x03000000 | at)))
    parents = {r['at']: r for r in s.records}
    grow = {}  # sizes of the subtrees the new records join (+) and the replaced one leaves (-)

    def chain(record):
        out = [record]
        while out[-1]['parent'] + 4 in starts:
            out.append(parents[out[-1]['parent'] + 4])
        return out
    for record, sign, amount in ((main, 1, n), (opt, 1, n), (parents[debug['parent'] + 4], -1, debug['size'])):
        for r in chain(record):
            grow[r['at']] = grow.get(r['at'], 0) + sign * amount
    edits += [(base + at, be(0x04000000 | (parents[at]['size'] + g))) for at, g in grow.items() if g]
    for r in s.records:
        if r['at'] != debug['at'] and r['parent'] + 4 in starts and final(r['parent'] + 4) != r['parent'] + 4:
            edits.append((base + r['at'] + 4, be(final(r['parent'] + 4) - 4)))
    for at, value in edits:
        work[at:at + len(value)] = value

    old_size = struct.unpack_from('<I', work, base - 0x40 + 4)[0]
    grown = sum(delta for _end, delta in deltas)
    work[base + options_at:base + options_at] = row_record(fref(opt), dsp['id'], options_pieces)
    work[base + pause_at:base + pause_at + debug['size']] = row_record(fref(main), debug['id'], pause_pieces)
    for offset in (0x08, 0x0c, 0x10):  # the script's size, code start and symbol table
        struct.pack_into('<I', work, base + offset, struct.unpack_from('<I', work, base + offset)[0] + grown)
    struct.pack_into('<I', work, base - 0x40 + 4, old_size + grown)
    work[base + old_size + grown:base + old_size + grown + -old_size % 16] = bytes(-(old_size + grown) % 16)
    spans = display_health_bar(work, [(a, b) for a, b in spans if not start <= a < end]
                              + [(pause_pieces['entry'], failsafe_start + len(failsafe))])
    native_cheats(work, spans)
    data[:] = work


def build(game, out, source=None, language=None):
    """{guest path: patched copy} for run.py to mount, from `source` (another patched copy) or the
    game's own R_UI.WAD; {} when the script is not the one this patch knows. The copy in
    out/pause_menu is kept until the source or language changes."""
    source = Path(source or Path(game) / WAD)
    target = Path(out) / 'pause_menu'
    copy = target / Path(WAD).name
    if not source.is_file():
        return {}
    _label_bytes, lang_key = resolve_label(language)
    stamp = json.dumps([VERSION, str(source), source.stat().st_size, source.stat().st_mtime_ns, lang_key])
    stamp_file = target / 'stamp.json'
    if not (stamp_file.is_file() and stamp_file.read_text() == stamp):
        shutil.rmtree(target, ignore_errors=True)
        target.mkdir(parents=True)
        data = bytearray(source.read_bytes())
        try:
            patch(data, language=lang_key)
        except ValueError as error:
            print(f'Pause menu: {error}; no Exit to Desktop row', flush=True)
            return {}
        copy.write_bytes(data)
        stamp_file.write_text(stamp)  # last: an interrupted build is redone
    if not copy.is_file():
        return {}
    return {'/app0/' + WAD: copy}
