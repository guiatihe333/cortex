#!/usr/bin/env python3
"""Tom, drawn as a cartoon (vector shapes, an outline, soft shading), from his photos: a lynx point tabby, warm
grey-fawn with dark tabby stripes, a long ringed tail with a dark tip, darker ears, a light muzzle and chin, a peach
nose, big pale blue eyes. Drawn at 8x and brought down (anti-aliased). Frames: six steps, a jump, sitting, a blink.

  gen_tom.py preview OUT.png         the frames, large
  gen_tom.py header OUT.h            app/tom_art.h: the header's (56 x 28) and the start screen's (112 x 56) frames,
                                     RGB444 + 4-bit alpha a pixel"""
import math
import sys
from PIL import Image, ImageDraw, ImageFilter

SS = 8                                   # supersampling
BW, BH = 112, 56                         # the base canvas (the start screen's size)

OUT = (44, 34, 30, 255)                  # the outline, a warm near-black
FAWN = (156, 140, 124, 255)
FAWN_D = (128, 112, 98, 255)             # shade
STRIPE = (82, 70, 62, 255)
EAR = (72, 62, 56, 255)
CREAM = (232, 224, 210, 255)
NOSE = (236, 150, 130, 255)
EYE = (156, 212, 236, 255)
PUPIL = (30, 34, 40, 255)
WHITE = (255, 255, 255, 255)


def P(x, y):
    return (x * SS, y * SS)


class Pen:
    def __init__(self):
        self.im = Image.new('RGBA', (BW * SS, BH * SS), (0, 0, 0, 0))
        self.d = ImageDraw.Draw(self.im)

    def ell(self, cx, cy, rx, ry, fill, out=1.2):
        if out:
            self.d.ellipse([P(cx - rx - out, cy - ry - out), P(cx + rx + out, cy + ry + out)], fill=OUT)
        self.d.ellipse([P(cx - rx, cy - ry), P(cx + rx, cy + ry)], fill=fill)

    def poly(self, pts, fill, out=1.2):
        if out:
            # an outline: the polygon grown by its edges drawn thick
            self.d.line([P(*p) for p in pts + [pts[0]]], fill=OUT, width=int(2 * out * SS), joint='curve')
        self.d.polygon([P(*p) for p in pts], fill=fill)

    def capsule(self, x0, y0, x1, y1, r, fill, out=1.2):
        """a limb: a thick rounded line from (x0, y0) to (x1, y1)"""
        if out:
            self.d.line([P(x0, y0), P(x1, y1)], fill=OUT, width=int(2 * (r + out) * SS))
            for x, y in ((x0, y0), (x1, y1)):
                self.d.ellipse([P(x - r - out, y - r - out), P(x + r + out, y + r + out)], fill=OUT)
        self.d.line([P(x0, y0), P(x1, y1)], fill=fill, width=int(2 * r * SS))
        for x, y in ((x0, y0), (x1, y1)):
            self.d.ellipse([P(x - r, y - r), P(x + r, y + r)], fill=fill)

    def stroke(self, pts, col, w):
        self.d.line([P(*p) for p in pts], fill=col, width=max(1, int(w * SS)), joint='curve')


def bez(p0, p1, p2, p3, n=24):
    out = []
    for i in range(n + 1):
        t = i / n
        a = (1 - t) ** 3, 3 * (1 - t) ** 2 * t, 3 * (1 - t) * t * t, t ** 3
        out.append((a[0] * p0[0] + a[1] * p1[0] + a[2] * p2[0] + a[3] * p3[0],
                    a[0] * p0[1] + a[1] * p1[1] + a[2] * p2[1] + a[3] * p3[1]))
    return out


def tail(pen, base, ctrl1, ctrl2, tip, r0=3.0, r1=2.2):
    pts = bez(base, ctrl1, ctrl2, tip, 30)
    for k, (x, y) in enumerate(pts):            # the outline first, the whole length
        r = r0 + (r1 - r0) * k / len(pts)
        pen.d.ellipse([P(x - r - 1.2, y - r - 1.2), P(x + r + 1.2, y + r + 1.2)], fill=OUT)
    for k, (x, y) in enumerate(pts):            # its rings, the tip dark
        r = r0 + (r1 - r0) * k / len(pts)
        ring = (k // 4) % 2 == 1 or k > len(pts) - 5
        pen.d.ellipse([P(x - r, y - r), P(x + r, y + r)], fill=STRIPE if ring else FAWN)


def head_side(pen, cx, cy, tilt=0.0, blink=False):
    """his head in profile, looking right (one big eye, a cartoon's three-quarter)"""
    # ears: dark, pointed, a lighter inside
    for ex, lean in ((cx - 5, -0.35), (cx + 4, 0.15)):
        pts = [(ex - 4.5, cy - 7), (ex + 4.5, cy - 7), (ex + 2 * lean * 10, cy - 19)]
        pen.poly(pts, EAR)
        inner = [(ex - 2.2, cy - 8), (ex + 2.2, cy - 8), (ex + 2 * lean * 10 * 0.8, cy - 15.5)]
        pen.poly(inner, FAWN_D, out=0)
    pen.ell(cx, cy, 12.5, 11, FAWN)                       # the head
    pen.ell(cx + 7.5, cy + 4.5, 6.5, 4.8, CREAM, out=0)    # the muzzle
    pen.ell(cx + 4, cy + 7.5, 7.5, 3.5, CREAM, out=0)      # the chin
    for k in range(3):                                    # the tabby M, stripes over the brow
        x = cx - 4 + k * 3.2
        pen.stroke([(x, cy - 10.5), (x + 0.8, cy - 6)], STRIPE, 1.1)
    pen.stroke([(cx - 11, cy - 1), (cx - 6, cy + 0.5)], STRIPE, 1.0)   # cheek stripes
    pen.stroke([(cx - 11, cy + 2.5), (cx - 6.5, cy + 3.5)], STRIPE, 1.0)
    ex, ey = cx + 3.5, cy - 1.5                           # the eye: big, pale blue, a highlight
    if blink:
        pen.stroke([(ex - 3.5, ey + 0.5), (ex, ey + 1.6), (ex + 3.5, ey + 0.5)], OUT, 1.0)
    else:
        pen.ell(ex, ey, 3.8, 3.4, EYE, out=0.9)
        pen.ell(ex + 0.9, ey + 0.2, 1.3, 2.6, PUPIL, out=0)
        pen.ell(ex + 1.8, ey - 1.3, 0.9, 0.9, WHITE, out=0)
    pen.poly([(cx + 11.5, cy + 2.2), (cx + 13.5, cy + 2.2), (cx + 12.6, cy + 3.8)], NOSE, out=0.5)   # the nose
    pen.stroke([(cx + 12.5, cy + 4), (cx + 12.3, cy + 5.2), (cx + 10.8, cy + 6)], OUT, 0.6)          # the mouth
    for dy in (-0.6, 1.2):                                # whiskers
        pen.stroke([(cx + 13, cy + 4.5 + dy), (cx + 22, cy + 3 + dy * 2)], (245, 240, 230, 255), 0.45)


def walking(phase, jump=False, blink=False):
    """a step at phase (0..1): legs swinging in pairs, the body bobbing, the tail swaying"""
    pen = Pen()
    bob = 0 if jump else 0.8 * math.sin(phase * 4 * math.pi)
    by = 35 + bob - (3 if jump else 0)
    sw = math.sin(phase * 2 * math.pi)
    tail(pen, (24, by - 3), (10, by - 8 + 4 * sw), (8, by - 22), (16 + 4 * sw, by - 26))
    # far legs (darker), then the body, then the near legs
    legs = [(34, 0.0, True), (66, 0.5, True), (38, 0.5, False), (70, 0.0, False)]
    for x, off, far in legs:
        if jump:
            a = -0.95 if x < 50 else 0.95   # (back legs back, front legs forward)
        else:
            a = 0.55 * math.sin((phase + off) * 2 * math.pi)
        x1, y1 = x + 12 * math.sin(a), by + 6 + 12 * math.cos(a)
        if far:
            pen.capsule(x, by + 4, x1, y1, 3.0, FAWN_D)
            pen.ell(x1 + 1, y1 + 0.5, 3.2, 2.2, FAWN_D, out=0.9)
    pen.ell(52, by, 26, 11.5, FAWN)                       # the body
    pen.ell(54, by + 6, 18, 5, CREAM, out=0)              # the belly
    for k in range(5):                                    # the stripes, curved bands over the back
        x = 36 + k * 7.5
        pen.stroke(bez((x - 2, by - 11), (x + 1, by - 7), (x - 1, by - 2), (x + 1.5, by + 3), 10), STRIPE, 1.6)
    for x, off, far in legs:
        if far:
            continue
        if jump:
            a = -1.05 if x < 50 else 1.05
        else:
            a = 0.55 * math.sin((phase + off) * 2 * math.pi)
        x1, y1 = x + 12 * math.sin(a), by + 6 + 12 * math.cos(a)
        pen.capsule(x, by + 4, x1, y1, 3.2, FAWN)
        for k in range(2):                                # a leg's own stripes
            t = 0.35 + 0.25 * k
            px, py = x + (x1 - x) * t, by + 4 + (y1 - by - 4) * t
            pen.stroke([(px - 2.6, py), (px + 2.6, py)], STRIPE, 1.0)
        pen.ell(x1 + 1, y1 + 0.5, 3.4, 2.3, CREAM, out=0.9)   # the paw
    head_side(pen, 84, by - 9 - (2 if jump else 0), blink=blink)
    return pen.im


def sitting(blink=False):
    """sitting, facing you a little: two eyes, the tail round his feet"""
    pen = Pen()
    cx = 56
    tail(pen, (cx + 12, 50), (cx + 26, 52), (cx + 30, 42), (cx + 24, 34), 2.8, 2.0)
    pen.ell(cx, 37, 14, 16, FAWN)                         # the body
    pen.ell(cx, 41, 7.5, 11, CREAM, out=0)                # the chest
    for k in range(3):
        y = 30 + k * 6
        pen.stroke(bez((cx - 13, y), (cx - 10, y + 2), (cx - 8, y + 1), (cx - 6, y + 3), 8), STRIPE, 1.4)
        pen.stroke(bez((cx + 13, y), (cx + 10, y + 2), (cx + 8, y + 1), (cx + 6, y + 3), 8), STRIPE, 1.4)
    for dx in (-5.5, 5.5):                                # the front paws
        pen.capsule(cx + dx, 40, cx + dx, 50, 3.0, FAWN)
        pen.ell(cx + dx, 51, 3.6, 2.3, CREAM, out=0.9)
    hy = 20
    for side in (-1, 1):                                  # ears
        ex = cx + side * 8
        pen.poly([(ex - 4.5, hy - 5), (ex + 4.5, hy - 5), (ex + side * 3, hy - 17)], EAR)
        pen.poly([(ex - 2.2, hy - 6), (ex + 2.2, hy - 6), (ex + side * 2.4, hy - 13.5)], FAWN_D, out=0)
    pen.ell(cx, hy, 13.5, 11.5, FAWN)                     # the head
    pen.ell(cx, hy + 6, 7.5, 4.8, CREAM, out=0)           # the muzzle
    for k in range(3):                                    # the M
        x = cx - 3 + k * 3
        pen.stroke([(x, hy - 10.5), (x + (k - 1) * 0.6, hy - 6)], STRIPE, 1.1)
    for side in (-1, 1):                                  # eyes
        ex, ey = cx + side * 5.5, hy - 0.5
        if blink:
            pen.stroke([(ex - 3.2, ey + 0.4), (ex, ey + 1.5), (ex + 3.2, ey + 0.4)], OUT, 1.0)
        else:
            pen.ell(ex, ey, 3.4, 3.4, EYE, out=0.9)
            pen.ell(ex, ey + 0.2, 1.2, 2.5, PUPIL, out=0)
            pen.ell(ex + 1.2, ey - 1.3, 0.85, 0.85, WHITE, out=0)
        pen.stroke([(cx + side * 9, hy + 4.5), (cx + side * 21, hy + 3)], (245, 240, 230, 255), 0.45)
        pen.stroke([(cx + side * 9, hy + 6), (cx + side * 21, hy + 7)], (245, 240, 230, 255), 0.45)
    pen.poly([(cx - 1.6, hy + 3.4), (cx + 1.6, hy + 3.4), (cx, hy + 5.2)], NOSE, out=0.5)
    pen.stroke([(cx, hy + 5.3), (cx - 1.6, hy + 6.6)], OUT, 0.6)
    pen.stroke([(cx, hy + 5.3), (cx + 1.6, hy + 6.6)], OUT, 0.6)
    return pen.im


def frames():
    out = [walking(k / 6.0) for k in range(6)]
    out.append(walking(0.0, jump=True))
    out.append(sitting())
    out.append(sitting(blink=True))
    return out


def down(im, w, h):
    return im.resize((w, h), Image.LANCZOS)


def preview(path):
    fs = frames()
    big = [down(f, BW * 3, BH * 3) for f in fs]
    small = [down(f, 56, 28).resize((56 * 4, 28 * 4), Image.NEAREST) for f in fs[:3]]
    W = 3 * BW * 3 + 40
    rows = (len(big) + 2) // 3
    sheet = Image.new('RGB', (W, rows * (BH * 3 + 10) + 28 * 4 + 20), (40, 14, 60))
    for i, f in enumerate(big):
        sheet.paste(f, ((i % 3) * (BW * 3 + 10), (i // 3) * (BH * 3 + 10)), f)
    y = rows * (BH * 3 + 10) + 10
    for i, f in enumerate(small):
        bg = Image.new('RGB', f.size, (40, 14, 60))
        sheet.paste(f, (i * (56 * 4 + 10), y), f)
    sheet.save(path)


def pack(im):
    """RGB444 + alpha 4 bits a pixel, as uint16: A R G B (one nibble each)"""
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
             '/* Tom, drawn (tools/gen_tom.py, from his photos): frames 0..5 his steps, 6 a jump, 7 sitting, 8 a blink;',
             ' * each pixel A R G B a nibble (A 0: nothing there). TOM_S: 56 x 28 (the header), TOM_L: 112 x 56 (the start) */',
             '#define TOM_NF 9', '#define TOM_SW 56', '#define TOM_SH 28', '#define TOM_LW 112', '#define TOM_LH 56']
    for name, (w, h) in (('TOM_S', (56, 28)), ('TOM_L', (112, 56))):
        lines.append('static const uint16_t %s[TOM_NF][%d] = {' % (name, w * h))
        for f in fs:
            d = pack(down(f, w, h))
            lines.append('    {' + ','.join('0x%X' % v for v in d) + '},')
        lines.append('};')
    open(path, 'w').write('\n'.join(lines) + '\n')


if __name__ == '__main__':
    {'preview': preview, 'header': header}[sys.argv[1]](sys.argv[2])
