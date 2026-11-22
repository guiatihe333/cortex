/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 guiatihe333 */
/* CORTEX VIDEO's high-resolution painting, for the computer's mirror only (VS_HD; the FM-1 never builds it): the same
 * frame as vs_paint, from the same state, at S times the screen's resolution (240 S square, RGBA): the field smooth
 * (bicubic, between its cells) through a 24-bit palette, the rows' shifts and the colour planes' split S times wider,
 * then the geometry, Rutt's lines and the objects drawn again at that size, their edges soft. It changes the engine's
 * state exactly as vs_paint does (SYNC's tears fade once a frame): the mirror stays in step with the FM-1 */
#ifdef VS_HD
static uint8_t vhd_pal[256][3];
static uint8_t *vhd_px;
static int vhd_w;
static float vhd_cubic(float p0, float p1, float p2, float p3, float t)   /* Catmull-Rom */
{
    return p1 + 0.5f * t * (p2 - p0 + t * (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3 + t * (3.0f * (p1 - p2) + p3 - p0)));
}
static void vhd_rgb(uint16_t c565, uint8_t *rgb)  /* (RGB565, not swapped, to 8 bits a plane) */
{
    const uint32_t r = c565 >> 11, g = c565 >> 5 & 63u, b = c565 & 31u;
    rgb[0] = (uint8_t)(r << 3 | r >> 2), rgb[1] = (uint8_t)(g << 2 | g >> 4), rgb[2] = (uint8_t)(b << 3 | b >> 2);
}
static void vhd_plot(int x, int y, const uint8_t *c, float a, int mode)   /* a pixel, a over what is there, BLEND's
                                                                             * way (vobj.inc vo_mix) */
{
    uint8_t *p;
    float s[3];
    int k;
    if ((unsigned)x >= (unsigned)vhd_w || (unsigned)y >= (unsigned)vhd_w || a <= 0.0f)
        return;
    p = vhd_px + ((size_t)y * (size_t)vhd_w + (size_t)x) * 4u;
    a = a > 1.0f ? 1.0f : a;
    for (k = 0; k < 3; k++)
        s[k] = (float)c[k];
    switch (mode) {
    case 1:
        for (k = 0; k < 3; k++)
            s[k] = (float)p[k] + (float)c[k];
        break;
    case 3: {
        const float l = 0.30f * (float)p[0] + 0.59f * (float)p[1] + 0.11f * (float)p[2], kk = l - (float)vo_key;
        a *= kk <= 0.0f ? 1.0f : kk >= 32.0f ? 0.0f : 1.0f - kk / 32.0f;
        break;
    }
    case 4:
        for (k = 0; k < 3; k++)
            s[k] = fm_fabsf((float)p[k] - (float)c[k]);
        break;
    case 5:
        for (k = 0; k < 3; k++)
            s[k] = (float)p[k] * (float)c[k] * (1.0f / 255.0f);
        break;
    default: break;
    }
    for (k = 0; k < 3; k++) {
        const float v = (float)p[k] + (fm_minf(s[k], 255.0f) - (float)p[k]) * a;
        p[k] = (uint8_t)(v > 255.0f ? 255.0f : v < 0.0f ? 0.0f : v);
    }
}
static void vhd_line(float x0, float y0, float x1, float y1, float r, const uint8_t *c, int add)
{                                                /* a soft line of radius r: discs along it, each pixel once */
    const float dx = x1 - x0, dy = y1 - y0, len = fm_sqrtf(dx * dx + dy * dy);
    const int bx0 = (int)fm_floorf(fm_minf(x0, x1) - r - 1.0f), bx1 = (int)fm_floorf(fm_maxf(x0, x1) + r + 1.0f),
              by0 = (int)fm_floorf(fm_minf(y0, y1) - r - 1.0f), by1 = (int)fm_floorf(fm_maxf(y0, y1) + r + 1.0f);
    int x, y;
    if (len > 3.0f * (float)vhd_w)
        return;                                  /* (from behind the camera) */
    if ((bx1 - bx0) * (by1 - by0) > 6 * (int)(len + 2.0f) * (int)(2.0f * r + 3.0f)) {
        const int n = (int)(len / (r * 0.7f + 0.3f)) + 1;   /* (a long diagonal: walk it instead of its box) */
        int i;
        for (i = 0; i <= n; i++) {
            const float t = (float)i / (float)n, cx = x0 + dx * t, cy = y0 + dy * t;
            for (y = (int)(cy - r - 1.0f); y <= (int)(cy + r + 1.0f); y++)
                for (x = (int)(cx - r - 1.0f); x <= (int)(cx + r + 1.0f); x++) {
                    const float ex = (float)x + 0.5f - cx, ey = (float)y + 0.5f - cy,
                                d = fm_sqrtf(ex * ex + ey * ey);
                    if (d < r + 0.5f)
                        vhd_plot(x, y, c, (r + 0.5f - d) * 0.6f, add);
                }
        }
        return;
    }
    for (y = by0; y <= by1; y++)                 /* the distance of each pixel of its box to the segment */
        for (x = bx0; x <= bx1; x++) {
            const float px = (float)x + 0.5f - x0, py = (float)y + 0.5f - y0;
            float t = len > 1e-6f ? (px * dx + py * dy) / (len * len) : 0.0f, ex, ey, d;
            t = t < 0.0f ? 0.0f : t > 1.0f ? 1.0f : t;
            ex = px - dx * t, ey = py - dy * t;
            d = fm_sqrtf(ex * ex + ey * ey);
            if (d < r + 0.5f)
                vhd_plot(x, y, c, r + 0.5f - d, add);
        }
}

/* STYLE on the finished HD frame, as the FM-1's app.c post_fx does on its screen (the same settings, at S times the
 * size: its cells and its rows S wide): VHS, EDGES, EMBOSS, SOLARIZE, SWAP, SATURATION, CRT, HALFTONE, LED. MOTION is
 * the FM-1's clean camera's alone. Its own randoms (VHS): the engine's stay untouched, the mirror in step */
static void vhd_post(uint8_t *px, int W, int S)
{
    const int crt = VPAR(CCRT), vhs = VPAR(CVHS), edge = VPAR(CEDGE), emboss = VPAR(CEMBOSS), half = VPAR(CHALF),
              led = VPAR(CLED), solar = VPAR(CSOLAR), swap = VPAR(CSWAP), sat = VPAR(CSAT);
    static uint8_t lp[960], lc[960], ln[960], row[960 * 4];
    static int8_t jit[240];
    static uint32_t seed = 12345u;
    static int band;
    int x, y, vb = -100, vh = 0;
    if (!(crt | vhs | edge | emboss | half | led | solar | swap) && sat == 100)
        return;
    if (vhs) {                                   /* (the rows' wobble, the tracking band rolling down) */
        for (y = 0; y < 240; y++) {
            seed = seed * 1664525u + 1013904223u;
            jit[y] = (int8_t)(((int)(seed >> 29) - 3) * vhs / 50);
        }
        band = (band + 3) % 300;
        vb = band - 30, vh = 6 + vhs / 6;
        for (y = vb; y < vb + vh; y++)
            if (y >= 0 && y < 240)
                jit[y] = (int8_t)(jit[y] * 3 + 8);
    }
#define LUMA(p) ((uint8_t)((77u * (p)[0] + 150u * (p)[1] + 29u * (p)[2]) >> 8))
    for (x = 0; x < W; x++)
        lc[x] = LUMA(px + 4 * x), lp[x] = lc[x];
    for (y = 0; y < W; y++) {
        uint8_t *r0 = px + (size_t)y * (size_t)W * 4u;
        const uint8_t *below = px + (size_t)(y + 1 < W ? y + 1 : y) * (size_t)W * 4u;
        const int y240 = y / S, hc = 4 + half / 10, lcell = 4 + led / 10;
        memcpy(row, r0, (size_t)W * 4u);
        for (x = 0; x < W; x++)
            ln[x] = LUMA(below + 4 * x);
        for (x = 0; x < W; x++) {
            int xs = x, r, g, b;
            const int x240 = x / S;
            if (vhs) {
                xs = x - jit[y240] * S;
                while (xs < 0) xs += W;
                while (xs >= W) xs -= W;
            }
            r = row[4 * xs], g = row[4 * xs + 1], b = row[4 * xs + 2];
            if (vhs) {
                const int k = (1 + vhs / 25) * S, xl = xs - k < 0 ? 0 : xs - k, xr = xs + k >= W ? W - 1 : xs + k;
                r = (r + row[4 * xl]) >> 1, b = (b + row[4 * xr + 2]) >> 1;
                if (y240 >= vb && y240 < vb + vh) {
                    const int n = (int)(((uint32_t)x240 * 2654435761u + (uint32_t)y240 * 40503u + seed) >> 25) - 64;
                    r += n, g += n, b += n;
                }
            }
            if (edge) {
                const int xl = x > 0 ? x - 1 : 0, xr = x < W - 1 ? x + 1 : W - 1;
                int e = lc[xr] - lc[xl], e2 = ln[x] - lp[x];
                e = ((e < 0 ? -e : e) + (e2 < 0 ? -e2 : e2)) * (2 + edge / 12) * S;   /* (the steps of S pixels) */
                e = e > 255 ? 255 : e;
                {
                    const int keep = 256 - edge * 2;
                    r = ((r * keep) >> 8) + ((vhd_pal[e][0] * e) >> 8);
                    g = ((g * keep) >> 8) + ((vhd_pal[e][1] * e) >> 8);
                    b = ((b * keep) >> 8) + ((vhd_pal[e][2] * e) >> 8);
                }
            }
            if (emboss) {
                const int xl = x > 0 ? x - 1 : 0, xr = x < W - 1 ? x + 1 : W - 1, a = emboss * 256 / 100;
                int v = 128 + (ln[xr] - lp[xl]) * (1 + emboss / 20) * S;
                v = v < 0 ? 0 : v > 255 ? 255 : v;
                r += ((v - r) * a) >> 8, g += ((v - g) * a) >> 8, b += ((v - b) * a) >> 8;
            }
            if (solar) {
                if (r > 128) r += ((255 - 2 * r) * solar) / 100;
                if (g > 128) g += ((255 - 2 * g) * solar) / 100;
                if (b > 128) b += ((255 - 2 * b) * solar) / 100;
            }
            if (swap) {
                const int s = r;
                if (swap >= 50)
                    r = g, g = b, b = s;
                else
                    r = b, b = s;
            }
            if (sat != 100) {
                const int Y = (77 * r + 150 * g + 29 * b) >> 8;
                r = Y + (r - Y) * sat / 100, g = Y + (g - Y) * sat / 100, b = Y + (b - Y) * sat / 100;
            }
            if (crt) {
                const int k = x240 % 3, d = crt * 2;
                if (y240 & 1)
                    r = r * (256 - d) >> 8, g = g * (256 - d) >> 8, b = b * (256 - d) >> 8;
                if (k)
                    r = r * (256 - d / 2) >> 8;
                if (k != 1)
                    g = g * (256 - d / 2) >> 8;
                if (k != 2)
                    b = b * (256 - d / 2) >> 8;
            }
            if (half || led) {                   /* (a round dot a cell: its size the light's, at the cell's middle) */
                const int isled = led > half, c = (isled ? lcell : hc) * S, px0 = x % c, py0 = y % c,
                          dx = px0 - c / 2, dy = py0 - c / 2, cx = x - px0 + c / 2 < W ? x - px0 + c / 2 : W - 1;
                const int L = lc[cx], rad = isled ? c * c * 3 / 16 : (L * c * c) >> 9;
                if (dx * dx + dy * dy > rad) {
                    const int a = (isled ? led : half) * 256 / 100;
                    r -= (r * a) >> 8, g -= (g * a) >> 8, b -= (b * a) >> 8;
                }
            }
            r0[4 * x] = (uint8_t)(r < 0 ? 0 : r > 255 ? 255 : r);
            r0[4 * x + 1] = (uint8_t)(g < 0 ? 0 : g > 255 ? 255 : g);
            r0[4 * x + 2] = (uint8_t)(b < 0 ? 0 : b > 255 ? 255 : b);
        }
        for (x = 0; x < W; x++)
            lp[x] = lc[x], lc[x] = ln[x];
    }
#undef LUMA
}

void vs_paint_hd(int S, uint8_t *rgba)
{
    const int W = 240 * S, ps = 1 + (int)((float)VPAR(VPIXEL) * 0.09f * (1.0f + vband[0]));
    const float syn = (float)VPAR(VSYNC) * 0.01f, rgb = (float)VPAR(VRGB) * 0.01f, fs = (float)S;
    const int sp = (int)(2.0f * rgb * (1.0f + 3.0f * vband[4] + 2.0f * vband[5]) + 0.5f), poster = VPAR(VPOSTER) > 0;
    static float hrow[VF_H][240 * 4];             /* the field, widened: each of its rows to W columns */
    static float colw[240 * 4][4];                /* (each column's four cells and their weight, once a frame) */
    static int colx[240 * 4][4];
    static uint16_t dummy[240];
    static int16_t o240[240];
    static int8_t s240[240];
    int y, x, i;
    if (S < 1 || S > 4)
        return;
    vhd_px = rgba, vhd_w = W;
    vs_out = dummy;
    vis_band(0, 0);                              /* (the frame's map: CHROMA, BRIGHT, INVERT, STROBE; nothing painted) */
    for (i = 0; i < 256; i++) {                  /* the palette in 24 bits: its five stops, eased between */
        const int s0 = (i >> 6) * 64, s1 = s0 + 64 > 255 ? 255 : s0 + 64;
        uint8_t a[3], b[3];
        const float t = (float)(i & 63) / 64.0f;
        int k;
        vhd_rgb(vpal[s0], a), vhd_rgb(vpal[s1], b);
        for (k = 0; k < 3; k++)
            vhd_pal[i][k] = (uint8_t)((float)a[k] + ((float)b[k] - (float)a[k]) * t + 0.5f);
    }
    for (y = 0; y < 240; y++) {                  /* each screen row's shift and colour split (vis_band's) */
        const int fi = y >> 1 < VF_H ? y >> 1 : VF_H - 1;
        o240[y] = (int16_t)(2 * ((int)((syn > 0.0f ? syn * 5.0f * fm_sinf((float)fi * 0.19f + vt * 2.3f) : 0.0f) + 0.5f) +
                                 vtear[fi]) +
                            ((fi & 1) ? (int)((float)VPAR(VSLICE) * 0.06f * (vband[3] + vbeat)) : 0));
        s240[y] = (int8_t)(sp + vchan[fi >> 3]);
        vrow_o[y] = o240[y];
    }
    for (x = 0; x < W; x++) {                    /* (screen x to the field's columns: a cell's middle at 2 c + 0.5) */
        const float fx = (((float)x + 0.5f) / fs - 0.5f - 0.5f) * 0.5f, f0 = fm_floorf(fx), t = fx - f0;
        int k;
        for (k = 0; k < 4; k++) {
            int c = (int)f0 - 1 + k;
            colx[x][k] = c < 0 ? 0 : c >= VF_W ? VF_W - 1 : c;
        }
        colw[x][0] = t;
    }
    for (i = 0; i < VF_H; i++) {                 /* the rows widened */
        const uint8_t *r = vf[i];
        for (x = 0; x < W; x++) {
            const float t = colw[x][0];
            if (ps > 1) {
                hrow[i][x] = r[(int)(((float)x / fs) * 0.5f) / ps * ps < VF_W ? (int)(((float)x / fs) * 0.5f) / ps * ps : VF_W - 1];
            } else {
                const float v = vhd_cubic(r[colx[x][0]], r[colx[x][1]], r[colx[x][2]], r[colx[x][3]], t);
                hrow[i][x] = v < 0.0f ? 0.0f : v > 255.0f ? 255.0f : v;
            }
        }
    }
    for (y = 0; y < W; y++) {                    /* each row: between four widened rows, the map, the palette */
        const int y240 = y / S, o = VPAR(FXV) ? 0 : o240[y240] * S, spr = VPAR(FXV) ? 0 : s240[y240] * S;
        uint8_t *out = rgba + (size_t)y * (size_t)W * 4u;
        static float line[240 * 4];
        static uint8_t idx[240 * 4];
        if (ps > 1) {
            const int fi = y240 >> 1 < VF_H ? y240 >> 1 : VF_H - 1;
            for (x = 0; x < W; x++)
                line[x] = hrow[fi / ps * ps][x];
        } else {
            const float fy = (((float)y + 0.5f) / fs - 0.5f - 1.0f) * 0.5f, f0 = fm_floorf(fy), t = fy - f0;
            int r[4], k;
            for (k = 0; k < 4; k++) {
                const int c = (int)f0 - 1 + k;
                r[k] = c < 0 ? 0 : c >= VF_H ? VF_H - 1 : c;
            }
            for (x = 0; x < W; x++) {
                const float v = vhd_cubic(hrow[r[0]][x], hrow[r[1]][x], hrow[r[2]][x], hrow[r[3]][x], t);
                line[x] = v < 0.0f ? 0.0f : v > 255.0f ? 255.0f : v;
            }
        }
        for (x = 0; x < W; x++) {                /* (between two of the map's steps, unless POSTER wants its steps) */
            const int a = (int)line[x], b = a < 255 ? a + 1 : 255;
            const float f = line[x] - (float)a;
            idx[x] = poster ? vlut[(int)(line[x] + 0.5f)] : (uint8_t)((float)vlut[a] + ((float)vlut[b] - (float)vlut[a]) * f + 0.5f);
        }
        for (x = 0; x < W; x++) {
            int xg = x - o, xr, xb;
            xg %= W, xg += xg < 0 ? W : 0;
            xr = (xg - spr) % W, xr += xr < 0 ? W : 0;
            xb = (xg + spr) % W, xb += xb < 0 ? W : 0;
            out[4 * x] = vhd_pal[idx[xr]][0];
            out[4 * x + 1] = vhd_pal[idx[xg]][1];
            out[4 * x + 2] = vhd_pal[idx[xb]][2];
            out[4 * x + 3] = 255;
        }
    }
    for (y = 1; y < 240; y += 2)                 /* (SYNC's tears fade as vs_paint fades them: once a frame) */
        if (y >> 1 < VF_H)
            vtear[y >> 1] = (int8_t)(vtear[y >> 1] * 3 / 4);
    {                                            /* the geometry, again at this size, broken as vis_band breaks it */
        const int mo = VPAR(VMOSH), co = VPAR(VCORRUPT), q = ps > 1 ? ps * 2 : 1;
        uint8_t gc[3];
        uint32_t hs = (uint32_t)(vt * 20.0f) * 2654435761u;
        vhd_rgb(VPAR(VFOLD) || VPAR(VPOSTER) ? vpal[vlut[236]] : vpal[236], gc);
        for (i = 0; i < vnseg; i++) {
            int xa = vseg[i].x0, ya = vseg[i].y0, xb = vseg[i].x1, yb = vseg[i].y1;
            hs = hs * 1664525u + 1013904223u;
            if (co && (int)(hs >> 25) < co / 3) {
                if (hs >> 16 & 1u)
                    continue;
                {
                    const int dx = (int)(hs >> 8 & 63u) - 32, dy = (int)(hs >> 14 & 31u) - 16;
                    xa += dx, xb += dx, ya += dy, yb += dy;
                }
            }
            if (mo) {
                const int k = 1 + mo / 25;
                if (xa >= 0 && ya >= 0 && xa < 240 && ya < 240) {
                    const int8_t *m = vmv[ya >> 4][xa >> 4];
                    xa += m[0] * k, ya += m[1] * k;
                }
                if (xb >= 0 && yb >= 0 && xb < 240 && yb < 240) {
                    const int8_t *m = vmv[yb >> 4][xb >> 4];
                    xb += m[0] * k, yb += m[1] * k;
                }
            }
            if (q > 1)
                xa = xa / q * q, ya = ya / q * q, xb = xb / q * q, yb = yb / q * q;
            {
                const int ra = ya < 0 ? 0 : ya > 239 ? 239 : ya, rb = yb < 0 ? 0 : yb > 239 ? 239 : yb,
                          oa = VPAR(FXV) ? 0 : o240[ra], ob = VPAR(FXV) ? 0 : o240[rb];
                vhd_line(((float)(xa + oa) + 0.5f) * fs, ((float)ya + 0.5f) * fs, ((float)(xb + ob) + 0.5f) * fs,
                         ((float)yb + 0.5f) * fs, 1.0f * fs, gc, 0);   /* (two of the FM-1's pixels wide, as vis_seg) */
            }
        }
    }
    if (vrutt) {                                 /* Rutt's scanlines, lifted by the light under them */
        const float lift = 0.10f + 0.12f * vgain;
        int j;
        for (j = 0; j < 32; j++) {
            const int by = 10 + j * 7, fy = by >> 1 < VF_H ? by >> 1 : VF_H - 1;
            float px = 0.0f, py = (float)by - (float)vf[fy][0] * lift;
            for (x = 4; x < 240; x += 4) {
                const int b = vf[fy][x >> 1];
                const float yy = (float)by - (float)b * lift;
                uint8_t c[3];
                vhd_rgb(vpal[b < 64 ? 64 + b : b], c);
                vhd_line((px + 0.5f) * fs, (py + 0.5f) * fs, ((float)x + 0.5f) * fs, (yy + 0.5f) * fs, 0.5f * fs, c, 0);
                px = (float)x, py = yy;
            }
        }
    }
    for (i = 0; i < vntri; i++) {                /* the objects' faces, far to near, at this size */
        const int o = vtri_ord[i];
        float X[3], Y[3], t;
        int ia = 0, ib = 1, ic = 2, k, ytop, ybot;
        uint8_t c[3];
        const uint16_t cs = vtri[o].c;
        vhd_rgb((uint16_t)(cs >> 8 | cs << 8), c);
        for (k = 0; k < 3; k++)
            X[k] = (float)vtri[o].x[k] * fs * (1.0f / 16.0f), Y[k] = (float)vtri[o].y[k] * fs * (1.0f / 16.0f);
        if (Y[ib] < Y[ia]) k = ia, ia = ib, ib = k;
        if (Y[ic] < Y[ia]) k = ia, ia = ic, ic = k;
        if (Y[ic] < Y[ib]) k = ib, ib = ic, ic = k;
        ytop = (int)-fm_floorf(0.5f - Y[ia]), ybot = (int)-fm_floorf(0.5f - Y[ic]);   /* (ceil) */
        ytop = ytop < 0 ? 0 : ytop, ybot = ybot > W ? W : ybot;
        for (y = ytop; y < ybot; y++) {
            const float ys = (float)y + 0.5f;
            float xl = X[ia] + (X[ic] - X[ia]) * (ys - Y[ia]) / (Y[ic] - Y[ia] != 0.0f ? Y[ic] - Y[ia] : 1.0f), xr;
            if (ys < Y[ib])
                xr = X[ia] + (X[ib] - X[ia]) * (ys - Y[ia]) / (Y[ib] - Y[ia] != 0.0f ? Y[ib] - Y[ia] : 1.0f);
            else
                xr = X[ib] + (X[ic] - X[ib]) * (ys - Y[ib]) / (Y[ic] - Y[ib] != 0.0f ? Y[ic] - Y[ib] : 1.0f);
            if (xl > xr)
                t = xl, xl = xr, xr = t;
            {                                    /* (the pixels whose middles are inside: neighbours meet, no seam) */
                const int x0 = (int)-fm_floorf(0.5f - xl), x1 = (int)-fm_floorf(0.5f - xr);
                for (x = x0 < 0 ? 0 : x0; x < x1 && x < W; x++)
                    vhd_plot(x, y, c, 1.0f, vo_mode);
            }
        }
    }
    for (i = 0; i < vnwire; i++) {               /* the edges (Points: dots) */
        uint8_t c[3];
        const uint16_t cs = vwire[i].c;
        const float x0 = (float)vwire[i].x0 * fs / 16.0f, y0 = (float)vwire[i].y0 * fs / 16.0f,
                    x1 = (float)vwire[i].x1 * fs / 16.0f, y1 = (float)vwire[i].y1 * fs / 16.0f;
        vhd_rgb((uint16_t)(cs >> 8 | cs << 8), c);
        if (vo_pts)
            vhd_line(x0, y0, x0, y0, 0.9f * fs, c, vo_mode);
        else
            vhd_line(x0, y0, x1, y1, 0.45f * fs, c, vo_mode);
    }
    if (VPAR(FXV)) {                             /* TARGETS Video All: the rows shifted and split after everything */
        static uint8_t row[960 * 4];
        for (y = 0; y < W; y++) {
            const int y240 = y / S, o = o240[y240] * S, s = s240[y240] * S;
            uint8_t *p = rgba + (size_t)y * (size_t)W * 4u;
            if (!o && !s)
                continue;
            memcpy(row, p, (size_t)W * 4u);
            for (x = 0; x < W; x++) {
                int xg = (x - o) % W, xr, xb;
                xg += xg < 0 ? W : 0;
                xr = (xg - s) % W, xr += xr < 0 ? W : 0;
                xb = (xg + s) % W, xb += xb < 0 ? W : 0;
                p[4 * x] = row[4 * xr], p[4 * x + 1] = row[4 * xg + 1], p[4 * x + 2] = row[4 * xb + 2];
            }
        }
    }
    vhd_post(rgba, W, S);                        /* STYLE, as the FM-1 shows it */
}
#endif
