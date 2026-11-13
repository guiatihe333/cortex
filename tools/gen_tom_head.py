#!/usr/bin/env python3
"""Tom's markings as an ink blot (1.5.38): from a photo of the top of his head (kept on the computer, never in the
repository), only his markings (the fur darker than around it, his dark points), no head round them, drawn as clean
ink shapes and folded as a Rorschach blot is (each side the mirror of the other), for the start screen's grey.

  gen_tom_head.py PHOTO OUT.h [preview.png]     app/tom_head.h: TOM_INK, 240 x 240 run-length coded, each byte a
                                                run of ink (bit 7) or of paper, its length - 1 (bits 0..6)
Needs numpy and Pillow."""
import sys

import numpy as np
from PIL import Image, ImageFilter, ImageOps

BOX = (0, 260, 1200, 1460)                     # (on a 1200 x 1600 photo: a square on his head, the nose at the bottom)
S = 720


def blur(a, r):
    """a (0..1) blurred, as 0..1"""
    im = Image.fromarray(np.clip(a * 255.0, 0, 255).astype(np.uint8))
    return np.asarray(im.filter(ImageFilter.GaussianBlur(r)), dtype=np.float32) / 255.0


def drop_small(m, area):
    from collections import deque
    H, W = m.shape
    seen = np.zeros(m.shape, bool)
    out = m.copy()
    for y, x in zip(*np.nonzero(m)):
        if seen[y, x]:
            continue
        comp, q = [], deque([(y, x)])
        seen[y, x] = True
        while q:
            cy, cx = q.popleft()
            comp.append((cy, cx))
            for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                yy, xx = cy + dy, cx + dx
                if 0 <= yy < H and 0 <= xx < W and m[yy, xx] and not seen[yy, xx]:
                    seen[yy, xx] = True
                    q.append((yy, xx))
        if len(comp) < area:
            for cy, cx in comp:
                out[cy, cx] = False
    return out


def ink(src):
    im = ImageOps.exif_transpose(Image.open(src)).convert('RGB')
    sx, sy = im.width / 1200.0, im.height / 1600.0
    im = im.crop((round(BOX[0] * sx), round(BOX[1] * sy), round(BOX[2] * sx), round(BOX[3] * sy))).resize((S, S), Image.LANCZOS)
    a = np.asarray(im, dtype=np.float32) / 255.0
    r, b = a[..., 0], a[..., 2]
    L = 0.3 * a[..., 0] + 0.59 * a[..., 1] + 0.11 * a[..., 2]
    cat = blur(((blur(b, 3) > blur(r, 3) + 0.05) & (blur(b, 3) > 0.27)) == 0, 12) > 0.5
    cat = blur(cat.astype(np.float32), 26) > 0.93      # (well inside him: no edge of a head)
    d = blur(L, 3.2) - blur(L, 22)                     # his stripes: darker than the fur round them (the shadows'
    m = cat & (d < -0.05)                              # broad dark is not a marking: only the local contrast counts)
    m = blur(m.astype(np.float32), 2.0) > 0.5
    m = drop_small(m, 140)                             # (no loose hairs)
    # folded on his own axis (the line the stripes mirror about, found), one real half and its mirror: his shapes
    # as they are, not an average
    cols = np.arange(S)
    best, ax = -1.0, S // 2
    for c in range(S // 2 - 60, S // 2 + 61, 2):
        w = min(c, S - 1 - c)
        lft, rgt = m[:, c - w:c], m[:, c + 1:c + 1 + w][:, ::-1]
        sc = (lft & rgt).sum() / max(1, (lft | rgt).sum())
        if sc > best:
            best, ax = sc, c
    half = 'left' if m[:, :ax].sum() >= m[:, ax:].sum() else 'right'
    print('axis', ax, 'symmetry %.2f' % best, 'half', half)
    out = np.zeros_like(m)
    for x in range(S):
        src = x if (x <= ax) == (half == 'left') else 2 * ax - x
        if 0 <= src < S:
            out[:, x] = m[:, src]
    m = np.roll(out, S // 2 - ax, axis=1)              # (his axis the screen's middle)
    m = blur(m.astype(np.float32), 0.8) > 0.5
    return np.asarray(Image.fromarray(m.astype(np.uint8) * 255).resize((240, 240), Image.LANCZOS)) > 127


def rle(m):
    v = m.ravel()
    out, i = [], 0
    while i < len(v):
        j = i
        while j < len(v) and v[j] == v[i] and j - i < 128:
            j += 1
        out.append((0x80 if v[i] else 0) | (j - i - 1))
        i = j
    return out


def main():
    m = ink(sys.argv[1])
    if len(sys.argv) > 3:
        g = np.where(m, 34, 150).astype(np.uint8)
        Image.fromarray(g).resize((480, 480), Image.NEAREST).save(sys.argv[3])
    data = rle(m)
    lines = ['/* SPDX-License-Identifier: MPL-2.0 */',
             '/* Tom\'s markings as an ink blot, folded (tools/gen_tom_head.py, from a photo of the top of his head): 240 x',
             ' * 240, run-length coded, each byte a run of ink (bit 7) or paper, its length - 1 (bits 0..6) */',
             '#define TOM_INK_W 240',
             'static const uint8_t TOM_INK[%d] = {' % len(data)]
    for k in range(0, len(data), 24):
        lines.append('    ' + ','.join('%d' % d for d in data[k:k + 24]) + ',')
    lines.append('};')
    # (1.5.41) how far each place is from his markings: a 60 x 60 grid (every 4 px), in px, for the contours radiating
    # from them; two chamfer passes over the 240 x 240 mask
    H, W = m.shape
    d = np.where(m, 0.0, 9999.0)
    for y in range(H):
        for x in range(W):
            if d[y, x]:
                for dy, dx, w in ((-1, 0, 1.0), (0, -1, 1.0), (-1, -1, 1.414), (-1, 1, 1.414)):
                    yy, xx = y + dy, x + dx
                    if 0 <= yy < H and 0 <= xx < W and d[yy, xx] + w < d[y, x]:
                        d[y, x] = d[yy, xx] + w
    for y in range(H - 1, -1, -1):
        for x in range(W - 1, -1, -1):
            if d[y, x]:
                for dy, dx, w in ((1, 0, 1.0), (0, 1, 1.0), (1, 1, 1.414), (1, -1, 1.414)):
                    yy, xx = y + dy, x + dx
                    if 0 <= yy < H and 0 <= xx < W and d[yy, xx] + w < d[y, x]:
                        d[y, x] = d[yy, xx] + w
    g = [min(255, int(round(d[gy * 4 + 2, gx * 4 + 2]))) for gy in range(60) for gx in range(60)]
    lines.append('/* how far from his markings, px (255: that far or more), on a 60 x 60 grid, a value every 4 px */')
    lines.append('static const uint8_t TOM_INK_D[3600] = {')
    for k in range(0, 3600, 30):
        lines.append('    ' + ','.join(str(v) for v in g[k:k + 30]) + ',')
    lines.append('};')
    open(sys.argv[2], 'w').write('\n'.join(lines) + '\n')
    print('%d bytes, ink %.1f%%' % (len(data), m.mean() * 100))


if __name__ == '__main__':
    main()
