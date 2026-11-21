/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal, guiatihe333 */
/* CORTEX's video synth (app/ui.c 1.6, the visual), made platform-free for CORTEX VIDEO: included by vsynth.c */
#define VF_W 120
#define VF_H 120
#define VCX 59.5f
#define VCY 59.5f
#define VIS_SCORE 9
#define VSEG_N 270
static uint8_t vf[VF_H][VF_W] VS_POOL;
static uint8_t vtmp[VF_H][VF_W] VS_POOL;      /* FEEDBACK: the last frame, turned and zoomed, before it comes back */
static uint16_t vpal_screen[256] VS_POOL;     /* screen byte order, prepared once per visual frame */
static uint16_t vpal[256] VS_POOL;            /* (RGB565, not yet swapped: its planes taken apart) */
static uint8_t vline[2][240] VS_POOL __attribute__((aligned(4)));         /* (a screen row of the field, smoothed; its colour index) */
static struct {
    float x, y, vx, vy;
    uint8_t on, v;
} vpart[48] VS_POOL;
static struct {
    float x, y, r, dr;
    int16_t v;
} vring[16] VS_POOL;
static struct {
    float s, a;
    int16_t v;
} vsq[20] VS_POOL;
static int8_t vhist[10][VF_W] VS_POOL;
static struct {
    int16_t x0, y0, x1, y1;
} vseg[VSEG_N] VS_POOL;                       /* GEOMETRY: this frame's lines, screen pixels */
static int vnseg, vpal_key = -1;
static uint8_t vhist_w, vis_ok;
static int8_t vtear[VF_H];                      /* SYNC: each row's tear now (a bass hit) */
static float vchroma[12], vorb[VS_NVOX][2], vt, vbass_prev, vband[VS_NBAND], vlv[VS_NVOX], vgain;
static uint32_t vbeat_n;                        /* (the beats counted: which way the next one turns) */
static float vpeak[VS_NBAND], vprev[VS_NBAND], vflux_avg, vbeat, vhue_kick;   /* the ear's: each band's recent peak,
                                                                                * the last frame's, the rise's average,
                                                                                * the beat (1 .. 0), HUE's jump */
static uint32_t vbeat_t;
/* the react matrix: the visual's settings as it reads them this frame (VPAR), each its knob's plus what its slots
 * give: a source (the ear's level, its bands, the beat, a voice, the notes, a chord's change, a slow LFO, a random
 * held on the beats) times its depth across the setting's range; a slot on All moves every setting of a range (not
 * the switches), each a third as much */
static int16_t veff[ST_N];
#define VPAR(k) veff[ST_##k]
static float vnote_p, vchord_p, vsh;
static uint8_t vchord_was = 255;
/* the genome: 16 values (0..1) the pictures, the geometry and the feedback take their shapes from, each drifting to a
 * target of its own (MUTATE: how fast, how far the next), thrown on a beat (CHAOS), held (REC), made anew (EDIT) */
#define VG_N 16
#define G(k, lo, hi) ((lo) + ((hi) - (lo)) * vg[k])
static float vg[VG_N] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f},
             vg_to[VG_N] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
/* MORPH: the picture now (vcur), the one it turns into (vnext, -1 none) and how far (vmw), the time to the next */
static int8_t vcur = -1, vnext = -1, vgeo = -1, vlast = -1, vgeo_now, vrutt;   /* (vrutt: Rutt in either geometry) */
static uint8_t vlut[256];                       /* CHROMA: the field's light to its palette index (folded, stepped, keyed) */
static float vmw, vmorph_left;
static int vamp = 256;                          /* (each picture's light, as it fades in or out: 256 whole) */
static uint32_t vseen, vrng = 0x2545F491u;
static float vis_rnd(void)                       /* 0..1 */
{
    vrng ^= vrng << 13;
    vrng ^= vrng >> 17;
    vrng ^= vrng << 5;
    return (float)(vrng >> 8) * (1.0f / 16777216.0f);
}
static uint8_t (*vdst)[VF_W] = vf;              /* what the pictures draw into: the field, or LAYER's (vtmp) */
static inline void vf_add(int x, int y, int v)
{
    v = (v * vamp) >> 8;
    if ((unsigned)x < VF_W && (unsigned)y < VF_H) {
        const int a = vdst[y][x] + v;
        vdst[y][x] = (uint8_t)(a > 255 ? 255 : a < 0 ? 0 : a);
    }
}
static void vf_addf(float x, float y, int v)    /* v at (x, y) between cells: spread on the four (no stairs) */
{
    const int ix = (int)fm_floorf(x), iy = (int)fm_floorf(y);
    const int fx = (int)((x - (float)ix) * 16.0f), fy = (int)((y - (float)iy) * 16.0f);
    vf_add(ix, iy, (v * (16 - fx) * (16 - fy)) >> 8);
    vf_add(ix + 1, iy, (v * fx * (16 - fy)) >> 8);
    vf_add(ix, iy + 1, (v * (16 - fx) * fy) >> 8);
    vf_add(ix + 1, iy + 1, (v * fx * fy) >> 8);
}
static void vf_disc(float cx, float cy, float r, int v)   /* soft: brightest in the middle */
{
    const int x0 = (int)(cx - r), x1 = (int)(cx + r) + 1, y0 = (int)(cy - r), y1 = (int)(cy + r) + 1;
    int x, y;
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++) {
            const float dx = (float)x - cx, dy = (float)y - cy, d2 = dx * dx + dy * dy, r2 = r * r;
            if (d2 < r2)
                vf_add(x, y, (int)((float)v * (1.0f - d2 / r2)));
        }
}
static void vf_line(float x0, float y0, float x1, float y1, int v)
{
    const float dx = x1 - x0, dy = y1 - y0, n = fm_maxf(fm_fabsf(dx), fm_fabsf(dy));
    int i, k = (int)n + 1;
    for (i = 0; i <= k; i++) {
        const float t = (float)i / (float)k;
        vf_addf(x0 + dx * t, y0 + dy * t, v);
    }
}
static void vf_circle(float cx, float cy, float r, int v)
{
    const int n = 8 + (int)(r * 5.0f);
    int i;
    for (i = 0; i < n; i++) {
        const float a = FM_TWO_PI * (float)i / (float)n;
        vf_addf(cx + r * fm_cosf(a), cy + r * fm_sinf(a), v);
    }
}
static void vseg_add(float x0, float y0, float x1, float y1)
{
    if (vnseg < VSEG_N) {
        vseg[vnseg].x0 = (int16_t)x0, vseg[vnseg].y0 = (int16_t)y0;
        vseg[vnseg].x1 = (int16_t)x1, vseg[vnseg].y1 = (int16_t)y1;
        vnseg++;
    }
}
static float vo_note = 60.0f;                   /* Osc's SYNC Notes: the last note's pitch, eased */
static int vo_note_to = 60;
static void vis_note(int mode, int voice, int note, int vel)   /* a note began: the picture answers it */
{
    int i, k;
    vo_note_to = note;
    if ((unsigned)voice >= VS_NVOX)
        voice = 0;
    if (mode == 0) {                             /* Bloom: a ring from its voice's orb, quicker the higher */
        for (i = 0; i < 16 && vring[i].v > 0; i++)
            ;
        if (i < 16) {
            vring[i].x = vorb[voice][0];
            vring[i].y = vorb[voice][1];
            vring[i].r = 2.0f;
            vring[i].dr = 0.6f + (float)(note - 36) * 0.025f;
            vring[i].v = (int16_t)(60 + vel);
        }
    } else if (mode == 2 || mode == 6) {         /* Prism, Spiral: its pitch class */
        vchroma[note % 12] = fm_minf(vchroma[note % 12] + (float)vel * 0.012f, 1.5f);
    } else if (mode == 3) {                      /* Swarm: sparks from where it sits on the keyboard */
        const int n = 4 + vel / 16;
        for (k = 0, i = 0; i < 48 && k < n; i++)
            if (!vpart[i].on) {
                vpart[i].on = 1;
                vpart[i].x = fm_clampf((float)(note - 30) * (float)VF_W / 72.0f, 1.0f, VF_W - 2.0f) +
                             (vis_rnd() - 0.5f) * 4.0f;
                vpart[i].y = VF_H - 2.0f;
                vpart[i].vx = (vis_rnd() - 0.5f) * 1.2f;
                vpart[i].vy = -(0.8f + (float)vel * 0.02f) * (voice == 2 ? 0.6f : 1.0f) * (0.7f + 0.6f * vis_rnd());
                vpart[i].v = (uint8_t)(120 + vel);
                k++;
            }
    }
}
static void vis_fold(uint8_t (*f)[VF_W], int m)  /* MIRROR: a plane folded (Mirror, Flip, Quad, Kaleido) */
{
    int x, y;
    if (m == 4)                                  /* Kaleido: the top-left quadrant's lower half its upper's mirror */
        for (y = 0; y < VF_H / 2; y++)
            for (x = 0; x < y; x++)
                f[y][x] = f[x][y];
    if (m == 1 || m >= 3)
        for (y = 0; y < VF_H; y++)
            for (x = 0; x < VF_W / 2; x++)
                f[y][VF_W - 1 - x] = f[y][x];
    if (m == 2 || m >= 3)
        for (y = 0; y < VF_H / 2; y++)
            memcpy(f[VF_H - 1 - y], f[y], VF_W);
}

/* the camera (vs_camera): its light, its two colour-difference planes, at the field's size (bilinear from 60 x 60) */
static uint8_t vcy[VF_H][VF_W] VS_POOL;
static int8_t vcb[VF_H][VF_W] VS_POOL, vcr[VF_H][VF_W] VS_POOL;
static const uint8_t *vcam;
static int vcam_on(void) { return VPAR(VMODE) >= VS_NMODE && vcam; }
void vs_camera(const uint8_t *rgb332) { vcam = rgb332; }
static void vis_cam_in(void)
{
    static uint8_t ty[256];
    static int8_t tb[256], tr[256];
    static uint8_t made;
    static uint8_t sy[60][60];
    static int8_t sb[60][60], sr[60][60];
    int x, y;
    if (!made) {                                 /* (RGB332 to Y, Cb, Cr: a table) */
        int i;
        for (i = 0; i < 256; i++) {
            const int r = (i >> 5) * 255 / 7, g = (i >> 2 & 7) * 255 / 7, b = (i & 3) * 85;
            ty[i] = (uint8_t)((77 * r + 150 * g + 29 * b) >> 8);
            tb[i] = (int8_t)((-43 * r - 85 * g + 128 * b) >> 9);    /* (halved: -64 .. 63) */
            tr[i] = (int8_t)((128 * r - 107 * g - 21 * b) >> 9);
        }
        made = 1;
    }
    for (y = 0; y < 60; y++)
        for (x = 0; x < 60; x++) {
            const uint8_t c = vcam[y * 60 + x];
            sy[y][x] = ty[c], sb[y][x] = tb[c], sr[y][x] = tr[c];
        }
    for (y = 0; y < VF_H; y++) {                 /* (row Y: between source rows i0, i1, w1 quarters of i1) */
        const int i0 = (y & 1) ? y >> 1 : (y >> 1) - 1 < 0 ? 0 : (y >> 1) - 1,
                  i1 = (y & 1) ? ((y >> 1) + 1 < 60 ? (y >> 1) + 1 : 59) : y >> 1,
                  w1 = (y & 1) ? 1 : 3;
        for (x = 0; x < VF_W; x++) {
            const int j0 = (x & 1) ? x >> 1 : (x >> 1) - 1 < 0 ? 0 : (x >> 1) - 1,
                      j1 = (x & 1) ? ((x >> 1) + 1 < 60 ? (x >> 1) + 1 : 59) : x >> 1,
                      v1 = (x & 1) ? 1 : 3;
#define BI(P) (((P[i0][j0] * (4 - v1) + P[i0][j1] * v1) * (4 - w1) + (P[i1][j0] * (4 - v1) + P[i1][j1] * v1) * w1) >> 4)
            vcy[y][x] = (uint8_t)BI(sy);
            vcb[y][x] = (int8_t)BI(sb);
            vcr[y][x] = (int8_t)BI(sr);
#undef BI
        }
    }
}
static void vis_cam(float tr)                    /* the camera's light into the field, TRAILS its smear */
{
    const int kt = (int)(tr * tr * 200.0f);
    int x, y;
    vis_cam_in();
    for (y = 0; y < VF_H; y++)
        for (x = 0; x < VF_W; x++)
            vf[y][x] = (uint8_t)((vcy[y][x] * (256 - kt) + vf[y][x] * kt) >> 8);
}
static void vis_mirror(int m)                    /* MIRROR: the field folded (the camera's colour with it) */
{
    vis_fold(vf, m);
    if (vcam_on()) {
        vis_fold((uint8_t (*)[VF_W])vcb, m);
        vis_fold((uint8_t (*)[VF_W])vcr, m);
    }
}
static void geo_lines(int geo, int sides);
static void geo_transform(void);
static uint32_t ghash(uint32_t x)                /* (a star's own numbers) */
{
    x ^= x >> 16;
    x *= 0x7FEB352Du;
    x ^= x >> 15;
    x *= 0x846CA68Bu;
    return x ^ (x >> 16);
}
static int seg_clip(float *x0, float *y0, float *x1, float *y1)   /* a line to the screen (Liang-Barsky); 0: off it */
{
    float t0 = 0.0f, t1 = 1.0f;
    const float dx = *x1 - *x0, dy = *y1 - *y0, p[4] = {-dx, dx, -dy, dy},
                q[4] = {*x0 - 0.0f, 238.0f - *x0, *y0 - 0.0f, 239.0f - *y0};
    int k;
    for (k = 0; k < 4; k++) {
        if (p[k] == 0.0f) {
            if (q[k] < 0.0f)
                return 0;
        } else {
            const float r = q[k] / p[k];
            if (p[k] < 0.0f) {
                if (r > t1)
                    return 0;
                if (r > t0)
                    t0 = r;
            } else {
                if (r < t0)
                    return 0;
                if (r < t1)
                    t1 = r;
            }
        }
    }
    *x1 = *x0 + t1 * dx, *y1 = *y0 + t1 * dy;
    *x0 = *x0 + t0 * dx, *y0 = *y0 + t0 * dy;
    return 1;
}
static int16_t geo_was = -1;                    /* (hoisted: vs_reset sets it) */
static float ga, gx, gy;                        /* TRANSFORM's lines: turned, carried so far */
static void vis_geometry(void)                   /* GEOMETRY's lines, this frame (screen pixels, 240 x 240) */
{
    int geo, i, n1;
    if (VPAR(VGEO) != geo_was) {       /* (GEOMETRY turned: that one, until MORPH picks another) */
        geo_was = VPAR(VGEO);
        vgeo = -1;
    }
    geo = VPAR(VGEO) && vgeo > 0 && VPAR(VMORPH) ? vgeo : VPAR(VGEO);
    vgeo_now = (int8_t)geo;
    vrutt = (int8_t)(geo == 6 || VPAR(VGEO2) == 6);
    vnseg = 0;
    geo_lines(geo, VPAR(VSIDES));
    n1 = vnseg;
    if (VPAR(VGEO2) && VPAR(VGEO2) != 6) {       /* SHAPE 2: a second, its own sides, SIZE 2 of the first's */
        const float s2 = (float)VPAR(VSIZE2) * 0.01f;
        geo_lines(VPAR(VGEO2), VPAR(VSIDES2));
        for (i = n1; i < vnseg; i++) {
            vseg[i].x0 = (int16_t)(120.0f + ((float)vseg[i].x0 - 120.0f) * s2);
            vseg[i].y0 = (int16_t)(120.0f + ((float)vseg[i].y0 - 120.0f) * s2);
            vseg[i].x1 = (int16_t)(120.0f + ((float)vseg[i].x1 - 120.0f) * s2);
            vseg[i].y1 = (int16_t)(120.0f + ((float)vseg[i].y1 - 120.0f) * s2);
        }
    }
    geo_transform();
    {                                            /* (the lines clipped at the screen's edge: they wrapped round) */
        int o = 0;
        for (i = 0; i < vnseg; i++) {
            float x0 = vseg[i].x0, y0 = vseg[i].y0, x1 = vseg[i].x1, y1 = vseg[i].y1;
            if (!seg_clip(&x0, &y0, &x1, &y1))
                continue;
            vseg[o].x0 = (int16_t)x0, vseg[o].y0 = (int16_t)y0, vseg[o].x1 = (int16_t)x1, vseg[o].y1 = (int16_t)y1;
            o++;
        }
        vnseg = o;
    }
}
static void geo_lines(int geo, int sides)        /* geometry geo's lines, added */
{
    const float lvl = fm_minf(en_master_level * vgain * 2.0f, 1.2f) + 0.4f * vbeat * (float)VPAR(VPULSE) * 0.01f;
    const int w = (int)en_wave_w;
    int i, k;
    switch (geo) {
    case 1:                                      /* Polygon: three nested, turning against each other */
        for (k = 0; k < 3; k++) {
            const float nr = G(8, 0.4f, 0.85f), r = (70.0f + 30.0f * lvl + 10.0f * vband[0]) * (k == 0 ? 1.0f : k == 1 ? nr : nr * nr),
                        a0 = vt * (k & 1 ? -0.4f : 0.3f) + (float)k * 0.3f;
            for (i = 0; i < sides; i++) {
                const float a = a0 + FM_TWO_PI * (float)i / (float)sides, b = a + FM_TWO_PI / (float)sides;
                vseg_add(120.0f + r * fm_cosf(a), 120.0f + r * fm_sinf(a), 120.0f + r * fm_cosf(b), 120.0f + r * fm_sinf(b));
            }
        }
        break;
    case 2: {                                    /* Lissajous: SIDES against SIDES + 1, its phase drifting */
        const float a = (float)sides * 0.5f, b = a + G(9, 0.25f, 1.5f), ph = vt * 0.3f, amp = 80.0f + 25.0f * lvl;
        float px = 120.0f + amp * fm_sinf(ph), py = 120.0f;
        for (i = 1; i <= 160; i++) {
            const float t = FM_TWO_PI * (float)i / 160.0f,
                        x = 120.0f + amp * (1.0f + 0.15f * vband[2]) * fm_sinf(a * t + ph), y = 120.0f + amp * fm_sinf(b * t);
            vseg_add(px, py, x, y);
            px = x, py = y;
        }
        break;
    }
    case 3: {                                    /* Grid: a floor to the horizon, running towards you */
        const float hz = G(10, 70.0f, 130.0f), run = vt * 0.5f;
        for (i = -8; i <= 8; i++)
            vseg_add(120.0f, hz, 120.0f + (float)i * 40.0f, 240.0f);
        for (k = 0; k < 8; k++) {
            float z = (float)k / 8.0f + run;
            z -= fm_floorf(z);
            {
                const float y = hz + 144.0f * z * z;
                vseg_add(0.0f, y, 239.0f, y);
            }
        }
        for (i = 0; i < 64; i++) {               /* the horizon: the sound's shape */
            const int s0 = en_wave[(unsigned)((int)w - 128 + i * 2) % VS_WAVE_N],
                      s1 = en_wave[(unsigned)((int)w - 128 + i * 2 + 2) % VS_WAVE_N];
            vseg_add((float)i * 3.75f, hz - fm_fabsf((float)s0) * 0.3f * vgain, (float)(i + 1) * 3.75f,
                     hz - fm_fabsf((float)s1) * 0.3f * vgain);
        }
        break;
    }
    case 4: {                                    /* Mandala: the sound's shape, SIDES times round, each fold mirrored */
        float px = 0.0f, py = 0.0f;
        const int n = sides * 20;
        for (i = 0; i <= n; i++) {
            const int f = i % 20, mi = (i / 20) & 1 ? 19 - f : f;
            const int s = en_wave[(unsigned)((int)w - 128 + mi * 5) % VS_WAVE_N];
            const float a = vt * 0.1f + FM_TWO_PI * (float)i / (float)n,
                        r = G(11, 25.0f, 65.0f) + 40.0f * lvl + fm_fabsf((float)s) * G(12, 0.2f, 0.9f) * vgain,
                        x = 120.0f + r * fm_cosf(a), y = 120.0f + r * fm_sinf(a);
            if (i)
                vseg_add(px, py, x, y);
            px = x, py = y;
        }
        break;
    }
    case 5: {                                    /* Scope: the sound, across */
        float px = 0.0f, py = 120.0f;
        for (i = 0; i < 128; i++) {
            const int s = en_wave[(unsigned)((int)w - 128 + i) % VS_WAVE_N];
            const float x = (float)i * (239.0f / 127.0f), y = 120.0f - (float)s * 0.7f * vgain;
            if (i)
                vseg_add(px, py, x, y);
            px = x, py = y;
        }
        break;
    }
    case 7:                                      /* Star: SIDES points, turning, breathing with the level */
        for (k = 0; k < 2; k++) {
            const float R = (60.0f + 40.0f * lvl) * (k ? 0.55f : 1.0f), a0 = vt * (k ? -0.25f : 0.2f);
            for (i = 0; i < 2 * sides; i++) {
                const float a = a0 + FM_PI * (float)i / (float)sides, b = a + FM_PI / (float)sides,
                            ra = i & 1 ? R * 0.42f : R, rb = i & 1 ? R : R * 0.42f;
                vseg_add(120.0f + ra * fm_cosf(a), 120.0f + ra * fm_sinf(a), 120.0f + rb * fm_cosf(b), 120.0f + rb * fm_sinf(b));
            }
        }
        break;
    case 8:                                      /* Rings: SIDES circles out from the middle, each a band's */
        for (k = 0; k < sides; k++) {
            float z = (float)k / (float)sides + vt * 0.08f;
            z -= fm_floorf(z);
            {
                const float r = 6.0f + 120.0f * z * (0.85f + 0.3f * vband[k % VS_NBAND]);
                float px = 120.0f + r, py = 120.0f;
                for (i = 1; i <= 20; i++) {
                    const float a = FM_TWO_PI * (float)i / 20.0f, x = 120.0f + r * fm_cosf(a), y = 120.0f + r * fm_sinf(a);
                    vseg_add(px, py, x, y);
                    px = x, py = y;
                }
            }
        }
        break;
    case 9: {                                    /* Spirograph: a hypotrochoid, SIDES lobes, its pen drifting */
        const float n = (float)sides, d = G(13, 0.4f, 1.2f) + 0.2f * fm_sinf(vt * 0.3f), sc = (70.0f + 25.0f * lvl) / (n - 1.0f + d);
        float px = 0.0f, py = 0.0f;
        for (i = 0; i <= 200; i++) {
            const float t = FM_TWO_PI * (float)i / 200.0f + vt * 0.05f,
                        x = 120.0f + sc * ((n - 1.0f) * fm_cosf(t) + d * fm_cosf((n - 1.0f) * t)),
                        y = 120.0f + sc * ((n - 1.0f) * fm_sinf(t) - d * fm_sinf((n - 1.0f) * t));
            if (i)
                vseg_add(px, py, x, y);
            px = x, py = y;
        }
        break;
    }
    case 10: {                                   /* Rose: r = cos(k t), k from SIDES, petals turning */
        const float kk = (float)sides * 0.5f, R = 80.0f + 30.0f * lvl;
        float px = 0.0f, py = 0.0f;
        for (i = 0; i <= 180; i++) {
            const float t = FM_TWO_PI * 2.0f * (float)i / 180.0f, r = R * fm_cosf(kk * t),
                        x = 120.0f + r * fm_cosf(t + vt * 0.15f), y = 120.0f + r * fm_sinf(t + vt * 0.15f);
            if (i)
                vseg_add(px, py, x, y);
            px = x, py = y;
        }
        break;
    }
    case 11: {                                   /* Cube: a wire cube turning in space, as big as the sound */
        static const int8_t E[12][2] = {{0, 1}, {1, 3}, {3, 2}, {2, 0}, {4, 5}, {5, 7}, {7, 6}, {6, 4},
                                        {0, 4}, {1, 5}, {2, 6}, {3, 7}};
        const float ax = vt * 0.31f, ay = vt * 0.47f, s = 55.0f + 25.0f * lvl;
        float px[8], py[8];
        for (i = 0; i < 8; i++) {
            const float x = i & 1 ? 1.0f : -1.0f, y = i & 2 ? 1.0f : -1.0f, z = i & 4 ? 1.0f : -1.0f,
                        y1 = y * fm_cosf(ax) - z * fm_sinf(ax), z1 = y * fm_sinf(ax) + z * fm_cosf(ax),
                        x2 = x * fm_cosf(ay) + z1 * fm_sinf(ay), z2 = -x * fm_sinf(ay) + z1 * fm_cosf(ay),
                        f = 3.0f / (4.0f + z2);
            px[i] = 120.0f + x2 * s * f;
            py[i] = 120.0f + y1 * s * f;
        }
        for (i = 0; i < 12; i++)
            vseg_add(px[E[i][0]], py[E[i][0]], px[E[i][1]], py[E[i][1]]);
        break;
    }
    case 12:                                     /* Tunnel: SIDES-gons going away, their corners joined */
        for (k = 0; k < 8; k++) {
            float z = (float)k / 8.0f + vt * 0.12f;
            z -= fm_floorf(z);
            {
                const float r = 8.0f + 150.0f * z * z, a0 = vt * 0.1f + z * 1.5f;
                for (i = 0; i < sides; i++) {
                    const float a = a0 + FM_TWO_PI * (float)i / (float)sides, b = a + FM_TWO_PI / (float)sides;
                    vseg_add(120.0f + r * fm_cosf(a), 120.0f + r * fm_sinf(a), 120.0f + r * fm_cosf(b), 120.0f + r * fm_sinf(b));
                }
            }
        }
        break;
    case 13:                                     /* Starfield: stars flying past, streaks as they near */
        for (k = 0; k < 48; k++) {
            const uint32_t hs = ghash((uint32_t)k * 2654435761u + 7u);
            const float a = (float)(hs & 1023u) * (FM_TWO_PI / 1024.0f), sp = 0.08f + 0.06f * (float)(hs >> 10 & 255u) / 255.0f;
            float z = (float)(hs >> 18 & 1023u) / 1024.0f + vt * sp * (1.0f + lvl);
            z -= fm_floorf(z);
            {
                const float r = 4.0f + 170.0f * z * z * z, r0 = r * (0.85f - 0.2f * lvl);
                vseg_add(120.0f + r0 * fm_cosf(a), 120.0f + r0 * fm_sinf(a), 120.0f + r * fm_cosf(a), 120.0f + r * fm_sinf(a));
            }
        }
        break;
    case 14: {                                   /* Circle: the sound wound round a circle (a polar scope) */
        float px = 0.0f, py = 0.0f;
        for (i = 0; i <= 128; i++) {
            const int s = en_wave[(unsigned)((int)w - 128 + (i & 127)) % VS_WAVE_N];
            const float a = FM_TWO_PI * (float)i / 128.0f + vt * 0.1f, r = 60.0f + (float)s * 0.45f * vgain,
                        x = 120.0f + r * fm_cosf(a), y = 120.0f + r * fm_sinf(a);
            if (i)
                vseg_add(px, py, x, y);
            px = x, py = y;
        }
        break;
    }
    case 15: {                                   /* Lattice: three sets of lines at 60 degrees, drifting (triangles) */
        const float sp = 240.0f / (float)sides, off = vt * 6.0f;
        for (k = 0; k < 3; k++) {
            const float a = (float)k * (FM_PI / 3.0f) + vt * 0.02f, ca = fm_cosf(a), sa = fm_sinf(a);
            for (i = -sides; i <= sides; i++) {
                float d = (float)i * sp + off;
                d -= sp * fm_floorf(d / sp + 0.5f) - (float)i * sp;
                vseg_add(120.0f + d * ca - 200.0f * sa, 120.0f + d * sa + 200.0f * ca, 120.0f + d * ca + 200.0f * sa,
                         120.0f + d * sa - 200.0f * ca);
            }
        }
        break;
    }
    case 16:                                     /* Spiral: SIDES arms out from the middle, turning */
        for (k = 0; k < sides; k++) {
            float px = 120.0f, py = 120.0f;
            for (i = 1; i <= 24; i++) {
                const float r = (float)i * (4.5f + 1.5f * lvl), a = vt * 0.4f + (float)i * 0.22f + FM_TWO_PI * (float)k / (float)sides,
                            x = 120.0f + r * fm_cosf(a), y = 120.0f + r * fm_sinf(a);
                vseg_add(px, py, x, y);
                px = x, py = y;
            }
        }
        break;
    default:
        break;
    }
}
static void geo_transform(void)
{
    int i;
    {   /* (1.5.22) the beat's punch on the lines, whatever TARGET: they leap out and turn a little, each beat the
         * other way, as far as PULSE says, falling back with the beat */
        const float k = vbeat * (float)VPAR(VPULSE) * 0.01f;
        if (k > 0.01f) {
            const float sc = 1.0f + 0.25f * k, an = 0.14f * k * (vbeat_n & 1u ? 1.0f : -1.0f),
                        ca = fm_cosf(an) * sc, sa = fm_sinf(an) * sc;
            for (i = 0; i < vnseg; i++) {
                const float x0 = (float)vseg[i].x0 - 120.0f, y0 = (float)vseg[i].y0 - 120.0f,
                            x1 = (float)vseg[i].x1 - 120.0f, y1 = (float)vseg[i].y1 - 120.0f;
                vseg[i].x0 = (int16_t)(120.0f + x0 * ca - y0 * sa), vseg[i].y0 = (int16_t)(120.0f + x0 * sa + y0 * ca);
                vseg[i].x1 = (int16_t)(120.0f + x1 * ca - y1 * sa), vseg[i].y1 = (int16_t)(120.0f + x1 * sa + y1 * ca);
            }
        }
    }
    {                                            /* (1.5.14) TRANSFORM on the lines too: turned on by ROTATE, carried by
                                                  * SHIFT (round the screen), grown or shrunk by ZOOM */
        const float sc = fm_exp2f((float)VPAR(VXZOOM) * 0.006f);   /* (x1.5 at 100, x0.66 at -100) */
        float ca, sa, ox, oy;
        const float shang = (float)VPAR(VOANGLE) * (FM_PI / 100.0f);
        if (VPAR(VXTGT) == 0) {                  /* (TARGET Feedback: the lines as drawn) */
            ga = gx = gy = 0.0f;
            return;
        }
        ga += (float)VPAR(VXROT) * 0.0008f;
        ga -= ga > FM_PI ? FM_TWO_PI : ga < -FM_PI ? -FM_TWO_PI : 0.0f;
        gx += (float)VPAR(VXSHX) * 0.04f * fm_cosf(shang);   /* (2 cells a frame, as the feedback's, along ANGLE) */
        gy += (float)VPAR(VXSHX) * 0.04f * fm_sinf(shang);
        gx -= gx >= 120.0f ? 240.0f : gx < -120.0f ? -240.0f : 0.0f;
        gy -= gy >= 120.0f ? 240.0f : gy < -120.0f ? -240.0f : 0.0f;
        if (!VPAR(VXROT) && !VPAR(VXSHX))
            ga = gx = gy = 0.0f;                 /* (the knobs back at 0: the lines as drawn) */
        if (ga == 0.0f && gx == 0.0f && gy == 0.0f && sc == 1.0f)
            return;
        ca = fm_cosf(ga) * sc, sa = fm_sinf(ga) * sc, ox = 120.0f - gx, oy = 120.0f - gy;
        for (i = 0; i < vnseg; i++) {
            const float x0 = (float)vseg[i].x0 - 120.0f, y0 = (float)vseg[i].y0 - 120.0f, x1 = (float)vseg[i].x1 - 120.0f,
                        y1 = (float)vseg[i].y1 - 120.0f;
            vseg[i].x0 = (int16_t)(ox + x0 * ca - y0 * sa), vseg[i].y0 = (int16_t)(oy + x0 * sa + y0 * ca);
            vseg[i].x1 = (int16_t)(ox + x1 * ca - y1 * sa), vseg[i].y1 = (int16_t)(oy + x1 * sa + y1 * ca);
        }
    }
}
static void vis_mutate(float amount)             /* the genome's targets thrown (0..1 how far) */
{
    int k;
    for (k = 0; k < VG_N; k++)
        vg_to[k] = fm_clampf(vg[k] + (vis_rnd() - 0.5f) * 2.0f * amount, 0.0f, 1.0f);
}
static void vis_life(void)                       /* MUTATE, CHAOS: the genome on its way */
{
    const float mu = (float)VPAR(VMUTATE) * 0.01f, ch = (float)VPAR(VCHAOS) * 0.01f;
    int k;
    if (vin.hold)
        return;
    if (ch > 0.0f && vbeat > 0.95f)              /* (a beat: thrown, as far as CHAOS says) */
        vis_mutate(0.35f * ch);
    for (k = 0; k < VG_N; k++) {
        vg[k] += (vg_to[k] - vg[k]) * (0.004f + 0.04f * mu);
        if (mu > 0.0f && fm_fabsf(vg_to[k] - vg[k]) < 0.02f)   /* there: the next, further the more it mutates */
            vg_to[k] = fm_clampf(vg[k] + (vis_rnd() - 0.5f) * (0.2f + 0.8f * mu), 0.0f, 1.0f);
    }
}
/* MOSH, SORT, CORRUPT, SLICE: the picture broken as a compressed video breaks. MOSH: the field in 8 x 8 blocks, each
 * with a motion vector it keeps (a P-frame with no I-frame): every frame each block carries the last one along its
 * vector, so what was there smears; a beat throws new vectors (the bass: longer), blocks come to rest as their vectors
 * run out. SORT: runs of bright cells along the rows laid out as ramps (pixel sorting; the highs lower its threshold).
 * CORRUPT: blocks copied where they don't belong (a buffer's offset gone wrong), blocks flattened to their mean
 * (macroblocks), the low bits crushed, more on the beats. SLICE: bands of rows shifted for a few frames, the odd rows
 * apart (interlace) */
#define VB_N (VF_W / 8)
static int8_t vmv[VB_N][VB_N][2];               /* MOSH: each block's motion vector (cells a frame) */
static uint8_t vmv_life[VB_N][VB_N];            /* ... frames it keeps it */
static int8_t vchan[VF_H / 8];                  /* SLICE: each band of 8 rows' colour planes apart */
static uint8_t vslice_left;
static void vis_mosh(void)
{
    const float mo = (float)VPAR(VMOSH) * 0.01f, so = (float)VPAR(VSORT) * 0.01f,
                co = (float)VPAR(VCORRUPT) * 0.01f;
    int bx, by, x, y, k;
    if (mo > 0.0f) {
        const int keep = (int)(256.0f * (0.55f + 0.44f * mo));   /* (how much of the carried frame stays) */
        for (by = 0; by < VB_N; by++)            /* new vectors: a beat throws many, the bass makes them long */
            for (bx = 0; bx < VB_N; bx++) {
                if (vmv_life[by][bx])
                    vmv_life[by][bx]--;
                else
                    vmv[by][bx][0] = vmv[by][bx][1] = 0;
                if (vis_rnd() < mo * (0.01f + 0.25f * vbeat)) {
                    const float a = vis_rnd() * FM_TWO_PI, len = 1.0f + (2.0f + 6.0f * vband[0]) * mo * vis_rnd();
                    vmv[by][bx][0] = (int8_t)(len * fm_cosf(a));
                    vmv[by][bx][1] = (int8_t)(len * fm_sinf(a));
                    vmv_life[by][bx] = (uint8_t)(6 + (int)(40.0f * mo * vis_rnd()));
                }
            }
        memcpy(vtmp, vf, sizeof vf);
        for (by = 0; by < VB_N; by++)
            for (bx = 0; bx < VB_N; bx++) {
                const int dx = vmv[by][bx][0], dy = vmv[by][bx][1];
                if (!dx && !dy)
                    continue;
                for (y = by * 8; y < by * 8 + 8; y++)
                    for (x = bx * 8; x < bx * 8 + 8; x++) {
                        int sx = x - dx, sy = y - dy;
                        sx = sx < 0 ? sx + VF_W : sx >= VF_W ? sx - VF_W : sx;
                        sy = sy < 0 ? sy + VF_H : sy >= VF_H ? sy - VF_H : sy;
                        {
                            const int c = vf[y][x], m = (vtmp[sy][sx] * keep) >> 8;
                            vf[y][x] = (uint8_t)(m > c ? m : c);   /* (the carried and the new, the brighter) */
                        }
                    }
            }
    }
    if (so > 0.0f) {                             /* SORT: the bright runs along the rows, as ramps */
        const int th = (int)(250.0f - 190.0f * so - 40.0f * vband[4]), up = ((int)(vt * 0.7f)) & 1;
        for (y = 0; y < VF_H; y++) {
            uint8_t *r = vf[y];
            x = 0;
            while (x < VF_W) {
                int x0, lo = 255, hi = 0;
                while (x < VF_W && r[x] < th)
                    x++;
                x0 = x;
                while (x < VF_W && r[x] >= th) {
                    lo = r[x] < lo ? r[x] : lo;
                    hi = r[x] > hi ? r[x] : hi;
                    x++;
                }
                if (x - x0 >= 4) {
                    const int n = x - x0;
                    for (k = 0; k < n; k++)
                        r[x0 + k] = (uint8_t)(lo + (hi - lo) * ((y + up) & 1 ? k : n - 1 - k) / (n - 1));
                }
            }
        }
    }
    if (co > 0.0f) {                             /* CORRUPT: blocks misplaced, flattened, crushed */
        const int n = (int)(co * (1.0f + 10.0f * vbeat) + vis_rnd());
        for (k = 0; k < n; k++) {
            const int w = 8 << (int)(vis_rnd() * 2.0f), h = 2 + (int)(vis_rnd() * 10.0f),
                      sx = (int)(vis_rnd() * (VF_W - w)), sy = (int)(vis_rnd() * (VF_H - h)),
                      dx = (int)(vis_rnd() * (VF_W - w)), dy = (int)(vis_rnd() * (VF_H - h));
            const float what = vis_rnd();
            for (y = 0; y < h; y++) {
                if (what < 0.5f) {               /* (a buffer's offset: another place's bytes) */
                    for (x = 0; x < w; x++)
                        vf[dy + y][dx + x] = vtmp[sy + y][sx + x];
                } else if (what < 0.8f) {        /* (a macroblock: its mean) */
                    int s = 0;
                    for (x = 0; x < w; x++)
                        s += vf[dy + y][dx + x];
                    memset(&vf[dy + y][dx], s / w, (uint32_t)w);
                } else {                         /* (its low bits crushed) */
                    for (x = 0; x < w; x++)
                        vf[dy + y][dx + x] &= 0xE0u;
                }
            }
        }
    }
}
static void vis_slice(void)                      /* SLICE: bands of rows shifted a while; the odd rows apart */
{
    const float sl = (float)VPAR(VSLICE) * 0.01f;
    int k, y;
    if (sl <= 0.0f)
        return;
    if (vslice_left)
        vslice_left--;
    else if (vis_rnd() < sl * (0.15f + 0.8f * vbeat)) {
        const int bands = 1 + (int)(vis_rnd() * 4.0f * sl);
        for (k = 0; k < bands; k++) {
            const int y0 = (int)(vis_rnd() * VF_H), n = 1 + (int)(vis_rnd() * 14.0f * sl);
            const int8_t o = (int8_t)((vis_rnd() - 0.5f) * 90.0f * sl);
            for (y = y0; y < y0 + n && y < VF_H; y++)
                vtear[y] = o;
        }
        for (k = 0; k < VF_H / 8; k++)
            vchan[k] = (int8_t)(vis_rnd() < sl * 0.5f ? (vis_rnd() - 0.5f) * 12.0f * sl : 0.0f);
        vslice_left = (uint8_t)(2 + (int)(vis_rnd() * 8.0f));
    }
}
static float vis_src(int s)                      /* a react source now, 0..1 */
{
    switch (s) {
    case 1: return fm_minf(en_master_level * vgain * 2.0f, 1.0f);
    case 2: return fm_minf((vband[0] + vband[1]) * 0.5f, 1.0f);
    case 3: return fm_minf((vband[2] + vband[3]) * 0.5f, 1.0f);
    case 4: return fm_minf((vband[4] + vband[5]) * 0.5f, 1.0f);
    case 5: return vbeat;
    case 6: case 7: case 8: return fm_minf(vlv[s - 6], 1.0f);
    case 9: return vnote_p;
    case 10: return vchord_p;
    case 11: return 0.5f + 0.5f * fm_sinf(vt * 0.3f);
    case 12: return vsh;
    case SRC_TEMPO: return 0.5f + 0.5f * fm_sinf(FM_TWO_PI * vin.phase);   /* (CORTEX VIDEO: a beat a cycle) */
    case SRC_BAR: return vin.phase * 0.25f - fm_floorf(vin.phase * 0.25f);  /* (a ramp a bar of four) */
    case SRC_CC: return (float)vin.cc * (1.0f / 127.0f);
    case SRC_VEL: return vvel_p;
    case SRC_KICK: return (float)vin.kick * (1.0f / 255.0f);   /* (the FM-1's drum detectors, from its FFT) */
    case SRC_SNARE: return (float)vin.snare * (1.0f / 255.0f);
    case SRC_HAT: return (float)vin.hat * (1.0f / 255.0f);
    case SRC_BRIGHT: return (float)vin.bright * (1.0f / 255.0f);
    default: return 0.0f;
    }
}
static void vis_react(void)                      /* this frame's settings: the knobs, moved by the react slots */
{
    int k, sl;
    for (k = ST_VMODE; k < ST_N; k++)
        veff[k] = vpar[k];
    vnote_p *= 0.7f;
    vvel_p *= 0.85f;                             /* (VEL: the last velocity, falling as NOTES does, slower) */
    vchord_p *= 0.9f;
    if (en_chord_root != vchord_was) {
        vchord_was = en_chord_root;
        vchord_p = 1.0f;
    }
    if (vbeat > 0.95f)
        vsh = vis_rnd();
    for (sl = 0; sl < VS_NSLOT; sl++) {
        const int b = ST_AR1S + sl * VS_SLOT_W, src = vpar[b], tg = vpar[b + 1], dp = vpar[b + 2], md = vpar[b + 3],
                  lag = vpar[b + 4];
        float x;
        if (!src || !tg || !dp) {
            vslot[sl] = 0.0f;
            continue;
        }
        x = vis_src(src);                        /* (CORTEX VIDEO: MODE, then LAG, then DEPTH) */
        if (md == 1)
            x = 1.0f - x;                        /* Invert: the quiet moves it */
        else if (md == 2) {                      /* Beat hold: taken on each beat, held to the next */
            if (vbeat > 0.95f)
                vslot_h[sl] = x;
            x = vslot_h[sl];
        } else if (md == 3)
            x = x > 0.5f ? 1.0f : 0.0f;          /* Gate: on or off */
        vslot[sl] += (x - vslot[sl]) * (lag ? fm_minf(1.0f, vdtk * (1.0f - 0.0097f * (float)lag)) : 1.0f);
        x = vslot[sl] * (float)dp * 0.01f;
        for (k = ST_VMODE; k <= VS_TGT_LAST; k++) {
            const vs_par_t *pi = vs_par_info(k);
            const int span = pi->hi - pi->lo;
            int v;
            if (tg < VS_NTGT ? k != ST_VMODE + tg - 1 : span < 10)
                continue;
            v = veff[k] + (int)(x * (float)span * (tg < VS_NTGT ? 1.0f : 0.33f) + (x < 0.0f ? -0.5f : 0.5f));
            veff[k] = (int16_t)(v < pi->lo ? pi->lo : v > pi->hi ? pi->hi : v);
        }
    }
}
static void vis_draw(int mode, float tr);
static void vis_step(int mode)                   /* a frame of the field and the geometry */
{
    static const float BW[VS_NBAND] = {1.0f, 1.4f, 2.0f, 2.8f, 4.0f, 6.0f};
    const float tr = (float)VPAR(VTRAIL) * 0.01f, fb = (float)VPAR(VFEED) * 0.01f,
                syn = (float)VPAR(VSYNC) * 0.01f;
    const int keep = mode == 1 ? 0 : (int)fm_clampf(256.0f - (116.0f - 114.0f * tr) * vdtk, 0.0f, 254.0f);   /* (CORTEX
                                                  * VIDEO: TRAILS' fade by the clock, as at 20 frames a second) */
    int x, y, k;
    if (!vis_ok) {                               /* (the pool isn't cleared at the start) */
        memset(vf, 0, sizeof vf);
        memset(vtmp, 0, sizeof vtmp);
        memset(vpart, 0, sizeof vpart);
        memset(vring, 0, sizeof vring);
        memset(vsq, 0, sizeof vsq);
        memset(vhist, 0, sizeof vhist);
        vis_ok = 1;
    }
    vis_react();
    mode = VPAR(VMODE) < VIS_SCORE ? VPAR(VMODE) : mode;
    vgain = 0.4f + 2.6f * (float)VPAR(VREACT) * 0.01f;
    vt += vdtk * 0.05f * (0.25f + 1.75f * (float)VPAR(VSPEED) * 0.01f) * G(0, 0.6f, 1.5f);   /* SPEED */
    if (vcur < 0)
        vcur = vlast = (int8_t)mode;
    if (mode != vlast) {                         /* (VISUAL turned, or a key: it turns into that picture, 3 s) */
        vlast = (int8_t)mode;
        if (vcur != mode) {
            vnext = (int8_t)mode;
            vmw = 0.0f;
        }
    }
    if (VPAR(VMORPH) && vnext < 0 && VPAR(VMODE) != VS_OSC) {   /* (not Osc: a picture made by hand) */   /* MORPH: now and then another picture, the geometry with it */
        vmorph_left -= 0.05f;
        if (vmorph_left <= 0.0f) {
            int n = (int)(vis_rnd() * 8.0f);
            if (n == vcur)
                n = (n + 1) % 8;
            vnext = (int8_t)n;
            vmw = 0.0f;
            vmorph_left = 40.0f - 34.0f * (float)VPAR(VMORPH) * 0.01f + 6.0f * vis_rnd();
            if (VPAR(VGEO))
                vgeo = (int8_t)(1 + (int)(vis_rnd() * 15.99f));
        }
    }
    vis_palette();
    vis_life();
    {                                            /* the ear: each band against its own recent peak (loud and quiet
                                                  * alike), held down in silence; the beats: the bands' rise, the low
                                                  * ones counting most, over 1.6 times its own average */
        float flux = 0.0f;
        const float pres = fm_minf(en_master_level * 30.0f, 1.0f);
        for (k = 0; k < VS_NBAND; k++) {
            const float e = en_bands[k] * BW[k];
            float nb;
            vpeak[k] = fm_maxf(e, fm_maxf(vpeak[k] * 0.993f, 1e-4f));
            nb = e / vpeak[k];
            nb = nb * nb * (3.0f - 2.0f * nb);   /* (a softer knee) */
            {   /* EAR: smoothed (0 at once); 1.5.22: it leaps up at once and lets go slower, as a VU's needle */
                const float tg = fm_minf(nb * pres * vgain * 0.55f + e * vgain * 2.0f, 1.5f),
                            rel = 1.0f - (float)VPAR(VEAR) * 0.0085f;
                vband[k] += (tg - vband[k]) * (tg > vband[k] ? fm_maxf(rel, 0.8f) : rel * 0.6f);
            }
            flux += fm_maxf(vband[k] - vprev[k], 0.0f) * (k < 2 ? 1.5f : 1.0f);
            vprev[k] = vband[k];
        }
        if (vin.clock_beat || (flux > vflux_avg * 1.6f + 0.08f && vin.ms - vbeat_t > 120u)) {
            vbeat = 1.0f;
            vbeat_t = vin.ms;
            vbeat_n++;                           /* (1.5.22: each beat turns the other way) */
            vhue_kick += 0.6f * (float)VPAR(VPULSE) * 0.01f;
        } else {
            vbeat *= 0.75f;
        }
        vflux_avg += (flux - vflux_avg) * 0.08f;
    }
    for (k = 0; k < VS_NVOX; k++)
        vlv[k] = fm_minf(en_vox_level[k] * vgain * 2.0f, 1.5f);
    if (fb > 0.0f && keep) {                     /* FEEDBACK: the last frame back in, zoomed in and turned (the bass
                                                  * pushes it), fading by TRAILS */
        const float pu = (float)VPAR(VPULSE) * 0.01f, fl = (float)VPAR(VFLOW) * 0.01f,
                    z = (1.0f - 0.07f * fb * G(1, -0.6f, 1.4f) - 0.03f * fb * vband[0] - 0.08f * pu * vbeat) *
                        (1.0f - (float)(VPAR(VXTGT) != 1 ? VPAR(VXZOOM) : 0) * 0.0006f),   /* TRANSFORM's ZOOM (TARGET
                                                  * Feedback or Both): up to 6 % a frame, in or out */
                    an = fb * (0.05f * fm_sinf(vt * 0.37f) + 0.02f * vband[1] + G(2, -0.05f, 0.05f)) +
                         0.06f * pu * vbeat * (vbeat_n & 1u ? 1.0f : -1.0f) +   /* (1.5.22: the beat's jolt) */
                         (float)(VPAR(VXTGT) != 1 ? VPAR(VXROT) : 0) * 0.0008f,   /* ROTATE: up to 0.08 rad a frame */
                    ca = fm_cosf(an * vdtk) * (1.0f - (1.0f - z) * vdtk), sa = fm_sinf(an * vdtk) * (1.0f - (1.0f - z) * vdtk), ff = G(3, 0.04f, 0.22f), fa = 3.0f * fl * (0.5f + vband[2]);
        /* SHIFT, along Osc's ANGLE: 2 cells a frame at 100 (TARGET Feedback or Both) */
        const float sh = VPAR(VXTGT) != 1 ? (float)VPAR(VXSHX) * 0.02f : 0.0f,
                    shang = (float)VPAR(VOANGLE) * (FM_PI / 100.0f), vshx = sh * fm_cosf(shang), vshy = sh * fm_sinf(shang);
        static int8_t flx[VF_H] VS_POOL, fly[VF_W] VS_POOL;   /* FLOW: the swirl the feedback is carried by
                                                                    * (sixteenths of a cell) */
        if (fa != 0.0f) {
            for (y = 0; y < VF_H; y++)
                flx[y] = (int8_t)fm_clampf(16.0f * fa * fm_sinf((float)y * ff + vt * 1.3f), -127.0f, 127.0f);
            for (x = 0; x < VF_W; x++)
                fly[x] = (int8_t)fm_clampf(16.0f * fa * fm_cosf((float)x * ff - vt * 1.1f), -127.0f, 127.0f);
        } else {                                /* FLOW off: every displacement is exactly zero */
            memset(flx, 0, sizeof flx);
            memset(fly, 0, sizeof fly);
        }
        for (y = 0; y < VF_H; y++)
            for (x = 0; x < VF_W; x++) {
                const float dx = (float)x - VCX, dy = (float)y - VCY;
                const float fx = VCX + dx * ca - dy * sa + (float)flx[y] * 0.0625f - vshx,
                            fy = VCY + dx * sa + dy * ca + (float)fly[x] * 0.0625f - vshy;   /* (between
                                                                                                    * four cells) */
                const int sx = (int)fm_floorf(fx), sy = (int)fm_floorf(fy), wx = (int)((fx - (float)sx) * 256.0f),
                          wy = (int)((fy - (float)sy) * 256.0f);
                int v = 0;
                if (sx >= 0 && sy >= 0 && sx + 1 < VF_W && sy + 1 < VF_H) {
                    const int t0 = vf[sy][sx] * (256 - wx) + vf[sy][sx + 1] * wx,
                              t1 = vf[sy + 1][sx] * (256 - wx) + vf[sy + 1][sx + 1] * wx;
                    v = (t0 * (256 - wy) + t1 * wy) >> 16;
                }
                vrng = vrng * 1664525u + 1013904223u;
                vtmp[y][x] = (uint8_t)((v * keep + (int)(vrng >> 24)) >> 8);
            }
        memcpy(vf, vtmp, sizeof vf);
    } else {
        for (y = 0; y < VF_H; y++)               /* TRAILS: what was drawn fades (rounded at random: no steps) */
            for (x = 0; x < VF_W; x++) {
                vrng = vrng * 1664525u + 1013904223u;
                vf[y][x] = (uint8_t)((vf[y][x] * keep + (int)(vrng >> 24)) >> 8);
            }
    }
    {                                            /* the notes begun since the last frame (the input's) */
        int i;
        for (i = 0; i < vin.nnote && i < VS_NNOTE; i++) {
            vnote_p = 1.0f;
            vis_note(mode, vin.note[i].voice, vin.note[i].note, vin.note[i].vel);
            if (VPAR(VLAYER) - 1 != mode && VPAR(VLAYER))
                vis_note(VPAR(VLAYER) - 1, vin.note[i].voice, vin.note[i].note, vin.note[i].vel);
        }
    }
    mode = vcur;
    if (vnext >= 0) {                            /* turning into the next: each at its share of the light */
        vmw += 0.05f / 3.0f;                     /* (3 s) */
        if (vmw >= 1.0f) {
            vcur = vnext;
            mode = vcur;
            vnext = -1;
            vmw = 0.0f;
        }
    }
    vamp = vnext >= 0 ? (int)(256.0f * (1.0f - vmw)) : 256;
    if (vcam_on())
        vis_cam(tr);
    else
        vis_draw(mode, tr);
    if (vnext >= 0 && !vcam_on()) {
        memset(vtmp, 0, sizeof vtmp);
        vdst = vtmp;
        vamp = (int)(256.0f * vmw);
        vis_draw(vnext, tr);
        vdst = vf;
        for (y = 0; y < VF_H; y++)
            for (x = 0; x < VF_W; x++) {
                const int o = vf[y][x] + vtmp[y][x];
                vf[y][x] = (uint8_t)(o > 255 ? 255 : o);
            }
    }
    vamp = 256;
    {                                            /* LAYER: a second picture, drawn alone, blended in by BLEND, MIX */
        const int ly = VPAR(VLAYER) - 1, bl = VPAR(VBLEND);
        const int m = (int)(VPAR(VMIX) * 256 / 100);
        if (ly >= 0 && ly != mode) {
            memset(vtmp, 0, sizeof vtmp);
            vdst = vtmp;
            vis_draw(ly, tr);
            vdst = vf;
            for (y = 0; y < VF_H; y++)
                for (x = 0; x < VF_W; x++) {
                    const int a = vf[y][x], b = (vtmp[y][x] * m) >> 8;
                    int o;
                    switch (bl) {
                    case 1: o = 255 - (((255 - a) * (255 - b)) >> 8); break;   /* Screen */
                    case 2: o = a > b ? a : b; break;                           /* Max */
                    case 3: o = a > b ? a - b : b - a; break;                   /* Difference */
                    case 4: o = (a * (b + ((256 - m) * 255 >> 8))) >> 8; break;  /* Mask: the layer lets it through */
                    default: o = a + b; break;                                  /* Add */
                    }
                    vf[y][x] = (uint8_t)(o > 255 ? 255 : o);
                }
        }
    }
    if (vbeat > 0.05f && VPAR(VPULSE)) {   /* PULSE: the field flashed on the beat */
        const int fl = (int)(60.0f * vbeat * (float)VPAR(VPULSE) * 0.01f);
        for (y = 0; y < VF_H; y++)
            for (x = 0; x < VF_W; x++) {
                const int v = vf[y][x] + ((vf[y][x] * fl) >> 7);
                vf[y][x] = (uint8_t)(v > 255 ? 255 : v);
            }
    }
    vis_mirror(VPAR(VMIRROR));
    vis_geometry();
    {                                            /* the geometry into the field too, when it is to be broken: the
                                                  * blocks smear it, SORT runs it, CORRUPT misplaces it */
        const int br = VPAR(VMOSH) + VPAR(VSORT) + VPAR(VCORRUPT),
                  tf = VPAR(VFEED) && VPAR(VXTGT) && (VPAR(VXZOOM) || VPAR(VXROT) || VPAR(VXSHX));   /* (and with
                                                  * FEEDBACK and a TRANSFORM: its echoes carried by it, 1.5.14) */
        if (br > 0 || tf) {
            const int v = 60 + (br > 150 ? 150 : br) + (tf ? 60 : 0);
            int i;
            vdst = vf;
            vamp = 256;
            for (i = 0; i < vnseg; i++)
                vf_line((float)vseg[i].x0 * 0.5f, (float)vseg[i].y0 * 0.5f, (float)vseg[i].x1 * 0.5f,
                        (float)vseg[i].y1 * 0.5f, v);
        }
    }
    vis_mosh();
    vis_slice();
    if (syn > 0.0f && vband[0] > 0.45f && vband[0] > vbass_prev * 1.4f)   /* SYNC: a bass hit tears bands of rows */
        for (k = 0; k < 3; k++) {
            const int y0 = (int)(vis_rnd() * VF_H), n = 2 + (int)(vis_rnd() * 6.0f);
            const int8_t o = (int8_t)((vis_rnd() - 0.5f) * 40.0f * syn);
            for (y = y0; y < y0 + n && y < VF_H; y++)
                vtear[y] = o;
        }
    vbass_prev = vband[0];
}
/* Osc (1.5.9): LZX's ramps and oscillators, Hydra's osc() and modulate(): a wave across the field at ANGLE, FREQ
 * cycles across it, moving by SCROLL; a second (FREQ 2) square to it, met by COMBINE: added, multiplied, their
 * difference, or bending the first (Warp; Field: the picture as it is bends it, its feedback with it), WARP how far.
 * SYNC Notes: the notes' pitch its frequency (an octave up, twice the stripes); Beat: from the top on the beat. In
 * 16.16 phase, a table a wave: integer work, row by row */
static uint8_t vosin[256] VS_POOL, vonz[33] VS_POOL;   /* the sine, the noise's 32 points (the pool: made at the
                                                 * first frame); the other waves worked out */
static uint8_t vowave_ok;
static inline int osc_w(int w, int i)            /* wave w at i (a cycle in 256): 0 .. 255 */
{
    i &= 255;
    switch (w) {
    case 1: return i < 128 ? i * 2 : (255 - i) * 2;   /* Tri */
    case 2: return i;                                 /* Saw */
    case 3: return i < 128 ? 255 : 0;                 /* Square */
    case 4: return (vonz[i >> 3] * (8 - (i & 7)) + vonz[(i >> 3) + 1] * (i & 7)) >> 3;   /* Noise: 32 bumps, smoothed */
    default: return vosin[i];                         /* Sine */
    }
}
static uint32_t vo_ph1, vo_ph2;
static uint8_t vo_beat_was;
static inline float vo_atan2(float y, float x)   /* (an arctangent within 0.01 rad: enough for a picture) */
{
    const float ax = fm_fabsf(x), ay = fm_fabsf(y), a = fm_minf(ax, ay) / (fm_maxf(ax, ay) + 1e-9f), s = a * a;
    float r = ((-0.0464964749f * s + 0.15931422f) * s - 0.327622764f) * s * a + a;
    if (ay > ax)
        r = 1.57079637f - r;
    if (x < 0.0f)
        r = 3.14159274f - r;
    return y < 0.0f ? -r : r;
}
/* Osc's radial waves (WAVE 5 .. 7): its phase at (x, y) in a cycle's 256ths: Rings, out from the middle; Polygon, the
 * same on SIDES sides (Hydra's shape(), turned by ANGLE); Spiral, the rings wound into SIDES arms */
static int vo_radial(int w, int x, int y, float f1, float ang, int n, uint32_t ph)
{
    const float dx = (float)x - VCX, dy = (float)y - VCY, r = fm_sqrtf(dx * dx + dy * dy) * (1.0f / (float)VF_W);
    float q = r;
    if (w != 5) {
        const float th = vo_atan2(dy, dx) - ang;
        if (w == 6) {                           /* (the polygon's distance: its sides' apothem across the sector) */
            const float sec = FM_TWO_PI / (float)n, t = th - sec * fm_floorf(th / sec) - sec * 0.5f;
            q = r * fm_cosf(FM_PI / (float)n) / fm_cosf(t);
        } else {                                /* (n arms: the angle a turn of n cycles) */
            q = r + th * (float)n / (FM_TWO_PI * fm_maxf(f1, 0.1f));
        }
    }
    return (int)((q * f1 + (float)ph * (1.0f / 65536.0f)) * 256.0f) & 255;
}
static void vis_osc(void)
{
    const float ang = (float)VPAR(VOANGLE) * (FM_PI / 100.0f) + (vg[2] - 0.5f) * 0.6f,   /* (LIFE's genome turns it, */
                sc = (float)VPAR(VOSPEED) * 0.01f * (0.25f + 1.75f * (float)VPAR(VSPEED) * 0.01f) * 0.5f;   /* SPEED
                                                  * paces it, as the other pictures) */
    float f1 = 0.5f * fm_exp2f((float)VPAR(VOFREQ) * 0.06f), f2 = VPAR(VOFREQ2) ? 0.5f * fm_exp2f((float)VPAR(VOFREQ2) * 0.06f)
                                                                                : 0.0f;
    const int w = VPAR(VOSHAPE) < 8 ? VPAR(VOSHAPE) : 0, comb = VPAR(VOCOMB), wa = VPAR(VOWARP) * 2,
              ns = VPAR(VSIDES) < 3 ? 3 : VPAR(VSIDES),
              keep = VPAR(VFEED) * 255 / 100;   /* (drawn clean; FEEDBACK how much of the last frame stays under it) */
    int x, y;
    if (!vowave_ok) {                            /* the five waves, a cycle in 256 */
        uint32_t r = 0x9E3779B9u;
        int i;
        for (i = 0; i < 32; i++) {
            r = r * 1664525u + 1013904223u;
            vonz[i] = (uint8_t)(r >> 24);
        }
        vonz[32] = vonz[0];
        for (i = 0; i < 256; i++)
            vosin[i] = (uint8_t)(128.0f + 127.0f * fm_sinf(FM_TWO_PI * (float)i / 256.0f));
        vowave_ok = 1;
    }
    if (VPAR(VOSYNC) == 1) {                     /* Notes: the pitch the frequency (eased, as a slew) */
        vo_note += ((float)vo_note_to - vo_note) * 0.3f;
        f1 *= fm_exp2f((vo_note - 60.0f) * (1.0f / 12.0f));
        if (f2 > 0.0f)
            f2 *= fm_exp2f((vo_note - 60.0f) * (1.0f / 12.0f));
    } else if (VPAR(VOSYNC) == 2 && vbeat > 0.95f && !vo_beat_was) {
        vo_ph1 = vo_ph2 = 0;                     /* Beat: from the top */
    }
    vo_beat_was = vbeat > 0.95f;
    f1 = fm_minf(f1 * G(1, 0.8f, 1.25f), 60.0f);   /* (and LIFE's genome breathes in its frequencies) */
    f2 = fm_minf(f2 * G(3, 0.8f, 1.25f), 60.0f);
    vo_ph1 += (uint32_t)(int32_t)(sc * 0.2f * 65536.0f);   /* SCROLL: up to a fifth of a cycle a frame */
    vo_ph2 -= (uint32_t)(int32_t)(sc * 0.14f * 65536.0f);
    {
        const float ca = fm_cosf(ang), sa = fm_sinf(ang);
        const int32_t dx1 = (int32_t)(ca * f1 * 65536.0f / (float)VF_W), dy1 = (int32_t)(sa * f1 * 65536.0f / (float)VF_W),
                      dx2 = (int32_t)(-sa * f2 * 65536.0f / (float)VF_W), dy2 = (int32_t)(ca * f2 * 65536.0f / (float)VF_W);
        for (y = 0; y < VF_H; y++) {
            int32_t p1 = (int32_t)vo_ph1 - (int32_t)(VCX * (float)dx1) + (int32_t)(((float)y - VCY) * (float)dy1),
                    p2 = (int32_t)vo_ph2 - (int32_t)(VCX * (float)dx2) + (int32_t)(((float)y - VCY) * (float)dy2);
            for (x = 0; x < VF_W; x++, p1 += dx1, p2 += dx2) {
                const int b = f2 > 0.0f ? osc_w(w, (int)((uint32_t)p2 >> 8 & 255u)) : 128;
                int i1 = w >= 5 ? vo_radial(w, x, y, f1, ang, ns, vo_ph1) : (int)((uint32_t)p1 >> 8 & 255u), v;
                if (comb == 3 && f2 > 0.0f)       /* Warp: the second bends the first */
                    i1 += ((b - 128) * wa) >> 6;
                else if (comb == 4)               /* Field: the picture as it is bends it (Hydra's modulate(o0)) */
                    i1 += (vf[y][x] * wa) >> 6;
                {
                    const int a = osc_w(w, i1);
                    v = f2 <= 0.0f || comb >= 3 ? a : comb == 0 ? (a + b) >> 1 : comb == 1 ? (a * b) >> 8
                        : a > b ? a - b : b - a;
                }
                v = (v * vamp) >> 8;
                {
                    const int old = (vdst[y][x] * keep) >> 8;
                    vdst[y][x] = (uint8_t)(v > old ? v : old);
                }
            }
        }
    }
}
static void vis_draw(int mode, float tr)         /* picture mode into vdst (the field, or LAYER's) */
{
    int x, y, i, k;
    if (mode == VS_OSC) {
        vis_osc();
        return;
    }
    switch (mode) {
    case 0:                                      /* Bloom: an orb a voice, wandering, as big as it is loud; rings */
        for (k = 0; k < VS_NVOX; k++) {
            const float of = G(4, 0.4f, 2.0f), orr = G(5, 22.0f, 52.0f);
            vorb[k][0] = VCX + orr * fm_sinf(vt * (0.21f + 0.07f * (float)k) * of + (float)k * 2.1f);
            vorb[k][1] = VCY + orr * fm_sinf(vt * (0.17f + 0.05f * (float)k) * of * G(6, 0.5f, 2.0f) + (float)k * 1.3f);
            vf_disc(vorb[k][0], vorb[k][1], (3.0f + 18.0f * vlv[k]) * G(7, 0.5f, 1.6f), (int)(30.0f + 110.0f * vlv[k]));
        }
        for (k = 0; k < VS_NBAND; k++)           /* the bands: sparks around the middle, higher further out */
            if (vband[k] > 0.25f && vis_rnd() < vband[k]) {
                const float a = vis_rnd() * FM_TWO_PI, r = 6.0f + (float)k * 9.0f;
                vf_addf(VCX + r * fm_cosf(a), VCY + r * fm_sinf(a), 200);
            }
        for (i = 0; i < 16; i++)
            if (vring[i].v > 0) {
                vf_circle(vring[i].x, vring[i].y, vring[i].r, vring[i].v / 2);
                vring[i].r += vring[i].dr;
                vring[i].v = (int16_t)(vring[i].r > 90.0f ? 0 : vring[i].v - 5);
            }
        break;
    case 1: {                                    /* Ridge: the sound's last moments, stacked, the nearest in front */
        const int n = 6 + (int)(4.0f * tr), w = (int)en_wave_w;
        for (x = 0; x < VF_W; x++) {
            const int s = en_wave[(unsigned)(w - 128 + x * 128 / VF_W) % VS_WAVE_N];
            vhist[vhist_w % 10][x] = (int8_t)fm_clampf((float)s * vgain, -127.0f, 127.0f);
        }
        vhist_w++;
        for (k = n - 1; k >= 0; k--) {           /* the oldest at the back (top), drawn first */
            const int8_t *hl = vhist[(vhist_w - 1u - (uint32_t)k) % 10u];
            const float base = G(4, 14.0f, 40.0f) + (float)(n - 1 - k) * (G(5, 60.0f, 100.0f) / (float)n),
                        sc = (0.15f + 0.25f * (float)(n - k) / (float)n) * G(6, 0.5f, 1.8f);
            const int vv = 70 + 185 * (n - k) / n;
            int py = -1;
            for (x = 4; x < VF_W - 4; x++) {
                const float win = fm_sinf(FM_PI * (float)(x - 4) / (float)(VF_W - 8)), wv = win * win;
                const int yy = (int)(base - fm_fabsf((float)hl[x]) * sc * wv);
                int j;
                for (j = yy + 1 < 0 ? 0 : yy + 1; j < VF_H; j++)   /* (what is behind it, hidden) */
                    vdst[j][x] = 0;
                if (py >= 0)
                    for (j = yy < py ? yy : py; j <= (yy < py ? py : yy); j++)
                        vf_add(x, j, vv);
                else
                    vf_add(x, yy, vv);
                py = yy;
            }
        }
        break;
    }
    case 2: {                                    /* Prism: twelve spokes, the notes sounding; the bands, rings */
        const float a0 = vt * 0.15f;
        for (k = 0; k < 12; k++) {
            const float a = a0 + FM_TWO_PI * (float)k / 12.0f, len = 6.0f + G(4, 25.0f, 60.0f) * vchroma[k];
            if (vchroma[k] > 0.02f)
                vf_line(VCX + 5.0f * fm_cosf(a), VCY + 5.0f * fm_sinf(a), VCX + len * fm_cosf(a), VCY + len * fm_sinf(a),
                        (int)(40.0f + 120.0f * vchroma[k]));
            vchroma[k] *= 0.94f;
        }
        for (k = 0; k < VS_NBAND; k++)
            if (vband[k] > 0.05f)
                vf_circle(VCX, VCY, 8.0f + (float)k * G(5, 4.0f, 11.0f) + vband[k] * 8.0f, (int)(20.0f + 130.0f * vband[k]));
        break;
    }
    case 3: {                                    /* Swarm: the sparks rise; the bass pushes them out, the highs stir */
        const float push = (vband[0] + vband[1]) * 0.08f * G(4, 0.3f, 2.0f), stir = (vband[3] + vband[4]) * G(5, 0.1f, 1.5f),
                    lvl = fm_minf(en_master_level * vgain * 2.0f, 1.0f);
        if (vis_rnd() < 0.2f + 0.8f * lvl)       /* (1.5.9: and sparks from the mix as it sounds, not only the notes) */
            for (i = 0; i < 48; i++)
                if (!vpart[i].on) {
                    vpart[i].on = 1;
                    vpart[i].x = 4.0f + vis_rnd() * (VF_W - 8.0f);
                    vpart[i].y = VF_H - 2.0f;
                    vpart[i].vx = (vis_rnd() - 0.5f) * 1.0f;
                    vpart[i].vy = -(0.6f + 1.6f * lvl) * (0.7f + 0.6f * vis_rnd());
                    vpart[i].v = (uint8_t)(110 + (int)(120.0f * lvl));
                    break;
                }
        for (i = 0; i < 48; i++)
            if (vpart[i].on) {
                vpart[i].vx += (vpart[i].x < VCX ? -push : push) + fm_sinf(vpart[i].y * 0.2f + vt * 3.0f) * stir * 0.1f;
                vpart[i].vx *= 0.97f;
                vpart[i].x += vpart[i].vx;
                vpart[i].y += vpart[i].vy;
                vpart[i].vy *= 0.995f;
                if (vpart[i].y < 0.0f || vpart[i].x < 0.0f || vpart[i].x >= VF_W || vpart[i].v < 8) {
                    vpart[i].on = 0;
                    continue;
                }
                vf_disc(vpart[i].x, vpart[i].y, 2.6f, vpart[i].v);   /* (a spark, and its streak) */
                vf_line(vpart[i].x - vpart[i].vx * 6.0f, vpart[i].y - vpart[i].vy * 6.0f, vpart[i].x, vpart[i].y,
                        vpart[i].v / 2);
                vpart[i].v = (uint8_t)(vpart[i].v - 1);
            }
        for (x = 0; x < VF_W; x += 2)            /* the ground glows with the mix */
            vf_add(x, VF_H - 1, (int)(160.0f * fm_minf(en_master_level * vgain * 2.0f, 1.0f)));
        break;
    }
    case 4: {                                    /* Tunnel: squares from the middle, as loud as the mix, turned by the chord */
        const float lvl = fm_minf(en_master_level * vgain * 2.5f, 1.2f), rot = (float)en_chord_root * (FM_PI / 6.0f);
        for (i = 0; i < 20 && vsq[i].v > 0; i++)
            ;
        if (i < 20 && lvl > 0.03f && (int)(vt * 20.0f + 0.5f) % 3 == 0) {
            vsq[i].s = 2.0f;
            vsq[i].a = rot + vt * 0.1f;
            vsq[i].v = (int16_t)(20.0f + 150.0f * lvl);
        }
        for (i = 0; i < 20; i++)
            if (vsq[i].v > 0) {
                const int ns = 3 + (int)(vg[4] * 5.99f);   /* (its sides: the genome's) */
                float px[8], py[8];
                for (k = 0; k < ns; k++) {
                    const float a = vsq[i].a + (float)k * (FM_TWO_PI / (float)ns) + FM_PI / 4.0f;
                    px[k] = VCX + vsq[i].s * fm_cosf(a);
                    py[k] = VCY + vsq[i].s * fm_sinf(a);
                }
                for (k = 0; k < ns; k++)
                    vf_line(px[k], py[k], px[(k + 1) % ns], py[(k + 1) % ns], vsq[i].v / 3);
                vsq[i].s *= G(5, 1.03f, 1.10f) + 0.06f * vband[0];
                vsq[i].v = (int16_t)(vsq[i].s > 110.0f ? 0 : vsq[i].v - 3);
            }
        break;
    }
    case 5: {                                    /* Plasma: four waves crossing, their scales and speeds the bands' */
        static int8_t sx[VF_W] VS_POOL, sy[VF_H] VS_POOL, sd[VF_W + VF_H] VS_POOL, se[VF_W + VF_H] VS_POOL;
        const float k1 = (0.06f + 0.05f * vband[0]) * G(4, 0.4f, 2.2f), k2 = (0.07f + 0.05f * vband[2]) * G(5, 0.4f, 2.2f),
                    k3 = (0.04f + 0.04f * vband[4]) * G(6, 0.4f, 2.2f),
                    lvl = 0.35f + 0.65f * fm_minf(en_master_level * vgain * 2.0f, 1.0f);
        for (x = 0; x < VF_W; x++)
            sx[x] = (int8_t)(127.0f * fm_sinf((float)x * k1 + vt * 1.1f));
        for (y = 0; y < VF_H; y++)
            sy[y] = (int8_t)(127.0f * fm_sinf((float)y * k2 - vt * 0.9f));
        for (i = 0; i < VF_W + VF_H; i++) {
            sd[i] = (int8_t)(127.0f * fm_sinf((float)i * k3 + vt * 0.7f));
            se[i] = (int8_t)(127.0f * fm_sinf((float)(i - VF_H) * k3 * 1.3f - vt * 0.5f));
        }
        for (y = 0; y < VF_H; y++)
            for (x = 0; x < VF_W; x++) {
                const int s = sx[x] + sy[y] + sd[x + y] + se[x - y + VF_H];   /* (four sines, 8 bits each) */
                const int v = ((int)((float)(s + 508) * 0.251f * lvl) * vamp) >> 8;
                if (v > vdst[y][x])
                    vdst[y][x] = (uint8_t)(v > 255 ? 255 : v);
            }
        break;
    }
    case 7: {                                    /* XOR: the cells' coordinates XORed, scaled and scrolled by the
                                                  * genome; two of its bit planes lit, one by the lows, one by the
                                                  * highs (LZX's logic, munching squares) */
        const int sx = 1 + (int)(vg[4] * 3.99f), sy = 1 + (int)(vg[5] * 3.99f), t = (int)(vt * 20.0f * G(6, 0.3f, 2.0f)),
                  p = 2 + (int)(vg[7] * 3.99f),
                  la = (int)(60.0f + 190.0f * fm_minf((vband[0] + vband[1] + vband[2]) * 0.5f, 1.0f)),
                  lb = (int)(120.0f * fm_minf((vband[3] + vband[4] + vband[5]) * 0.5f, 1.0f));
        for (y = 0; y < VF_H; y++)
            for (x = 0; x < VF_W; x++) {
                const int xv = ((x * sx + t) ^ (y * sy - t / 2)) & 255;
                int v = (xv >> p & 1 ? la : 0) + (xv >> (p + 2) & 1 ? lb : 0);
                v = (v * vamp) >> 8;
                vdst[y][x] = (uint8_t)(v > 255 ? 255 : v);   /* (drawn afresh: its pattern, not its trail) */
            }
        break;
    }
    default: {                                   /* Spiral: three arms out from the middle, each ring a band, the notes
                                                  * brightening their pitch class's turn */
        const float tw = G(4, 0.04f, 0.2f) + 0.05f * vband[1];
        const int arms = 2 + (int)(vg[5] * 4.99f);
        for (k = 0; k < arms; k++)
            for (i = 2; i < 84; i++) {
                const float r = (float)i * 0.75f, a = vt * 0.4f + r * tw + FM_TWO_PI * (float)k / (float)arms,
                            bnd = vband[(i / 14) % VS_NBAND], ch = vchroma[(i / 7) % 12];
                const int v = (int)(40.0f + 150.0f * bnd + 80.0f * ch);
                vf_addf(VCX + r * fm_cosf(a), VCY + r * fm_sinf(a), v);
            }
        for (k = 0; k < 12; k++)
            vchroma[k] *= 0.95f;
        break;
    }
    }
}
/* screen rows y0 .. y0 + h - 1 of the field, smooth (each screen pixel between four cells), through the palette:
 * RGB its planes apart (wider with the highs), SYNC its rows bent and torn; then the geometry's lines over it */
static uint8_t vpixel_x[VF_W];                 /* mosaic cell indices, shared by every row */
static int16_t vrow_o[240];                  /* the band's rows' offsets (SYNC, SLICE): the sharp lines move with
                                                 * them */
static void vis_seg(int x0, int yy0, int x1, int yy1, uint16_t c, int y0, int h)   /* a sharp line, in the band */
{
    const int dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = yy1 > yy0 ? yy1 - yy0 : yy0 - yy1, sx = x0 < x1 ? 1 : -1,
              sy = yy0 < yy1 ? 1 : -1;
    int err = dx - dy, n = dx + dy + 1;
    if ((yy0 < y0 && yy1 < y0) || (yy0 >= y0 + h && yy1 >= y0 + h))
        return;
    while (n-- > 0) {
        if (yy0 >= y0 && yy0 < y0 + h) {
            int xs = x0 + vrow_o[yy0 - y0];
            xs = xs < 0 ? xs + 240 : xs >= 240 ? xs - 240 : xs;
            if (xs < 239) {
                uint16_t *p = vs_out + (uint32_t)(yy0 - y0) * 240u + (uint32_t)xs;
                p[0] = p[1] = c;
            }
        }
        if (x0 == x1 && yy0 == yy1)
            break;
        {
            const int e2 = 2 * err;
            if (e2 > -dy) {
                err -= dy;
                x0 += sx;
            }
            if (e2 < dx) {
                err += dx;
                yy0 += sy;
            }
        }
    }
}
/* (1.5.44) a row of the picture, from the field to the screen's pixels, run from RAM (VS_RAM): the visual's
 * heaviest loop out of the way of the flash cache's misses, which the audio's interrupts keep causing. The same
 * pixels as before: between the field's two rows (wb quarters), a quarter towards each side's neighbour, sharpened
 * along the row, dithered (dr, unless POSTER), mapped (lut), then to the screen through the palette (pal), shifted
 * by o, the RGB planes spr apart. No calls: RAM code may call only RAM code */
static VS_RAM void vis_row(const uint8_t *a, const uint8_t *b, int wb, uint8_t *col, uint8_t *ln,
                                  const int8_t *bayer, int dith, const uint8_t *lut, const uint16_t *pal, uint16_t *out,
                                  int o, int spr, int vfw)
{
    const int d0 = bayer[0], d1 = bayer[1], d2 = bayer[2], d3 = bayer[3];
    int x, pl;
    for (x = 0; x < vfw; x++)
        col[x] = (uint8_t)((a[x] * (4 - wb) + b[x] * wb) >> 2);
    for (x = 0; x < vfw; x++) {
        const int l = col[x > 0 ? x - 1 : 0], r = col[x + 1 < vfw ? x + 1 : vfw - 1], c = col[x];
        ln[2 * x] = (uint8_t)((3 * c + l) >> 2);
        ln[2 * x + 1] = (uint8_t)((3 * c + r) >> 2);
    }
    pl = ln[0];
    for (x = 0; x < 240; x++) {
        const int c = ln[x];
        int v = c, d;
        if (x > 0 && x < 239) {
            const int sh = 2 * c - pl - ln[x + 1];
            v = c + (sh >= 0 ? (int)(((uint32_t)sh * 43691u) >> 17) : -(int)(((uint32_t)-sh * 43691u) >> 17));
            v = v < 0 ? 0 : v > 255 ? 255 : v;
        }
        pl = c;
        if (dith && v > 8) {
            const int k = x & 3;
            d = v + (k == 0 ? d0 : k == 1 ? d1 : k == 2 ? d2 : d3);
        } else {
            d = v;
        }
        ln[x] = lut[d < 0 ? 0 : d > 255 ? 255 : d];
    }
    if (!spr && o > -240 && o < 240) {           /* no RGB separation: the row's shift as two runs */
        const int oo = o < 0 ? o + 240 : o;
        for (x = 0; x < oo; x++)
            out[x] = pal[ln[x + 240 - oo]];
        for (; x < 240; x++)
            out[x] = pal[ln[x - oo]];
    } else if (!spr) {
        for (x = 0; x < 240; x++) {
            int xg = x - o;
            xg = xg < 0 ? xg + 240 : xg >= 240 ? xg - 240 : xg;
            out[x] = pal[ln[xg]];
        }
    } else {
        for (x = 0; x < 240; x++) {
            int xg = x - o, xr, xb;
            xg = xg < 0 ? xg + 240 : xg >= 240 ? xg - 240 : xg;
            xr = xg - spr;
            xb = xg + spr;
            xr = xr < 0 ? xr + 240 : xr >= 240 ? xr - 240 : xr;
            xb = xb < 0 ? xb + 240 : xb >= 240 ? xb - 240 : xb;
            xr = xr < 0 ? xr + 240 : xr;
            xb = xb >= 240 ? xb - 240 : xb;
            out[x] = (pal[ln[xr]] & 0x00F8u) | (pal[ln[xg]] & 0xE007u) | (pal[ln[xb]] & 0x1F00u);
        }
    }
}
static void vis_lut(void)                        /* CHROMA's map, once a frame: KEY, COLORAMA, POSTER; Rutt dims; BRIGHT,
                                                  * INVERT, STROBE (the field's light to its palette index; the objects'
                                                  * too with TARGETS Chroma All) */
{
    const float fo = (float)VPAR(VFOLD) * 0.01f, po = (float)VPAR(VPOSTER) * 0.01f;
    const int kth = VPAR(VKEY) * 2, mul = 1 + (int)(fo * 7.0f), off = (int)(vt * 40.0f * fo),
              lv = 2 + (int)((1.0f - po) * 14.0f), step = 255 / (lv - 1),
              ps = 1 + (int)((float)VPAR(VPIXEL) * 0.09f * (1.0f + vband[0]));
    int i;
    if (ps > 1)
        for (i = 0; i < VF_W; i++)
            vpixel_x[i] = (uint8_t)(i / ps * ps);
    for (i = 0; i < 256; i++) {
        int v = i;
        if (fo > 0.0f) {                         /* (folded back on itself: a triangle) */
            const int m = (v * mul + off) & 511;
            v = m < 256 ? m : 511 - m;
        }
        if (po > 0.0f)
            v = (v + step / 2) / step * step;
        if (i < kth)
            v = 0;
        if (vrutt)
            v = v * 3 / 10;                      /* (Rutt: the picture under its lines) */
        v = v * (VPAR(VBRIGHT) + 50) / 100;      /* BRIGHT */
        v = v > 255 ? 255 : v;
        if (vinv)                                /* INVERT (CORTEX VIDEO): the light turned over, as far as it says */
            v += ((255 - 2 * v) * vinv) >> 8;
        if (vflash)                              /* STROBE: towards white on the beat (or a key held) */
            v += ((255 - v) * vflash) >> 8;
        vlut[i] = (uint8_t)(v > 255 ? 255 : v < 0 ? 0 : v);
        vpal_screen[i] = swap16(vpal[i]);
    }
}
static void vis_post(int y0, int h, uint16_t *out)   /* TARGETS Video All: each row shifted (SYNC, SLICE) and its colour
                                                  * planes split (RGB) after everything is in it */
{
    static uint16_t row[240];
    int y, x;
    for (y = y0; y < y0 + h; y++) {
        uint16_t *p = out + (uint32_t)(y - y0) * 240u;
        const int o = vpost_o[y], s = vpost_s[y];
        if (!o && !s)
            continue;
        memcpy(row, p, sizeof row);
        for (x = 0; x < 240; x++) {
            int xg = (x - o) % 240, xr, xb;
            xg += xg < 0 ? 240 : 0;
            xr = (xg - s) % 240, xr += xr < 0 ? 240 : 0;
            xb = (xg + s) % 240, xb += xb < 0 ? 240 : 0;
            p[x] = s ? (uint16_t)((row[xr] & 0x00F8u) | (row[xg] & 0xE007u) | (row[xb] & 0x1F00u)) : row[xg];
        }
    }
}
/* a row of the camera's picture: the field's light (sharpened, dithered, CHROMA's map) with the camera's colour put
 * back; COLORAMA turns it to the palette's colour (all of it at 40); shifted by o, the RGB planes spr apart; PIXEL's
 * blocks when ps > 1 */
static void vis_row_cam(int ya, int yb, int wb, int ps, int fi, const int8_t *dr, int dith, uint16_t *out, int o,
                        int spr)
{
    static uint8_t ly[240];
    static int8_t lb[240], lr[240];
    static uint16_t row[240];
    const int pm = VPAR(VFOLD) >= 40 ? 256 : VPAR(VFOLD) * 256 / 40, neg = vinv > 128;
    int x, pl;
    if (ps > 1) {
        const int qy = fi / ps * ps;
        for (x = 0; x < 240; x++) {
            const int fx = vpixel_x[x >> 1];
            ly[x] = vf[qy][fx], lb[x] = vcb[qy][fx], lr[x] = vcr[qy][fx];
        }
    } else {
        int c0 = (vf[ya][0] * (4 - wb) + vf[yb][0] * wb) >> 2, cl = c0;
        for (x = 0; x < VF_W; x++) {
            const int xr = x + 1 < VF_W ? x + 1 : VF_W - 1,
                      cr = (vf[ya][xr] * (4 - wb) + vf[yb][xr] * wb) >> 2;
            ly[2 * x] = (uint8_t)((3 * c0 + cl) >> 2);
            ly[2 * x + 1] = (uint8_t)((3 * c0 + cr) >> 2);
            lb[2 * x] = lb[2 * x + 1] = (int8_t)((vcb[ya][x] * (4 - wb) + vcb[yb][x] * wb) >> 2);
            lr[2 * x] = lr[2 * x + 1] = (int8_t)((vcr[ya][x] * (4 - wb) + vcr[yb][x] * wb) >> 2);
            cl = c0, c0 = cr;
        }
    }
    pl = ly[0];
    for (x = 0; x < 240; x++) {
        const int c = ly[x];
        int v = c, d, Y, cb = lb[x] * 2, cr = lr[x] * 2, r, g, b;
        if (ps <= 1 && x > 0 && x < 239) {       /* (sharpened along the row, as the synthesis is) */
            const int sh = 2 * c - pl - ly[x + 1];
            v = c + sh / 3;
            v = v < 0 ? 0 : v > 255 ? 255 : v;
        }
        pl = c;
        d = dith && v > 8 ? v + dr[x & 3] : v;
        Y = vlut[d < 0 ? 0 : d > 255 ? 255 : d];
        if (neg)
            cb = -cb, cr = -cr;
        if (!Y)
            cb = cr = 0;
        r = Y + ((359 * cr) >> 8), g = Y - ((88 * cb + 183 * cr) >> 8), b = Y + ((454 * cb) >> 8);
        if (pm) {
            const uint32_t P = vpal[Y];
            const int pr = (int)(P >> 11) << 3, pg = (int)(P >> 5 & 63u) << 2, pb = (int)(P & 31u) << 3;
            r += ((pr - r) * pm) >> 8, g += ((pg - g) * pm) >> 8, b += ((pb - b) * pm) >> 8;
        }
        r = r < 0 ? 0 : r > 255 ? 255 : r;
        g = g < 0 ? 0 : g > 255 ? 255 : g;
        b = b < 0 ? 0 : b > 255 ? 255 : b;
        row[x] = swap16((uint32_t)((r >> 3) << 11 | (g >> 2) << 5 | (b >> 3)));
    }
    for (x = 0; x < 240; x++) {
        int xg = (x - o) % 240, xr, xb;
        xg += xg < 0 ? 240 : 0;
        if (!spr) {
            out[x] = row[xg];
            continue;
        }
        xr = (xg - spr) % 240, xr += xr < 0 ? 240 : 0;
        xb = (xg + spr) % 240, xb += xb < 0 ? 240 : 0;
        out[x] = (uint16_t)((row[xr] & 0x00F8u) | (row[xg] & 0xE007u) | (row[xb] & 0x1F00u));
    }
}
static void vis_band(int y0, int h)
{
    const float syn = (float)VPAR(VSYNC) * 0.01f, rgb = (float)VPAR(VRGB) * 0.01f;
    const int sp = (int)(2.0f * rgb * (1.0f + 3.0f * vband[4] + 2.0f * vband[5]) + 0.5f);
    const uint16_t gc = swap16(vpal[236]);
    const int ps = 1 + (int)((float)VPAR(VPIXEL) * 0.09f * (1.0f + vband[0])),   /* PIXEL: its blocks, cells */
              dith = !VPAR(VPOSTER);
    int y, x, i;
    if (y0 == 0)                                 /* (once a frame: the same map vs_frame made for the objects) */
        vis_lut();
    for (y = y0; y < y0 + h; y++) {
        const int fy2 = y - 1, ya = fy2 < 0 ? 0 : fy2 >> 1, yb = ya + 1 < VF_H ? ya + 1 : VF_H - 1,
                  wb = fy2 < 0 ? 0 : (fy2 & 1) ? 3 : 1;   /* (screen row y sits a quarter or three quarters between) */
        const uint8_t *a = vf[ya], *b = vf[yb];
        uint8_t *col = vline[0], *ln = vline[1];
        uint16_t *out = vs_out + (uint32_t)(y - y0) * 240u;
        const int fi = y >> 1 < VF_H ? y >> 1 : VF_H - 1,
                  o0 = 2 * ((int)((syn > 0.0f ? syn * 5.0f * fm_sinf((float)fi * 0.19f + vt * 2.3f) : 0.0f) + 0.5f) + vtear[fi]) +
                      ((fi & 1) ? (int)((float)VPAR(VSLICE) * 0.06f * (vband[3] + vbeat)) : 0),   /* (interlace) */
                  s0 = sp + vchan[fi >> 3], whole = VPAR(FXV) != 0,
                  o = whole ? 0 : o0, spr = whole ? 0 : s0;   /* (TARGETS Video All: after everything, vis_post) */
        vpost_o[y] = (int16_t)(whole ? o0 : 0), vpost_s[y] = (int8_t)(whole ? s0 : 0);
        vrow_o[y - y0] = (int16_t)o;
        /* the 4 x 4 Bayer offsets already halved, as BAYER[][] / 2 was (C's division: toward zero) */
        static const int8_t BAYER2[4][4] = {{-3, 1, -2, 2}, {3, -1, 4, 0}, {-1, 2, -2, 1}, {4, 0, 3, 0}};
        const int8_t *dr = BAYER2[y & 3];
        if (vcam_on()) {
            vis_row_cam(ya, yb, wb, ps, fi, dr, dith, out, o, spr);
        } else if (ps <= 1) {                    /* (1.5.44: the row in RAM code) */
            VS_FAR(vis_row)(a, b, wb, col, ln, dr, dith, vlut, vpal_screen, out, o, spr, VF_W);
        } else {                                 /* PIXEL: blocks of ps cells (Fairlight's mosaic) */
            const uint8_t *q = vf[fi / ps * ps];
            for (x = 0; x < VF_W; x++)
                ln[2 * x] = ln[2 * x + 1] = q[vpixel_x[x]];
            for (x = 0; x < VF_W * 2; x++) {     /* (dithered between the palette's steps, unless POSTER; CHROMA) */
                const int d = ln[x] + (dith && ln[x] > 8 ? dr[x & 3] : 0);
                ln[x] = vlut[d < 0 ? 0 : d > 255 ? 255 : d];
            }
            if (!spr && o > -240 && o < 240) {
                const int oo = o < 0 ? o + 240 : o;
                for (x = 0; x < oo; x++)
                    out[x] = vpal_screen[ln[x + 240 - oo]];
                for (; x < 240; x++)
                    out[x] = vpal_screen[ln[x - oo]];
            } else if (!spr) {
                for (x = 0; x < 240; x++) {
                    int xg = x - o;
                    xg = xg < 0 ? xg + 240 : xg >= 240 ? xg - 240 : xg;
                    out[x] = vpal_screen[ln[xg]];
                }
            } else for (x = 0; x < 240; x++) {
                int xg = x - o, xr, xb;
                xg = xg < 0 ? xg + 240 : xg >= 240 ? xg - 240 : xg;
                xr = xg - spr;
                xb = xg + spr;
                xr = xr < 0 ? xr + 240 : xr >= 240 ? xr - 240 : xr;
                xb = xb < 0 ? xb + 240 : xb >= 240 ? xb - 240 : xb;
                xr = xr < 0 ? xr + 240 : xr;
                xb = xb >= 240 ? xb - 240 : xb;
                out[x] = (vpal_screen[ln[xr]] & 0x00F8u) | (vpal_screen[ln[xg]] & 0xE007u) |
                         (vpal_screen[ln[xb]] & 0x1F00u);
            }
        }
        if ((y & 1) && y >> 1 < VF_H)
            vtear[y >> 1] = (int8_t)(vtear[y >> 1] * 3 / 4);
    }
    {                                            /* the geometry: sharp, at the screen's resolution, two pixels wide;
                                                  * broken as the picture is */
        const int mo = VPAR(VMOSH), co = VPAR(VCORRUPT), q = ps > 1 ? ps * 2 : 1;
        const uint16_t gcc = swap16(vpal[vlut[236]]);
        uint32_t hs = (uint32_t)(vt * 20.0f) * 2654435761u;   /* (the same throws in every band of a frame) */
        for (i = 0; i < vnseg; i++) {
            int xa = vseg[i].x0, ya = vseg[i].y0, xb = vseg[i].x1, yb = vseg[i].y1;
            hs = hs * 1664525u + 1013904223u;
            if (co && (int)(hs >> 25) < co / 3) {    /* CORRUPT: thrown somewhere else, or gone */
                if (hs >> 16 & 1u)
                    continue;
                {
                    const int dx = (int)(hs >> 8 & 63u) - 32, dy = (int)(hs >> 14 & 31u) - 16;
                    xa += dx, xb += dx, ya += dy, yb += dy;
                }
            }
            if (mo) {                            /* MOSH: each end carried by its block's vector */
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
            if (q > 1) {                         /* PIXEL: its ends on the blocks' grid */
                xa = xa / q * q, ya = ya / q * q, xb = xb / q * q, yb = yb / q * q;
            }
            vis_seg(xa, ya, xb, yb, VPAR(VFOLD) || VPAR(VPOSTER) ? gcc : gc, y0, h);
        }
    }
    if (vrutt) {                                 /* Rutt: 32 scanlines, each lifted by the picture's light under it
                                                  * (the Rutt/Etra scan processor), coloured by it */
        const float lift = 0.10f + 0.12f * vgain;
        int j;
        for (j = 0; j < 32; j++) {
            const int by = 10 + j * 7, fy = by >> 1 < VF_H ? by >> 1 : VF_H - 1;
            int px = 0, py = by - (int)((float)vf[fy][0] * lift);
            for (x = 4; x < 240; x += 4) {
                const int b = vf[fy][x >> 1], yy = by - (int)((float)b * lift);
                vis_seg(px, py, x, yy, swap16(vpal[b < 64 ? 64 + b : b]), y0, h);
                px = x, py = yy;
            }
        }
    }
}
