#!/usr/bin/env python3
"""Convert a 32x32 PNG into an NDS banner bitmap.

ndstool expects a 32x32 BMP with 16 palette entries. Palette index 0 is
transparent in the DS launcher and Twilight Menu, so the darkest color
(the icon background) is moved to that slot.
"""
import struct
import sys
from PIL import Image


def main():
    if len(sys.argv) != 3:
        sys.exit("usage: png2banner.py input.png output.bmp")
    src, dst = sys.argv[1], sys.argv[2]
    image = Image.open(src).convert("RGB")
    if image.size != (32, 32):
        sys.exit("NDS banner icon must be 32x32, got %dx%d" % image.size)

    quantized = image.quantize(
        colors=16,
        method=Image.Quantize.MEDIANCUT,
        dither=Image.Dither.NONE,
    )
    palette = quantized.getpalette()
    indices = list(quantized.getdata())
    darkest = min(
        range(16),
        key=lambda i: palette[i * 3] ** 2 + palette[i * 3 + 1] ** 2 + palette[i * 3 + 2] ** 2,
    )
    if darkest != 0:
        indices = [0 if p == darkest else darkest if p == 0 else p for p in indices]
        palette[0:3], palette[darkest * 3:darkest * 3 + 3] = (
            palette[darkest * 3:darkest * 3 + 3],
            palette[0:3],
        )
    palette[0:3] = [0, 0, 0]
    write_bmp(dst, indices, palette[:48])


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
