"""Makes the port's icon (the SP monogram, packaging/icon/monogram.png) as .ico and .png. Needs Pillow.
usage: make_icon.py launcher/spartanport.ico launcher/spartanport.png"""
import sys
from pathlib import Path
from PIL import Image, ImageFilter

MASTER = Path(__file__).resolve().parent.parent / 'icon' / 'monogram.png'
SIZES = (16, 24, 32, 48, 64, 128, 256)
MARGIN = 0.04  # of the side, around the monogram


def square(image):
    """The monogram cropped to its pixels and centred on a transparent square with a small margin."""
    image = image.convert('RGBA')
    image = image.crop(image.getchannel('A').point(lambda a: 255 if a > 8 else 0).getbbox())
    side = round(max(image.size) * (1 + 2 * MARGIN))
    out = Image.new('RGBA', (side, side), (0, 0, 0, 0))
    out.paste(image, ((side - image.width) // 2, (side - image.height) // 2))
    return out


def frame(big, size):
    small = big.resize((size, size), Image.LANCZOS)
    # The metal's fine shading blurs away at the smallest sizes: sharpen them so the letters keep an edge.
    return small.filter(ImageFilter.UnsharpMask(radius=1, percent=120, threshold=0)) if size <= 24 else small


def main(ico, png):
    with Image.open(MASTER) as master:
        big = square(master)
    frames = [frame(big, size) for size in SIZES]
    frames[-1].save(ico, sizes=[(s, s) for s in SIZES], append_images=frames[:-1])
    frames[-1].save(png)


if __name__ == '__main__':
    main(*sys.argv[1:3])
