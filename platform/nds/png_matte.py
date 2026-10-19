"""Flatten a PNG's alpha for grit.

grit ignores the alpha channel, so a white graphic on transparent white comes
out as one color. This writes an RGB copy where every transparent pixel is
magenta (FF00FF), which the .grit file names as the transparent color.
Images without alpha are copied through unchanged.

usage: png_matte.py in.png out.png
"""
import sys

from PIL import Image

MATTE = (255, 0, 255)


def main():
    src, dst = sys.argv[1], sys.argv[2]
    im = Image.open(src)
    has_alpha = im.mode in ("RGBA", "LA", "PA") or "transparency" in im.info
    rgba = im.convert("RGBA")
    out = Image.new("RGB", rgba.size)
    px = rgba.load()
    op = out.load()
    for y in range(rgba.height):
        for x in range(rgba.width):
            r, g, b, a = px[x, y]
            op[x, y] = MATTE if (has_alpha and a < 128) else (r, g, b)
    out.save(dst)


if __name__ == "__main__":
    main()
