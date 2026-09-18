#!/usr/bin/env python3
"""
gen_font.py - STONE font atlas generator.

Rasterizes ASCII 32..126 from a TTF into a single-channel (alpha) atlas,
compresses it with a simple RLE, encodes it as Base64 and emits a C source
file (font_data.c) that the app decodes at startup.

Usage:
    python3 tools/gen_font.py <regular.ttf> <bold.ttf> <output.c>

The generated file has no external dependency: the decoder lives in
renderer/font.c. Re-run this only if you want to change the typeface.
"""
import sys, base64
from PIL import Image, ImageFont, ImageDraw

FIRST, LAST = 32, 126
PX = 28  # rasterization size in pixels


def build(path):
    font = ImageFont.truetype(path, PX)
    ascent, descent = font.getmetrics()
    glyphs = []
    maxw = maxh = 0
    for c in range(FIRST, LAST + 1):
        ch = chr(c)
        bbox = font.getbbox(ch)
        if bbox is None:
            bbox = (0, 0, 0, 0)
        x0, y0, x1, y1 = bbox
        w, h = max(0, x1 - x0), max(0, y1 - y0)
        adv = font.getlength(ch)
        glyphs.append({"ch": ch, "w": w, "h": h, "bx": x0, "by": y0, "adv": adv})
        maxw, maxh = max(maxw, w), max(maxh, h)
    cw, chh = maxw + 2, maxh + 2
    cols = 16
    rows = (len(glyphs) + cols - 1) // cols
    img = Image.new("L", (cols * cw, rows * chh), 0)
    d = ImageDraw.Draw(img)
    for i, g in enumerate(glyphs):
        cx, cy = (i % cols) * cw, (i // cols) * chh
        d.text((cx + 1 - g["bx"], cy + 1 - g["by"]), g["ch"], fill=255, font=font)
        g["u"], g["v"] = cx + 1, cy + 1
    return img, glyphs, cw, chh, ascent, descent


def rle(data):
    out = bytearray()
    i = 0
    n = len(data)
    while i < n:
        v = data[i]
        run = 1
        while i + run < n and data[i + run] == v and run < 255:
            run += 1
        out.append(v)
        out.append(run)
        i += run
    return bytes(out)


def emit_b64(name, blob, f):
    b64 = base64.b64encode(blob).decode()
    f.write("static const char %s[] =\n" % name)
    for i in range(0, len(b64), 100):
        f.write('    "%s"\n' % b64[i:i + 100])
    f.write(";\n\n")


def main():
    reg, bold, out = sys.argv[1], sys.argv[2], sys.argv[3]
    with open(out, "w") as f:
        f.write("/* GENERATED FILE - do not edit by hand.\n"
                "   Produced by tools/gen_font.py. Typeface: Liberation Sans (SIL OFL 1.1). */\n")
        f.write('#include "font.h"\n\n')
        for tag, path in (("regular", reg), ("bold", bold)):
            img, glyphs, cw, chh, ascent, descent = build(path)
            blob = rle(img.tobytes())
            emit_b64("k_%s_b64" % tag, blob, f)
            f.write("static const StoneGlyph k_%s_glyphs[STONE_FONT_GLYPHS] = {\n" % tag)
            for g in glyphs:
                f.write("    {%d,%d,%d,%d,%d,%d,%.3ff},\n" %
                        (g["u"], g["v"], g["w"], g["h"], g["bx"], g["by"], g["adv"]))
            f.write("};\n\n")
            f.write("const StoneFontData k_stone_font_%s = {\n" % tag)
            f.write("    %d, %d, %d, %d, %.1ff, %.1ff, %.1ff, %.1ff,\n"
                    "    k_%s_b64, (int)sizeof(k_%s_b64) - 1, k_%s_glyphs\n};\n\n" %
                    (img.width, img.height, cw, chh, float(PX), float(ascent),
                     float(descent), float(ascent + descent), tag, tag, tag))
    print("wrote", out)


if __name__ == "__main__":
    main()
