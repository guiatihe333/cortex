#!/usr/bin/env python3
"""Tom, drawn as a cartoon from his photos: a lynx point tabby, warm grey-fawn with dark tabby stripes, a long ringed
tail with a dark tip, darker ears, a light muzzle and chin, a peach nose, big pale blue eyes. Each frame is painted in
flat shapes on a layer, its silhouette grown into one outline round the whole of him (no lines where his parts
overlap), then shaded (a soft shadow under, a light along the back) and the details drawn on top (stripes, eyes,
nose, whiskers). (1.5.38: a cat's curves, his blotches and broken stripes, the brow's M, light rims round the eyes,
darker points.) Drawn at 8x and brought down (anti-aliased). Frames: six steps, a jump, sitting, a blink, asleep,
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


MARK = (92, 76, 66, 150)        # the markings, soft (blotches, broken stripes)
MARK_D = (74, 60, 52, 215)      # the darker ones (the brow's M, the bars, the cheek lines)
POINT = (118, 100, 88, 255)     # his points: the far legs, the mask's shadow
RIM = (246, 240, 230, 255)      # the light rim round a lynx point's eyes


def chain(*segs, n=20):
    """bezier segments (each 4 points, each starting where the last ended) as one list of points"""
    out = []
    for sg in segs:
        pts = bez(*sg, n=n)
        out.extend(pts if not out else pts[1:])
    return out


def stroke(lay, pts, w, col, taper=True):
    """a brush stroke along pts: thick in the middle, thin at the ends"""
    dense = [pts[0]]
    for (x0, y0), (x1, y1) in zip(pts, pts[1:]):           # (dense enough to be a line, not dots)
        m = max(1, int(math.hypot(x1 - x0, y1 - y0) / 0.15))
        dense.extend((x0 + (x1 - x0) * i / m, y0 + (y1 - y0) * i / m) for i in range(1, m + 1))
    pts = dense
    n = len(pts)
    for k, (x, y) in enumerate(pts):
        t = k / max(1, n - 1)
        r = w * (0.35 + 0.65 * math.sin(math.pi * t)) if taper else w
        lay.ell(x, y, r / 2, r / 2, col)


def finish2(fill, marks, details, shade_box=None, outline=1.5):
    """as finish, the markings between: clipped to his silhouette (never past the outline)"""
    a = fill.im.split()[3]
    m = marks.im.copy()
    m.putalpha(ImageChops.multiply(m.split()[3], a))
    body = Image.alpha_composite(fill.im, m)
    f2 = Layer()
    f2.im = body
    return finish(f2, details, shade_box, outline)


def eye2(det, ex, ey, rx, ry, look=0.0, closed=False):
    if closed:
        det.line(bez((ex - rx - 0.6, ey), (ex - rx * 0.4, ey + ry * 0.8), (ex + rx * 0.4, ey + ry * 0.8),
                     (ex + rx + 0.6, ey - 0.2), 10), OUT, 0.9)
        return
    det.ell(ex, ey, rx + 1.5, ry + 1.4, RIM)            # (the lynx point's light rim)
    det.ell(ex, ey, rx + 0.6, ry + 0.6, OUT)
    det.ell(ex, ey, rx, ry, IRIS)
    det.ell(ex, ey + ry * 0.35, rx * 0.95, ry * 0.6, IRIS_D)
    det.ell(ex + look, ey + 0.1, rx * 0.36, ry * 0.8, PUPIL)
    det.ell(ex + look + rx * 0.45, ey - ry * 0.42, rx * 0.28, rx * 0.28, WHITE)
    det.ell(ex + look - rx * 0.25, ey + ry * 0.45, rx * 0.12, rx * 0.12, WHITE)
    det.line([(ex + rx + 0.2, ey + 0.4), (ex + rx + 2.6, ey + 1.6)], OUT, 0.55)   # (the outer corner, a cat's)


def head2(fill, marks, det, cx, cy, closed=False):
    """his head in profile, facing right: ears, a round skull, full cheeks, the muzzle"""
    for ex, tip in ((cx - 6, (cx - 10, cy - 20)), (cx + 3.5, (cx + 5.5, cy - 20.5))):
        fill.poly(chain(((ex - 5.5, cy - 5), (ex - 5.5, cy - 11), tip, tip),
                        (tip, tip, (ex + 5.5, cy - 12), (ex + 5.5, cy - 6.5)), n=10) + [(ex, cy - 4)], EAR)
        det.poly(chain(((ex - 2.6, cy - 7.5), (ex - 2.4, cy - 11), (tip[0] * 0.72 + ex * 0.28, tip[1] * 0.78 + (cy - 8) * 0.22),
                        (tip[0] * 0.72 + ex * 0.28, tip[1] * 0.78 + (cy - 8) * 0.22)), n=8) + [(ex + 2.8, cy - 8.3)], EAR_IN)
    fill.ell(cx, cy - 0.5, 12.5, 11, FAWN)                 # the skull
    fill.ell(cx - 4.5, cy + 4, 8.5, 7, FAWN)               # the cheek, full (a ruff behind it)
    fill.ell(cx + 7.5, cy + 3.5, 7, 5.2, FAWN)             # the muzzle
    fill.ell(cx + 3, cy + 7.6, 7.5, 3.6, FAWN)             # the jaw
    marks.ell(cx - 1, cy - 4, 9.5, 6.5, (110, 94, 82, 120))   # (the mask's shade: a lynx point's darker face)
    marks.ell(cx + 8.5, cy + 4.6, 6.4, 4.4, CREAM)          # the muzzle and chin, light
    marks.ell(cx + 4, cy + 8.2, 7, 3.2, CREAM)
    for k in range(3):                                       # the brow's M, its strokes up the forehead
        x = cx - 3.5 + k * 3.0
        stroke(marks, bez((x + 0.8, cy - 5.5), (x + 0.4, cy - 7.5), (x - 0.2, cy - 9.5), (x - 0.8, cy - 11.5), 10), 1.5, MARK_D)
    stroke(marks, bez((cx - 0.5, cy + 0.5), (cx - 4, cy - 0.5), (cx - 8, cy + 0.5), (cx - 11.5, cy + 2.5), 12), 1.2, MARK_D)
    stroke(marks, bez((cx + 0.5, cy + 3.5), (cx - 3, cy + 3.5), (cx - 7, cy + 5), (cx - 10.5, cy + 7.5), 12), 1.2, MARK_D)
    for k, (x, y) in enumerate(((cx + 9.5, cy + 4.6), (cx + 11.2, cy + 4.2), (cx + 10.2, cy + 5.8), (cx + 11.8, cy + 5.6))):
        det.ell(x, y, 0.35, 0.35, POINT)                    # (the whisker pads' dots)
    eye2(det, cx + 3.2, cy - 1.6, 3.5, 3.4, look=0.8, closed=closed)
    det.poly([(cx + 12.6, cy + 2.2), (cx + 14.6, cy + 2.0), (cx + 13.7, cy + 3.8)], NOSE)
    det.line([(cx + 13.7, cy + 3.8), (cx + 13.5, cy + 5.0), (cx + 12.0, cy + 6.0)], OUT, 0.6)
    for dy in (-0.7, 0.9, 2.4):
        det.line(bez((cx + 13.5, cy + 4.3 + dy * 0.3), (cx + 17, cy + 3.5 + dy), (cx + 20, cy + 3 + dy * 1.5),
                     (cx + 24, cy + 2.6 + dy * 2), 8), WHISK, 0.4)


def tail2(fill, pts, r0, r1):
    fill.tube(pts, r0, r1, ringed)


def walking(phase, jump=False, closed=False):
    fill, marks, det = Layer(), Layer(), Layer()
    bob = 0 if jump else 0.8 * math.sin(phase * 4 * math.pi)
    by = 35 + bob - (3 if jump else 0)
    sw = math.sin(phase * 2 * math.pi)
    stretch_ = 4 if jump else 0
    # the tail: up from the rump in an S, its tip curling with the step
    tail2(fill, bez((29, by - 6), (18, by - 4 + 3 * sw), (10, by - 18), (16 + 4 * sw, by - 28), 44), 3.0, 2.0)

    def paw(x, off, hind):
        if jump:
            a = 1.15 if hind else -0.95                     # (the hind legs pushing back, the front reaching)
            a = -a
        else:
            a = 0.5 * math.sin((phase + off) * 2 * math.pi)
        return x + 12.5 * math.sin(a), by + 6 + 12.5 * math.cos(a)

    def hind_leg(lay, x, off, col, near):
        x1, y1 = paw(x, off, True)
        hx, hy = x - 4 + (x1 - x) * 0.35, by + 7 + (y1 - by - 6) * 0.35   # the hock, bent back
        lay.ell(x + 1, by + 1, 9.5 if near else 8.5, 9.5 if near else 8.5, col)   # the thigh, round
        lay.limb(x, by + 4, hx, hy, 4.4, 3.4, col)
        lay.limb(hx, hy, x1, y1, 3.0, 2.6, col)
        lay.ell(x1 + 1.3, y1 + 0.5, 3.4, 2.3, col)
        return hx, hy, x1, y1

    def fore_leg(lay, x, off, col):
        x1, y1 = paw(x, off, False)
        ex, ey = x + (x1 - x) * 0.45 - 0.6, by + 4 + (y1 - by - 4) * 0.45   # the elbow
        lay.limb(x, by + 1, ex, ey, 4.4, 3.6, col)
        lay.limb(ex, ey, x1, y1, 3.5, 2.8, col)
        lay.ell(x1 + 1.3, y1 + 0.5, 3.5, 2.3, col)
        return ex, ey, x1, y1

    far_h = hind_leg(fill, 39, 0.5, POINT, False)
    far_f = fore_leg(fill, 70, 0.0, POINT)
    # the body: an arched back dipping a little behind the shoulders, the rump round, the belly tucked, the chest deep
    body = chain(((70 + stretch_, by - 11), (60, by - 14.5), (47, by - 10), (36 - stretch_, by - 12)),
                 ((36 - stretch_, by - 12), (25 - stretch_, by - 13), (21 - stretch_, by + 1), (30 - stretch_, by + 7)),
                 ((30 - stretch_, by + 7), (41, by + 11), (54, by + 9), (63, by + 10)),
                 ((63, by + 10), (73 + stretch_, by + 11), (80 + stretch_, by + 1), (77 + stretch_, by - 7)),
                 ((77 + stretch_, by - 7), (75 + stretch_, by - 10), (73 + stretch_, by - 11), (70 + stretch_, by - 11)))
    fill.poly(body, FAWN)
    fill.ell(77 + stretch_, by - 6, 8, 8.5, FAWN)           # the neck into the head
    near_h = hind_leg(fill, 36, 0.0, FAWN, True)
    near_f = fore_leg(fill, 67, 0.5, FAWN)
    # the markings: the saddle (soft blotches along the spine), the flank's stripes broken into spots, the shoulder's
    for k in range(7):
        x = 32 + k * 6.3
        y = by - 11.2 + 1.6 * math.sin(k * 1.7) + (1.5 if 2 <= k <= 4 else 0)
        marks.ell(x, y, 3.6 + 0.8 * math.sin(k * 2.3), 2.6, MARK)
    for k in range(5):
        x = 38 + k * 6.8
        top = bez((x + 1, by - 8), (x - 1, by - 5), (x + 1.5, by - 2), (x - 0.5, by + 1), 10)
        stroke(marks, top[:6], 2.0, MARK)
        stroke(marks, top[7:], 1.7, MARK)
        marks.ell(x + 0.5, by + 4, 1.3, 1.0, MARK)          # (a spot below, the stripe breaking up)
    stroke(marks, bez((64, by - 9), (61, by - 5), (63, by - 1), (60, by + 2), 12), 1.8, MARK)   # the shoulder
    marks.ell(53, by + 9.5, 16, 3.4, (236, 228, 214, 200))   # the belly, light
    marks.ell(78 + stretch_, by + 2, 3.2, 5.5, (236, 228, 214, 170))   # the chest, light at its front
    for (hx, hy, x1, y1), dark in ((near_h, MARK_D), (near_f[:2] + near_f[2:], MARK_D)):
        for t in (0.3, 0.6):                                 # bars on his legs
            px, py = hx + (x1 - hx) * t, hy + (y1 - hy) * t
            stroke(marks, [(px - 2.6, py - 0.3), (px, py + 0.3), (px + 2.6, py - 0.3)], 1.1, dark, taper=False)
    det.line(bez((29 - stretch_, by - 2), (31, by + 3), (36, by + 6), (40, by + 8), 10), FAWN_D, 0.5)   # (the thigh's edge)
    head2(fill, marks, det, 87 + stretch_, by - 11 - (2 if jump else 0), closed=closed)
    return finish2(fill, marks, det, (28, by - 13, 80, by + 12))


def sitting(closed=False):
    fill, marks, det = Layer(), Layer(), Layer()
    cx = 56
    tail2(fill, bez((cx + 9, 51.5), (cx + 26, 54), (cx + 32, 44), (cx + 24, 35), 40), 3.0, 2.0)
    # a pear: narrow shoulders, wide haunches, the chest pushed forward
    body = chain(((cx, 26), (cx + 10, 26), (cx + 12, 33), (cx + 13, 38)),
                 ((cx + 13, 38), (cx + 18, 42), (cx + 19, 52), (cx + 11, 53)),
                 ((cx + 11, 53), (cx + 4, 53.5), (cx - 4, 53.5), (cx - 11, 53)),
                 ((cx - 11, 53), (cx - 19, 52), (cx - 18, 42), (cx - 13, 38)),
                 ((cx - 13, 38), (cx - 12, 33), (cx - 10, 26), (cx, 26)))
    fill.poly(body, FAWN)
    for side in (-1, 1):                                     # the haunches
        fill.ell(cx + side * 11.5, 46, 7.5, 7, FAWN)
        det.line(bez((cx + side * 6, 41), (cx + side * 10, 38.5), (cx + side * 16, 40), (cx + side * 17.5, 47), 10), FAWN_D, 0.55)
    for dx in (-5, 5):                                       # the front legs, straight, the paws together
        fill.limb(cx + dx, 38, cx + dx * 0.9, 50.5, 3.4, 3.0, FAWN)
        fill.ell(cx + dx * 0.9, 51.3, 3.8, 2.4, FAWN)
        for y in (43, 47):
            stroke(marks, [(cx + dx * 0.9 - 2.6, y), (cx + dx * 0.9, y + 0.5), (cx + dx * 0.9 + 2.6, y)], 1.0, MARK_D, taper=False)
        det.line([(cx + dx * 0.9 - 1, 50.6), (cx + dx * 0.9 - 1, 52.4)], FAWN_D, 0.5)
        det.line([(cx + dx * 0.9 + 1, 50.6), (cx + dx * 0.9 + 1, 52.4)], FAWN_D, 0.5)
    marks.ell(cx, 37, 7.5, 9.5, CREAM)                       # the bib
    for k, y in enumerate((31.5, 35)):                       # the necklaces
        stroke(marks, bez((cx - 7, y), (cx - 3, y + 2.5), (cx + 3, y + 2.5), (cx + 7, y), 12), 1.0, MARK)
    for side in (-1, 1):                                     # his sides: stripes breaking into spots
        for k in range(3):
            y = 31 + k * 5.5
            pts = bez((cx + side * 14.5, y), (cx + side * 12.5, y + 1.5), (cx + side * 10.5, y + 1), (cx + side * 8.5, y + 2.5), 10)
            stroke(marks, pts[:5], 1.8, MARK)
            stroke(marks, pts[7:], 1.5, MARK)
        for k in range(3):                                   # the haunch's spots
            marks.ell(cx + side * (11 + k * 2), 44 + k * 2.5, 1.4, 1.1, MARK)
    hy = 20
    for side in (-1, 1):
        ex = cx + side * 7.5
        tip = (ex + side * 4, hy - 18.5)
        fill.poly(chain(((ex - 5, hy - 3), (ex - 5, hy - 9), tip, tip), (tip, tip, (ex + 5, hy - 9), (ex + 5, hy - 3)), n=10), EAR)
        det.poly([(ex - 2.4, hy - 5.5), (ex + 2.4, hy - 5.5), (ex + side * 3, hy - 14)], EAR_IN)
    fill.ell(cx, hy, 13, 11, FAWN)
    for side in (-1, 1):                                     # the cheeks, full
        fill.ell(cx + side * 8.5, hy + 4.5, 6.5, 5, FAWN)
    marks.ell(cx, hy - 3, 11, 6.5, (110, 94, 82, 120))      # (the mask's shade)
    marks.ell(cx, hy + 6, 7.4, 4.6, CREAM)
    for k in range(4):                                       # the brow's M
        x = cx - 4.5 + k * 3
        stroke(marks, [(x, hy - 5.5), (x + (k - 1.5) * 0.4, hy - 8.5), (x + (k - 1.5) * 0.8, hy - 11)], 1.4, MARK_D)
    for side in (-1, 1):                                     # the cheek lines
        stroke(marks, bez((cx + side * 8, hy + 1), (cx + side * 10.5, hy + 0.5), (cx + side * 12.5, hy + 1.5), (cx + side * 14, hy + 3), 10), 1.1, MARK_D)
        stroke(marks, bez((cx + side * 8, hy + 3.8), (cx + side * 10.5, hy + 4), (cx + side * 12.5, hy + 5.2), (cx + side * 13.8, hy + 7), 10), 1.1, MARK_D)
    for side in (-1, 1):
        eye2(det, cx + side * 5.4, hy - 0.4, 3.2, 3.3, closed=closed)
        for dy in (0, 1.6):
            det.line(bez((cx + side * 8.5, hy + 4.5 + dy), (cx + side * 13, hy + 3.8 + dy * 1.4), (cx + side * 17, hy + 3.4 + dy * 1.8),
                         (cx + side * 22, hy + 3 + dy * 2.5), 8), WHISK, 0.4)
        det.ell(cx + side * 2.6, hy + 5.4, 0.35, 0.35, POINT)
        det.ell(cx + side * 3.6, hy + 6.4, 0.35, 0.35, POINT)
    det.poly([(cx - 1.7, hy + 3.4), (cx + 1.7, hy + 3.4), (cx, hy + 5.3)], NOSE)
    det.line([(cx, hy + 5.3), (cx - 1.7, hy + 6.7)], OUT, 0.6)
    det.line([(cx, hy + 5.3), (cx + 1.7, hy + 6.7)], OUT, 0.6)
    return finish2(fill, marks, det, (cx - 16, 24, cx + 16, 53))


def asleep():
    fill, marks, det = Layer(), Layer(), Layer()
    cx, cy = 54, 44
    body = chain(((cx - 28, cy + 9), (cx - 31, cy - 6), (cx - 16, cy - 11), (cx - 2, cy - 10)),
                 ((cx - 2, cy - 10), (cx + 10, cy - 9), (cx + 16, cy - 2), (cx + 14, cy + 9)),
                 ((cx + 14, cy + 9), (cx, cy + 11), (cx - 16, cy + 11), (cx - 28, cy + 9)))
    fill.poly(body, FAWN)
    fill.tube(bez((cx - 26, cy + 6), (cx - 34, cy - 4), (cx - 12, cy + 13), (cx + 8, cy + 10), 40), 3.0, 2.2, ringed)
    for k in range(6):                                       # the saddle's blotches, the stripes broken below
        x = cx - 22 + k * 6.5
        marks.ell(x, cy - 9 + 0.8 * math.sin(k * 2.1), 3.4, 2.4, MARK)
        pts = bez((x, cy - 7), (x + 1.5, cy - 4), (x - 0.5, cy - 1), (x + 1, cy + 2), 10)
        stroke(marks, pts[:5], 1.7, MARK)
        marks.ell(x + 0.6, cy + 4, 1.2, 0.9, MARK)
    hx, hy = cx + 18, cy - 3
    for ex, tip in ((hx - 5, (hx - 8.5, hy - 17)), (hx + 4, (hx + 6, hy - 17.5))):
        fill.poly(chain(((ex - 4.8, hy - 4), (ex - 4.8, hy - 9), tip, tip), (tip, tip, (ex + 4.8, hy - 10), (ex + 4.8, hy - 5)), n=10), EAR)
        det.poly([(ex - 2.2, hy - 6.5), (ex + 2.2, hy - 7), (tip[0] * 0.75 + ex * 0.25, tip[1] * 0.8 + (hy - 7) * 0.2)], EAR_IN)
    fill.ell(hx, hy, 11.5, 10, FAWN)
    fill.ell(hx - 3, hy + 4, 7.5, 6, FAWN)
    fill.ell(hx + 2, cy + 7, 7, 3, FAWN)                     # a paw under his chin
    marks.ell(hx + 2, cy + 7, 6, 2.4, CREAM)
    marks.ell(hx, hy - 3, 9, 6, (110, 94, 82, 120))
    marks.ell(hx + 5, hy + 4, 6, 4, CREAM)
    for k in range(3):
        x = hx - 3.5 + k * 3
        stroke(marks, [(x + 0.6, hy - 5), (x + 0.2, hy - 7.5), (x - 0.3, hy - 9.5)], 1.4, MARK_D)
    stroke(marks, bez((hx - 1, hy + 1.5), (hx - 4, hy + 1), (hx - 7, hy + 2), (hx - 9.5, hy + 4), 10), 1.1, MARK_D)
    eye2(det, hx + 1, hy - 0.5, 3.4, 3, closed=True)
    eye2(det, hx + 8, hy - 0.8, 2.4, 2.4, closed=True)
    det.poly([(hx + 9.5, hy + 2.4), (hx + 11.6, hy + 2.3), (hx + 10.6, hy + 3.8)], NOSE)
    return finish2(fill, marks, det, (cx - 28, cy - 10, cx + 20, cy + 10))


def stretch():
    fill, marks, det = Layer(), Layer(), Layer()
    by = 36
    fill.tube(bez((24, by - 12), (14, by - 22), (16, by - 30), (24, by - 32), 40), 3.0, 2.0, ringed)
    fill.ell(29, by - 7, 9.5, 9.5, FAWN)                     # the rump, high
    fill.limb(28, by - 4, 25, by + 6, 4.6, 3.6, POINT)       # the far hind leg
    fill.limb(25, by + 6, 31, by + 15, 3.2, 2.8, POINT)
    fill.limb(32, by - 4, 30, by + 6, 4.6, 3.6, FAWN)
    fill.limb(30, by + 6, 35, by + 15, 3.2, 2.8, FAWN)
    fill.ell(36, by + 15.5, 3.5, 2.3, FAWN)
    for k, (x, y) in enumerate(bez((28, by - 10), (44, by - 13), (58, by - 3), (70, by + 7), 34)):   # the back, a long slope
        r = 9.5 - 3.2 * k / 34
        fill.ell(x, y + 0.8, r, r, FAWN)
    fill.limb(66, by + 10, 88, by + 15, 3.4, 2.8, POINT)
    fill.limb(64, by + 12, 84, by + 16, 3.4, 2.8, FAWN)
    fill.ell(90, by + 15.5, 3.6, 2.2, FAWN)
    for k in range(5):
        x = 33 + k * 7.5
        y0 = by - 15 + k * 3.2
        marks.ell(x, y0 + 1.5, 3.2, 2.3, MARK)
        pts = bez((x + 1, y0 + 3), (x + 2.5, y0 + 6), (x + 1, y0 + 9), (x + 2, y0 + 12), 10)
        stroke(marks, pts[:5], 1.7, MARK)
        marks.ell(x + 2, y0 + 13, 1.2, 0.9, MARK)
    for x, y in ((74, by + 13), (80, by + 14.2)):
        stroke(marks, [(x, y - 2.4), (x + 0.4, y), (x, y + 2.4)], 1.0, MARK_D, taper=False)
    head2(fill, marks, det, 74, by + 2, closed=True)
    return finish2(fill, marks, det, (26, by - 15, 80, by + 14))


G = 52.0                                          # the ground, where a paw's middle rests


def ik(root, tgt, l1, l2, bend):
    """two segments from root to tgt: the middle joint (bend +1 forward, -1 back)"""
    dx, dy = tgt[0] - root[0], tgt[1] - root[1]
    d = max(abs(l1 - l2) + 0.01, min(l1 + l2 - 0.01, math.hypot(dx, dy)))
    a = math.atan2(dy, dx)
    b = math.acos(max(-1.0, min(1.0, (l1 * l1 + d * d - l2 * l2) / (2 * l1 * d))))
    ang = a - bend * b
    return root[0] + l1 * math.cos(ang), root[1] + l1 * math.sin(ang)


def cat(hip, sho, arch, paws, neck=30.0, tail=None, closed=False, meta=None, rot=0.0):
    """a cat facing right. hip, sho: the spine's ends; arch: its bend (up +); paws: hind far, hind near, fore far,
    fore near (x, y); neck: the head's angle up from forward; tail: three control points after its root (relative to
    the hip); meta: each hind foot's hock as an offset from its paw; rot: the whole tilted (degrees, nose up +)"""
    fill, marks, det = Layer(), Layer(), Layer()
    cx, cy = (hip[0] + sho[0]) / 2, (hip[1] + sho[1]) / 2
    ca, sa = math.cos(math.radians(-rot)), math.sin(math.radians(-rot))

    def T(p):
        x, y = p[0] - cx, p[1] - cy
        return cx + x * ca - y * sa, cy + x * sa + y * ca

    hip, sho = T(hip), T(sho)
    paws = [T(p) for p in paws]
    L = math.hypot(sho[0] - hip[0], sho[1] - hip[1])
    ux, uy = (sho[0] - hip[0]) / L, (sho[1] - hip[1]) / L       # along the spine
    nx, ny = uy, -ux                                             # its normal, up
    spine = bez(hip, (hip[0] + ux * L * 0.3 + nx * arch, hip[1] + uy * L * 0.3 + ny * arch),
                (sho[0] - ux * L * 0.3 + nx * arch, sho[1] - uy * L * 0.3 + ny * arch), sho, 40)

    def rad(t):                                                  # the hips, the waist tucked, the deep chest
        k = [(0.0, 8.6), (0.45, 7.2), (0.8, 9.6), (1.0, 9.0)]
        for (t0, r0), (t1, r1) in zip(k, k[1:]):
            if t <= t1:
                return r0 + (r1 - r0) * (t - t0) / (t1 - t0)
        return k[-1][1]

    meta = meta or [(-2.5, -6.5), (-2.5, -6.5)]
    # the tail, from the rump: a counterweight
    tl = tail or [(-8, -2), (-16, -4), (-22, -12)]
    root = (hip[0] - ux * 6 + nx * 3, hip[1] - uy * 6 + ny * 3)
    pts = [root] + [T((hip_[0] + p[0], hip_[1] + p[1])) for p in tl for hip_ in [(cx + (hip[0] - cx), cy + (hip[1] - cy))]]
    pts = [root] + [(hip[0] + p[0] * ca - p[1] * sa, hip[1] + p[0] * sa + p[1] * ca) for p in tl]
    fill.tube(bez(*pts, n=44), 3.0, 1.9, ringed)

    def hind(paw, m, col, near):
        hock = (paw[0] + m[0], paw[1] + m[1])
        r0 = (hip[0] + nx * -1.0, hip[1] + ny * -1.0)
        knee = ik(r0, hock, 10.5, 10.0, +1)
        fill.ell((r0[0] + knee[0]) / 2, (r0[1] + knee[1]) / 2, 7.2 if near else 6.6, 7.2 if near else 6.6, col)   # the thigh
        fill.limb(knee[0], knee[1], hock[0], hock[1], 3.4, 2.6, col)
        fill.limb(hock[0], hock[1], paw[0], paw[1], 2.5, 2.3, col)
        fill.ell(paw[0] + 1.2, paw[1] + 0.4, 3.3, 2.2, col)
        return knee, hock

    def fore(paw, col):
        r0 = (sho[0] - nx * 2.5 - ux * 1.0, sho[1] - ny * 2.5 - uy * 1.0)
        elbow = ik(r0, paw, 9.5, 10.5, -1)
        fill.limb(r0[0], r0[1], elbow[0], elbow[1], 4.4, 3.4, col)
        fill.limb(elbow[0], elbow[1], paw[0], paw[1], 3.2, 2.6, col)
        fill.ell(paw[0] + 1.3, paw[1] + 0.4, 3.4, 2.2, col)
        return elbow

    hind(paws[0], meta[0], POINT, False)
    fore(paws[2], POINT)
    for k, (x, y) in enumerate(spine):                          # the body along its spine
        t = k / (len(spine) - 1)
        r = rad(t)
        sag = 0.9 if 0.65 < t < 0.95 else -0.4 if 0.3 < t < 0.6 else 0.0   # (the chest low, the belly up)
        fill.ell(x - nx * sag, y - ny * sag, r, r, FAWN)
    hx, hy = sho[0] + ux * 3 + nx * 4, sho[1] + uy * 3 + ny * 4
    fill.ell(hx, hy, 7.5, 7.5, FAWN)                             # the neck
    knee, hock = hind(paws[1], meta[1], FAWN, True)
    elbow = fore(paws[3], FAWN)
    # his markings: the saddle along the spine, the flank's stripes breaking into spots, bars on the near legs
    for k in range(7):
        t = 0.1 + k * 0.12
        x, y = spine[int(t * (len(spine) - 1))]
        r = rad(t)
        marks.ell(x + nx * (r - 1.6), y + ny * (r - 1.6), 3.3 + 0.7 * math.sin(k * 2.3), 2.4, MARK)
        if 0.18 < t < 0.85:
            a0 = (x + nx * (r - 3.5), y + ny * (r - 3.5))
            a1 = (x - nx * (r * 0.35) + ux * 1.2, y - ny * (r * 0.35) + uy * 1.2)
            seg = [(a0[0] + (a1[0] - a0[0]) * i / 10 + ux * math.sin(i * 0.9) * 0.8,
                    a0[1] + (a1[1] - a0[1]) * i / 10 + uy * math.sin(i * 0.9) * 0.8) for i in range(11)]
            stroke(marks, seg[:5], 2.0, MARK)
            stroke(marks, seg[6:], 1.6, MARK)
    bx, by_ = spine[int(0.55 * (len(spine) - 1))]
    marks.ell(bx - nx * 6.5, by_ - ny * 6.5, 11, 3.2, (236, 228, 214, 190))   # the belly, light
    for (a, b) in ((knee, hock), (elbow, paws[3])):
        for t in (0.35, 0.7):
            px, py = a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t
            dx, dy = b[0] - a[0], b[1] - a[1]
            d = math.hypot(dx, dy) or 1
            ox, oy = -dy / d * 2.6, dx / d * 2.6
            stroke(marks, [(px - ox, py - oy), (px, py), (px + ox, py + oy)], 1.1, MARK_D, taper=False)
    ha = math.radians(neck + rot)
    head2(fill, marks, det, hx + 9 * math.cos(ha), hy - 9 * math.sin(ha), closed=closed)
    xs = [p[0] for p in spine]
    ys = [p[1] for p in spine]
    return finish2(fill, marks, det, (min(xs) - 6, min(ys) - 9, max(xs) + 6, max(ys) + 9))


def leg_cycle(q, stance, A, lift):
    """a paw over its cycle (q 0..1): on the ground from front to back (the stance), then lifted forward"""
    if q < stance:
        u = q / stance
        return A * (1 - 2 * u), 0.0
    u = (q - stance) / (1 - stance)
    e = u * u * (3 - 2 * u)
    return -A + 2 * A * e, -lift * math.sin(math.pi * u)


def gallop(p):
    """the rotary gallop: the spine gathered as the hind feet pass under the belly, stretched in the flight"""
    ext = math.cos(2 * math.pi * (p - 0.97))
    L = 31 + 5.5 * ext
    arch = 3.0 - 4.5 * ext
    by = 33.5 - 1.6 * max(0.0, ext) - 1.0 * max(0.0, -ext)
    cx = 54
    hip, sho = (cx - L / 2, by + 0.5), (cx + L / 2, by - 0.8)
    paws = []
    for root, ph, A, lift, st in ((hip, 0.58, 11, 7, 0.36), (hip, 0.5, 11, 7, 0.36),
                                  (sho, 0.08, 10, 9, 0.36), (sho, 0.0, 10, 9, 0.36)):
        dx, dy = leg_cycle((p + ph) % 1.0, st, A, lift)
        paws.append((root[0] + dx + (1.5 if root is hip else 1.0), G + dy))
    tail = [(-9, -2 + ext), (-17, -5 - 2 * ext), (-24, -9 + 3 * ext)]
    return cat(hip, sho, arch, paws, neck=24 - 6 * ext, tail=tail)


def walk(p):
    """the lateral walk: hind, fore of one side, then the other's; three feet down at a time"""
    cx = 54
    hip, sho = (cx - 15.5, 33.5), (cx + 15.5, 32.0)
    paws = []
    for root, ph, A in ((hip, 0.5, 7.5), (hip, 0.0, 7.5), (sho, 0.75, 7.0), (sho, 0.25, 7.0)):
        dx, dy = leg_cycle((p + ph) % 1.0, 0.62, A, 4.5)
        paws.append((root[0] + dx + 1.5, G + dy))
    bob = 0.6 * math.sin(4 * math.pi * p)
    hip, sho = (hip[0], hip[1] + bob), (sho[0], sho[1] + bob)
    return cat(hip, sho, 2.0, paws, neck=30, tail=[(-9, -3), (-12, -16), (-6, -26)])


def crouch():
    """before the pounce: low, the hind feet gathered under (treading), the forepaws out, the head low and still"""
    return cat((38, 42), (64, 43.5), 2.5, [(46, G), (48.5, G), (70, G), (73.5, G)], neck=8,
               tail=[(-10, 2), (-18, 4), (-26, 3)], meta=[(-6, -2.5), (-6, -2.5)])


def launch():
    """the push: the hind legs straight behind, the forelegs drawn up to the chest, the spine long, nose up"""
    return cat((36, 43), (68, 40), -1.0, [(24, 53), (27, 54), (78, 38), (80, 42)], neck=24,
               tail=[(-10, 3), (-18, 6), (-25, 7)], meta=[(-1, -6.5), (-1, -6.5)], rot=18)


def flight():
    """in the air: level and long, the forepaws reaching down for the landing, the hind legs trailing, the tail up"""
    return cat((34, 31), (68, 31), -1.0, [(20, 37), (23, 39), (82, 43), (85, 45)], neck=14,
               tail=[(-9, -3), (-16, -9), (-21, -17)], meta=[(-4, -4), (-4, -4)])


def land():
    """the landing: on the forepaws, the elbows taking it, the rump still high, the hind feet coming in"""
    return cat((36, 29), (64, 38), 3.5, [(40, 42), (43, 44), (72, G), (75.5, G)], neck=18,
               tail=[(-8, -4), (-14, -12), (-17, -20)], meta=[(-3, -5), (-3, -5)], rot=-6)


def tuck():
    """turned in the air off a wall: gathered, the four feet under, ready to push again"""
    return cat((42, 31), (64, 31), 7.0, [(50, 42), (52.5, 43), (60, 42), (62.5, 43)], neck=20,
               tail=[(-8, -3), (-12, -12), (-6, -18)], meta=[(-4, -3), (-4, -3)])


BIG = ['RUN0', 'RUN1', 'RUN2', 'RUN3', 'RUN4', 'RUN5', 'CROUCH', 'LAUNCH', 'FLIGHT', 'LAND', 'TUCK', 'SIT', 'BLINK']


def frames_big():
    return [gallop(k / 6.0) for k in range(6)] + [crouch(), launch(), flight(), land(), tuck(), sitting(),
                                                  sitting(closed=True)]


def frames():                                     # the header's: walking (6), a jump, sitting, a blink, asleep, a stretch
    return [walk(k / 6.0) for k in range(6)] + [flight(), sitting(), sitting(closed=True), asleep(), stretch()]


def down(im, w, h):
    return im.resize((w, h), Image.LANCZOS)


def preview(path):
    fb = frames_big()
    big = [down(f, BW * 2, BH * 2) for f in fb]
    small = [down(f, 56, 28).resize((56 * 3, 28 * 3), Image.NEAREST) for f in frames()]
    c = 4
    rows = (len(big) + c - 1) // c
    srows = (len(small) + 5) // 6
    sheet = Image.new('RGB', (c * (BW * 2 + 8), rows * (BH * 2 + 8) + srows * (28 * 3 + 6) + 10), (40, 14, 60))
    for i, f in enumerate(big):
        sheet.paste(f, ((i % c) * (BW * 2 + 8), (i // c) * (BH * 2 + 8)), f)
    y0 = rows * (BH * 2 + 8) + 10
    for i, f in enumerate(small):
        sheet.paste(f, ((i % 6) * (56 * 3 + 6), y0 + (i // 6) * (28 * 3 + 6)), f)
    sheet.save(path)


def pack444(c):
    r, g, b, a = c
    a4 = (a * 15 + 127) // 255
    return 0 if a4 == 0 else (a4 << 12) | ((r >> 4) << 8) | ((g >> 4) << 4) | (b >> 4)


def header(path):
    big = [down(f, BW, BH) for f in frames_big()]
    small = [down(f, 56, 28) for f in frames()]
    # one palette for every frame: the ARGB4444 colours they use (255 at most; 0 the empty pixel)
    cols = {}
    for im in big + small:
        for c in im.getdata():
            v = pack444(c)
            if v:
                cols[v] = cols.get(v, 0) + 1
    pal = sorted(cols, key=lambda v: -cols[v])
    if len(pal) > 255:                            # (too many: the rarest to their nearest kept)
        keep = pal[:255]
        def near(v):
            def comp(u):
                return [(u >> s) & 15 for s in (12, 8, 4, 0)]
            cv = comp(v)
            return min(range(255), key=lambda i: sum((a - b) ** 2 for a, b in zip(comp(keep[i]), cv)))
        remap = {v: keep[near(v)] for v in pal[255:]}
        pal = keep
    else:
        remap = {}
    idx = {v: i + 1 for i, v in enumerate(pal)}

    def ind(im):
        out = []
        for c in im.getdata():
            v = pack444(c)
            out.append(0 if v == 0 else idx[remap.get(v, v)])
        return out

    lines = ['/* SPDX-License-Identifier: MPL-2.0 */',
             '/* Tom, drawn (tools/gen_tom.py: a skeleton in a cat\'s poses, from his photos), each pixel an index into',
             ' * TOM_PAL (A R G B a nibble; 0: nothing there). TOM_S: 56 x 28 (the header): 0..5 his steps, 6 a jump, 7',
             ' * sitting, 8 a blink, 9 asleep, 10 a stretch. TOM_L: 112 x 56 (the start screen): TOM_L_* */',
             '#define TOM_NF %d' % len(small), '#define TOM_NL %d' % len(big), '#define TOM_SW 56', '#define TOM_SH 28',
             '#define TOM_LW %d' % BW, '#define TOM_LH %d' % BH]
    lines.append('enum { ' + ', '.join('TOM_L_%s = %d' % (n, i) for i, n in enumerate(BIG)) + ' };')
    lines.append('static const uint16_t TOM_PAL[256] = {0,' + ','.join('0x%X' % v for v in pal) + '};')
    for name, ims in (('TOM_S', small), ('TOM_L', big)):
        w, h = ims[0].size
        lines.append('static const uint8_t %s[%d][%d] = {' % (name, len(ims), w * h))
        for im in ims:
            lines.append('    {' + ','.join(str(v) for v in ind(im)) + '},')
        lines.append('};')
    open(path, 'w').write('\n'.join(lines) + '\n')
    print('palette', len(pal), 'colours;', len(remap), 'merged')


if __name__ == '__main__':
    {'preview': preview, 'header': header}[sys.argv[1]](sys.argv[2])
