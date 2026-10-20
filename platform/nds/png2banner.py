#!/usr/bin/env python3
"""Convert a 32x32 PNG into an NDS banner bitmap.

ndstool expects a 32x32 BMP with 16 palette entries. Palette index 0 is
transparent in the DS launcher and Twilight Menu, so that slot is left
unused and the background is stored as opaque black.
"""
import struct
import sys
from PIL import Image


def main():
    if len(sys.argv) != 3:
        sys.exit("usage: png2banner.py input.png output.bmp")
    src, dst = sys.argv[1], sys.argv[2]
    image = Image.open(src).convert("RGBA")
    if image.size != (32, 32):
        sys.exit("NDS banner icon must be 32x32, got %dx%d" % image.size)
    # Transparent pixels become black, so the banner background is solid.
    matte = Image.new("RGBA", image.size, (0, 0, 0, 255))
    image = Image.alpha_composite(matte, image).convert("RGB")

    quantized = image.quantize(
        colors=16,
        method=Image.Quantize.MEDIANCUT,
        dither=Image.Dither.NONE,
    )
    palette = list(quantized.getpalette()[:48])
    indices = list(quantized.getdata())
    background = min(range(16), key=lambda i: _dist2(_rgb(palette, i), (0, 0, 0)))
    # One slot has to stay empty for the transparent color. Fold the shade
    # closest to the background into it; on this icon that shade is the
    # near-black fringe, which is already black in 15-bit color.
    nearest = min(
        (i for i in range(16) if i != background),
        key=lambda i: _dist2(_rgb(palette, i), _rgb(palette, background)),
    )
    indices = [background if p == nearest else p for p in indices]
    if nearest != 0:
        indices = [nearest if p == 0 else 0 if p == nearest else p for p in indices]
        palette[0:3], palette[nearest * 3:nearest * 3 + 3] = (
            palette[nearest * 3:nearest * 3 + 3],
            palette[0:3],
        )
        if background == 0:
            background = nearest
    palette[background * 3:background * 3 + 3] = [0, 0, 0]
    if any(p == 0 for p in indices):
        sys.exit("banner background was left on the transparent palette entry")
    write_bmp(dst, indices, palette)


def _rgb(palette, index):
    return tuple(palette[index * 3:index * 3 + 3])


def _dist2(a, b):
    return (a[0] - b[0]) ** 2 + (a[1] - b[1]) ** 2 + (a[2] - b[2]) ** 2


def write_bmp(path, indices, palette_rgb):
    palette = bytearray()
    for i in range(16):
        r, g, b = palette_rgb[i * 3:i * 3 + 3]
        palette += bytes((b, g, r, 0))
    pixels = bytearray()
    for y in range(31, -1, -1):
        pixels += bytes(indices[y * 32:(y + 1) * 32])
    header_size = 14 + 40 + len(palette)
    file_size = header_size + len(pixels)
    file_header = struct.pack("<2sIHHI", b"BM", file_size, 0, 0, header_size)
    info_header = struct.pack(
        "<IiiHHIIiiII",
        40, 32, 32, 1, 8, 0, len(pixels), 0, 0, 16, 0,
    )
    with open(path, "wb") as out:
        out.write(file_header)
        out.write(info_header)
        out.write(palette)
        out.write(pixels)


if __name__ == "__main__":
    main()
