#!/usr/bin/env python3
"""Experimental GoW III Persian font patcher.

Uses isolated Arabic/Persian presentation forms and reverses Persian runs. This
is deliberately marked experimental: full contextual joining still needs to be
validated in-game.
"""
from __future__ import annotations
import argparse
import re
import struct
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import glyphs  # noqa: E402
from wad_lang import replace_messages  # noqa: E402

HEADER = 0x40
SMF_RECORD_OFFSET = 100
SMF_RECORD_SIZE = 20
FONT_PATH = Path("C:/Windows/Fonts/arial.ttf")

# Persian/Arabic characters used by the draft and their isolated presentation forms.
FORMS = {
    'ا':'ﺍ','آ':'ﺁ','ب':'ﺏ','پ':'ﭖ','ت':'ﺕ','ث':'ﺙ','ج':'ﺝ','چ':'ﭺ','ح':'ﺡ','خ':'ﺥ',
    'د':'ﺩ','ذ':'ﺫ','ر':'ﺭ','ز':'ﺯ','ژ':'ﮊ','س':'ﺱ','ش':'ﺵ','ص':'ﺹ','ض':'ﺽ','ط':'ﻁ',
    'ظ':'ﻅ','ع':'ﻉ','غ':'ﻍ','ف':'ﻑ','ق':'ﻕ','ک':'ﮎ','گ':'ﮒ','ل':'ﻝ','م':'ﻡ','ن':'ﻥ',
    'و':'ﻭ','ه':'ﻩ','ی':'ﻱ','ء':'ﺀ','ئ':'ﺉ','ؤ':'ﺅ','ة':'ﺓ','ۀ':'ﮦ','ـ':'ـ',
    'أ':'ﺃ','،':',','؛':';','؟':'?','ً':'','۰':'0','۱':'1','۲':'2','۳':'3','۴':'4','۵':'5','۶':'6','۷':'7','۸':'8','۹':'9',
}


def entries(data):
    off = 0
    while off + HEADER <= len(data):
        size = struct.unpack_from('<I', data, off + 4)[0]
        if off + HEADER + size > len(data): raise ValueError('invalid WAD entry')
        name = data[off + 8:off + HEADER].split(b'\0', 1)[0].decode('latin1')
        yield off, size, name
        off += HEADER + ((size + 15) & ~15)
    if off != len(data): raise ValueError('WAD trailing data')


def shape_isolated(text):
    # Preserve controls and reverse only contiguous Persian/Arabic runs.
    pattern = re.compile(r'[\u0600-\u06ff\u0750-\u077f\ufb50-\ufdff\ufe70-\ufeff]+')
    return pattern.sub(lambda m: ''.join(FORMS.get(c, c) for c in reversed(m.group(0))), text)


def components(image):
    pix = image.load(); w, h = image.size; seen = set(); result = []
    for y in range(h):
        for x in range(w):
            if (x, y) in seen or pix[x, y] < 40: continue
            stack = [(x, y)]; seen.add((x, y)); pts = []
            while stack:
                xx, yy = stack.pop(); pts.append((xx, yy))
                for nx, ny in ((xx-1, yy),(xx+1, yy),(xx, yy-1),(xx, yy+1)):
                    if 0 <= nx < w and 0 <= ny < h and (nx, ny) not in seen and pix[nx, ny] >= 40:
                        seen.add((nx, ny)); stack.append((nx, ny))
            if len(pts) > 8:
                xs = [p[0] for p in pts]; ys = [p[1] for p in pts]
                result.append((min(xs), min(ys), max(xs)+1, max(ys)+1))
    return result


def patch(base, messages):
    data = bytearray(base)
    smf_entry = next((x for x in entries(data) if x[2] == 'SMF_0'), None)
    tx_entries = [(o, s) for o, s, n in entries(data) if n.startswith('TXR_GoWIII')]
    if not smf_entry or len(tx_entries) < 2: raise ValueError('expected SMF_0 and two font pages')
    smf_off, smf_size, _ = smf_entry
    smf = bytearray(data[smf_off + HEADER:smf_off + HEADER + smf_size])
    pages = []
    for i, (off, size) in enumerate(tx_entries[:2]):
        payload = data[off + HEADER:off + HEADER + size]
        dds = payload.find(b'DDS ')
        pixels = payload[dds + 128:dds + 128 + 1024 * 1024] if dds >= 0 else payload[:1024 * 1024]
        pages.append(Image.frombytes('L', (1024, 1024), bytes(pixels)))
    comps = [components(page) for page in pages]
    records = []
    for pos in range(SMF_RECORD_OFFSET, len(smf) - SMF_RECORD_SIZE + 1, SMF_RECORD_SIZE):
        cp = struct.unpack_from('<I', smf, pos)[0]
        if cp: records.append((pos, cp))
    needed = sorted(set(v for v in FORMS.values() if v))
    used = {cp for _, cp in records}
    available = [(pos, cp) for pos, cp in records if cp >= 0x4e00 and cp not in needed]
    if len(available) < len(needed): raise ValueError('not enough CJK slots')
    font = ImageFont.truetype(str(FONT_PATH), 32)
    changed = 0
    for target, (pos, oldcp) in zip(needed, available):
        index = (pos - SMF_RECORD_OFFSET) // SMF_RECORD_SIZE
        page_no = index // 1024
        page = pages[page_no]
        word1 = struct.unpack_from('<I', smf, pos + 4)[0]
        x = (word1 & 0xffff) / 16.0; y = ((word1 >> 16) & 0xffff) / 16.0
        box = min(comps[page_no], key=lambda b: abs(b[0]-x) + abs(b[1]-y))
        x0, y0, x1, y1 = box
        crop = Image.new('L', (x1-x0, y1-y0), 0)
        draw = ImageDraw.Draw(crop)
        bbox = draw.textbbox((0, 0), target, font=font)
        tw, th = bbox[2]-bbox[0], bbox[3]-bbox[1]
        scale = min((x1-x0-2)/max(1,tw), (y1-y0-2)/max(1,th), 1.0)
        if scale < 1:
            font2 = ImageFont.truetype(str(FONT_PATH), max(8, int(32*scale)))
            bbox = draw.textbbox((0,0), target, font=font2); font_use = font2
        else: font_use = font
        bbox = draw.textbbox((0,0), target, font=font_use)
        draw.text(((crop.width-(bbox[2]-bbox[0]))//2-bbox[0], (crop.height-(bbox[3]-bbox[1]))//2-bbox[1]), target, font=font_use, fill=255)
        page.paste(crop, (x0, y0))
        struct.pack_into('<I', smf, pos, ord(target))
        changed += 1
    data[smf_off + HEADER:smf_off + HEADER + smf_size] = smf
    for (off, size), page in zip(tx_entries[:2], pages):
        payload = bytearray(data[off + HEADER:off + HEADER + size]); dds = payload.find(b'DDS ')
        start = dds + 128 if dds >= 0 else 0
        payload[start:start+1024*1024] = page.tobytes()
        data[off + HEADER:off + HEADER + size] = payload
    return bytes(data), changed


def main():
    ap = argparse.ArgumentParser(); ap.add_argument('base_wad', type=Path); ap.add_argument('messages', type=Path); ap.add_argument('output', type=Path)
    a = ap.parse_args(); base = a.base_wad.read_bytes(); msg = a.messages.read_text('utf-8').splitlines(keepends=True)
    shaped = ''.join(shape_isolated(x) for x in msg).encode('utf-8')
    result, count = patch(base, shaped)
    result = replace_messages(result, shaped)
    a.output.parent.mkdir(parents=True, exist_ok=True); a.output.write_bytes(result)
    print(f'wrote {a.output} ({len(result)} bytes; patched {count} glyphs; isolated experimental shaping)')

if __name__ == '__main__': main()
