# SPDX-License-Identifier: GPL-2.0-or-later
"""The Weapon Switching tutorials for the switch attacks (SPARTAN_SWITCH_ATTACKS=1, src/switch_hook.h).

With the option, L1 + Cross no longer cycles through the weapons: it repeats the switch attack of
the weapon in Kratos' hands, and L1 with a d-pad direction switches to that weapon with its switch
attack. The tutorial cards (texts 190, 283 and 286: a title, "Press <direction> for <weapon>"
lines, then the L1 + Cross sentence) say so instead of the old sentence. build() writes patched
copies of the language WADs and returns the guest paths run.py mounts them on; the game folder is
never written.

The text is the MSGS_TXT entry of each R_LANG_<code>.WAD, UTF-8: CRLF lines, `*<id>*` (with a T
for a tutorial) before each text. In each card the lines from the first one with [L1Button] to the
end of the card are replaced, so the patch does not depend on how a language worded or wrapped the
old sentence. The entry grows (its size field, its padding to 16, and everything after it moves).
A language is left as it is when its text lacks one of the new characters: each language's font
only has the characters its own text uses."""
import json
from pathlib import Path
import re
import shutil
import struct

from glyphs import entries

VERSION = 2  # of the text: a change rewrites the cached copies
FOLDER = 'EXEC/WAD/PS4_LE'
CARDS = (190, 283, 286)
# R_LANG_<code>.WAD -> the lines that end the cards.
ENGLISH = ('Hold [L1Button] to attack when switching weapons.',
           'Use [L1Button] and [XButton] to repeat switch attacks.')
SPANISH = ('Mantén [L1Button] para atacar al cambiar de arma.',
           'Usa [L1Button] y [XButton] para repetir el ataque de cambio.')
TEXT = {
    'EN': ENGLISH, 'UK': ENGLISH,
    'DE': ('Halte [L1Button] gedrückt, um beim Waffenwechsel anzugreifen.',
           'Drücke [L1Button] und [XButton], um Wechselangriffe zu wiederholen.'),
    'FR': ('Maintenez [L1Button] pour attaquer en changeant d\'arme.',
           'Utilisez [L1Button] et [XButton] pour répéter l\'attaque de changement.'),
    'IT': ('Tieni premuto [L1Button] per attaccare mentre cambi arma.',
           'Usa [L1Button] e [XButton] per ripetere l\'attacco di cambio.'),
    'ES': SPANISH, 'LA': SPANISH,
    'PT': ('Mantém [L1Button] premido para atacar ao trocar de arma.',
           'Usa [L1Button] e [XButton] para repetir o ataque de troca.'),
    'BR': ('Segure [L1Button] para atacar ao trocar de arma.',
           'Use [L1Button] e [XButton] para repetir o ataque de troca.'),
    'NL': ('Houd [L1Button] vast om aan te vallen bij een wapenwissel.',
           'Gebruik [L1Button] en [XButton] om wisselaanvallen te herhalen.'),
    'PL': ('Przytrzymaj [L1Button], aby zaatakować przy zmianie broni.',
           'Użyj [L1Button] i [XButton], aby powtórzyć atak zmiany broni.'),
    'RU': ('Удерживайте [L1Button], чтобы атаковать при смене оружия.',
           'Нажмите [L1Button] и [XButton], чтобы повторить атаку смены оружия.'),
    'TR': ('Silah değiştirirken saldırmak için [L1Button] tuşunu basılı tut.',
           'Değiştirme saldırısını tekrarlamak için [L1Button] ve [XButton] kullan.'),
    'JA': ('[L1Button]を押しながら武器を切り替えると攻撃します。',
           '[L1Button]と[XButton]で切り替え攻撃を繰り返します。'),
    'KO': ('[L1Button] 버튼을 누른 채 무기를 바꾸면 공격합니다.',
           '[L1Button] 버튼과 [XButton] 버튼으로 전환 공격을 반복합니다.'),
    'ZH': ('按住[L1Button]切換武器時會發動攻擊。',
           '使用[L1Button]和[XButton]重複切換攻擊。'),
}
CARD = re.compile(r'\r\n\*(\d+)\*T?\r\n')


def rewrite(text, lines):
    """The MSGS_TXT text with the cards ending in `lines`, and how many cards changed; (text, 0)
    when no card has an [L1Button] line, or the language's text lacks a character of `lines`."""
    used = set(text)
    if any(c not in used for line in lines for c in line):
        return text, 0
    marks = list(CARD.finditer(text))
    out, at, count = [], 0, 0
    for mark, following in zip(marks, marks[1:] + [None]):
        if int(mark.group(1)) not in CARDS:
            continue
        end = following.start() if following else len(text)
        body = text[mark.end():end].split('\r\n')
        first = next((i for i, line in enumerate(body) if '[L1Button]' in line), None)
        if first is None:
            continue
        out += [text[at:mark.end()], '\r\n'.join(body[:first] + list(lines))]
        at, count = end, count + 1
    return ''.join(out) + text[at:], count


def patch(data, lines):
    """Ends the Weapon Switching cards of the WAD in `data` (a bytearray) with `lines`, growing its
    MSGS_TXT entry; returns how many cards changed."""
    for offset, size, name in entries(data):
        if name != 'MSGS_TXT':
            continue
        start, end = offset + 0x40, offset + 0x40 + size
        try:
            text = bytes(data[start:end]).decode('utf-8')
        except UnicodeDecodeError:
            return 0
        text, count = rewrite(text, lines)
        if count:
            raw = text.encode('utf-8')
            data[start:end + -size % 16] = raw + bytes(-len(raw) % 16)
            struct.pack_into('<I', data, offset + 4, len(raw))
        return count
    return 0


def build(game, out):
    """{guest path: patched copy} for run.py to mount. The copies in out/switch_text are kept until
    the game's language WADs change."""
    target = Path(out) / 'switch_text'
    sources = [(f'{FOLDER}/R_LANG_{code}.WAD', Path(game) / FOLDER / f'R_LANG_{code}.WAD') for code in TEXT]
    sources = [(name, path) for name, path in sources if path.is_file()]
    stamp = json.dumps([VERSION, [(name, path.stat().st_size, path.stat().st_mtime_ns) for name, path in sources]])
    stamp_file = target / 'stamp.json'
    made = sorted(path.name for path in target.glob('*.WAD')) if target.is_dir() else []
    if not (stamp_file.is_file() and stamp_file.read_text() == stamp):
        shutil.rmtree(target, ignore_errors=True)
        target.mkdir(parents=True)
        made = []
        for name, path in sources:
            data = bytearray(path.read_bytes())
            if patch(data, TEXT[path.stem[len('R_LANG_'):]]):
                (target / path.name).write_bytes(data)
                made.append(path.name)
            else:
                print(f'Switch attacks: {path.name} keeps its tutorial text', flush=True)
        stamp_file.write_text(stamp)  # last: an interrupted build is redone
    return {'/app0/' + name: target / Path(name).name for name, _path in sources if Path(name).name in made}
