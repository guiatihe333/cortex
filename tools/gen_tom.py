#!/usr/bin/env python3
"""Tom, drawn as a cartoon from his photos: a lynx point tabby, warm grey-fawn with dark tabby stripes, a long ringed
tail with a dark tip, darker ears, a light muzzle and chin, a peach nose, big pale blue eyes. Each frame is painted in
flat shapes on a layer, its silhouette grown into one outline round the whole of him (no lines where his parts
overlap), then shaded (a soft shadow under, a light along the back) and the details drawn on top (stripes, eyes,
nose, whiskers). Drawn at 8x and brought down (anti-aliased). Frames: six steps, a jump, sitting, a blink, asleep,
a stretch.

  gen_tom.py preview OUT.png         the frames, large
  gen_tom.py header OUT.h            app/tom_art.h: the header's (56 x 28) and the start screen's (112 x 56) frames,
                                     each pixel A R G B a nibble (A 0: nothing there)"""
import math
import sys
from PIL import Image, ImageChops, ImageDraw, ImageFilter

SS = 8
BW, BH = 112, 56

OUT = (40, 30, 26, 255)
FAWN = (160, 144, 128, 255)
FAWN_D = (126, 110, 96, 255)
STRIPE = (86, 72, 62, 255)
EAR = (74, 62, 56, 255)
EAR_IN = (196, 150, 140, 255)
CREAM = (236, 228, 214, 255)
NOSE = (238, 150, 130, 255)
IRIS = (150, 208, 236, 255)
IRIS_D = (92, 160, 200, 255)
PUPIL = (24, 28, 34, 255)
WHITE = (255, 255, 255, 255)
WHISK = (250, 246, 238, 230)


def P(x, y):
    return (round(x * SS), round(y * SS))


class Layer:
    def __init__(self):
        self.im = Image.new('RGBA', (BW * SS, BH * SS), (0, 0, 0, 0))
        self.d = ImageDraw.Draw(self.im)

    def ell(self, cx, cy, rx, ry, fill):
        self.d.ellipse([P(cx - rx, cy - ry), P(cx + rx, cy + ry)], fill=fill)

    def poly(self, pts, fill):
        self.d.polygon([P(*p) for p in pts], fill=fill)

    def limb(self, x0, y0, x1, y1, r0, r1, fill):
        n = 24
        for k in range(n + 1):
            t = k / n
            r = r0 + (r1 - r0) * t
            self.ell(x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, r, r, fill)

    def line(self, pts, col, w):
        self.d.line([P(*p) for p in pts], fill=col, width=max(1, round(w * SS)), joint='curve')

    def tube(self, pts, r0, r1, colf):
        n = len(pts)
        for k, (x, y) in enumerate(pts):
            r = r0 + (r1 - r0) * k / max(1, n - 1)
            self.ell(x, y, r, r, colf(k, n))


def bez(p0, p1, p2, p3, n=24):
    out = []
    for i in range(n + 1):
        t = i / n
        a = (1 - t) ** 3, 3 * (1 - t) ** 2 * t, 3 * (1 - t) * t * t, t ** 3
        out.append((a[0] * p0[0] + a[1] * p1[0] + a[2] * p2[0] + a[3] * p3[0],
                    a[0] * p0[1] + a[1] * p1[1] + a[2] * p2[1] + a[3] * p3[1]))
    return out


def ringed(k, n):
    return STRIPE if (k // 5) % 2 == 1 or k > n - 6 else FAWN


def finish(fill, details, shade_box=None, outline=1.5):
    """one outline round the silhouette, the shading inside, then the details"""
    a = fill.im.split()[3]
    grown = a.filter(ImageFilter.MaxFilter(int(outline * SS) * 2 + 1))
    out = Image.new('RGBA', fill.im.size, OUT)
    out.putalpha(grown)
    img = Image.alpha_composite(out, fill.im)
    if shade_box:
        x0, y0, x1, y1 = shade_box
        sh = Image.new('L', fill.im.size, 0)
        ImageDraw.Draw(sh).ellipse([P(x0, y0 + (y1 - y0) * 0.55), P(x1, y1 + (y1 - y0) * 0.4)], fill=90)
        sh = ImageChops.multiply(sh.filter(ImageFilter.GaussianBlur(3 * SS)), a)
        dark = Image.new('RGBA', fill.im.size, (60, 44, 36, 255))
        dark.putalpha(sh)
        img = Image.alpha_composite(img, dark)
        hl = Image.new('L', fill.im.size, 0)
        ImageDraw.Draw(hl).ellipse([P(x0 + 4, y0 - 1), P(x1 - 4, y0 + (y1 - y0) * 0.35)], fill=70)
        hl = ImageChops.multiply(hl.filter(ImageFilter.GaussianBlur(2.5 * SS)), a)
        light = Image.new('RGBA', fill.im.size, (255, 246, 230, 255))
        light.putalpha(hl)
        img = Image.alpha_composite(img, light)
    return Image.alpha_composite(img, details.im)


def eye(det, ex, ey, rx, ry, look=0.0, closed=False):
    if closed:
        det.line(bez((ex - rx, ey), (ex - rx * 0.4, ey + ry * 0.7), (ex + rx * 0.4, ey + ry * 0.7), (ex + rx, ey), 10), OUT, 0.9)
        return
    det.ell(ex, ey, rx + 0.7, ry + 0.7, OUT)
    det.ell(ex, ey, rx, ry, IRIS)
    det.ell(ex, ey + ry * 0.35, rx * 0.95, ry * 0.6, IRIS_D)
    det.ell(ex + look, ey + 0.1, rx * 0.36, ry * 0.78, PUPIL)
    det.ell(ex + look + rx * 0.45, ey - ry * 0.42, rx * 0.28, rx * 0.28, WHITE)
    det.ell(ex + look - rx * 0.25, ey + ry * 0.45, rx * 0.12, rx * 0.12, WHITE)


def head_profile(fill, det, cx, cy, closed=False):
    for ex, tip in ((cx - 5.5, (cx - 9.5, cy - 20)), (cx + 4.5, (cx + 6.0, cy - 20.5))):
        fill.poly([(ex - 5, cy - 6), (ex + 5, cy - 7.5), tip], EAR)
        det.poly([(ex - 2.4, cy - 7.5), (ex + 2.6, cy - 8.3),
                  (tip[0] * 0.75 + ex * 0.25, tip[1] * 0.8 + (cy - 8) * 0.2)], EAR_IN)
    fill.ell(cx, cy, 13.5, 11.5, FAWN)
    fill.ell(cx - 6, cy + 4, 7, 6, FAWN)
    det.ell(cx + 8, cy + 4.8, 6.6, 4.6, CREAM)
    det.ell(cx + 4.5, cy + 8, 7.5, 3.4, CREAM)
    for k in range(3):
        x = cx - 4.5 + k * 3.2
        det.line([(x - 0.5, cy - 10.8), (x + 0.6, cy - 5.5)], STRIPE, 1.15)
    det.line(bez((cx - 12.5, cy - 1.5), (cx - 10, cy - 1), (cx - 8, cy), (cx - 6, cy + 0.5), 8), STRIPE, 1.0)
    det.line(bez((cx - 12.5, cy + 2.5), (cx - 10, cy + 3), (cx - 8.5, cy + 3.5), (cx - 6.5, cy + 4), 8), STRIPE, 1.0)
    eye(det, cx + 3.6, cy - 1.6, 3.6, 3.5, look=0.8, closed=closed)
    det.poly([(cx + 12, cy + 2.4), (cx + 14.2, cy + 2.2), (cx + 13.2, cy + 4)], NOSE)
    det.line([(cx + 13.2, cy + 4), (cx + 13, cy + 5.2), (cx + 11.5, cy + 6.1)], OUT, 0.6)
    for dy in (-0.7, 0.9, 2.4):
        det.line(bez((cx + 13, cy + 4.3 + dy * 0.3), (cx + 17, cy + 3.5 + dy), (cx + 20, cy + 3 + dy * 1.5),
                     (cx + 24, cy + 2.6 + dy * 2), 8), WHISK, 0.4)


def walking(phase, jump=False, closed=False):
    fill, det = Layer(), Layer()
    bob = 0 if jump else 0.9 * math.sin(phase * 4 * math.pi)
    by = 35 + bob - (3 if jump else 0)
    sw = math.sin(phase * 2 * math.pi)
    fill.tube(bez((26, by - 4), (12, by - 6 + 4 * sw), (8, by - 22), (17 + 4 * sw, by - 27), 40), 3.1, 2.2, ringed)
    legs = [(35, 0.0, True), (68, 0.5, True), (39, 0.5, False), (72, 0.0, False)]

    def leg_end(x, off):
        a = (-1.05 if x < 52 else 1.05) if jump else 0.55 * math.sin((phase + off) * 2 * math.pi)
        return x + 12 * math.sin(a), by + 6 + 12 * math.cos(a)

    for x, off, far in legs:
        if far:
            x1, y1 = leg_end(x, off)
            fill.limb(x, by + 3, x1, y1, 3.6, 2.8, FAWN_D)
            fill.ell(x1 + 1.0, y1 + 0.4, 3.3, 2.3, FAWN_D)
    fill.ell(53, by, 27, 12, FAWN)
    fill.ell(70, by - 2, 10, 10, FAWN)
    det.ell(56, by + 6.5, 18, 4.6, CREAM)
    for k in range(5):
        x = 36 + k * 7.5
        det.line(bez((x - 2, by - 11.5), (x + 1, by - 7), (x - 1, by - 2), (x + 1.5, by + 3), 10), STRIPE, 1.7)
    for x, off, far in legs:
        if far:
            continue
        x1, y1 = leg_end(x, off)
        fill.limb(x, by + 3, x1, y1, 3.8, 3.0, FAWN)
        for k in range(2):
            t = 0.35 + 0.25 * k
            px, py = x + (x1 - x) * t, by + 3 + (y1 - by - 3) * t
            det.line([(px - 2.7, py), (px + 2.7, py)], STRIPE, 1.0)
        fill.ell(x1 + 1.2, y1 + 0.5, 3.6, 2.4, CREAM)
    head_profile(fill, det, 85, by - 10 - (2 if jump else 0), closed=closed)
    return finish(fill, det, (28, by - 12, 80, by + 12))


def sitting(closed=False):
    fill, det = Layer(), Layer()
    cx = 56
    fill.tube(bez((cx + 11, 51), (cx + 26, 53), (cx + 31, 43), (cx + 24, 34), 36), 3.0, 2.0, ringed)
    fill.ell(cx, 38, 14.5, 16, FAWN)
    det.ell(cx, 41, 7.5, 11, CREAM)
    for k in range(3):
        y = 31 + k * 6
        det.line(bez((cx - 14, y), (cx - 11, y + 2), (cx - 9, y + 1), (cx - 7, y + 3), 8), STRIPE, 1.4)
        det.line(bez((cx + 14, y), (cx + 11, y + 2), (cx + 9, y + 1), (cx + 7, y + 3), 8), STRIPE, 1.4)
    for dx in (-5.5, 5.5):
        fill.limb(cx + dx, 40, cx + dx, 50, 3.4, 3.0, FAWN)
        fill.ell(cx + dx, 51, 3.8, 2.4, CREAM)
        det.line([(cx + dx - 1, 50.5), (cx + dx - 1, 52.3)], FAWN_D, 0.5)
        det.line([(cx + dx + 1, 50.5), (cx + dx + 1, 52.3)], FAWN_D, 0.5)
    hy = 20
    for side in (-1, 1):
        ex = cx + side * 8
        fill.poly([(ex - 5, hy - 4), (ex + 5, hy - 4), (ex + side * 3.5, hy - 18)], EAR)
        det.poly([(ex - 2.4, hy - 5.5), (ex + 2.4, hy - 5.5), (ex + side * 2.6, hy - 13.5)], EAR_IN)
    fill.ell(cx, hy, 14, 12, FAWN)
    det.ell(cx, hy + 6.2, 7.8, 4.8, CREAM)
    for k in range(3):
        x = cx - 3 + k * 3
        det.line([(x, hy - 11), (x + (k - 1) * 0.7, hy - 6)], STRIPE, 1.15)
    for side in (-1, 1):
        eye(det, cx + side * 5.6, hy - 0.3, 3.3, 3.3, closed=closed)
        det.line(bez((cx + side * 9, hy + 4.5), (cx + side * 13, hy + 3.8), (cx + side * 17, hy + 3.4),
                     (cx + side * 22, hy + 3), 8), WHISK, 0.4)
        det.line(bez((cx + side * 9, hy + 6), (cx + side * 13, hy + 6.3), (cx + side * 17, hy + 6.8),
                     (cx + side * 22, hy + 7.5), 8), WHISK, 0.4)
    det.poly([(cx - 1.7, hy + 3.4), (cx + 1.7, hy + 3.4), (cx, hy + 5.3)], NOSE)
    det.line([(cx, hy + 5.3), (cx - 1.7, hy + 6.7)], OUT, 0.6)
    det.line([(cx, hy + 5.3), (cx + 1.7, hy + 6.7)], OUT, 0.6)
    return finish(fill, det, (cx - 14, 24, cx + 14, 52))


def asleep():
    fill, det = Layer(), Layer()
    cx, cy = 54, 44
    fill.ell(cx - 4, cy, 24, 10, FAWN)
    fill.tube(bez((cx - 24, cy + 4), (cx - 32, cy - 6), (cx - 12, cy + 12), (cx + 8, cy + 10), 36), 3.0, 2.2, ringed)
    for k in range(5):
        x = cx - 20 + k * 7
        det.line(bez((x - 1, cy - 9.5), (x + 1, cy - 6), (x - 1, cy - 2), (x + 1, cy + 2), 8), STRIPE, 1.5)
    hx, hy = cx + 18, cy - 3
    for ex, tip in ((hx - 5, (hx - 8.5, hy - 17)), (hx + 4, (hx + 6, hy - 17.5))):
        fill.poly([(ex - 4.6, hy - 5), (ex + 4.6, hy - 6), tip], EAR)
        det.poly([(ex - 2.2, hy - 6.5), (ex + 2.2, hy - 7),
                  (tip[0] * 0.75 + ex * 0.25, tip[1] * 0.8 + (hy - 7) * 0.2)], EAR_IN)
    fill.ell(hx, hy, 12, 10, FAWN)
    fill.ell(hx + 2, cy + 7, 7, 3, CREAM)
    det.ell(hx + 5, hy + 4, 6, 4, CREAM)
    for k in range(3):
        x = hx - 4 + k * 3
        det.line([(x, hy - 9.5), (x + 0.5, hy - 5)], STRIPE, 1.1)
    eye(det, hx + 1, hy - 0.5, 3.4, 3, closed=True)
    eye(det, hx + 8, hy - 0.8, 2.4, 2.4, closed=True)
    det.poly([(hx + 9.5, hy + 2.4), (hx + 11.6, hy + 2.3), (hx + 10.6, hy + 3.8)], NOSE)
    return finish(fill, det, (cx - 26, cy - 9, cx + 20, cy + 10))


def stretch():
    fill, det = Layer(), Layer()
    by = 36
    fill.tube(bez((22, by - 10), (12, by - 20), (14, by - 28), (22, by - 30), 36), 3.0, 2.2, ringed)
    fill.limb(30, by - 6, 32, by + 15, 3.8, 3.0, FAWN)
    fill.limb(36, by - 6, 38, by + 15, 3.8, 3.0, FAWN_D)
    fill.ell(33, by + 16, 3.6, 2.4, CREAM)
    for k, (x, y) in enumerate(bez((28, by - 9), (44, by - 12), (58, by - 2), (70, by + 8), 30)):
        r = 10 - 3 * k / 30
        fill.ell(x, y, r, r, FAWN)
    fill.limb(66, by + 10, 88, by + 15, 3.4, 2.8, FAWN)
    fill.limb(64, by + 12, 84, by + 16, 3.4, 2.8, FAWN_D)
    fill.ell(90, by + 15.5, 3.6, 2.2, CREAM)
    for k in range(4):
        x = 34 + k * 8
        det.line(bez((x, by - 15 + k * 3), (x + 2, by - 10 + k * 3), (x + 1, by - 6 + k * 3), (x + 2, by - 2 + k * 3), 8),
                 STRIPE, 1.5)
    head_profile(fill, det, 74, by + 2, closed=True)
    return finish(fill, det, (28, by - 14, 80, by + 14))


def frames():
    out = [walking(k / 6.0) for k in range(6)]
    out.append(walking(0.0, jump=True))
    out.append(sitting())
    out.append(sitting(closed=True))
    out.append(asleep())
    out.append(stretch())
    return out


def down(im, w, h):
    return im.resize((w, h), Image.LANCZOS)


def preview(path):
    fs = frames()
    big = [down(f, BW * 3, BH * 3) for f in fs]
    small = [down(f, 56, 28).resize((56 * 4, 28 * 4), Image.NEAREST) for f in (fs[0], fs[7], fs[9], fs[10])]
    rows = (len(big) + 2) // 3
    sheet = Image.new('RGB', (3 * (BW * 3 + 10), rows * (BH * 3 + 10) + 28 * 4 + 20), (40, 14, 60))
    for i, f in enumerate(big):
        sheet.paste(f, ((i % 3) * (BW * 3 + 10), (i // 3) * (BH * 3 + 10)), f)
    y = rows * (BH * 3 + 10) + 10
    for i, f in enumerate(small):
        sheet.paste(f, (i * (56 * 4 + 10), y), f)
    sheet.save(path)


def pack(im):
    px = im.load()
    out = []
    for y in range(im.height):
        for x in range(im.width):
            r, g, b, a = px[x, y]
            a4 = (a * 15 + 127) // 255
            out.append(0 if a4 == 0 else (a4 << 12) | ((r >> 4) << 8) | ((g >> 4) << 4) | (b >> 4))
    return out


def header(path):
    fs = frames()
    lines = ['/* SPDX-License-Identifier: MPL-2.0 */',
             '/* Tom, drawn (tools/gen_tom.py, from his photos): frames 0..5 his steps, 6 a jump, 7 sitting, 8 a blink,',
             ' * 9 asleep, 10 a stretch; each pixel A R G B a nibble (A 0: nothing there). TOM_S: 56 x 28 (the header), all',
             ' * eleven; TOM_L: 112 x 56 (the start screen), the first nine */',
             '#define TOM_NF 11', '#define TOM_NL 9', '#define TOM_SW 56', '#define TOM_SH 28', '#define TOM_LW 112',
             '#define TOM_LH 56']
    for name, (w, h), n in (('TOM_S', (56, 28), 11), ('TOM_L', (112, 56), 9)):
        lines.append('static const uint16_t %s[%d][%d] = {' % (name, n, w * h))
        for f in fs[:n]:
            lines.append('    {' + ','.join('0x%X' % v for v in pack(down(f, w, h))) + '},')
        lines.append('};')
    open(path, 'w').write('\n'.join(lines) + '\n')


if __name__ == '__main__':
    {'preview': preview, 'header': header}[sys.argv[1]](sys.argv[2])
