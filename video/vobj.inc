/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 guiatihe333 */
/* CORTEX VIDEO's objects: forms built from a few structural numbers, not picked from a list, drawn sharp at the
 * screen's resolution over the field (or into it, to be fed back). Included by vsynth.c after CORTEX's visual: it
 * reads the palette, the bands, the spectrum, the beat, the input. Every number from the settings and the input: the
 * mirror draws the same.
 *
 * PROFILE: Gielis's superformula, r(t) = (|cos(m t / 4)|^n2 + |sin(m t / 4)|^n3)^(-1 / n1): SYMMETRY m (0 a circle,
 *   3, 4, 5 .. 16 lobes or corners), POINT n1 (low: spiky, a star; high: full), CURVE n2 = n3 (low: pinched, high:
 *   square). SYMMETRY 2 is the second profile's, the latitudes' (a solid's or a ring's cross-section).
 * FORM: Flat (the profile, a disc; DEPTH gives it thickness, a prism; HOLE cuts its middle, an annulus), Solid (the
 *   spherical product of the two profiles: a sphere, a cube, a star, a shell, a flower; DEPTH squashes it), Ring (the
 *   second profile swept along the first: a torus and its kin; HOLE its thickness), Terrain (the spectrum's last
 *   moments as a floor), Wave (the sound's shape as a floor). TWIST turns the profile as it goes. DETAIL its facets.
 * DEFORM: SPECTRUM (each part of the form pushed out by its own band: round a flat or a ring, up a solid's latitudes),
 *   NOISE (a warp moving through it), MORPH (the sound's shape on its vertices), EXPLODE (its faces flung out along
 *   their normals on the beat), PULSE (bigger on the beat).
 * CLONE: COUNT copies in an ARRAY (a grid, a ring, a line, a spiral, scattered, an orbit, a cube grid) SPREAD apart,
 *   each REACT to its own part of the spectrum; and by its index: TURN, SCALE, HUE and LAG (the later copies behind in
 *   their turning) stepped copy to copy.
 * MOTION: SPIN, TILT, ZOOM; SYNC: the spin free or locked to the tempo (a turn in a beat, two, a bar, two bars).
 * BLEND: Over, Add, Feed (into the field too), Behind (the field's light over them past KEY, the edge soft: a luma
 *   key), Difference, Multiply. LIFE: the field's genome (MUTATE, CHAOS, the keys' throws) on their profile too.
 * TARGETS All (vsynth.h ST_FXM, ST_FXC): MOSH carries their corners along its blocks' vectors and feeds their light
 *   into the field to be smeared, CORRUPT throws or drops their faces, CHROMA's map colours them as it colours the
 *   field (COLORAMA, POSTER, KEY, INVERT, STROBE), PIXEL puts their corners on its grid.
 * TRIGGERS (three): a source (the kick, the snare, the hat, the beat, a note, the bar) fires an action, AMOUNT how much:
 *   Explode, Spin (a kick of turn), Colour (a jump of hue), Shuffle (the profile's numbers thrown, coming back),
 *   Throw (the field's genome), Flash, Invert, Zoom (a push of the camera).
 *
 * MODULATE (Hydra's modulate, the objects as the modulator): their shapes as a mask at the field's resolution; LENS
 *   carries the field along the mask's slope (out from the objects, or into them: a lens), SWIRL across it (the field
 *   turned round them), GLOW adds the mask to the field (a halo, fed back); VECTORS gives MOSH's blocks under each copy
 *   that copy's motion on the screen, so the smear follows what moves (a real datamosh's vectors, not thrown ones).
 *
 * vobj_frame (in vs_frame): the triggers, the mesh made, deformed, cloned, turned, projected, lit and sorted far to
 * near into vtri / vwire; vobj_modulate: MODULATE on the field; vobj_paint (in vs_paint): each band's rows of them
 * filled, over what is there */
#define VO_MAXV 1100                               /* a mesh's vertices, triangles at most */
#define VO_MAXF 2100
#define VO_TRI_N 1400                              /* a frame's triangles, wire edges at most */
#define VO_WIRE_N 1800
#define VO_FACES 1300                              /* the faces a frame aims at: the copies' mesh coarser past it */
enum { OF_OFF, OF_FLAT, OF_SOLID, OF_RING, OF_TERRAIN, OF_WAVE, OF_N };
static float vo_x[VO_MAXV] VS_POOL, vo_y[VO_MAXV] VS_POOL, vo_z[VO_MAXV] VS_POOL;   /* the mesh, its own space */
static uint8_t vo_band[VO_MAXV] VS_POOL;            /* each vertex's band of the spectrum (SPECTRUM) */
static uint16_t vo_f[VO_MAXF][3] VS_POOL;
static int vo_nv, vo_nf;
static struct {
    int16_t x[3], y[3];                            /* screen, 1/16 pixel */
    int16_t z;                                     /* (its depth, for the order) */
    uint16_t c;                                    /* its colour, the LCD's byte order */
} vtri[VO_TRI_N] VS_POOL;
static struct {
    int16_t x0, y0, x1, y1;
    uint16_t c;
} vwire[VO_WIRE_N] VS_POOL;
static uint16_t vtri_ord[VO_TRI_N] VS_POOL;
static int vntri, vnwire, vo_mode, vo_key, vo_pts;   /* (BLEND as painted: 0 Over, 1 Add, 3 Behind, 4 Difference,
                                                     * 5 Multiply; Behind's KEY, 0 .. 255) */
static float vo_spin, vo_grp, vo_land[12][12];     /* the turns so far; Terrain's rows (the sound's last moments) */
static uint8_t vo_land_w;
static float vo_r1[64] VS_POOL, vo_r2[34] VS_POOL;  /* the two profiles this frame, sampled */
/* the triggers' doings: EXPLODE's push, SPIN's kick, COLOUR's hue so far, SHUFFLE's throws (symmetry, point, curve,
 * symmetry 2) coming back to 0, FLASH, INVERT, ZOOM; each source's last state (a rise is a hit) */
static float vo_boom, vo_kick, vo_hue, vo_jit[4], vo_flash, vo_inv, vo_push;
static uint8_t vo_tr_was[3];
#define VM_W (VF_W / 2)                            /* MODULATE's mask: the objects at half the field's resolution */
static uint8_t vmask[VM_W][VM_W] VS_POOL;
static float vo_cprev[16][2];                      /* VECTORS: each copy's middle on the screen last frame (px) */
static uint8_t vo_chave[16];
static float vo_cx[16], vo_cy[16], vo_cr[16];      /* ... this frame's, and its reach (px) */
static uint8_t vo_cn;

static float vo_sf(float t, float m, float n1, float n23)   /* the superformula at angle t */
{
    const float c = fm_fabsf(fm_cosf(m * t * 0.25f)), s = fm_fabsf(fm_sinf(m * t * 0.25f));
    const float a = fm_exp2f(n23 * fm_log2f(c + 1e-6f)) + fm_exp2f(n23 * fm_log2f(s + 1e-6f));
    return fm_exp2f(-fm_log2f(a + 1e-9f) / n1);
}
static void vo_profile(float *r, int n, float t0, float t1, float m, float n1, float n23, int wrap)
{                                                  /* n samples of the profile from t0 to t1, its largest 1 */
    float mx = 1e-6f;
    int i;
    for (i = 0; i < n; i++) {
        const float t = t0 + (t1 - t0) * (float)i / (float)(wrap ? n : n - 1);
        r[i] = vo_sf(t, m, n1, n23);
        mx = fm_maxf(mx, r[i]);
    }
    for (i = 0; i < n; i++)
        r[i] = fm_minf(r[i] / mx, 1.0f);
}

/* the mesh, form f at detail d: vertices in [-1, 1], each with its band of the spectrum */
static void vo_mesh(int f, int d, float m1, float m2, float n1, float n23, float hole, float twist, float depth)
{
    int i, j;
    vo_nv = vo_nf = 0;
#define V(a, b, c, bd) (vo_x[vo_nv] = (a), vo_y[vo_nv] = (b), vo_z[vo_nv] = (c), vo_band[vo_nv] = (uint8_t)(bd), vo_nv++)
#define F(a, b, c) (vo_f[vo_nf][0] = (uint16_t)(a), vo_f[vo_nf][1] = (uint16_t)(b), vo_f[vo_nf][2] = (uint16_t)(c), vo_nf++)
    switch (f) {
    case OF_FLAT: {                                /* the profile: a disc, an annulus (HOLE), a prism (DEPTH) */
        const int nu = 4 * d > 64 ? 64 : 4 * d, layers = depth > 0.02f ? 2 : 1, ann = hole > 0.02f;
        const float h = depth * 0.45f;
        int L;
        vo_profile(vo_r1, nu, -FM_PI, FM_PI, m1, n1, n23, 1);
        for (L = 0; L < layers; L++) {
            const float z = layers == 2 ? (L ? h : -h) : 0.0f, tw = L ? twist * FM_PI : 0.0f;
            const int base = vo_nv;
            if (!ann)
                V(0.0f, 0.0f, z, 0);
            for (i = 0; i < nu; i++) {
                const float t = -FM_PI + FM_TWO_PI * (float)i / (float)nu + tw, r = vo_r1[i];
                const int bd = i * VS_NSPEC / nu;
                V(r * fm_cosf(t), r * fm_sinf(t), z, bd);
                if (ann)
                    V(hole * r * fm_cosf(t), hole * r * fm_sinf(t), z, bd);
            }
            for (i = 0; i < nu; i++) {             /* (its face: a fan, or the annulus's quads) */
                const int n = (i + 1) % nu;
                if (ann) {
                    F(base + 2 * i, base + 2 * n, base + 2 * i + 1);
                    F(base + 2 * n, base + 2 * n + 1, base + 2 * i + 1);
                } else {
                    F(base, base + 1 + i, base + 1 + n);
                }
            }
        }
        if (layers == 2) {                         /* (the prism's sides, and the hole's) */
            const int per = ann ? 2 * nu : nu + 1, o0 = ann ? 0 : 1;
            for (i = 0; i < nu; i++) {
                const int n = (i + 1) % nu, a = o0 + (ann ? 2 * i : i), b = o0 + (ann ? 2 * n : n);
                F(a, b, a + per);
                F(b, b + per, a + per);
                if (ann) {
                    F(a + 1, a + 1 + per, b + 1);
                    F(b + 1, a + 1 + per, b + 1 + per);
                }
            }
        }
        break;
    }
    case OF_SOLID: case OF_RING: {                 /* the two profiles' product: round u (the first), along v (the
                                                    * second): a solid's latitudes, a ring's cross-section */
        const int nu = 3 * d > 48 ? 48 : 3 * d, nv = (3 * d) / 2 > 24 ? 24 : (3 * d) / 2, ring = f == OF_RING,
                  rows = ring ? nv : nv + 1;
        const float tk = 0.12f + 0.55f * hole;     /* (a ring's thickness) */
        vo_profile(vo_r1, nu, -FM_PI, FM_PI, m1, n1, n23, 1);
        if (ring)
            vo_profile(vo_r2, nv, -FM_PI, FM_PI, m2, n1, n23, 1);
        else
            vo_profile(vo_r2, rows, -FM_PI * 0.5f, FM_PI * 0.5f, m2, n1, n23, 0);
        for (j = 0; j < rows; j++)
            for (i = 0; i < nu; i++) {
                const float u = -FM_PI + FM_TWO_PI * (float)i / (float)nu,
                            v = ring ? -FM_PI + FM_TWO_PI * (float)j / (float)nv : -FM_PI * 0.5f + FM_PI * (float)j / (float)nv;
                const float r1 = vo_r1[i], r2 = vo_r2[j];
                if (ring) {                        /* (the cross-section turned by TWIST as it goes round) */
                    const float vv = v + twist * u, cr = r1 + tk * r2 * fm_cosf(vv);
                    V(cr * fm_cosf(u) / (1.0f + tk), cr * fm_sinf(u) / (1.0f + tk), depth * tk * r2 * fm_sinf(vv) / (1.0f + tk),
                      i * VS_NSPEC / nu);
                } else {                           /* (the latitudes turned by TWIST, up the form) */
                    const float uu = u + twist * v * 2.0f, cv = fm_cosf(v);
                    V(r1 * fm_cosf(uu) * r2 * cv, r1 * fm_sinf(uu) * r2 * cv, depth * r2 * fm_sinf(v), j * (VS_NSPEC - 1) / nv);
                }
            }
        for (j = 0; j < nv; j++)
            for (i = 0; i < nu; i++) {
                const int a = j * nu + i, b = j * nu + (i + 1) % nu, c = ((j + 1) % rows) * nu + i,
                          e = ((j + 1) % rows) * nu + (i + 1) % nu;
                if (!ring && j + 1 >= rows)
                    continue;
                F(a, c, b);
                F(b, c, e);
            }
        break;
    }
    default: {                                     /* Terrain, Wave: a floor 12 x 12, its heights the sound's */
        for (j = 0; j < 12; j++)
            for (i = 0; i < 12; i++) {
                float hgt;
                if (f == OF_TERRAIN)
                    hgt = vo_land[(vo_land_w + 12u - 1u - (unsigned)j) % 12u][i];
                else
                    hgt = (float)vin.wave[(i * 10 + j) % VS_WAVE_N] * (1.0f / 127.0f) * 0.5f;
                V(-1.0f + (float)i * (2.0f / 11.0f), -hgt * (0.4f + depth), -1.0f + (float)j * (2.0f / 11.0f), 2 * i);
            }
        for (j = 0; j < 11; j++)
            for (i = 0; i < 11; i++) {
                const int a = j * 12 + i;
                F(a, a + 12, a + 1);
                F(a + 1, a + 12, a + 13);
            }
        break;
    }
    }
#undef V
#undef F
}

static uint16_t vo_shade(uint16_t c, float k)      /* an RGB565 colour times k (0 .. 1.5), to the LCD's order */
{
    int r = (int)((float)(c >> 11) * k), g = (int)((float)(c >> 5 & 63u) * k), b = (int)((float)(c & 31u) * k);
    r = r > 31 ? 31 : r < 0 ? 0 : r, g = g > 63 ? 63 : g < 0 ? 0 : g, b = b > 31 ? 31 : b < 0 ? 0 : b;
    return swap16((uint32_t)(r << 11 | g << 5 | b));
}

static uint16_t vo_fx(uint16_t c, int on)        /* INVERT, STROBE on a colour (LCD order), TARGETS Chroma All */
{
    uint32_t v;
    int r, g, b;
    if (!on || (!vinv && !vflash))
        return c;
    v = swap16(c);
    r = (int)(v >> 11) * 8, g = (int)(v >> 5 & 63u) * 4, b = (int)(v & 31u) * 8;
    if (vinv)
        r += ((255 - 2 * r) * vinv) >> 8, g += ((255 - 2 * g) * vinv) >> 8, b += ((255 - 2 * b) * vinv) >> 8;
    if (vflash)
        r += ((255 - r) * vflash) >> 8, g += ((255 - g) * vflash) >> 8, b += ((255 - b) * vflash) >> 8;
    r = r < 0 ? 0 : r > 255 ? 255 : r, g = g < 0 ? 0 : g > 255 ? 255 : g, b = b < 0 ? 0 : b > 255 ? 255 : b;
    return swap16((uint32_t)((r >> 3) << 11 | (g >> 2) << 5 | (b >> 3)));
}
static void vo_triggers(float dtk)                 /* the three triggers: a source's rise fires its action */
{
    int t;
    const int bar = (int)fm_floorf(vin.phase * 0.25f);
    static int bar_was;
    for (t = 0; t < 3; t++) {
        const int src = vpar[ST_TR1S + 3 * t], act = vpar[ST_TR1S + 3 * t + 1];
        const float amt = (float)vpar[ST_TR1S + 3 * t + 2] * 0.01f;
        int on = 0, fire;
        switch (src) {
        case 1: on = vin.kick > 200; break;
        case 2: on = vin.snare > 200; break;
        case 3: on = vin.hat > 200; break;
        case 4: on = vbeat > 0.95f; break;
        case 5: on = vin.nnote > 0; break;
        case 6: on = bar != bar_was; break;
        default: break;
        }
        fire = on && !vo_tr_was[t];
        vo_tr_was[t] = (uint8_t)on;
        if (!fire || amt <= 0.0f)
            continue;
        switch (act) {
        case 0: vo_boom = fm_maxf(vo_boom, amt); break;
        case 1: vo_kick += amt * (vis_rnd() < 0.5f ? 1.6f : -1.6f); break;
        case 2: vo_hue += amt * 80.0f * (0.3f + vis_rnd()); break;
        case 3:
            vo_jit[0] = (vis_rnd() - 0.5f) * 10.0f * amt, vo_jit[1] = (vis_rnd() - 0.5f) * 70.0f * amt;
            vo_jit[2] = (vis_rnd() - 0.5f) * 70.0f * amt, vo_jit[3] = (vis_rnd() - 0.5f) * 10.0f * amt;
            break;
        case 4: vis_mutate(0.5f * amt); break;
        case 5: vo_flash = fm_maxf(vo_flash, amt); break;
        case 6: vo_inv = fm_maxf(vo_inv, amt); break;
        default: vo_push = fm_maxf(vo_push, amt); break;
        }
    }
    bar_was = bar;
    {                                              /* (each one back: a fifth of a second or so) */
        const float k = fm_exp2f(-dtk * 0.9f), kj = fm_exp2f(-dtk * 0.35f);
        vo_boom *= k, vo_flash *= k, vo_inv *= k, vo_push *= k, vo_kick *= kj;
        for (t = 0; t < 4; t++)
            vo_jit[t] *= kj;
        vo_hue -= vo_hue > 215.0f ? 215.0f : 0.0f;
    }
}

static void vobj_frame(float dtk)
{
    const int form = VPAR(OFORM), draw = VPAR(ODRAW), count = VPAR(OCOUNT), arr = VPAR(OARRAY), sync = VPAR(OSYNC);
    const float size = 0.15f + 0.85f * (float)VPAR(OSIZE) * 0.01f, spread = 0.2f + 1.4f * (float)VPAR(OSPREAD) * 0.01f,
                react = (float)VPAR(OREACT) * 0.01f, pulse = 1.0f + 0.45f * vbeat * (float)VPAR(OPULSE) * 0.01f,
                morph = (float)VPAR(OMORPH) * 0.01f, light = (float)VPAR(OLIGHT) * 0.01f,
                spec = (float)VPAR(OSPEC) * 0.01f, noise = (float)VPAR(ONOISE) * 0.01f,
                boom = fm_maxf((float)VPAR(OEXPLODE) * 0.01f * vbeat, vo_boom),
                tilt = (float)VPAR(OTILT) * (FM_PI / 100.0f),
                dist = (3.6f - 2.4f * (float)VPAR(OZOOM) * 0.01f) * (1.0f - 0.45f * vo_push),
                life = (float)VPAR(OLIFE) * 0.01f,   /* (LIFE: the genome's 8 .. 11 on the profile, -1 .. 1 each) */
                m1 = fm_maxf((float)VPAR(OSYM) + vo_jit[0] + life * 6.0f * (vg[8] - 0.5f), 0.0f),
                m2 = fm_maxf((float)VPAR(OSYM2) + vo_jit[3] + life * 4.0f * (vg[11] - 0.5f), 0.0f),
                n1 = 0.15f * fm_exp2f(fm_clampf((float)VPAR(OPINCH) + vo_jit[1] + life * 70.0f * (vg[9] - 0.5f), 0.0f, 100.0f) * 0.07f),
                n23 = 0.2f * fm_exp2f(fm_clampf((float)VPAR(OCURVE) + vo_jit[2] + life * 70.0f * (vg[10] - 0.5f), 0.0f, 100.0f) * 0.07f),
                hole = (float)VPAR(OHOLE) * 0.01f, twist = (float)VPAR(OTWIST) * 0.01f + life * 0.6f * (vg[12] - 0.5f),
                depth = (float)VPAR(ODEPTH) * 0.01f, strot = (float)VPAR(OSTROT) * (FM_PI / 100.0f),
                stscale = (float)VPAR(OSTSCALE) * 0.005f, sthue = (float)VPAR(OSTHUE) * 0.01f,
                stlag = (float)VPAR(OSTTIME) * 0.01f;
    const int mo = VPAR(FXM) ? VPAR(VMOSH) : 0, co = VPAR(FXM) ? VPAR(VCORRUPT) : 0, chroma = VPAR(FXC) != 0,
              cmap = chroma && (VPAR(VFOLD) || VPAR(VPOSTER) || VPAR(VKEY)),
              q = chroma ? 2 * (1 + (int)((float)VPAR(VPIXEL) * 0.09f * (1.0f + vband[0]))) : 2;   /* (PIXEL's grid, px) */
    int det = VPAR(ODETAIL), inst, i, k;
    uint32_t hs = (uint32_t)(vt * 20.0f) * 2246822519u;   /* (CORRUPT's throws this frame) */
    float tx_c, tx_s;
    vntri = vnwire = 0;
    vo_cn = 0;
    vo_mode = VPAR(OBLEND) == 2 ? 0 : VPAR(OBLEND);
    vo_key = VPAR(OKEY) * 255 / 100;
    vo_pts = draw == 2;
    vo_triggers(dtk);
    {                                              /* Terrain's next row: two of the spectrum's 24 bands a column */
        float *row = vo_land[vo_land_w % 12u];
        for (i = 0; i < 12; i++)
            row[i] = (float)(vin.spec[2 * i] + vin.spec[2 * i + 1]) * (0.6f / 510.0f);
        vo_land_w++;
    }
    if (form == OF_OFF)
        return;
    if (sync) {                                    /* SYNC: the turn locked to the tempo (SPIN: how many and which way) */
        static const float PER[5] = {1.0f, 1.0f, 2.0f, 4.0f, 8.0f};
        const float ph = vin.phase / PER[sync < 5 ? sync : 4];
        vo_spin = FM_TWO_PI * (ph - fm_floorf(ph)) * (float)((VPAR(OSPIN) + (VPAR(OSPIN) < 0 ? -12 : 12)) / 25);
        vo_grp = vo_spin * 0.25f;
    } else {
        vo_spin += dtk * (float)VPAR(OSPIN) * 0.0012f * (0.3f + 1.7f * (float)VPAR(VSPEED) * 0.01f);
        vo_grp += dtk * (float)VPAR(OSPIN) * 0.0004f;
    }
    vo_spin += vo_kick * dtk * 0.25f;
    vo_spin -= vo_spin > FM_PI ? FM_TWO_PI : vo_spin < -FM_PI ? -FM_TWO_PI : 0.0f;
    vo_grp -= vo_grp > FM_PI ? FM_TWO_PI : vo_grp < -FM_PI ? -FM_TWO_PI : 0.0f;
    for (;;) {                                     /* (coarser while the copies would be too many faces) */
        vo_mesh(form, det, m1, m2, n1, n23, hole, twist, depth);
        if (det <= 3 || vo_nf * count <= VO_FACES || form >= OF_TERRAIN)
            break;
        det--;
    }
    tx_c = fm_cosf(tilt), tx_s = fm_sinf(tilt);
    for (inst = 0; inst < count; inst++) {
        static float sx[VO_MAXV] VS_POOL, sy[VO_MAXV] VS_POOL, sz[VO_MAXV] VS_POOL;   /* (camera space) */
        const float band = (float)vin.spec[count > 1 ? inst * VS_NSPEC / count : 2] * (1.0f / 210.0f),   /* (its own
                                                    * part of the spectrum, low to high across the copies) */
                    fi = (float)inst, own = fi * (0.37f + strot), spin = vo_spin - fi * stlag * 0.6f,
                    grp = vo_grp - fi * stlag * 0.2f;
        float px = 0.0f, py = 0.0f, pz = 0.0f,
              sc = size * pulse * (1.0f + 1.5f * react * band) * fm_exp2f(fi * stscale);
        const float oc = fm_cosf(own), os = fm_sinf(own), ty_c = fm_cosf(spin), ty_s = fm_sinf(spin), gy_c = fm_cosf(grp),
                    gy_s = fm_sinf(grp);
        const uint16_t base = vpal[40 + ((int)((float)VPAR(OCOLOR) * 2.15f + vo_hue + fi * sthue * 60.0f) % 215)];
        float ccx, ccy, ccz;                       /* (the copy's middle, in camera space: EXPLODE's out)  */
        if (count > 1) {                           /* ARRAY: where this copy is */
            const float q = fi / (float)(count - 1);
            sc *= count <= 4 ? 0.6f : count <= 9 ? 0.42f : 0.3f;
            switch (arr) {
            case 0: {                              /* Grid */
                int side = 1;
                while (side * side < count)
                    side++;
                px = ((float)(inst % side) - 0.5f * (float)(side - 1)) * spread * 2.8f / (float)side;
                py = ((float)(inst / side) - 0.5f * (float)(side - 1)) * spread * 2.8f / (float)side;
                break;
            }
            case 1: {                              /* Ring, facing you */
                const float a = FM_TWO_PI * fi / (float)count;
                px = spread * fm_cosf(a), py = spread * fm_sinf(a);
                break;
            }
            case 2:                                /* Line */
                px = (q * 2.0f - 1.0f) * spread * 1.2f;
                break;
            case 3: {                              /* Spiral */
                const float a = fi * 2.399963f, r = spread * fm_sqrtf((fi + 1.0f) / (float)count);
                px = r * fm_cosf(a), py = r * fm_sinf(a);
                break;
            }
            case 4: {                              /* Scattered, drifting */
                const uint32_t h = ghash((uint32_t)inst * 2654435761u + 99u);
                px = spread * ((float)(h & 1023u) / 511.5f - 1.0f) + 0.1f * fm_sinf(vt * 0.3f + own);
                py = spread * ((float)(h >> 10 & 1023u) / 511.5f - 1.0f) + 0.1f * fm_cosf(vt * 0.27f + own);
                pz = spread * 0.5f * ((float)(h >> 20 & 1023u) / 511.5f - 1.0f);
                break;
            }
            case 5: {                              /* Orbit: a ring lying down, going round */
                const float a = FM_TWO_PI * fi / (float)count;
                px = spread * fm_cosf(a), pz = spread * fm_sinf(a);
                break;
            }
            default: {                             /* Cube grid: in three dimensions */
                int side = 1;
                while (side * side * side < count)
                    side++;
                px = ((float)(inst % side) - 0.5f * (float)(side - 1)) * spread * 2.0f / (float)side;
                py = ((float)(inst / side % side) - 0.5f * (float)(side - 1)) * spread * 2.0f / (float)side;
                pz = ((float)(inst / (side * side)) - 0.5f * (float)(side - 1)) * spread * 2.0f / (float)side;
                break;
            }
            }
        }
        {
            const float x2 = px * gy_c + pz * gy_s, z2 = -px * gy_s + pz * gy_c;
            ccx = x2, ccy = py * tx_c - z2 * tx_s, ccz = py * tx_s + z2 * tx_c + dist;
            if (inst < 16 && ccz > 0.2f) {         /* (VECTORS: where it is on the screen, how far it reaches) */
                vo_cx[inst] = 120.0f + 150.0f * ccx / ccz, vo_cy[inst] = 120.0f + 150.0f * ccy / ccz;
                vo_cr[inst] = 150.0f * sc / ccz;
                vo_cn = (uint8_t)(inst + 1);
            }
        }
        for (i = 0; i < vo_nv; i++) {              /* the vertex: deformed, sized, turned, placed, the group's turn and
                                                    * TILT, then the camera */
            float x = vo_x[i], y = vo_y[i], z = vo_z[i], m = 1.0f;
            if (spec > 0.0f)                       /* SPECTRUM: out by its own band */
                m += spec * 0.9f * (float)vin.spec[vo_band[i] < VS_NSPEC ? vo_band[i] : VS_NSPEC - 1] * (1.0f / 255.0f);
            if (morph > 0.0f)                      /* MORPH: the sound's shape */
                m += morph * 0.5f * (float)vin.wave[(i * 7 + inst * 13) % VS_WAVE_N] * (1.0f / 127.0f);
            if (noise > 0.0f)                      /* NOISE: a warp moving through */
                m += noise * 0.45f * fm_sinf(x * 3.1f + vt * 1.3f) * fm_sinf(y * 2.7f - vt * 1.1f) * fm_sinf(z * 3.3f + vt * 0.9f + 1.0f);
            if (form >= OF_TERRAIN)
                y *= 1.0f + 2.0f * morph, m = 1.0f;
            x *= sc * m, y *= sc * m, z *= sc * m;
            if (count > 1) {                       /* (each copy turned its own way: TURN's steps) */
                const float x2 = x * oc - y * os, y2 = x * os + y * oc;
                x = x2, y = y2;
            }
            {                                      /* its spin (round y), then where it is */
                const float x2 = x * ty_c + z * ty_s, z2 = -x * ty_s + z * ty_c;
                x = x2 + px, z = z2 + pz, y += py;
            }
            {                                      /* the group's turn round y, then TILT round x */
                const float x2 = x * gy_c + z * gy_s, z2 = -x * gy_s + z * gy_c;
                const float y3 = y * tx_c - z2 * tx_s, z3 = y * tx_s + z2 * tx_c;
                sx[i] = x2, sy[i] = y3, sz[i] = z3 + dist;
            }
        }
        for (k = 0; k < vo_nf && vntri < VO_TRI_N; k++) {   /* the faces: lit, flung (EXPLODE), projected */
            const int a = vo_f[k][0], b = vo_f[k][1], c = vo_f[k][2];
            const int vi[3] = {a, b, c};
            float ux, uy, uz, wx, wy, wz, nx, ny, nz, nl, lit, ox = 0.0f, oy = 0.0f, oz = 0.0f;
            int16_t X[3], Y[3];
            int j, behind = 0;
            ux = sx[b] - sx[a], uy = sy[b] - sy[a], uz = sz[b] - sz[a];
            wx = sx[c] - sx[a], wy = sy[c] - sy[a], wz = sz[c] - sz[a];
            nx = uy * wz - uz * wy, ny = uz * wx - ux * wz, nz = ux * wy - uy * wx;
            nl = fm_sqrtf(nx * nx + ny * ny + nz * nz) + 1e-9f;
            if (boom > 0.01f) {                    /* (out from the copy's middle, as far as the beat says) */
                const float cx = (sx[a] + sx[b] + sx[c]) * (1.0f / 3.0f) - ccx, cy = (sy[a] + sy[b] + sy[c]) * (1.0f / 3.0f) - ccy,
                            cz = (sz[a] + sz[b] + sz[c]) * (1.0f / 3.0f) - ccz, dir = nx * cx + ny * cy + nz * cz < 0.0f ? -1.0f : 1.0f,
                            push = boom * 0.6f * sc * dir / nl;
                ox = nx * push, oy = ny * push, oz = nz * push;
            }
            for (j = 0; j < 3; j++) {
                const float zz = sz[vi[j]] + oz;
                if (zz < 0.2f) {
                    behind = 1;
                    break;
                }
                X[j] = (int16_t)fm_clampf((120.0f + 150.0f * (sx[vi[j]] + ox) / zz) * 16.0f, -8000.0f, 8000.0f);
                Y[j] = (int16_t)fm_clampf((120.0f + 150.0f * (sy[vi[j]] + oy) / zz) * 16.0f, -8000.0f, 8000.0f);
                if (mo) {                          /* MOSH (TARGETS All): the corner carried by its block's vector */
                    const int bx = X[j] >> 8, by = Y[j] >> 8;
                    if (bx >= 0 && by >= 0 && bx < VB_N && by < VB_N) {
                        const int kk = 16 * (1 + mo / 25);
                        X[j] = (int16_t)(X[j] + vmv[by][bx][0] * kk), Y[j] = (int16_t)(Y[j] + vmv[by][bx][1] * kk);
                    }
                }
                if (q > 2)                         /* PIXEL (TARGETS All): on its grid */
                    X[j] = (int16_t)(X[j] / (q * 16) * (q * 16)), Y[j] = (int16_t)(Y[j] / (q * 16) * (q * 16));
            }
            if (behind)
                continue;
            hs = hs * 1664525u + 1013904223u;
            if (co && (int)(hs >> 25) < co / 4) {  /* CORRUPT (TARGETS All): the face gone, or thrown */
                if (hs >> 16 & 1u)
                    continue;
                {
                    const int dx = ((int)(hs >> 8 & 63u) - 32) * 16, dy = ((int)(hs >> 14 & 31u) - 16) * 16;
                    for (j = 0; j < 3; j++)
                        X[j] = (int16_t)(X[j] + dx), Y[j] = (int16_t)(Y[j] + dy);
                }
            }
            lit = fm_fabsf((nx * -0.35f + ny * -0.55f + nz * -0.76f) / nl);   /* (a light up and to the left) */
            lit = (1.0f - light) + light * (0.2f + 1.1f * lit);
            if (cmap) {                            /* CHROMA (TARGETS All): its light through the field's map */
                const int idx = vlut[(int)fm_clampf(lit * 215.0f, 0.0f, 255.0f)];
                if (VPAR(VKEY) && idx == 0)
                    continue;                      /* (KEY: dark faces gone, as the dark of the field) */
                lit = -1.0f - (float)idx;          /* (a colour of the palette, not a shade: below) */
            }
            if (draw == 1 || draw == 2 || draw == 3) {   /* Wire, Points, Solid + wire: its edges (or corners) */
                const uint16_t wc = draw == 3 ? swap16(vpal[250]) : lit < 0.0f ? vo_fx(swap16(vpal[(int)(-1.0f - lit)]), chroma)
                                                                               : vo_fx(vo_shade(base, 1.2f), chroma);
                for (j = 0; j < 3 && vnwire < VO_WIRE_N; j++) {
                    vwire[vnwire].x0 = X[j], vwire[vnwire].y0 = Y[j];
                    vwire[vnwire].x1 = X[(j + 1) % 3], vwire[vnwire].y1 = Y[(j + 1) % 3];
                    vwire[vnwire].c = wc;
                    vnwire++;
                }
            }
            if (draw == 0 || draw == 3) {
                for (j = 0; j < 3; j++)
                    vtri[vntri].x[j] = X[j], vtri[vntri].y[j] = Y[j];
                vtri[vntri].z = (int16_t)fm_clampf((sz[a] + sz[b] + sz[c] + 3.0f * oz) * 1000.0f, -32000.0f, 32000.0f);
                vtri[vntri].c = lit < 0.0f ? vo_fx(swap16(vpal[(int)(-1.0f - lit)]), chroma) : vo_fx(vo_shade(base, lit), chroma);
                vntri++;
            }
        }
    }
    for (i = 0; i < vntri; i++)                    /* far to near (insertion: mostly in order already) */
        vtri_ord[i] = (uint16_t)i;
    for (i = 1; i < vntri; i++) {
        const uint16_t o = vtri_ord[i];
        const int16_t z = vtri[o].z;
        int j = i - 1;
        while (j >= 0 && vtri[vtri_ord[j]].z < z) {
            vtri_ord[j + 1] = vtri_ord[j];
            j--;
        }
        vtri_ord[j + 1] = o;
    }
    if (VPAR(OBLEND) == 2 || mo) {                 /* FEED (or MOSH All): their light into the field too, to be fed back,
                                                    * smeared */
        const int fv = VPAR(OBLEND) == 2 ? 160 : 40 + mo;
        for (i = 0; i < vntri; i++) {
            const int o = vtri_ord[i];
            const float cx = (float)(vtri[o].x[0] + vtri[o].x[1] + vtri[o].x[2]) * (1.0f / 96.0f),
                        cy = (float)(vtri[o].y[0] + vtri[o].y[1] + vtri[o].y[2]) * (1.0f / 96.0f);
            vf_disc(cx, cy, 1.6f, fv);
        }
        for (i = 0; i < vnwire; i += 3)
            vf_line((float)vwire[i].x0 * (1.0f / 32.0f), (float)vwire[i].y0 * (1.0f / 32.0f),
                    (float)vwire[i].x1 * (1.0f / 32.0f), (float)vwire[i].y1 * (1.0f / 32.0f), 120);
    }
}

static void vobj_modulate(void)                    /* MODULATE: the objects on the field (vs_frame, after vobj_frame) */
{
    const int lens = VPAR(MLENS), swirl = VPAR(MSWIRL), glow = VPAR(MGLOW), vec = VPAR(MVEC);
    int i, x, y;
    if (vec && VPAR(VMOSH)) {                      /* VECTORS: the blocks under each copy moved by its motion */
        for (i = 0; i < vo_cn; i++) {
            if (vo_chave[i]) {
                const float k = 0.5f + (float)vec * 0.03f,   /* (a cell's motion a frame: half the screen's px) */
                            dx = fm_clampf((vo_cx[i] - vo_cprev[i][0]) * 0.5f * k, -12.0f, 12.0f),
                            dy = fm_clampf((vo_cy[i] - vo_cprev[i][1]) * 0.5f * k, -12.0f, 12.0f),
                            r = fm_minf(vo_cr[i], 120.0f);
                const int b0x = (int)((vo_cx[i] - r) / 16.0f), b1x = (int)((vo_cx[i] + r) / 16.0f),
                          b0y = (int)((vo_cy[i] - r) / 16.0f), b1y = (int)((vo_cy[i] + r) / 16.0f);
                int bx, by;
                if (dx * dx + dy * dy > 0.25f)
                    for (by = b0y < 0 ? 0 : b0y; by <= b1y && by < VB_N; by++)
                        for (bx = b0x < 0 ? 0 : b0x; bx <= b1x && bx < VB_N; bx++) {
                            vmv[by][bx][0] = (int8_t)dx, vmv[by][bx][1] = (int8_t)dy;
                            vmv_life[by][bx] = (uint8_t)(3 + vec / 12);
                        }
            }
            vo_cprev[i][0] = vo_cx[i], vo_cprev[i][1] = vo_cy[i], vo_chave[i] = 1;
        }
        for (; i < 16; i++)
            vo_chave[i] = 0;
    }
    if (!lens && !swirl && !glow)
        return;
    memset(vmask, 0, sizeof vmask);                /* the mask: each face a soft disc at its middle, its size */
    for (i = 0; i < vntri; i++) {
        const int16_t *X = vtri[i].x, *Y = vtri[i].y;
        const float cx = (float)(X[0] + X[1] + X[2]) * (1.0f / 192.0f), cy = (float)(Y[0] + Y[1] + Y[2]) * (1.0f / 192.0f);
        const int mnx = X[0] < X[1] ? (X[0] < X[2] ? X[0] : X[2]) : (X[1] < X[2] ? X[1] : X[2]),
                  mxx = X[0] > X[1] ? (X[0] > X[2] ? X[0] : X[2]) : (X[1] > X[2] ? X[1] : X[2]);
        const float r = fm_clampf((float)(mxx - mnx) * (1.0f / 128.0f) + 1.0f, 1.0f, 6.0f);
        const int x0 = (int)(cx - r), x1 = (int)(cx + r) + 1, y0 = (int)(cy - r), y1 = (int)(cy + r) + 1;
        for (y = y0 < 0 ? 0 : y0; y <= y1 && y < VM_W; y++)
            for (x = x0 < 0 ? 0 : x0; x <= x1 && x < VM_W; x++) {
                const float ex = (float)x - cx, ey = (float)y - cy, d = 1.0f - (ex * ex + ey * ey) / (r * r);
                if (d > 0.0f) {
                    const int v = (int)(255.0f * d);
                    vmask[y][x] = (uint8_t)(v > vmask[y][x] ? v : vmask[y][x]);
                }
            }
    }
    for (i = 0; i < 2; i++)                        /* (softened twice: the slopes smooth) */
        for (y = 1; y < VM_W - 1; y++)
            for (x = 1; x < VM_W - 1; x++)
                vmask[y][x] = (uint8_t)((vmask[y][x] * 4 + vmask[y][x - 1] + vmask[y][x + 1] + vmask[y - 1][x] +
                                         vmask[y + 1][x]) >> 3);
    if (lens || swirl) {                           /* LENS along the slope, SWIRL across it: the field resampled */
        const float kl = (float)lens * (0.10f / 255.0f), ks = (float)swirl * (0.10f / 255.0f);
        memcpy(vtmp, vf, sizeof vf);
        for (y = 0; y < VF_H; y++)
            for (x = 0; x < VF_W; x++) {
                const int mx = x >> 1, my = y >> 1;
                const int gx = (mx + 1 < VM_W ? vmask[my][mx + 1] : 0) - (mx > 0 ? vmask[my][mx - 1] : 0),
                          gy = (my + 1 < VM_W ? vmask[my + 1][mx] : 0) - (my > 0 ? vmask[my - 1][mx] : 0);
                float sx, sy;
                int ix, iy, wx, wy, v;
                if (!gx && !gy)
                    continue;
                sx = (float)x - kl * (float)gx + ks * (float)gy;
                sy = (float)y - kl * (float)gy - ks * (float)gx;
                sx = fm_clampf(sx, 0.0f, (float)VF_W - 1.01f), sy = fm_clampf(sy, 0.0f, (float)VF_H - 1.01f);
                ix = (int)sx, iy = (int)sy, wx = (int)((sx - (float)ix) * 256.0f), wy = (int)((sy - (float)iy) * 256.0f);
                v = ((vtmp[iy][ix] * (256 - wx) + vtmp[iy][ix + 1] * wx) * (256 - wy) +
                     (vtmp[iy + 1][ix] * (256 - wx) + vtmp[iy + 1][ix + 1] * wx) * wy) >> 16;
                vf[y][x] = (uint8_t)v;
            }
    }
    if (glow)                                      /* GLOW: their halo into the field (fed back with it) */
        for (y = 0; y < VF_H; y++)
            for (x = 0; x < VF_W; x++) {
                const int v = vf[y][x] + ((vmask[y >> 1][x >> 1] * glow) >> 8);
                vf[y][x] = (uint8_t)(v > 255 ? 255 : v);
            }
}

static inline uint16_t vo_mix(uint16_t dst, uint16_t src)   /* BLEND as painted, the two (LCD order) */
{
    const uint32_t a = swap16(dst), b = swap16(src);
    int ar = (int)(a >> 11), ag = (int)(a >> 5 & 63u), ab = (int)(a & 31u), br = (int)(b >> 11), bg = (int)(b >> 5 & 63u),
        bb = (int)(b & 31u), r, g, bl;
    switch (vo_mode) {
    case 1: r = ar + br, g = ag + bg, bl = ab + bb; break;                       /* Add */
    case 3: {                                                                    /* Behind: the field's light over it */
        const int l = ar * 8 * 77 + ag * 4 * 150 + ab * 8 * 29, kk = (l >> 8) - vo_key,   /* (its light past KEY,
                                                                                         * 32 levels of soft edge) */
                  w = kk <= 0 ? 256 : kk >= 32 ? 0 : 256 - kk * 8;
        r = (br * w + ar * (256 - w)) >> 8, g = (bg * w + ag * (256 - w)) >> 8, bl = (bb * w + ab * (256 - w)) >> 8;
        break;
    }
    case 4: r = ar > br ? ar - br : br - ar, g = ag > bg ? ag - bg : bg - ag, bl = ab > bb ? ab - bb : bb - ab; break;
    case 5: r = ar * br / 31, g = ag * bg / 63, bl = ab * bb / 31; break;        /* Multiply */
    default: return src;
    }
    r = r > 31 ? 31 : r, g = g > 63 ? 63 : g, bl = bl > 31 ? 31 : bl;
    return swap16((uint32_t)(r << 11 | g << 5 | bl));
}
static void vobj_paint(int y0, int h, uint16_t *out)
{
    int i, y;
    for (i = 0; i < vntri; i++) {                  /* the faces, far to near, row by row in this band */
        const int o = vtri_ord[i];
        int ia = 0, ib = 1, ic = 2, t;
        int ytop, ybot;
        const int16_t *X = vtri[o].x, *Y = vtri[o].y;
        const uint16_t c = vtri[o].c;
        if (Y[ib] < Y[ia]) t = ia, ia = ib, ib = t;
        if (Y[ic] < Y[ia]) t = ia, ia = ic, ic = t;
        if (Y[ic] < Y[ib]) t = ib, ib = ic, ic = t;
        ytop = (Y[ia] + 7) >> 4, ybot = (Y[ic] + 7) >> 4;   /* (the rows whose middles, y 16 + 8, are within: no
                                                     * edge drawn past its end) */
        if (ybot <= y0 || ytop >= y0 + h || ybot <= ytop)
            continue;
        if (ytop < y0)
            ytop = y0;
        if (ybot > y0 + h)
            ybot = y0 + h;
        for (y = ytop; y < ybot; y++) {
            const int32_t ys = y * 16 + 8;
            int32_t xl, xr, x;
            /* the long edge a-c, and a-b or b-c on the other side (16ths of a pixel) */
            xl = X[ia] + ((int32_t)(X[ic] - X[ia]) * (ys - Y[ia]) / (Y[ic] - Y[ia] ? Y[ic] - Y[ia] : 1));
            if (ys < Y[ib])
                xr = X[ia] + ((int32_t)(X[ib] - X[ia]) * (ys - Y[ia]) / (Y[ib] - Y[ia] ? Y[ib] - Y[ia] : 1));
            else
                xr = X[ib] + ((int32_t)(X[ic] - X[ib]) * (ys - Y[ib]) / (Y[ic] - Y[ib] ? Y[ic] - Y[ib] : 1));
            if (xl > xr)
                t = xl, xl = xr, xr = t;
            xl = (xl + 8) >> 4, xr = (xr + 8) >> 4;
            xl = xl < 0 ? 0 : xl, xr = xr > 240 ? 240 : xr;
            {
                uint16_t *p = out + (uint32_t)(y - y0) * 240u;
                if (vo_mode)
                    for (x = xl; x < xr; x++)
                        p[x] = vo_mix(p[x], c);
                else
                    for (x = xl; x < xr; x++)
                        p[x] = c;
            }
        }
    }
    for (i = 0; i < vnwire; i++) {                 /* the edges (Points: their first corner, a dot) */
        int x0 = vwire[i].x0 >> 4, ya = vwire[i].y0 >> 4, x1 = vwire[i].x1 >> 4, yb = vwire[i].y1 >> 4;
        const uint16_t c = vwire[i].c;
        if (vo_pts) {
            if (ya >= y0 && ya + 1 < y0 + h && x0 >= 0 && x0 + 1 < 240) {
                uint16_t *p = out + (uint32_t)(ya - y0) * 240u + (uint32_t)x0;
                p[0] = p[1] = p[240] = p[241] = c;
            }
            continue;
        }
        if ((ya < y0 && yb < y0) || (ya >= y0 + h && yb >= y0 + h))
            continue;
        {
            const int dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = yb > ya ? yb - ya : ya - yb, sx = x0 < x1 ? 1 : -1,
                      sy = ya < yb ? 1 : -1;
            int err = dx - dy, n = dx + dy + 1;
            if (n > 1200)
                continue;                          /* (a line from behind the camera: not drawn) */
            while (n-- > 0) {
                if (ya >= y0 && ya < y0 + h && x0 >= 0 && x0 < 240) {
                    uint16_t *p = out + (uint32_t)(ya - y0) * 240u + (uint32_t)x0;
                    *p = vo_mode ? vo_mix(*p, c) : c;
                }
                if (x0 == x1 && ya == yb)
                    break;
                {
                    const int e2 = 2 * err;
                    if (e2 > -dy)
                        err -= dy, x0 += sx;
                    if (e2 < dx)
                        err += dx, ya += sy;
                }
            }
        }
    }
}
