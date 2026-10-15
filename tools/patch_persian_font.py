#!/usr/bin/env python3
"""Experimental GoW III Persian font patcher.

Uses contextual Arabic/Persian presentation forms, reverses Persian runs, and
copies Latin glyph metrics into the replacement records. Full in-game validation
is still required.
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

# Base character -> (isolated, final, initial, medial) presentation forms.
# None means the character cannot join on that side.
JOIN_FORMS = {
    'ا': ('ﺍ','ﺎ',None,None), 'آ': ('ﺁ','ﺂ',None,None), 'أ': ('ﺃ','ﺄ',None,None),
    'ب': ('ﺏ','ﺐ','ﺑ','ﺒ'), 'پ': ('ﭖ','ﭗ','ﭘ','ﭙ'), 'ت': ('ﺕ','ﺖ','ﺗ','ﺘ'),
    'ث': ('ﺙ','ﺚ','ﺛ','ﺜ'), 'ج': ('ﺝ','ﺞ','ﺟ','ﺠ'), 'چ': ('ﭺ','ﭻ','ﭼ','ﭽ'),
    'ح': ('ﺡ','ﺢ','ﺣ','ﺤ'), 'خ': ('ﺥ','ﺦ','ﺧ','ﺨ'), 'د': ('ﺩ','ﺪ',None,None),
    'ذ': ('ﺫ','ﺬ',None,None), 'ر': ('ﺭ','ﺮ',None,None), 'ز': ('ﺯ','ﺰ',None,None),
    'ژ': ('ﮊ','ﮋ',None,None), 'س': ('ﺱ','ﺲ','ﺳ','ﺴ'), 'ش': ('ﺵ','ﺶ','ﺷ','ﺸ'),
    'ص': ('ﺹ','ﺺ','ﺻ','ﺼ'), 'ض': ('ﺽ','ﺾ','ﺿ','ﻀ'), 'ط': ('ﻁ','ﻂ','ﻃ','ﻄ'),
    'ظ': ('ﻅ','ﻆ','ﻇ','ﻈ'), 'ع': ('ﻉ','ﻊ','ﻋ','ﻌ'), 'غ': ('ﻍ','ﻎ','ﻏ','ﻐ'),
    'ف': ('ﻑ','ﻒ','ﻓ','ﻔ'), 'ق': ('ﻕ','ﻖ','ﻗ','ﻘ'), 'ک': ('ﮎ','ﮏ','ﮐ','ﮑ'),
    'گ': ('ﮒ','ﮓ','ﮔ','ﮕ'), 'ل': ('ﻝ','ﻞ','ﻟ','ﻠ'), 'م': ('ﻡ','ﻢ','ﻣ','ﻤ'),
    'ن': ('ﻥ','ﻦ','ﻧ','ﻨ'), 'و': ('ﻭ','ﻮ',None,None), 'ه': ('ﻩ','ﻪ','ﻫ','ﻬ'),
    'ی': ('ﻱ','ﻲ','ﻳ','ﻴ'), 'ء': ('ﺀ',None,None,None), 'ئ': ('ﺉ','ﺊ','ﺋ','ﺌ'),
    'ؤ': ('ﺅ','ﺆ',None,None), 'ة': ('ﺓ','ﺔ',None,None), 'ۀ': ('ﮦ','ﮧ',None,None),
    'ـ': ('ـ','ـ','ـ','ـ'),
}
FORMS = {base: forms[0] for base, forms in JOIN_FORMS.items()}
FORMS.update({'،':',','؛':';','؟':'?','ً':'','۰':'0','۱':'1','۲':'2','۳':'3','۴':'4','۵':'5','۶':'6','۷':'7','۸':'8','۹':'9'})


def _can_join_right(ch):
    return ch in JOIN_FORMS and JOIN_FORMS[ch][2] is not None


def _can_join_left(ch):
    return ch in JOIN_FORMS and JOIN_FORMS[ch][1] is not None


def shape_isolated(text):
    """Shape logical Persian runs, then reverse them for the game's LTR renderer."""
    pattern = re.compile(r'[\u0600-\u06ff\u0750-\u077f\ufb50-\ufdff\ufe70-\ufeff]+')
    def shape(match):
        run = match.group(0)
        shaped = []
        for i, ch in enumerate(run):
            if ch not in JOIN_FORMS:
                shaped.append(FORMS.get(ch, ch)); continue
            prev = run[i - 1] if i else ''
            nxt = run[i + 1] if i + 1 < len(run) else ''
            join_prev = _can_join_left(ch) and _can_join_right(prev)
            join_next = _can_join_right(ch) and _can_join_left(nxt)
            forms = JOIN_FORMS[ch]
            shaped.append(forms[3] if join_prev and join_next else forms[1] if join_prev else forms[2] if join_next else forms[0])
        return ''.join(reversed(shaped))
    return pattern.sub(shape, text)

def entries(data):
    off = 0
    while off + HEADER <= len(data):
        size = struct.unpack_from('<I', data, off + 4)[0]
        if off + HEADER + size > len(data): raise ValueError('invalid WAD entry')
        name = data[off + 8:off + HEADER].split(b'\0', 1)[0].decode('latin1')
        yield off, size, name
        off += HEADER + ((size + 15) & ~15)
    if off != len(data): raise ValueError('WAD trailing data')


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
    metric_template = next((pos for pos, cp in records if cp == ord('A')), None)
    if metric_template is None: raise ValueError('ASCII A metrics not found')
    latin_metrics = bytes(smf[metric_template + 8:metric_template + SMF_RECORD_SIZE])
    needed = sorted(set(form for forms in JOIN_FORMS.values() for form in forms if form))
    used = {cp for _, cp in records}
    available = [(pos, cp) for pos, cp in records if cp >= 0x4e00 and cp not in needed]
    if len(available) < len(needed): raise ValueError('not enough CJK slots')
    slot_map = {form: cp for form, (_, cp) in zip(needed, available)}
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
        # Keep the original supported CJK codepoint. The message text is
        # remapped to this slot below, avoiding unsupported Arabic codepoints.
        # SMF2 stores horizontal bearing/width in word 3 and advance in word 4.
        # Keep word 2 and atlas coordinates from the original record.
        glyph_box = font_use.getbbox(target)
        bearing = int(round(glyph_box[0] * 16))
        width = max(1, int(round((glyph_box[2] - glyph_box[0]) * 16)))
        advance = max(1, int(round(font_use.getlength(target) * 16)))
        struct.pack_into('<I', smf, pos + 12,
                         ((width & 0xffff) << 16) | (bearing & 0xffff))
        struct.pack_into('<I', smf, pos + 16, advance)
        changed += 1
    data[smf_off + HEADER:smf_off + HEADER + smf_size] = smf
    for (off, size), page in zip(tx_entries[:2], pages):
        payload = bytearray(data[off + HEADER:off + HEADER + size]); dds = payload.find(b'DDS ')
        start = dds + 128 if dds >= 0 else 0
        payload[start:start+1024*1024] = page.tobytes()
        data[off + HEADER:off + HEADER + size] = payload
    return bytes(data), changed, slot_map


def main():
    ap = argparse.ArgumentParser(); ap.add_argument('base_wad', type=Path); ap.add_argument('messages', type=Path); ap.add_argument('output', type=Path)
    a = ap.parse_args(); base = a.base_wad.read_bytes(); msg = a.messages.read_text('utf-8').splitlines(keepends=True)
    shaped_text = ''.join(shape_isolated(x) for x in msg)
    result, count, slot_map = patch(base, shaped_text)
    encoded_text = ''.join(chr(slot_map[ch]) if ch in slot_map else ch for ch in shaped_text).encode('utf-8')
    result = replace_messages(result, encoded_text)
    a.output.parent.mkdir(parents=True, exist_ok=True); a.output.write_bytes(result)
    print(f'wrote {a.output} ({len(result)} bytes; patched {count} glyph slots; CJK slot remap; experimental shaping)')

if __name__ == '__main__': main()
