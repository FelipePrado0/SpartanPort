# SPDX-License-Identifier: GPL-2.0-or-later
"""Controller button glyphs for the game's own prompts (tutorials, move list, QTE buttons) and the
button discs of the Muses' lyre in the Chamber of the Flame.

The prompts are DDS textures in R_UI.WAD (one more copy of the QTE circle in R_PERM.WAD); the lyre's
discs are a corner of five level textures in OLY150.WAD (a gold mechanism with the PlayStation
shapes shares that corner in two more). build() writes patched copies of those WADs,
with the glyphs of an Xbox or Nintendo controller (or the keyboard keys of SPARTAN_KEY_MAP) drawn over
the PlayStation ones, and returns the guest paths run.py mounts them on. The lyre keeps its letters
with the keyboard style: its keys are whatever the player mapped, and the discs are part of a
puzzle, not a prompt. The game folder is never
written: a texture keeps its size and format, so only its pixels change inside the copy.

WAD (little-endian): entries of a 0x40 header (u16 tag, u16 flags, u32 size, name[56]) and `size`
bytes padded to 16 (the EntityCount marker of level WADs, tag 0, carries a count in its size field
and no data). A texture entry is named TX<hash>.<hash>.dds and holds a short prefix and a plain DDS
file (DXT1 or DXT5), which the materials reference by that name."""
import io
import json
import math
from pathlib import Path
import shutil
import struct

VERSION = 11  # of the drawing: a change redraws the cached copies
WADS = ('EXEC/WAD/PS4_LE/R_UI.WAD', 'EXEC/WAD/PS4_LE/R_PERM.WAD', 'EXEC/WAD/PS4_LE/OLY150.WAD')

FONT = Path(__file__).resolve().parent.parent / 'launcher' / 'Cinzel-Black.ttf'
# The lyre's letters are a Didone (the game sets Χ Ο Π Δ in one) with sturdy hairlines: Bodoni Moda
# (OFL) at its smallest optical size, thickened by LYRE_STROKE pixels to the original's stem weights.
LYRE_FONT = Path(__file__).resolve().parent.parent / 'launcher' / 'BodoniModa.ttf'
CONTROLLER_TEXTURE = 'TXCE352DC5'
# GPL-2.0-or-later controller drawings by Kristian Andreev, adapted from PR #234.
CONTROLLER_ART = Path(__file__).resolve().parent.parent / 'scripts' / 'controller_diagrams'

# Texture (the first hash of its name) -> how it is drawn and the game button(s) it shows.
# inline: the 64 px icons inside text; qte: the large prompt on black; atlas: two QTE labels,
# the first upside down in the top half; lyre: the Muses' lyre discs in a corner of a level
# texture (diffuse, specular, decal, normal, glow), repainted in place; gold: the shapes
# engraved into the gold mechanism's discs in the same corner (its diffuse and specular map).
TEXTURES = {
    'TX1BBACF4D': ('inline', 'cross'), 'TXBA40AFD2': ('inline', 'circle'),
    'TX64F3D812': ('inline', 'square'), 'TX573C282A': ('inline', 'triangle'),
    'TX91A47980': ('inline', 'l1'), 'TX5CED5AE9': ('inline', 'r1'),
    'TX6FC83ACE': ('inline', 'l2'), 'TX67E86446': ('inline', 'r2'),
    'TX9AE3BA81': ('inline', 'l3'), 'TXFB33513A': ('inline', 'r3'),
    'TX192AE9D1': ('inline', 'options'), 'TXD43D1443': ('inline', 'touchpad_right'),
    'TX7FF61BF7': ('qte', 'cross'), 'TX9D590CC8': ('qte', 'circle'),
    'TX1F5FC983': ('qte', 'square'), 'TXE8889579': ('qte', 'triangle'),
    'TXFB91136E': ('atlas', ('l2', 'l1')), 'TX0FFF3CCB': ('atlas', ('r2', 'r1')),
    'TX2CDFA5F8': ('lyre', 'diffuse'), 'TX4158CBF9': ('lyre', 'specular'), 'TX0C09B6DB': ('lyre', 'decal'),
    'TXF489D97F': ('lyre', 'normal'), 'TXEACA5E71': ('lyre', 'glow'),
    'TXBD54B283': ('gold', 'diffuse'), 'TXCEE0EE2B': ('gold', 'specular'),
}
# The lyre: the top-right 512 px of the 2048 px level textures (the glow mask is 512 px, the same
# corner at a quarter), four discs of radius 110 at these centres, one game button each. The
# letters are Didone capitals of 104 px cap height carved into dark marble: dim in the diffuse,
# light grey with a dark rim in the specular map, lit in the decal (their colour on black, which
# the game draws over the discs) and the glow mask, and raised in the normal map (DXT5, X in alpha
# and Y in green). The gold mechanism shares the corner and the centres: its discs are gold, 83 px,
# and show the PlayStation shapes as a 7 px line engraved into them.
LYRE_CORNER = (1536, 0, 2048, 512)
LYRE_DISCS = {'cross': (167, 110), 'circle': (395, 110), 'square': (167, 335), 'triangle': (395, 333)}
LYRE_RADIUS = 110
LYRE_FORMS = {'lyre': {'inner': 0.76, 'cap': 104, 'dark': False}, 'gold': {'inner': 0.68, 'cap': 80, 'dark': True}}
LYRE_DIM, LYRE_DECAL, LYRE_SHINE, LYRE_RIM, LYRE_STROKE, LYRE_RELIEF = 0.62, 0.83, 190, 80, 3, 1.2
GOLD_LINE = 8

# Drawn only with the keyboard style: the stick icons (the left one also unlettered) and the D-pad
# icon, whose gamepad look already fits every controller.
KEYBOARD_TEXTURES = {
    'TXDCA1B818': ('inline', 'lstick'), 'TXE2BB1428': ('inline', 'lstick'),
    'TX77AAD542': ('inline', 'rstick'), 'TXBF2035D0': ('inline', 'up'),
}
# The gamepad button (SDL name) that presses each game button (src/runtime_pad.c pad_map).
DEFAULT_MAP = {'cross': 'a', 'circle': 'b', 'square': 'x', 'triangle': 'y', 'l1': 'leftshoulder',
               'r1': 'rightshoulder', 'l2': 'lefttrigger', 'r2': 'righttrigger', 'l3': 'leftstick',
               'r3': 'rightstick', 'options': 'start', 'touchpad_right': 'back'}
# The key (SDL scancode name) that presses each game button (src/runtime_pad.c key_map).
DEFAULT_KEYS = {'cross': 'Space', 'circle': 'Left Shift', 'square': 'E', 'triangle': 'Q', 'l1': '1', 'r1': '3',
                'l2': 'R', 'r2': 'F', 'l3': 'Z', 'r3': 'C', 'options': 'Return', 'touchpad_right': 'Backspace',
                'up': 'I'}
# The keys that move the sticks (src/runtime_pad.c key_axis). ponytail: fixed, as in the runtime;
# read them from SPARTAN_KEY_MAP once movement can be remapped.
STICK_KEYS = {'lstick': 'WASD', 'rstick': 'Arrows'}
# Key caps for the longer SDL key names (casefolded); the others print their name in capitals.
KEY_LABELS = {'space': 'SPACE', 'left shift': 'SHIFT', 'right shift': 'R SHIFT', 'left ctrl': 'CTRL',
              'right ctrl': 'R CTRL', 'left alt': 'ALT', 'right alt': 'ALT GR', 'left gui': 'WIN',
              'right gui': 'WIN', 'return': 'ENTER', 'keypad enter': 'ENTER', 'return2': 'ENTER',
              'backspace': 'BKSP', 'tab': 'TAB', 'escape': 'ESC', 'capslock': 'CAPS', 'delete': 'DEL',
              'insert': 'INS', 'pageup': 'PG UP', 'pagedown': 'PG DN', 'home': 'HOME', 'end': 'END',
              'up': 'dpup', 'down': 'dpdown', 'left': 'dpleft', 'right': 'dpright',
              'mouse left': 'LMB', 'mouse right': 'RMB', 'mouse middle': 'MMB', 'mouse back': 'M4',
              'mouse forward': 'M5', 'wheel up': 'WH UP', 'wheel down': 'WH DN', 'arrows': 'arrows'}
POSITIONS = {'a': 'south', 'b': 'east', 'x': 'west', 'y': 'north'}  # SDL names are positions
STYLES = {
    'xbox': {'faces': {'south': 'a', 'east': 'b', 'west': 'x', 'north': 'y'},
             'colors': {'a': (108, 196, 74), 'b': (232, 72, 66), 'x': (66, 142, 232), 'y': (244, 196, 48)},
             'labels': {'leftshoulder': 'LB', 'rightshoulder': 'RB', 'lefttrigger': 'LT', 'righttrigger': 'RT',
                        'leftstick': 'LS', 'rightstick': 'RS', 'start': 'menu', 'back': 'view'}},
    'nintendo': {'faces': {'south': 'b', 'east': 'a', 'west': 'y', 'north': 'x'}, 'colors': {},
                 'labels': {'leftshoulder': 'L', 'rightshoulder': 'R', 'lefttrigger': 'ZL', 'righttrigger': 'ZR',
                            'leftstick': 'LS', 'rightstick': 'RS', 'start': 'plus', 'back': 'minus'}},
    'keyboard': {'faces': {}, 'colors': {}, 'labels': {}},  # key caps: see key_label
}
ICONS = {'menu', 'view', 'plus', 'minus', 'dpup', 'dpdown', 'dpleft', 'dpright', 'arrows'}
INK = (240, 240, 240)
SCALE = 6  # drawn this many times larger, then reduced


def entries(data):
    """(offset, size, name) of each WAD entry; stops at the first one that does not fit."""
    offset = 0
    while offset + 0x40 <= len(data):
        tag, _flags, size = struct.unpack_from('<HHI', data, offset)
        name = bytes(data[offset + 8:offset + 0x40]).split(b'\0')[0].decode('latin1')
        if tag == 0 and name == 'EntityCount':
            size = 0
        if offset + 0x40 + size > len(data):
            return
        yield offset, size, name
        offset += 0x40 + ((size + 15) & ~15)


def pad_buttons(pad_map):
    """Game button -> gamepad button, with a SPARTAN_PAD_MAP value ('cross=b,circle=a') applied.
    L2/R2 stay on the triggers, as in the runtime."""
    buttons = dict(DEFAULT_MAP)
    for entry in (pad_map or '').split(','):
        game, _, host = (part.strip().casefold() for part in entry.partition('='))
        if game in buttons and host and game not in ('l2', 'r2'):
            buttons[game] = host
    return buttons


def key_buttons(key_map):
    """Game button -> keyboard key, with a SPARTAN_KEY_MAP value ('cross=Q,circle=Left Shift') applied.
    Unlike the gamepad, L2/R2 are keys too. 'None' (a button left without a key) keeps the default,
    except L3/R3 with Rage of Sparta on one input: they show the rage key, which presses both."""
    buttons, unbound, rage = {**DEFAULT_KEYS, **STICK_KEYS}, set(), ''
    for entry in (key_map or '').split(','):
        game, _, host = (part.strip() for part in entry.partition('='))
        game = game.casefold()
        if game == 'rage':
            rage = host if host.casefold() != 'none' else ''
        elif game in buttons and host.casefold() == 'none':
            unbound.add(game)
        elif game in buttons and host:
            buttons[game] = host
    for game in unbound & {'l3', 'r3'} if rage else ():
        buttons[game] = rage
    return buttons


def key_label(name):
    """What a key cap shows for an SDL key name: 'Left Shift' -> 'SHIFT', 'Up' -> the dpup arrow,
    'Keypad 4' -> 'NUM 4', 'e' -> 'E'."""
    key = name.strip().casefold()
    if key in KEY_LABELS:
        return KEY_LABELS[key]
    if key.startswith('keypad '):
        return 'NUM ' + name.strip()[7:].upper()
    return name.strip().upper()


def glyph(host, style, faces=None):
    """(shape, label, color) of a gamepad button: a letter or an ICONS name on a disc or a plate.
    `faces` are the letters this controller prints on south/east/west/north. With the keyboard
    style `host` is a key name, drawn on a square cap (one or two characters) or a wide one."""
    if style == 'keyboard':
        label = key_label(host)
        return ('key' if len(label) <= 2 or label in ICONS else 'plate'), label, INK
    look = STYLES[style]
    if host in POSITIONS:
        letter = (faces or {}).get(POSITIONS[host])
        if letter not in ('a', 'b', 'x', 'y'):
            letter = look['faces'][POSITIONS[host]]
        return 'disc', letter.upper(), look['colors'].get(letter, INK)
    label = look['labels'].get(host) or (host if host in ICONS else host[:2].upper())
    round_button = host.endswith('stick') or label in ('plus', 'minus')
    return ('disc' if round_button else 'plate'), label, INK


def parse_gamepad(output):
    """(style, faces, name) from the line `spartanport-probe --gamepad` prints, None without a gamepad."""
    for line in output.splitlines():
        if line.startswith('Gamepad: type='):
            fields, _, name = line[len('Gamepad: '):].partition(' name=')
            values = dict(field.split('=', 1) for field in fields.split() if '=' in field)
            kind = values.get('type', '')
            style = ('playstation' if kind.startswith('ps') else
                     'nintendo' if kind.startswith(('switch', 'joycon', 'gamecube')) else 'xbox')
            return style, {position: values.get(position) for position in POSITIONS.values()}, name.strip()
    return None


def resolve(choice, probe_output=''):
    """(style, faces, controller name) for a SPARTAN_GLYPH_STYLE value. 'auto' follows the connected
    gamepad (the keyboard keys without one); a chosen style uses its own letters."""
    if choice in STYLES:
        return choice, None, ''
    if choice != 'auto':
        return 'playstation', None, ''
    return parse_gamepad(probe_output) or ('keyboard', None, '')


def draw_label(image, box, label, color):
    """Draws a letter label or an ICONS shape centered in box."""
    from PIL import ImageDraw, ImageFont
    draw = ImageDraw.Draw(image)
    x0, y0, x1, y1 = box
    cx, cy, r = (x0 + x1) / 2, (y0 + y1) / 2, min(x1 - x0, y1 - y0) / 2
    t = max(1, r * 0.26)
    if label in ('plus', 'minus', 'menu'):
        for row in ((-0.55, 0, 0.55) if label == 'menu' else (0,)):
            draw.rounded_rectangle((cx - r * 0.8, cy + row * r - t / 2, cx + r * 0.8, cy + row * r + t / 2), t / 3, fill=color)
        if label == 'plus':
            draw.rounded_rectangle((cx - t / 2, cy - r * 0.8, cx + t / 2, cy + r * 0.8), t / 3, fill=color)
    elif label == 'view':
        for dx, dy in ((-0.25, -0.2), (0.25, 0.2)):
            draw.rectangle((cx + (dx - 0.55) * r, cy + (dy - 0.45) * r, cx + (dx + 0.55) * r, cy + (dy + 0.45) * r),
                           outline=color, width=int(t * 0.7))
    elif label in ICONS:  # a d-pad direction: an arrow head; arrows: the four of them, small
        heads = ([(a, 0.5) for a in ('dpup', 'dpdown', 'dpleft', 'dpright')] if label == 'arrows' else [(label, 1)])
        for name, k in heads:
            ax, ay = {'dpup': (0, -1), 'dpdown': (0, 1), 'dpleft': (-1, 0), 'dpright': (1, 0)}[name]
            ox, oy = cx + ax * r * (1 - k), cy + ay * r * (1 - k)
            draw.polygon([(ox + ax * r * 0.8 * k, oy + ay * r * 0.8 * k),
                          (ox - ax * r * 0.5 * k - ay * r * 0.7 * k, oy - ay * r * 0.5 * k - ax * r * 0.7 * k),
                          (ox - ax * r * 0.5 * k + ay * r * 0.7 * k, oy - ay * r * 0.5 * k + ax * r * 0.7 * k)], fill=color)
    else:
        size = int(y1 - y0)
        while True:
            try:
                font = ImageFont.truetype(str(FONT), size)
            except OSError:  # the launcher's font is missing: Pillow's own
                font = ImageFont.load_default(size)
            left, top, right, bottom = draw.textbbox((0, 0), label, font=font)
            if (right - left <= x1 - x0 and bottom - top <= y1 - y0) or size <= 6:
                break
            size = int(size * 0.92)
        draw.text((cx - (left + right) / 2, cy - (top + bottom) / 2), label, font=font, fill=color)


def draw_inline(shape, label, color, width, height):
    """An icon as the game draws them inside text: a dark button with the label on it."""
    from PIL import Image, ImageDraw
    w, h, k = width * SCALE, height * SCALE, SCALE
    image = Image.new('RGBA', (w, h), (0, 0, 0, 0))
    if shape in ('disc', 'key'):
        d = min(w, h) - 2 * k
        box = ((w - d) // 2, (h - d) // 2, (w + d) // 2, (h + d) // 2)
    else:
        bh = min(h - 2 * k, 36 * k)
        box = (2 * k, (h - bh) // 2, w - 2 * k, (h + bh) // 2)
    size = (box[2] - box[0], box[3] - box[1])
    shade = Image.linear_gradient('L').resize(size)  # lit from above
    body = Image.merge('RGB', [shade.point(lambda v, a=a, b=b: a + (b - a) * v // 255) for a, b in ((84, 26), (84, 26), (86, 28))])
    mask = Image.new('L', size, 0)
    outline = (ImageDraw.Draw(mask).ellipse if shape == 'disc' else
               lambda area, **style: ImageDraw.Draw(mask).rounded_rectangle(area, 5 * k, **style))
    outline((0, 0, size[0] - 1, size[1] - 1), fill=255)
    image.paste(body, box[:2], mask)
    edge = ImageDraw.Draw(image)
    if shape == 'disc':
        edge.ellipse(box, outline=(14, 14, 14, 255), width=k * 3 // 2)
    else:
        edge.rounded_rectangle(box, 5 * k, outline=(14, 14, 14, 255), width=k * 3 // 2)
    # A key name (SPACE, SHIFT, M4) is wider than a face letter: on a 64 px icon it needs most of the cap.
    inset_x = size[0] * (0.2 if shape == 'disc' else 0.12 if shape == 'key' else 0.1)
    inset_y = size[1] * (0.2 if shape == 'disc' else 0.14 if shape == 'key' else 0.16)
    draw_label(image, (box[0] + inset_x, box[1] + inset_y, box[2] - inset_x, box[3] - inset_y), label, color)
    return image.resize((width, height), Image.LANCZOS)


def draw_prompt(label, color, width, height, cap=False):
    """A QTE label: bright on black (the game adds these textures to the picture). `cap` frames it
    as a keyboard key."""
    from PIL import Image, ImageDraw, ImageFilter
    w, h = width * SCALE, height * SCALE
    layer = Image.new('RGBA', (w, h), (0, 0, 0, 0))
    if cap:
        side = min(w, h)
        # The cap fills the prompt, leaving room for the glow, so the key reads at a glance mid-fight.
        ImageDraw.Draw(layer).rounded_rectangle((w * 0.06, h * 0.06, w * 0.94, h * 0.94), side * 0.1,
                                                outline=color, width=max(1, int(side * 0.035)))
        draw_label(layer, (w * 0.15, h * 0.22, w * 0.85, h * 0.78), label, color)
    else:
        draw_label(layer, (w * 0.24, h * 0.2, w * 0.76, h * 0.8), label, color)
    image = Image.new('RGBA', (w, h), (0, 0, 0, 255))
    image.alpha_composite(layer.filter(ImageFilter.GaussianBlur(SCALE * 2.5)))
    image.alpha_composite(layer)
    return image.resize((width, height), Image.LANCZOS)


def render(form, buttons, style, faces, pad_map, width, height, key_map=''):
    """The picture that replaces a texture of TEXTURES, at its size."""
    from PIL import Image
    if form == 'controller':
        with Image.open(CONTROLLER_ART / f'{style}.png') as image:
            return image.convert('RGBA').resize((width, height), Image.Resampling.LANCZOS)
    keyboard = style == 'keyboard'
    hosts = key_buttons(key_map) if keyboard else pad_buttons(pad_map)
    if form == 'atlas':
        image = Image.new('RGBA', (width, height), (0, 0, 0, 255))
        # In the shoulder-button atlas, prompts are centered ~52 px from the midline (y=128),
        # not in the middle of each 128 px half (y=64), matching the game's HUD quad alignment.
        offset = int(height * 12 / 256)
        for row, button in enumerate(buttons):
            half = Image.new('RGBA', (width, height // 2), (0, 0, 0, 255))
            half.paste(draw_prompt(glyph(hosts[button], style, faces)[1], INK, width, height // 2, keyboard),
                       (0, -offset))
            image.paste(half.rotate(180) if row == 0 else half, (0, row * (height // 2)))
        return image
    shape, label, color = glyph(hosts[buttons], style, faces)
    return (draw_inline(shape, label, color, width, height) if form == 'inline' else
            draw_prompt(label, color, width, height, keyboard))


def lyre_letter(size, label, centre, cap, color, scale=4, margin=0):
    """An RGBA layer of `size` with `label` in the lyre's face, `cap` pixels of cap height (a
    longer label shrinks to the disc), centred on `centre`, `margin` pixels fatter."""
    from PIL import Image, ImageDraw, ImageFont
    big = Image.new('RGBA', (size[0] * scale, size[1] * scale), (0, 0, 0, 0))
    draw = ImageDraw.Draw(big)
    px = cap * scale
    try:
        font = ImageFont.truetype(str(LYRE_FONT), px)
        left, top, right, bottom = draw.textbbox((0, 0), 'H', font=font)
        px = int(px * px / (bottom - top))  # the cap height of H is `cap`
        width_limit = LYRE_RADIUS * 1.5 * scale
        stroke = (LYRE_STROKE + margin) * scale
        while True:
            font = ImageFont.truetype(str(LYRE_FONT), px)
            left, top, right, bottom = draw.textbbox((0, 0), label, font=font, stroke_width=stroke)
            if right - left <= width_limit or px <= 8 * scale:
                break
            px = int(px * 0.9)
    except OSError:  # the font is missing: Pillow's own
        font, stroke = ImageFont.load_default(px), 0
        left, top, right, bottom = draw.textbbox((0, 0), label, font=font)
    draw.text((centre[0] * scale - (left + right) / 2, centre[1] * scale - (top + bottom) / 2), label, font=font,
              fill=tuple(color) + (255,), stroke_width=stroke, stroke_fill=tuple(color) + (255,))
    return big.resize(size, Image.LANCZOS)


def gold_letter(size, label, centre, cap, width, color, scale=4):
    """An RGBA layer of `size` with `label` as a line of `width` pixels with round ends, like the
    shapes engraved into the gold discs: A, B, X and Y are drawn; another label is set in the
    lyre's face."""
    from PIL import Image, ImageDraw
    height = cap
    wide = 0.8 * height
    strokes = {
        'X': [[(0, 0), (wide, height)], [(wide, 0), (0, height)]],
        'A': [[(0, height), (wide / 2, 0), (wide, height)], [(wide * 0.2, height * 0.64), (wide * 0.8, height * 0.64)]],
        'Y': [[(0, 0), (wide / 2, height / 2), (wide, 0)], [(wide / 2, height / 2), (wide / 2, height)]],
        'B': [[(0, 0), (0, height)]],
    }.get(label)
    if strokes is None:
        return lyre_letter(size, label, centre, cap, color, scale, margin=-LYRE_STROKE)
    if label == 'B':
        wide = height * 0.62
    big = Image.new('RGBA', (size[0] * scale, size[1] * scale), (0, 0, 0, 0))
    draw = ImageDraw.Draw(big)
    fill = tuple(color) + (255,)
    thick = width * scale

    def at(point):
        return ((centre[0] - wide / 2 + point[0]) * scale, (centre[1] - height / 2 + point[1]) * scale)

    def dot(point):
        x, y = at(point)
        draw.ellipse((x - thick / 2, y - thick / 2, x + thick / 2, y + thick / 2), fill=fill)

    for stroke in strokes:
        draw.line([at(point) for point in stroke], fill=fill, width=int(thick), joint='curve')
        dot(stroke[0])
        dot(stroke[-1])
    if label == 'B':  # two bowls on the stem: a straight top, a half circle, a straight return
        for top, reach in ((0, height * 0.28), (height / 2, height * 0.32)):
            radius = height / 4
            draw.line([at((0, top)), at((reach, top))], fill=fill, width=int(thick))
            draw.line([at((0, top + 2 * radius)), at((reach, top + 2 * radius))], fill=fill, width=int(thick))
            x, y = at((reach, top + radius))
            draw.arc((x - radius * scale, y - radius * scale, x + radius * scale, y + radius * scale), -90, 90,
                     fill=fill, width=int(thick))
    return big.resize(size, Image.LANCZOS)


def lyre_level(band, mask, fraction):
    """The value below which `fraction` of the pixels of `band` (an L image) under `mask` fall."""
    histogram = band.histogram(mask=mask)
    wanted, seen = sum(histogram) * fraction, 0
    for value, count in enumerate(histogram):
        seen += count
        if seen >= wanted:
            return value
    return 255


def lyre_letter_mask(disc, inner, dark=False, enclosed=True):
    """An L mask of the letter on `disc` (a square around one disc) and its carved edge, a 5 px
    margin: the pixels inside `inner` brighter (`dark`: darker) than the disc's own interior,
    which is the larger part of it, in the brightest channel. An engraved shape (`dark`) takes
    what it encloses with it, unless `enclosed` is off."""
    from PIL import ImageChops, ImageDraw, ImageFilter
    red, green, blue = disc.split()[:3]
    bright = ImageChops.lighter(red, ImageChops.lighter(green, blue))
    interior = lyre_level(bright, inner, 0.5)
    if dark:
        threshold = interior - (interior - lyre_level(bright, inner, 0.001)) * 0.35
        mask = bright.point(lambda v: 255 if v < threshold else 0)
    else:
        threshold = interior + (lyre_level(bright, inner, 0.999) - interior) * 0.35
        mask = bright.point(lambda v: 255 if v > threshold else 0)
    mask = ImageChops.multiply(ImageChops.multiply(mask, inner).filter(ImageFilter.MaxFilter(11)), inner)
    if dark and enclosed:
        outside = ImageChops.lighter(mask, ImageChops.invert(inner))
        left, top, right, bottom = inner.getbbox()
        # flood from clear points towards the edge, off the diagonals, in every part of the disc
        # the shape's lines divide; what stays unreached is enclosed
        for reach, angle in ((f * (right - left) / 2, a) for f in (0.9, 0.85, 0.8) for a in (15, 75, 105, 165, 195, 255, 285, 345)):
            seed = (int(disc.width / 2 + reach * math.cos(math.radians(angle))),
                    int(disc.height / 2 - reach * math.sin(math.radians(angle))))
            if not outside.getpixel(seed):
                ImageDraw.floodfill(outside, seed, 128)
        mask = ImageChops.lighter(mask, outside.point(lambda v: 255 if v == 0 else 0))
    return mask


def lyre_fill(disc, mask, inner, others, slide=None):
    """`disc` with the pixels under `mask` replaced by the disc's own interior texture: taken
    from `others`, the other discs (each with its mask), matched to this disc's shading, where
    they show no letter, then from the disc turned about its centre, then from the median
    colour. `slide`, the mask of the letter alone, puts the disc's own texture slid sideways
    first (the engraved gold discs)."""
    from PIL import Image, ImageChops, ImageFilter
    clean = ImageChops.subtract(inner, mask)
    medians = tuple(lyre_level(band, clean, 0.5) for band in disc.split())

    def shading(image, hole):
        """The slow changes of tone across `image`, with `hole` filled by the tone around it."""
        around = ImageChops.subtract(ImageChops.multiply(hole.filter(ImageFilter.MaxFilter(25)), inner), hole)
        flat = image.copy()
        flat.paste(tuple(lyre_level(band, around, 0.5) for band in image.split()), mask=hole)
        return flat.filter(ImageFilter.GaussianBlur(10))

    # another disc's pixels are brought to this disc's shading: lighter centre, darker edge
    tone = shading(disc, mask)
    sources = [(ImageChops.add(other, ImageChops.subtract(tone, shading(other, other_mask), 1, 128), 1, -128), other_mask)
               for other, other_mask in others]
    sources += [(source.transpose(turn), source_mask.transpose(turn))
                for source, source_mask in list(sources) for turn in (Image.ROTATE_90, Image.ROTATE_180, Image.ROTATE_270)]
    sources += [(disc.rotate(angle, Image.BICUBIC), mask.rotate(angle, Image.BICUBIC).point(lambda v: 255 if v else 0))
                for angle in (90, 270, 180, 45, 135, 225, 315, 30, 60, 120, 150, 210, 240, 300, 330)]
    sources.reverse()
    if slide is not None:
        matched = ImageChops.add(disc, ImageChops.subtract(tone, shading(disc, slide), 1, 128), 1, -128)
        for dx, dy in ((40, 0), (-40, 0), (0, 40), (0, -40), (28, 28), (-28, 28), (28, -28), (-28, -28)):
            sources.append((ImageChops.offset(matched, dx, dy),
                            ImageChops.lighter(ImageChops.offset(mask, dx, dy), ImageChops.invert(ImageChops.offset(inner, dx, dy)))))
    filled = disc.copy()
    filled.paste(medians, mask=mask)
    # the best source last, each blended in over the earlier ones with a soft edge
    for source, source_mask in sources:
        usable = ImageChops.subtract(mask, source_mask).filter(ImageFilter.MinFilter(5))
        if usable.getbbox():
            filled = Image.composite(source, filled, ImageChops.multiply(usable.filter(ImageFilter.GaussianBlur(2)), mask))
    result = disc.copy()
    result.paste(filled, mask=mask.filter(ImageFilter.GaussianBlur(1)))
    return result, medians


def lyre_dome(disc, inner, radius):
    """The normal map `disc` (RGBA: X in alpha, Y in green) with the raised letter smoothed away:
    the disc is a dome, so its normals point outward by an amount that depends on the distance
    from the centre; that profile, the median at each distance, replaces the pixels that stray
    from it (the letter's edges, with a margin)."""
    from PIL import Image, ImageChops, ImageFilter
    red, green, blue, alpha = disc.split()
    x_band, y_band = alpha.load(), green.load()
    size = disc.width
    centre = size / 2 - 0.5
    inside = inner.load()
    slant = [[] for _ in range(size)]  # outward tilt by distance, in pixels
    for y in range(size):
        for x in range(size):
            if inside[x, y]:
                dx, dy = x - centre, y - centre
                distance = (dx * dx + dy * dy) ** 0.5
                if distance >= 1:
                    slant[int(distance)].append(((128 - x_band[x, y]) * dx + (y_band[x, y] - 128) * dy) / distance)
    profile = [sorted(values)[len(values) // 2] if values else 0 for values in slant]
    dome_x, dome_y = Image.new('L', disc.size, 128), Image.new('L', disc.size, 128)
    put_x, put_y = dome_x.load(), dome_y.load()
    for y in range(size):
        for x in range(size):
            if inside[x, y]:
                dx, dy = x - centre, y - centre
                distance = (dx * dx + dy * dy) ** 0.5
                if distance >= 1:
                    tilt = profile[int(distance)] / distance
                    put_x[x, y] = max(0, min(255, round(128 - tilt * dx)))
                    put_y[x, y] = max(0, min(255, round(128 + tilt * dy)))
    strays = ImageChops.lighter(ImageChops.difference(alpha, dome_x), ImageChops.difference(green, dome_y))
    mask = ImageChops.multiply(strays.point(lambda v: 255 if v > 24 else 0), inner)
    mask = ImageChops.multiply(mask.filter(ImageFilter.MaxFilter(11)), inner).filter(ImageFilter.GaussianBlur(1))
    alpha.paste(dome_x, mask=mask)
    green.paste(dome_y, mask=mask)
    return Image.merge('RGBA', (red, green, blue, alpha))


def lyre_relief(image, height):
    """The normal map `image` (RGBA) with `height` (an L image: the letters) raised on it: the
    slopes of its edges tilt the normals, as the game's own letters do."""
    from PIL import Image, ImageChops, ImageFilter
    red, green, blue, alpha = image.split()
    smooth = height.filter(ImageFilter.GaussianBlur(1.5))
    tilt_x = ImageChops.subtract(ImageChops.offset(smooth, -1, 0), ImageChops.offset(smooth, 1, 0), LYRE_RELIEF, 128)
    tilt_y = ImageChops.subtract(ImageChops.offset(smooth, 0, -1), ImageChops.offset(smooth, 0, 1), LYRE_RELIEF, 128)
    alpha = ImageChops.add(alpha, tilt_x, 1, -128)
    green = ImageChops.subtract(green, tilt_y, 1, 128)
    return Image.merge('RGBA', (red, green, blue, alpha))


def lyre_paint(image, form, kind, style, faces, pad_map):
    """The level texture `image` (the lyre's or the gold mechanism's, of `kind`) with its four
    discs showing the gamepad buttons: the letter is replaced by the disc's own interior (black in
    the glow mask) and the host button's letter drawn in its place."""
    from PIL import Image, ImageDraw, ImageFilter
    hosts = pad_buttons(pad_map)
    look = LYRE_FORMS[form]
    shrink = 2048 // image.width
    x0, y0, x1, y1 = (v // shrink for v in LYRE_CORNER)
    normal = kind == 'normal'
    corner = image.crop((x0, y0, x1, y1)).convert('RGBA' if normal else 'RGB')
    radius = LYRE_RADIUS // shrink
    inner = Image.new('L', (2 * radius, 2 * radius), 0)
    ImageDraw.Draw(inner).ellipse((radius * (1 - look['inner']), radius * (1 - look['inner']),
                                   radius * (1 + look['inner']), radius * (1 + look['inner'])), fill=255)
    boxes = {button: (cx // shrink - radius, cy // shrink - radius, cx // shrink + radius, cy // shrink + radius)
             for button, (cx, cy) in LYRE_DISCS.items()}
    medians = {}
    if kind == 'glow':
        for box in boxes.values():
            corner.paste((0, 0, 0), box, mask=inner)
    elif normal:
        for button, box in boxes.items():
            corner.paste(lyre_dome(corner.crop(box), inner, radius), box)
    else:
        discs = {button: corner.crop(box) for button, box in boxes.items()}
        masks = {button: lyre_letter_mask(disc, inner, look['dark']) for button, disc in discs.items()}
        # as sources the discs only lose their letters: what a shape encloses is good texture
        lettered = {button: lyre_letter_mask(disc, inner, look['dark'], enclosed=False) for button, disc in discs.items()}
        for button, box in boxes.items():
            others = [(discs[other], lettered[other]) for other in discs if other != button]
            filled, medians[button] = lyre_fill(discs[button], masks[button], inner, others,
                                                lettered[button] if look['dark'] else None)
            corner.paste(filled, box)
    size = (LYRE_CORNER[2] - LYRE_CORNER[0], LYRE_CORNER[3] - LYRE_CORNER[1])
    letters = Image.new('RGBA', size, (0, 0, 0, 0))
    for button, centre in LYRE_DISCS.items():
        _shape, label, color = glyph(hosts[button], style, faces)
        if form == 'gold':  # a line engraved into the gold: a light bevel around a dark groove
            median = medians.get(button, (156, 116, 60))
            if kind == 'diffuse':  # the far wall of the groove catches the light
                light = tuple(min(255, int(c * 1.2)) for c in median)
                lit = (centre[0] + 2, centre[1] + 2)
                letters.alpha_composite(gold_letter(size, label, lit, look['cap'], GOLD_LINE + 5, light).filter(ImageFilter.GaussianBlur(1.5)))
                groove = tuple(int(c * 0.5) for c in median)
            else:
                groove = tuple(int(c * 0.9) for c in median)
            letters.alpha_composite(gold_letter(size, label, centre, look['cap'], GOLD_LINE, groove).filter(ImageFilter.GaussianBlur(0.6)))
            continue
        if kind == 'diffuse':
            color = tuple(int(c * LYRE_DIM) for c in color)
        elif kind == 'decal':
            color = tuple(int(c * LYRE_DECAL) for c in color)
        elif kind == 'specular':  # light grey with the dark rim of a carved edge
            letters.alpha_composite(lyre_letter(size, label, centre, look['cap'], (LYRE_RIM,) * 3, margin=2))
            color = (LYRE_SHINE,) * 3
        else:
            color = (255, 255, 255)
        letters.alpha_composite(lyre_letter(size, label, centre, look['cap'], color))
    if shrink > 1:
        letters = letters.resize(corner.size, Image.LANCZOS)
    if normal:
        base = lyre_relief(corner, letters.split()[3])
    else:
        base = corner.convert('RGBA')
        base.alpha_composite(letters)
    out = image.convert('RGBA' if normal else 'RGB')
    out.paste(base if normal else base.convert('RGB'), (x0, y0))
    return out


def encode_region(data, dds, end, image, fourcc, region, original=None):
    """Re-encodes the blocks of `region` (x0, y0, x1, y1 in the top level) of the texture at
    `dds` in `data`, in every mip level, from `image` (the whole top level); the other blocks are
    kept, and so are the region's blocks whose pixels `original` (the texture as it was) still has.
    Levels too small to hold the region's blocks are re-encoded whole."""
    from PIL import Image, ImageChops
    height, width = struct.unpack_from('<II', data, dds + 12)
    block = 16 if fourcc == 'DXT5' else 8
    mode = 'RGBA' if fourcc == 'DXT5' else 'RGB'
    image = image.convert(mode)
    original = original.convert(mode) if original else None
    offset = dds + 128
    level = 0
    while offset < end:
        w, h = max(1, width >> level), max(1, height >> level)
        bw, bh = -(-w // 4), -(-h // 4)
        size = bw * bh * block
        if offset + size > end:
            break
        x0, y0, x1, y1 = (max(0, region[0] >> level) // 4 * 4, max(0, region[1] >> level) // 4 * 4,
                          min(w, -(-(region[2] >> level) // 4) * 4), min(h, -(-(region[3] >> level) // 4) * 4))
        if w <= 8 or h <= 8 or x1 - x0 < 4 or y1 - y0 < 4:
            x0, y0, x1, y1 = 0, 0, w, h
        small = image if level == 0 else image.resize((w, h), Image.LANCZOS)
        changed = None
        if original is not None:
            before = original if level == 0 else original.resize((w, h), Image.LANCZOS)
            changed = ImageChops.difference(small, before).convert('L')
        for y in range(y0, y1, 4):
            strip = io.BytesIO()
            small.crop((x0, y, x1, min(y + 4, h))).save(strip, 'DDS', pixel_format=fourcc)
            pixels = strip.getvalue()[128:]
            row = offset + (y // 4) * bw * block
            for x in range(x0, x1, 4):
                if changed is not None and not changed.crop((x, y, x + 4, y + 4)).getbbox():
                    continue
                i = (x - x0) // 4 * block
                data[row + (x // 4) * block:row + (x // 4 + 1) * block] = pixels[i:i + block]
        offset += size
        level += 1
        if w == 1 and h == 1:
            break


def encode(image, fourcc, length):
    """DXT pixel data of image and its smaller levels, exactly `length` bytes as in the original
    texture (Pillow writes one level per file)."""
    image = image.convert('RGBA' if fourcc == 'DXT5' else 'RGB')
    pixels = b''
    while len(pixels) < length:
        # Pillow 11.2.1's BC encoder ignores its output buffer size. Independent block rows
        # keep the controller diagram below that limit without changing the encoded format.
        for y in range(0, image.height, 4):
            level = io.BytesIO()
            image.crop((0, y, image.width, min(y + 4, image.height))).save(level, 'DDS', pixel_format=fourcc)
            pixels += level.getvalue()[128:]
        if image.size == (1, 1):
            break
        image = image.resize((max(1, image.width // 2), max(1, image.height // 2)))
    if len(pixels) != length:
        raise ValueError(f'{len(pixels)} bytes drawn for a texture of {length}')
    return pixels


def textures(style):
    """The textures a style draws: the keyboard adds the stick and D-pad icons and leaves the lyre."""
    if style == 'keyboard':
        return {**{name: entry for name, entry in TEXTURES.items() if entry[0] not in LYRE_FORMS}, **KEYBOARD_TEXTURES}
    return TEXTURES


def patch(data, style, faces=None, pad_map='', key_map=''):
    """Replaces the glyph textures inside the WAD in `data` (a bytearray); returns how many."""
    count, drawn = 0, textures(style)
    for offset, size, name in entries(data):
        controller = name[:10] == CONTROLLER_TEXTURE and style in ('xbox', 'nintendo')
        if not name.endswith('.dds') or (name[:10] not in drawn and not controller):
            continue
        end = offset + 0x40 + size
        dds = data.find(b'DDS ', offset + 0x40, end)
        fourcc = bytes(data[dds + 84:dds + 88]).decode('latin1') if dds >= 0 else ''
        if fourcc not in ('DXT1', 'DXT5'):
            print(f'Glyphs: {name}: not a DXT texture, left as it is', flush=True)
            continue
        height, width = struct.unpack_from('<II', data, dds + 12)
        form, buttons = ('controller', None) if controller else drawn[name[:10]]
        try:
            if form in LYRE_FORMS:
                from PIL import Image
                with Image.open(io.BytesIO(bytes(data[dds:end]))) as texture:
                    original = texture.convert('RGBA' if fourcc == 'DXT5' else 'RGB')
                painted = lyre_paint(original, form, buttons, style, faces, pad_map)
                region = tuple(v // (2048 // width) for v in LYRE_CORNER)
                encode_region(data, dds, end, painted, fourcc, region, original)
            else:
                data[dds + 128:end] = encode(render(form, buttons, style, faces, pad_map, width, height, key_map),
                                             fourcc, end - dds - 128)
        except (OSError, ValueError) as error:
            print(f'Glyphs: {name}: {error}, left as it is', flush=True)
            continue
        count += 1
    return count


def build(game, out, style, faces=None, pad_map='', key_map=''):
    """{guest path: patched copy} for run.py to mount; {} (and no copies) for the PlayStation
    glyphs. The copies in out/glyphs are kept until the style, the mapping (the keyboard one for
    the keyboard style, the gamepad one otherwise) or the game's WADs change."""
    target = Path(out) / 'glyphs'
    if style not in STYLES:
        shutil.rmtree(target, ignore_errors=True)
        return {}
    if style == 'keyboard':
        pad_map = ''
    else:
        key_map = ''
    sources = [(name, Path(game) / name) for name in WADS if (Path(game) / name).is_file()]
    art = CONTROLLER_ART / f'{style}.png'
    stamp = json.dumps([VERSION, style, faces, pad_map, *([key_map] if key_map else []),
                        art.stat().st_mtime_ns if style in ('xbox', 'nintendo') and art.is_file() else None,
                        [(name, path.stat().st_size, path.stat().st_mtime_ns) for name, path in sources]])
    stamp_file = target / 'stamp.json'
    made = sorted(path.name for path in target.glob('*.WAD')) if target.is_dir() else []
    if not (stamp_file.is_file() and stamp_file.read_text() == stamp):
        shutil.rmtree(target, ignore_errors=True)
        target.mkdir(parents=True)
        made = []
        for name, path in sources:
            data = bytearray(path.read_bytes())
            if patch(data, style, faces, pad_map, key_map):
                (target / path.name).write_bytes(data)
                made.append(path.name)
        stamp_file.write_text(stamp)  # last: an interrupted build is redone
    return {'/app0/' + name: target / Path(name).name for name, _path in sources if Path(name).name in made}
