# SPDX-License-Identifier: GPL-2.0-or-later
"""Finds textures in the game's WADs by looking rather than by name: contact sheets, a search for
prompt-like textures, a word search over entry names, and extraction of single textures.

    "SpartanPort.exe" --script wad_textures.py "<game>\\EXEC\\WAD\\PS4_LE" OLY1* OLY2* -o sheets
    "SpartanPort.exe" --script wad_textures.py --extract "<...>\\OLY200.WAD" 40 127 -o textures
    "SpartanPort.exe" --script wad_textures.py --find "<game>\\EXEC\\WAD\\PS4_LE" * -o found
    "SpartanPort.exe" --script wad_textures.py --grep "<game>\\EXEC\\WAD\\PS4_LE" Muse Lyre -o grep.txt

(From a source tree: python tools/wad_textures.py ..., with Pillow installed.)

One or more name patterns (shell-style, case-insensitive, .WAD implied) pick the WADs. Every DXT
texture is tiled as a 112 px thumbnail with its number; a WAD with many textures spans several
sheets (<WAD>_<n>.png) and <WAD>.txt lists number, name, size and format. Non-DXT textures (rare)
are listed but not drawn. --extract writes the numbered textures of one WAD as .dds files (the
numbers from its sheet or .txt). --grep searches every WAD for words (case-insensitive) and
reports, per WAD, how often each occurs and the entry names around the hits: the WAD of a room,
found by what the room contains. --find scans the matching WADs for small textures that look
like button prompts (a dark background with several saturated colours) and writes the best 96 to
one candidates.png with their WAD and number, plus candidates.txt. Needs Pillow, which the port's
executable carries.
"""
import argparse
import fnmatch
import io
import struct
import sys
from pathlib import Path

THUMB = 112
COLUMNS = 24
ROWS = 24  # per sheet: 24 x 24 = 576 textures


def entries(data):
    """(offset, size, name) of every entry: a 0x40 header (u16 tag, u16 flags, u32 size, name[56]) and
    `size` bytes padded to 16. The EntityCount marker of level WADs (tag 0) carries a count in its
    size field and no data."""
    offset = 0
    while offset + 0x40 <= len(data):
        tag, _, size = struct.unpack_from('<HHI', data, offset)
        name = bytes(data[offset + 8:offset + 0x40]).split(b'\0')[0].decode('latin1')
        if tag == 0 and name == 'EntityCount':
            size = 0
        if offset + 0x40 + size > len(data):
            return
        yield offset, size, name
        offset += 0x40 + ((size + 15) & ~15)


def top_mip_dds(data, dds, end):
    """The DDS at `dds` cut down to its first mip, as a bytes object Pillow can open."""
    height, width = struct.unpack_from('<II', data, dds + 12)
    fourcc = bytes(data[dds + 84:dds + 88]).decode('latin1')
    if fourcc not in ('DXT1', 'DXT3', 'DXT5'):
        return None, width, height, fourcc
    block = 8 if fourcc == 'DXT1' else 16
    size = max(1, (width + 3) // 4) * max(1, (height + 3) // 4) * block
    header = bytearray(data[dds:dds + 128])
    struct.pack_into('<I', header, 28, 1)  # one mip level
    flags = struct.unpack_from('<I', header, 8)[0] & ~0x20000  # no DDSD_MIPMAPCOUNT
    struct.pack_into('<I', header, 8, flags)
    return bytes(header) + bytes(data[dds + 128:min(dds + 128 + size, end)]), width, height, fourcc


def sheet_wad(path, out, font, crop=None):
    from PIL import Image, ImageDraw
    data = Path(path).read_bytes()
    textures = []
    for offset, size, name in entries(data):
        if not name.endswith('.dds'):
            continue
        end = offset + 0x40 + size
        dds = data.find(b'DDS ', offset + 0x40, end)
        if dds < 0:
            continue
        textures.append((name, dds, end))
    if not textures:
        print(f'{Path(path).name}: no textures')
        return 0
    stem = Path(path).stem
    lines = []
    sheets = []
    per_sheet = COLUMNS * ROWS
    for n, (name, dds, end) in enumerate(textures):
        blob, width, height, fourcc = top_mip_dds(data, dds, end)
        lines.append(f'{n:5d}  {name:32s} {width:5d}x{height:<5d} {fourcc}')
        if blob is None:
            continue
        page, slot = divmod(n, per_sheet)
        while len(sheets) <= page:
            sheets.append(Image.new('RGB', (COLUMNS * THUMB, ROWS * (THUMB + 14)), (40, 40, 40)))
        try:
            image = Image.open(io.BytesIO(blob)).convert('RGBA')
        except Exception as error:  # a texture Pillow cannot read: show the slot empty
            lines[-1] += f'  (not decoded: {error})'
            continue
        if crop:  # a part of every texture, as fractions of its size
            image = image.crop(tuple(int(f * side) for f, side in zip(crop, (image.width, image.height) * 2)))
        image.thumbnail((THUMB - 4, THUMB - 4))
        # checkerboard behind transparency so cut-out glyphs stay visible
        back = Image.new('RGBA', image.size, (90, 90, 90, 255))
        for y in range(0, image.height, 8):
            for x in range(0, image.width, 8):
                if (x // 8 + y // 8) % 2:
                    back.paste((120, 120, 120, 255), (x, y, x + 8, y + 8))
        back.alpha_composite(image)
        x, y = (slot % COLUMNS) * THUMB, (slot // COLUMNS) * (THUMB + 14)
        sheet = sheets[page]
        sheet.paste(back.convert('RGB'), (x + 2 + (THUMB - 4 - image.width) // 2, y + 2 + (THUMB - 4 - image.height) // 2))
        ImageDraw.Draw(sheet).text((x + 3, y + THUMB), f'{n} {width}x{height}', fill=(230, 230, 230), font=font)
    out.mkdir(parents=True, exist_ok=True)
    for page, sheet in enumerate(sheets):
        used = min(per_sheet, len(textures) - page * per_sheet)
        rows = (used + COLUMNS - 1) // COLUMNS
        sheet.crop((0, 0, sheet.width, rows * (THUMB + 14))).save(out / f'{stem}_{page}.png', optimize=True)
    (out / f'{stem}.txt').write_text('\n'.join(lines) + '\n')
    print(f'{Path(path).name}: {len(textures)} textures -> {len(sheets)} sheet(s)', flush=True)
    return len(textures)


def extract(path, numbers, out):
    data = Path(path).read_bytes()
    wanted = {int(n) for n in numbers}
    out.mkdir(parents=True, exist_ok=True)
    n = 0
    for offset, size, name in entries(data):
        if not name.endswith('.dds'):
            continue
        if n in wanted:
            end = offset + 0x40 + size
            dds = data.find(b'DDS ', offset + 0x40, end)
            target = out / f'{Path(path).stem}_{n}_{name}'
            target.write_bytes(data[dds:end] if dds >= 0 else data[offset + 0x40:end])
            print(f'{n:5d}  {name} -> {target} ({end - max(dds, offset + 0x40)} bytes)')
        n += 1


def prompt_score(image):
    """How much a small texture looks like a button prompt: saturated hue families (the face buttons
    are green, orange, blue and pink) and a dark or transparent surround."""
    small = image.convert('RGBA').resize((48, 48))
    px = small.load()
    families, dark, bright, total = set(), 0, 0, 0
    import colorsys
    for y in range(48):
        for x in range(48):
            r, g, b, a = px[x, y]
            total += 1
            if a < 40:
                dark += 1
                continue
            h, sat, v = colorsys.rgb_to_hsv(r / 255, g / 255, b / 255)
            if v < 0.2:
                dark += 1
                continue
            if sat > 0.45 and v > 0.45:
                bright += 1
                if 0.2 < h < 0.45:
                    families.add('green')
                elif 0.55 < h < 0.72:
                    families.add('blue')
                elif h < 0.1 or h > 0.95:
                    families.add('orange')
                elif 0.8 <= h <= 0.95:
                    families.add('pink')
    surround = dark / total
    return len(families) * 2 + surround * 2 + (0.02 < bright / total < 0.5), families, surround


def find(folder, patterns, out):
    from PIL import Image, ImageDraw, ImageFont
    font = ImageFont.load_default()
    wads = sorted(p for p in Path(folder).glob('*.WAD')
                  if any(fnmatch.fnmatch(p.stem.upper(), pat.upper().removesuffix('.WAD')) for pat in patterns))
    found = []
    for path in wads:
        data = Path(path).read_bytes()
        n = -1
        for offset, size, name in entries(data):
            if not name.endswith('.dds'):
                continue
            n += 1
            end = offset + 0x40 + size
            dds = data.find(b'DDS ', offset + 0x40, end)
            if dds < 0:
                continue
            blob, width, height, fourcc = top_mip_dds(data, dds, end)
            if blob is None or width > 512 or height > 512:
                continue
            try:
                image = Image.open(io.BytesIO(blob)).convert('RGBA')
            except Exception:
                continue
            score, families, surround = prompt_score(image)
            if surround < 0.3 or not families:
                continue
            found.append((score, path.stem, n, name, width, height, fourcc, sorted(families), image))
        print(f'{path.name}: {n + 1} textures, {sum(1 for f in found if f[1] == path.stem)} candidates', flush=True)
    found.sort(key=lambda f: -f[0])
    found = found[:96]
    out.mkdir(parents=True, exist_ok=True)
    cell, cols = 160, 12
    rows = (len(found) + cols - 1) // cols
    sheet = Image.new('RGB', (cols * cell, max(1, rows) * (cell + 28)), (40, 40, 40))
    draw = ImageDraw.Draw(sheet)
    lines = []
    for i, (score, wad, n, name, width, height, fourcc, families, image) in enumerate(found):
        image.thumbnail((cell - 4, cell - 4))
        back = Image.new('RGBA', image.size, (90, 90, 90, 255))
        back.alpha_composite(image)
        x, y = (i % cols) * cell, (i // cols) * (cell + 28)
        sheet.paste(back.convert('RGB'), (x + 2 + (cell - 4 - image.width) // 2, y + 2 + (cell - 4 - image.height) // 2))
        draw.text((x + 3, y + cell), f'{i}: {wad} #{n}', fill=(230, 230, 230), font=font)
        draw.text((x + 3, y + cell + 13), f'{width}x{height} {",".join(families)}', fill=(180, 180, 180), font=font)
        lines.append(f'{i:3d}  {wad:24s} #{n:<5d} {name:32s} {width}x{height} {fourcc} score={score:.1f} {",".join(families)}')
    sheet.save(out / 'candidates.png', optimize=True)
    (out / 'candidates.txt').write_text('\n'.join(lines) + '\n')
    print(f'{len(found)} candidates -> {out / "candidates.png"}')


def grep(folder, words, out):
    import re
    wads = sorted(Path(folder).glob('*.WAD'))
    patterns = [(w, re.compile(re.escape(w).encode(), re.I)) for w in words]
    report = []
    for path in wads:
        data = Path(path).read_bytes()
        names = [(offset, name) for offset, size, name in entries(data)]
        starts = [o for o, _ in names]
        import bisect
        lines = []
        for word, rx in patterns:
            hits = [m.start() for m in rx.finditer(data)]
            if not hits:
                continue
            near = []
            for h in hits[:400]:
                i = bisect.bisect_right(starts, h) - 1
                if i >= 0 and names[i][1] not in near:
                    near.append(names[i][1])
            lines.append(f'  {word}: {len(hits)} hit(s) in entries: {", ".join(near[:12])}{" ..." if len(near) > 12 else ""}')
        if lines:
            textures = [n for _, n in names if n.endswith('.dds')]
            report.append(f'{path.name}  ({len(textures)} textures)')
            report.extend(lines)
            print(report[-len(lines) - 1], flush=True)
    text = '\n'.join(report) + '\n'
    Path(out).write_text(text)
    print(text if len(text) < 6000 else f'{len(report)} lines -> {out}')


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('folder', help='the WAD folder (EXEC\\WAD\\PS4_LE), or one WAD with --extract')
    ap.add_argument('patterns', nargs='+', help='WAD name patterns, e.g. OLY1* HDS350; texture numbers with --extract')
    ap.add_argument('-o', '--out', default='sheets')
    ap.add_argument('--extract', action='store_true', help='write the numbered textures of the WAD as .dds files')
    ap.add_argument('--find', action='store_true', help='look for button-prompt-like textures in the matching WADs')
    ap.add_argument('--grep', action='store_true', help='search every WAD for the given words; -o names the report file')
    ap.add_argument('--crop', help='sheet only this part of every texture, as fractions left,top,right,bottom: '
                                   '0.75,0,1,0.25 is the top-right quarter')
    args = ap.parse_args()
    if args.grep:
        grep(args.folder, args.patterns, args.out if args.out != 'sheets' else 'grep.txt')
        return
    if args.extract:
        extract(args.folder, args.patterns, Path(args.out))
        return
    if args.find:
        find(args.folder, args.patterns, Path(args.out))
        return
    try:
        from PIL import ImageFont
        font = ImageFont.load_default()
    except ImportError:
        sys.exit('Pillow is needed (run this through "SpartanPort.exe --script")')
    folder = Path(args.folder)
    wads = sorted(p for p in folder.glob('*.WAD')
                  if any(fnmatch.fnmatch(p.stem.upper(), pat.upper().removesuffix('.WAD')) for pat in args.patterns))
    if not wads:
        sys.exit(f'no WAD in {folder} matches {" ".join(args.patterns)}')
    total = 0
    crop = tuple(float(value) for value in args.crop.split(',')) if args.crop else None
    for path in wads:
        total += sheet_wad(path, Path(args.out), font, crop)
    print(f'{total} textures from {len(wads)} WAD(s) -> {args.out}')


if __name__ == '__main__':
    main()
