# SPDX-License-Identifier: GPL-2.0-or-later
"""Controller button glyphs for the game's own prompts (tutorials, move list, QTE buttons).

The prompts are DDS textures in R_UI.WAD (one more copy of the QTE circle in R_PERM.WAD). build()
writes patched copies of those WADs, with the glyphs of an Xbox or Nintendo controller drawn over
the PlayStation ones, and returns the guest paths run.py mounts them on. The game folder is never
written: a texture keeps its size and format, so only its pixels change inside the copy.

WAD (little-endian): entries of a 0x40 header (u16 tag, u16 flags, u32 size, name[56]) and `size`
bytes padded to 16. A texture entry is named TX<hash>.<hash>.dds and holds a short prefix and a
plain DDS file (DXT1 or DXT5), which the materials reference by that name."""
import io
import json
from pathlib import Path
import shutil
import struct

VERSION = 2  # of the drawing: a change redraws the cached copies
WADS = ('EXEC/WAD/PS4_LE/R_UI.WAD', 'EXEC/WAD/PS4_LE/R_PERM.WAD')
FONT = Path(__file__).resolve().parent.parent / 'launcher' / 'Cinzel-Black.ttf'

# Texture (the first hash of its name) -> how it is drawn and the game button(s) it shows.
# inline: the 64 px icons inside text; qte: the large prompt on black; atlas: two QTE labels,
# the first upside down in the top half.
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
}
# The gamepad button (SDL name) that presses each game button (src/runtime_pad.c pad_map).
DEFAULT_MAP = {'cross': 'a', 'circle': 'b', 'square': 'x', 'triangle': 'y', 'l1': 'leftshoulder',
               'r1': 'rightshoulder', 'l2': 'lefttrigger', 'r2': 'righttrigger', 'l3': 'leftstick',
               'r3': 'rightstick', 'options': 'start', 'touchpad_right': 'back'}
POSITIONS = {'a': 'south', 'b': 'east', 'x': 'west', 'y': 'north'}  # SDL names are positions
STYLES = {
    'xbox': {'faces': {'south': 'a', 'east': 'b', 'west': 'x', 'north': 'y'},
             'colors': {'a': (108, 196, 74), 'b': (232, 72, 66), 'x': (66, 142, 232), 'y': (244, 196, 48)},
             'labels': {'leftshoulder': 'LB', 'rightshoulder': 'RB', 'lefttrigger': 'LT', 'righttrigger': 'RT',
                        'leftstick': 'LS', 'rightstick': 'RS', 'start': 'menu', 'back': 'view'}},
    'nintendo': {'faces': {'south': 'b', 'east': 'a', 'west': 'y', 'north': 'x'}, 'colors': {},
                 'labels': {'leftshoulder': 'L', 'rightshoulder': 'R', 'lefttrigger': 'ZL', 'righttrigger': 'ZR',
                            'leftstick': 'LS', 'rightstick': 'RS', 'start': 'plus', 'back': 'minus'}},
}
ICONS = {'menu', 'view', 'plus', 'minus', 'dpup', 'dpdown', 'dpleft', 'dpright'}
INK = (240, 240, 240)
SCALE = 6  # drawn this many times larger, then reduced


def entries(data):
    """(offset, size, name) of each WAD entry; stops at the first one that does not fit."""
    offset = 0
    while offset + 0x40 <= len(data):
        size = struct.unpack_from('<I', data, offset + 4)[0]
        if offset + 0x40 + size > len(data):
            return
        yield offset, size, bytes(data[offset + 8:offset + 0x40]).split(b'\0')[0].decode('latin1')
        offset += 0x40 + ((size + 15) & ~15)


def pad_buttons(pad_map):
    """Game button -> gamepad button, with a GOW3_PAD_MAP value ('cross=b,circle=a') applied.
    L2/R2 stay on the triggers, as in the runtime."""
    buttons = dict(DEFAULT_MAP)
    for entry in (pad_map or '').split(','):
        game, _, host = (part.strip().casefold() for part in entry.partition('='))
        if game in buttons and host and game not in ('l2', 'r2'):
            buttons[game] = host
    return buttons


def glyph(host, style, faces=None):
    """(shape, label, color) of a gamepad button: a letter or an ICONS name on a disc or a plate.
    `faces` are the letters this controller prints on south/east/west/north."""
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
    """(style, faces, name) from the line `gow3-probe --gamepad` prints, None without a gamepad."""
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
    """(style, faces, controller name) for a GOW3_GLYPH_STYLE value. 'auto' follows the connected
    gamepad (the PlayStation glyphs without one); a chosen style uses its own letters."""
    if choice in STYLES:
        return choice, None, ''
    return (parse_gamepad(probe_output) if choice == 'auto' else None) or ('playstation', None, '')


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
    elif label in ICONS:  # a d-pad direction: an arrow head
        ax, ay = {'dpup': (0, -1), 'dpdown': (0, 1), 'dpleft': (-1, 0), 'dpright': (1, 0)}[label]
        draw.polygon([(cx + ax * r * 0.8, cy + ay * r * 0.8), (cx - ax * r * 0.5 - ay * r * 0.7, cy - ay * r * 0.5 - ax * r * 0.7),
                      (cx - ax * r * 0.5 + ay * r * 0.7, cy - ay * r * 0.5 + ax * r * 0.7)], fill=color)
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
    if shape == 'disc':
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
    inset_x = size[0] * (0.2 if shape == 'disc' else 0.1)
    inset_y = size[1] * (0.2 if shape == 'disc' else 0.16)
    draw_label(image, (box[0] + inset_x, box[1] + inset_y, box[2] - inset_x, box[3] - inset_y), label, color)
    return image.resize((width, height), Image.LANCZOS)


def draw_prompt(label, color, width, height):
    """A QTE label: bright on black (the game adds these textures to the picture)."""
    from PIL import Image, ImageFilter
    w, h = width * SCALE, height * SCALE
    layer = Image.new('RGBA', (w, h), (0, 0, 0, 0))
    draw_label(layer, (w * 0.24, h * 0.2, w * 0.76, h * 0.8), label, color)
    image = Image.new('RGBA', (w, h), (0, 0, 0, 255))
    image.alpha_composite(layer.filter(ImageFilter.GaussianBlur(SCALE * 2.5)))
    image.alpha_composite(layer)
    return image.resize((width, height), Image.LANCZOS)


def render(form, buttons, style, faces, pad_map, width, height):
    """The picture that replaces a texture of TEXTURES, at its size."""
    from PIL import Image
    hosts = pad_buttons(pad_map)
    if form == 'atlas':
        image = Image.new('RGBA', (width, height), (0, 0, 0, 255))
        for row, button in enumerate(buttons):
            half = draw_prompt(glyph(hosts[button], style, faces)[1], INK, width, height // 2)
            image.paste(half.rotate(180) if row == 0 else half, (0, row * (height // 2)))
        return image
    shape, label, color = glyph(hosts[buttons], style, faces)
    return draw_inline(shape, label, color, width, height) if form == 'inline' else draw_prompt(label, color, width, height)


def encode(image, fourcc, length):
    """DXT pixel data of image and its smaller levels, exactly `length` bytes as in the original
    texture (Pillow writes one level per file)."""
    image = image.convert('RGBA' if fourcc == 'DXT5' else 'RGB')
    pixels = b''
    while len(pixels) < length:
        level = io.BytesIO()
        image.save(level, 'DDS', pixel_format=fourcc)
        pixels += level.getvalue()[128:]
        if image.size == (1, 1):
            break
        image = image.resize((max(1, image.width // 2), max(1, image.height // 2)))
    if len(pixels) != length:
        raise ValueError(f'{len(pixels)} bytes drawn for a texture of {length}')
    return pixels


def patch(data, style, faces=None, pad_map=''):
    """Replaces the glyph textures inside the WAD in `data` (a bytearray); returns how many."""
    count = 0
    for offset, size, name in entries(data):
        if not name.endswith('.dds') or name[:10] not in TEXTURES:
            continue
        end = offset + 0x40 + size
        dds = data.find(b'DDS ', offset + 0x40, end)
        fourcc = bytes(data[dds + 84:dds + 88]).decode('latin1') if dds >= 0 else ''
        if fourcc not in ('DXT1', 'DXT5'):
            print(f'Glyphs: {name}: not a DXT texture, left as it is', flush=True)
            continue
        height, width = struct.unpack_from('<II', data, dds + 12)
        form, buttons = TEXTURES[name[:10]]
        try:
            data[dds + 128:end] = encode(render(form, buttons, style, faces, pad_map, width, height),
                                         fourcc, end - dds - 128)
        except ValueError as error:
            print(f'Glyphs: {name}: {error}, left as it is', flush=True)
            continue
        count += 1
    return count


def build(game, out, style, faces=None, pad_map=''):
    """{guest path: patched copy} for run.py to mount; {} (and no copies) for the PlayStation
    glyphs. The copies in out/glyphs are kept until the style, the mapping or the game's WADs
    change."""
    target = Path(out) / 'glyphs'
    if style not in STYLES:
        shutil.rmtree(target, ignore_errors=True)
        return {}
    sources = [(name, Path(game) / name) for name in WADS if (Path(game) / name).is_file()]
    stamp = json.dumps([VERSION, style, faces, pad_map,
                        [(name, path.stat().st_size, path.stat().st_mtime_ns) for name, path in sources]])
    stamp_file = target / 'stamp.json'
    made = sorted(path.name for path in target.glob('*.WAD')) if target.is_dir() else []
    if not (stamp_file.is_file() and stamp_file.read_text() == stamp):
        shutil.rmtree(target, ignore_errors=True)
        target.mkdir(parents=True)
        made = []
        for name, path in sources:
            data = bytearray(path.read_bytes())
            if patch(data, style, faces, pad_map):
                (target / path.name).write_bytes(data)
                made.append(path.name)
        stamp_file.write_text(stamp)  # last: an interrupted build is redone
    return {'/app0/' + name: target / Path(name).name for name, _path in sources if Path(name).name in made}
