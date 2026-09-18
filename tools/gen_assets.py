#!/usr/bin/env python3
"""gen_assets.py - builds app/src/main/assets/stone_pack.stpk

STONE ships its artwork as one uncompressed, mip-mapped RGBA texture pack
instead of a folder of PNGs. Three reasons, all deliberate:

  * there is no PNG/JPEG decoder anywhere in the project (no libpng, no
    stb_image, no Java) - the renderer wants raw RGBA and nothing else;
  * the pack is stored *uncompressed* inside the APK (see aaptOptions
    noCompress 'stpk' in app/build.gradle), so AAsset_getBuffer() hands the
    loader a pointer straight into the mmap'd APK: zero copies, zero
    inflate, no malloc of the whole file;
  * every texture carries a full mip chain, so the loader uploads the level
    that matches the device's screen instead of a 1024px master on a 720p
    phone. The masters stay in the file for high-density panels and tablets.

Run from the project root:

    python3 tools/gen_assets.py

Created by sobing4413 - Exter Interactive.
"""

import math
import os
import struct
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

# --------------------------------------------------------------- palette
BG        = (14, 17, 22)
SURFACE   = (23, 28, 36)
PRIMARY   = (39, 217, 163)
PRIM_DARK = (20, 88, 74)
ACCENT    = (249, 138, 60)
WARN      = (250, 204, 21)
DANGER    = (242, 85, 90)
TEXT      = (243, 246, 250)

SIZE = 1024
OUT  = os.path.join("app", "src", "main", "assets", "stone_pack.stpk")

MAGIC   = b"STPK"
VERSION = 1
FMT_RGBA8 = 0
NAME_LEN  = 32


# ------------------------------------------------------------------ utils
def lerp(a, b, t):
    return tuple(int(round(a[i] + (b[i] - a[i]) * t)) for i in range(3))


def vgrad(w, h, top, bottom, curve=1.0):
    """Vertical gradient as an RGBA image."""
    t = np.linspace(0.0, 1.0, h, dtype=np.float32) ** curve
    t = t[:, None]
    arr = np.zeros((h, w, 4), dtype=np.float32)
    for c in range(3):
        arr[:, :, c] = top[c] + (bottom[c] - top[c]) * t
    arr[:, :, 3] = 255.0
    return Image.fromarray(arr.astype(np.uint8), "RGBA")


def radial_glow(w, h, cx, cy, radius, color, strength=1.0, falloff=2.0):
    """Additive radial glow layer."""
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    d = np.sqrt((xx - cx) ** 2 + (yy - cy) ** 2) / max(radius, 1.0)
    a = np.clip(1.0 - d, 0.0, 1.0) ** falloff * strength
    arr = np.zeros((h, w, 4), dtype=np.float32)
    for c in range(3):
        arr[:, :, c] = color[c]
    arr[:, :, 3] = a * 255.0
    return Image.fromarray(arr.astype(np.uint8), "RGBA")


def add_layer(base, layer):
    return Image.alpha_composite(base, layer)


def screen_blend(base, layer):
    """Additive-ish blend that keeps highlights from clipping to white."""
    b = np.asarray(base, dtype=np.float32) / 255.0
    l = np.asarray(layer, dtype=np.float32) / 255.0
    a = l[:, :, 3:4]
    rgb = 1.0 - (1.0 - b[:, :, :3]) * (1.0 - l[:, :, :3] * a)
    out = np.concatenate([rgb, b[:, :, 3:4]], axis=2)
    return Image.fromarray((np.clip(out, 0, 1) * 255.0).astype(np.uint8), "RGBA")


def noise_layer(w, h, amount=6, seed=7):
    rng = np.random.default_rng(seed)
    n = rng.normal(0.0, amount, (h, w, 1)).astype(np.float32)
    arr = np.concatenate([np.repeat(n, 3, axis=2), np.full((h, w, 1), 255.0)], axis=2)
    arr[:, :, :3] += 128.0
    return Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8), "RGBA")


def grain(img, amount=5, seed=3):
    """Film grain keeps big flat gradients from banding on cheap panels."""
    rng = np.random.default_rng(seed)
    a = np.asarray(img, dtype=np.float32)
    n = rng.normal(0.0, amount, a.shape[:2] + (1,))
    a[:, :, :3] = np.clip(a[:, :, :3] + n, 0, 255)
    return Image.fromarray(a.astype(np.uint8), "RGBA")


def draw_ring(d, cx, cy, r, width, color, alpha=255, start=0, end=360):
    box = (cx - r, cy - r, cx + r, cy + r)
    d.arc(box, start, end, fill=color + (alpha,), width=width)


# ------------------------------------------------------------- artworks

# The app's real brand mark (replaces the old procedural hex+"S" glyph so the
# launch screen matches the launcher icon instead of a placeholder logo).
BRAND_ICON = os.path.join(os.path.dirname(__file__), "brand_icon.png")


def _paste_brand_mark(img, cx, cy, diameter):
    """Composites BRAND_ICON, centred at (cx, cy), scaled to `diameter`."""
    mark = Image.open(BRAND_ICON).convert("RGBA")
    mark = mark.resize((diameter, diameter), Image.LANCZOS)
    layer = Image.new("RGBA", img.size, (0, 0, 0, 0))
    layer.paste(mark, (int(cx - diameter / 2), int(cy - diameter / 2)), mark)
    return Image.alpha_composite(img, layer)


def art_splash():
    """Launch screen master: brand mark, orbit rings, energy bloom."""
    img = vgrad(SIZE, SIZE, (10, 13, 18), (18, 26, 32), curve=1.35)
    img = add_layer(img, radial_glow(SIZE, SIZE, 512, 430, 620, PRIMARY, 0.22, 2.4))
    img = add_layer(img, radial_glow(SIZE, SIZE, 250, 820, 480, PRIM_DARK, 0.30, 2.0))
    img = add_layer(img, radial_glow(SIZE, SIZE, 830, 210, 420, ACCENT, 0.10, 2.6))

    ov = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    d = ImageDraw.Draw(ov)

    # orbit rings
    for i, (r, wdt, al) in enumerate([(430, 3, 40), (368, 2, 26), (300, 6, 70)]):
        draw_ring(d, 512, 430, r, wdt, PRIMARY, al)
    # progress arc, 3/4 sweep
    draw_ring(d, 512, 430, 300, 14, PRIMARY, 235, start=-90, end=170)
    draw_ring(d, 512, 430, 300, 14, ACCENT, 200, start=170, end=225)

    ov = ov.filter(ImageFilter.GaussianBlur(0.6))
    img = add_layer(img, ov)

    # brand mark, dropped in after the blur so it stays crisp
    img = _paste_brand_mark(img, 512, 430, 340)

    ov = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    d = ImageDraw.Draw(ov)

    # orbiting nodes
    for k, (ang, rad, col) in enumerate([(-30, 430, PRIMARY), (72, 430, ACCENT),
                                         (188, 368, WARN), (250, 430, PRIMARY)]):
        a = math.radians(ang)
        x, y = 512 + math.cos(a) * rad, 430 + math.sin(a) * rad
        d.ellipse((x - 16, y - 16, x + 16, y + 16), fill=col + (255,))
        d.ellipse((x - 30, y - 30, x + 30, y + 30), outline=col + (70,), width=3)

    ov = ov.filter(ImageFilter.GaussianBlur(0.6))
    img = add_layer(img, ov)

    # baseline plinth + tagline rule
    ov2 = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    d2 = ImageDraw.Draw(ov2)
    d2.rounded_rectangle((330, 828, 694, 838), 5, fill=PRIMARY + (90,))
    d2.rounded_rectangle((430, 868, 594, 874), 3, fill=TEXT + (40,))
    img = add_layer(img, ov2)
    return grain(img, 4, 11)


def _figure(d, cx, cy, scale, color, pose="stand"):
    """Very stylised athlete silhouette built from capsules."""
    s = scale

    def cap(x0, y0, x1, y1, w, col=None):
        col = col or color
        d.line([(x0, y0), (x1, y1)], fill=col + (255,), width=int(w * s), joint="curve")
        for (px, py) in ((x0, y0), (x1, y1)):
            r = w * s * 0.5
            d.ellipse((px - r, py - r, px + r, py + r), fill=col + (255,))

    head_r = 34 * s
    d.ellipse((cx - head_r, cy - 210 * s - head_r, cx + head_r, cy - 210 * s + head_r),
              fill=color + (255,))
    cap(cx, cy - 170 * s, cx, cy - 40 * s, 40)          # torso

    if pose == "squat":
        cap(cx, cy - 40 * s, cx - 70 * s, cy + 40 * s, 30)
        cap(cx - 70 * s, cy + 40 * s, cx - 60 * s, cy + 150 * s, 28)
        cap(cx, cy - 40 * s, cx + 70 * s, cy + 40 * s, 30)
        cap(cx + 70 * s, cy + 40 * s, cx + 60 * s, cy + 150 * s, 28)
        cap(cx - 10 * s, cy - 150 * s, cx - 130 * s, cy - 150 * s, 24)
        cap(cx + 10 * s, cy - 150 * s, cx + 130 * s, cy - 150 * s, 24)
    elif pose == "run":
        cap(cx, cy - 40 * s, cx - 95 * s, cy + 60 * s, 30)
        cap(cx - 95 * s, cy + 60 * s, cx - 60 * s, cy + 150 * s, 26)
        cap(cx, cy - 40 * s, cx + 80 * s, cy + 80 * s, 30)
        cap(cx + 80 * s, cy + 80 * s, cx + 130 * s, cy + 150 * s, 26)
        cap(cx - 5 * s, cy - 150 * s, cx - 120 * s, cy - 90 * s, 24)
        cap(cx + 5 * s, cy - 150 * s, cx + 110 * s, cy - 200 * s, 24)
    else:
        cap(cx, cy - 40 * s, cx - 45 * s, cy + 150 * s, 30)
        cap(cx, cy - 40 * s, cx + 45 * s, cy + 150 * s, 30)
        cap(cx - 10 * s, cy - 150 * s, cx - 120 * s, cy - 60 * s, 24)
        cap(cx + 10 * s, cy - 150 * s, cx + 120 * s, cy - 60 * s, 24)


def _onboard_base(seed, glow_color):
    img = vgrad(SIZE, SIZE, (12, 16, 21), (20, 28, 35), curve=1.2)
    img = add_layer(img, radial_glow(SIZE, SIZE, 512, 560, 700, glow_color, 0.20, 2.2))
    ov = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    d = ImageDraw.Draw(ov)
    # faint grid floor
    for i in range(0, 14):
        y = 700 + i * i * 2.4
        if y > SIZE:
            break
        d.line([(0, y), (SIZE, y)], fill=TEXT + (max(4, 26 - i * 2),), width=2)
    for i in range(-8, 9):
        d.line([(512 + i * 40, 700), (512 + i * 190, SIZE)], fill=TEXT + (12,), width=2)
    img = add_layer(img, ov)
    return img


def art_onboard_workout():
    img = _onboard_base(1, PRIMARY)
    ov = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    d = ImageDraw.Draw(ov)
    draw_ring(d, 512, 470, 330, 4, PRIMARY, 45)
    draw_ring(d, 512, 470, 330, 18, PRIMARY, 220, start=-90, end=130)
    _figure(d, 512, 520, 1.15, PRIMARY, "squat")
    # barbell
    d.rounded_rectangle((250, 342, 774, 366), 12, fill=TEXT + (210,))
    for x in (250, 726):
        d.rounded_rectangle((x - 40, 292, x + 88, 416), 18, fill=ACCENT + (235,))
    img = add_layer(img, ov)
    return grain(img, 4, 21)


def art_onboard_diet():
    img = _onboard_base(2, ACCENT)
    ov = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    d = ImageDraw.Draw(ov)
    # plate
    d.ellipse((202, 250, 822, 720), fill=SURFACE + (255,), outline=TEXT + (40,), width=6)
    d.ellipse((252, 282, 772, 688), outline=TEXT + (26,), width=4)
    # macro wedges
    d.pieslice((262, 290, 762, 680), -90, 40, fill=PRIMARY + (230,))
    d.pieslice((262, 290, 762, 680), 40, 170, fill=ACCENT + (230,))
    d.pieslice((262, 290, 762, 680), 170, 270, fill=WARN + (210,))
    d.ellipse((412, 402, 612, 568), fill=SURFACE + (255,))
    # cutlery
    d.rounded_rectangle((92, 300, 116, 690), 12, fill=TEXT + (170,))
    d.rounded_rectangle((70, 290, 140, 400), 24, fill=TEXT + (170,))
    d.rounded_rectangle((908, 300, 932, 690), 12, fill=TEXT + (170,))
    for k in range(3):
        d.rounded_rectangle((892 + k * 18, 280, 902 + k * 18, 380), 6, fill=TEXT + (170,))
    img = add_layer(img, ov)
    return grain(img, 4, 22)


def art_onboard_progress():
    img = _onboard_base(3, WARN)
    ov = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    d = ImageDraw.Draw(ov)
    # chart frame
    d.rounded_rectangle((132, 230, 892, 730), 34, fill=SURFACE + (225,),
                        outline=TEXT + (32,), width=4)
    for k in range(4):
        y = 320 + k * 110
        d.line([(190, y), (836, y)], fill=TEXT + (22,), width=3)
    pts = [(200, 640), (300, 592), (400, 604), (500, 512), (600, 470), (700, 392), (826, 326)]
    d.line(pts, fill=PRIMARY + (255,), width=14, joint="curve")
    for (x, y) in pts:
        d.ellipse((x - 15, y - 15, x + 15, y + 15), fill=BG + (255,))
        d.ellipse((x - 11, y - 11, x + 11, y + 11), fill=PRIMARY + (255,))
    # bars behind
    for k, h in enumerate([120, 176, 150, 220, 198, 260]):
        x = 210 + k * 104
        d.rounded_rectangle((x, 700 - h, x + 62, 700), 14, fill=ACCENT + (60,))
    # trend arrow
    d.polygon([(826, 300), (872, 326), (826, 352)], fill=PRIMARY + (255,))
    img = add_layer(img, ov)
    return grain(img, 4, 23)


def art_hero():
    """Five stacked banner bands, one per tab, sampled as sub-rects."""
    img = Image.new("RGBA", (SIZE, SIZE), BG + (255,))
    bands = [
        ((16, 34, 44), (26, 62, 58), PRIMARY, "grid"),
        ((38, 24, 16), (60, 38, 22), ACCENT, "wave"),
        ((16, 26, 44), (24, 44, 66), (96, 165, 250), "bars"),
        ((34, 30, 16), (58, 50, 22), WARN, "line"),
        ((26, 18, 34), (44, 30, 56), (167, 139, 250), "dots"),
    ]
    bh = SIZE // len(bands)
    for i, (top, bot, accent, kind) in enumerate(bands):
        band = vgrad(SIZE, bh, top, bot, 1.0)
        band = add_layer(band, radial_glow(SIZE, bh, 820, bh * 0.4, bh * 1.6, accent, 0.30, 2.0))
        ov = Image.new("RGBA", (SIZE, bh), (0, 0, 0, 0))
        d = ImageDraw.Draw(ov)
        if kind == "grid":
            for x in range(0, SIZE, 48):
                d.line([(x, 0), (x - 60, bh)], fill=accent + (26,), width=3)
        elif kind == "wave":
            for k in range(3):
                pts = [(x, bh * 0.55 + math.sin(x / 90.0 + k) * (26 + k * 10))
                       for x in range(0, SIZE + 1, 16)]
                d.line(pts, fill=accent + (40 - k * 10,), width=6, joint="curve")
        elif kind == "bars":
            for k in range(26):
                h = 20 + ((k * 37) % 90)
                d.rounded_rectangle((20 + k * 39, bh - h - 14, 20 + k * 39 + 22, bh - 14),
                                    8, fill=accent + (34,))
        elif kind == "line":
            pts = [(x, bh * 0.7 - (x / SIZE) ** 1.4 * bh * 0.45
                    + math.sin(x / 60.0) * 10) for x in range(0, SIZE + 1, 20)]
            d.line(pts, fill=accent + (55,), width=7, joint="curve")
        else:
            for k in range(70):
                x = (k * 137) % SIZE
                y = (k * 83) % bh
                r = 4 + (k % 5) * 3
                d.ellipse((x - r, y - r, x + r, y + r), fill=accent + (26,))
        d.rounded_rectangle((0, bh - 5, SIZE, bh), 0, fill=accent + (120,))
        band = add_layer(band, ov)
        img.paste(band, (0, i * bh))
    return grain(img, 3, 31)


BADGE_EMBLEMS = [
    "flame", "star", "bolt", "heart",
    "dumbbell", "plate", "chart", "scale",
    "crown", "shield", "clock", "leaf",
    "mountain", "trophy", "medal", "diamond",
]


def _emblem(d, kind, cx, cy, s, col):
    def R(x0, y0, x1, y1, r=6):
        d.rounded_rectangle((cx + x0 * s, cy + y0 * s, cx + x1 * s, cy + y1 * s),
                            int(r * s), fill=col + (255,))

    if kind == "flame":
        d.polygon([(cx, cy - 46 * s), (cx + 30 * s, cy - 4 * s), (cx + 24 * s, cy + 34 * s),
                   (cx, cy + 46 * s), (cx - 24 * s, cy + 34 * s), (cx - 30 * s, cy - 4 * s)],
                  fill=col + (255,))
    elif kind == "star":
        pts = []
        for k in range(10):
            a = math.radians(-90 + k * 36)
            r = 46 * s if k % 2 == 0 else 19 * s
            pts.append((cx + math.cos(a) * r, cy + math.sin(a) * r))
        d.polygon(pts, fill=col + (255,))
    elif kind == "bolt":
        d.polygon([(cx + 6 * s, cy - 48 * s), (cx - 30 * s, cy + 6 * s), (cx - 2 * s, cy + 6 * s),
                   (cx - 8 * s, cy + 48 * s), (cx + 30 * s, cy - 8 * s), (cx + 2 * s, cy - 8 * s)],
                  fill=col + (255,))
    elif kind == "heart":
        d.polygon([(cx - 44 * s, cy - 8 * s), (cx + 44 * s, cy - 8 * s), (cx, cy + 48 * s)],
                  fill=col + (255,))
        d.ellipse((cx - 44 * s, cy - 44 * s, cx - 2 * s, cy - 2 * s), fill=col + (255,))
        d.ellipse((cx + 2 * s, cy - 44 * s, cx + 44 * s, cy - 2 * s), fill=col + (255,))
    elif kind == "dumbbell":
        R(-46, -10, 46, 10, 8)
        R(-46, -30, -28, 30, 8)
        R(28, -30, 46, 30, 8)
    elif kind == "plate":
        d.ellipse((cx - 46 * s, cy - 46 * s, cx + 46 * s, cy + 46 * s),
                  outline=col + (255,), width=int(9 * s))
        d.ellipse((cx - 16 * s, cy - 16 * s, cx + 16 * s, cy + 16 * s), fill=col + (255,))
    elif kind == "chart":
        R(-42, 6, -20, 46, 5)
        R(-11, -16, 11, 46, 5)
        R(20, -40, 42, 46, 5)
    elif kind == "scale":
        R(-44, -6, 44, 8, 6)
        R(-6, -40, 6, -6, 4)
        d.arc((cx - 44 * s, cy + 2 * s, cx + 44 * s, cy + 60 * s), 0, 180,
              fill=col + (255,), width=int(9 * s))
    elif kind == "crown":
        d.polygon([(cx - 46 * s, cy + 34 * s), (cx - 46 * s, cy - 26 * s), (cx - 18 * s, cy + 2 * s),
                   (cx, cy - 40 * s), (cx + 18 * s, cy + 2 * s), (cx + 46 * s, cy - 26 * s),
                   (cx + 46 * s, cy + 34 * s)], fill=col + (255,))
    elif kind == "shield":
        d.polygon([(cx, cy - 48 * s), (cx + 40 * s, cy - 28 * s), (cx + 34 * s, cy + 20 * s),
                   (cx, cy + 50 * s), (cx - 34 * s, cy + 20 * s), (cx - 40 * s, cy - 28 * s)],
                  fill=col + (255,))
    elif kind == "clock":
        d.ellipse((cx - 46 * s, cy - 46 * s, cx + 46 * s, cy + 46 * s),
                  outline=col + (255,), width=int(8 * s))
        d.line([(cx, cy), (cx, cy - 28 * s)], fill=col + (255,), width=int(8 * s))
        d.line([(cx, cy), (cx + 22 * s, cy + 8 * s)], fill=col + (255,), width=int(8 * s))
    elif kind == "leaf":
        d.pieslice((cx - 48 * s, cy - 48 * s, cx + 40 * s, cy + 40 * s), 135, 315,
                   fill=col + (255,))
        d.line([(cx - 40 * s, cy + 40 * s), (cx + 30 * s, cy - 30 * s)],
               fill=col + (255,), width=int(8 * s))
    elif kind == "mountain":
        d.polygon([(cx - 48 * s, cy + 38 * s), (cx - 12 * s, cy - 30 * s), (cx + 6 * s, cy + 2 * s),
                   (cx + 20 * s, cy - 18 * s), (cx + 48 * s, cy + 38 * s)], fill=col + (255,))
    elif kind == "trophy":
        R(-30, -44, 30, 6, 10)
        R(-9, 6, 9, 28, 4)
        R(-30, 28, 30, 44, 7)
        d.arc((cx - 52 * s, cy - 42 * s, cx - 20 * s, cy - 6 * s), 90, 270,
              fill=col + (255,), width=int(7 * s))
        d.arc((cx + 20 * s, cy - 42 * s, cx + 52 * s, cy - 6 * s), 270, 90,
              fill=col + (255,), width=int(7 * s))
    elif kind == "medal":
        d.polygon([(cx - 30 * s, cy - 48 * s), (cx - 8 * s, cy - 48 * s), (cx + 4 * s, cy - 6 * s),
                   (cx - 18 * s, cy - 6 * s)], fill=col + (200,))
        d.polygon([(cx + 30 * s, cy - 48 * s), (cx + 8 * s, cy - 48 * s), (cx - 4 * s, cy - 6 * s),
                   (cx + 18 * s, cy - 6 * s)], fill=col + (200,))
        d.ellipse((cx - 30 * s, cy - 10 * s, cx + 30 * s, cy + 50 * s), fill=col + (255,))
    else:  # diamond
        d.polygon([(cx, cy - 48 * s), (cx + 44 * s, cy), (cx, cy + 48 * s), (cx - 44 * s, cy)],
                  fill=col + (255,))


def art_badges():
    """4x4 atlas of 256px achievement badges."""
    img = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    cell = SIZE // 4
    tints = [PRIMARY, ACCENT, WARN, (167, 139, 250),
             (96, 165, 250), PRIMARY, ACCENT, (52, 211, 153),
             WARN, (96, 165, 250), (167, 139, 250), PRIMARY,
             (251, 146, 60), WARN, (244, 114, 182), (125, 211, 252)]
    for i, kind in enumerate(BADGE_EMBLEMS):
        col = tints[i]
        tile = Image.new("RGBA", (cell, cell), (0, 0, 0, 0))
        d = ImageDraw.Draw(tile)
        c = cell // 2
        # medallion plate
        d.ellipse((14, 14, cell - 14, cell - 14), fill=lerp(BG, col, 0.16) + (255,))
        d.ellipse((14, 14, cell - 14, cell - 14), outline=col + (150,), width=5)
        d.ellipse((30, 30, cell - 30, cell - 30), outline=col + (55,), width=3)
        # notches around the rim
        for k in range(12):
            a = math.radians(k * 30)
            x, y = c + math.cos(a) * (c - 22), c + math.sin(a) * (c - 22)
            d.ellipse((x - 4, y - 4, x + 4, y + 4), fill=col + (120,))
        _emblem(d, kind, c, c, cell / 128.0, col)
        # gloss
        gl = Image.new("RGBA", (cell, cell), (0, 0, 0, 0))
        dg = ImageDraw.Draw(gl)
        dg.pieslice((14, 14, cell - 14, cell - 14), 185, 340, fill=(255, 255, 255, 26))
        tile = Image.alpha_composite(tile, gl)
        img.paste(tile, ((i % 4) * cell, (i // 4) * cell))
    return img


# ------------------------------------------------------------------ packer
def mip_chain(img):
    """Full box-filtered mip chain, level 0 first."""
    levels = []
    cur = img
    while True:
        levels.append(cur.tobytes())
        if cur.width == 1 and cur.height == 1:
            break
        cur = cur.resize((max(1, cur.width // 2), max(1, cur.height // 2)),
                         Image.LANCZOS)
    return levels


def build(entries, path):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    header_size = 4 + 4 + 4 + 4
    table_size = len(entries) * (NAME_LEN + 4 * 4 + 8 + 8)
    data_offset = header_size + table_size

    table = b""
    blobs = []
    cursor = data_offset
    for name, img in entries:
        levels = mip_chain(img)
        blob = b"".join(levels)
        nm = name.encode("ascii")
        assert len(nm) < NAME_LEN, name
        table += nm + b"\0" * (NAME_LEN - len(nm))
        table += struct.pack("<IIII", img.width, img.height, len(levels), FMT_RGBA8)
        table += struct.pack("<QQ", cursor, len(blob))
        blobs.append(blob)
        cursor += len(blob)

    with open(path, "wb") as f:
        f.write(MAGIC)
        f.write(struct.pack("<III", VERSION, len(entries), data_offset))
        f.write(table)
        for b in blobs:
            f.write(b)

    total = os.path.getsize(path)
    print("wrote %s" % path)
    for name, img in entries:
        print("   %-12s %dx%d RGBA8 + mips" % (name, img.width, img.height))
    print("   total %.2f MB (%d bytes), stored uncompressed in the APK"
          % (total / (1024.0 * 1024.0), total))


def main():
    root = os.getcwd()
    if not os.path.isdir(os.path.join(root, "app")):
        print("run this from the project root (the folder that holds app/)")
        return 1
    entries = [
        ("splash",   art_splash()),
        ("onboard1", art_onboard_workout()),
        ("onboard2", art_onboard_diet()),
        ("onboard3", art_onboard_progress()),
        ("hero",     art_hero()),
        ("badges",   art_badges()),
    ]
    build(entries, os.path.join(root, OUT))
    return 0


if __name__ == "__main__":
    sys.exit(main())
