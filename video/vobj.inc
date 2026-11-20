/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 guiatihe333 */
/* CORTEX VIDEO's objects: sharp shapes drawn at the screen's own resolution over the field (or into it, to be fed
 * back), as a patcher's GL objects are: flat ones (Disc, Ring, Polygon, Star, Bars) and solid ones (Cube, Sphere,
 * Torus, Icosa, Tube, Terrain, Wave), drawn solid and lit, as wire, as points, or both; repeated (COUNT, ARRAY: a grid,
 * a ring, a line, a spiral, scattered, an orbit), each copy moved by a band of its own (REACT) and the beat (PULSE);
 * their vertices pushed by the sound (MORPH). Included by vsynth.c after CORTEX's visual: it reads the palette, the
 * bands, the beat, the input. Every number from the settings and the input: the mirror draws the same.
 *
 * vobj_frame (in vs_frame): the meshes made, turned, placed, projected, lit and sorted far to near into vtri / vwire;
 * vobj_paint (in vs_paint): each band's rows of them filled, over what is there */
#define VO_MAXV 320                                /* a mesh's vertices, triangles at most */
#define VO_MAXF 600
#define VO_TRI_N 1400                              /* a frame's triangles, wire edges, points at most */
#define VO_WIRE_N 1800
#define VO_FACES 1200                              /* the faces a frame aims at: the copies' mesh coarser past it */
enum { OS_OFF, OS_DISC, OS_RING, OS_POLY, OS_STAR, OS_BARS, OS_CUBE, OS_SPHERE, OS_TORUS, OS_ICOSA, OS_TUBE,
       OS_TERRAIN, OS_WAVE, OS_N };
static float vo_x[VO_MAXV] VS_POOL, vo_y[VO_MAXV] VS_POOL, vo_z[VO_MAXV] VS_POOL;   /* the mesh, its own space */
static uint16_t vo_f[VO_MAXF][3] VS_POOL;
static int vo_nv, vo_nf, vo_flat;                  /* (flat: a shape with no inside; every face is drawn, far
                                                    * to near, the nearer over: no culling) */
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
static int vntri, vnwire, vo_add, vo_pts;
static float vo_spin, vo_grp, vo_land[12][12];     /* the turns so far; Terrain's rows (the sound's last moments) */
static uint8_t vo_land_w;

static void vo_mesh(int shape, int res)            /* shape's mesh at resolution res (SIDES, maybe coarser) */
{
    int i, j;
    vo_nv = vo_nf = 0, vo_flat = shape <= OS_BARS;
#define V(a, b, c) (vo_x[vo_nv] = (a), vo_y[vo_nv] = (b), vo_z[vo_nv] = (c), vo_nv++)
#define F(a, b, c) (vo_f[vo_nf][0] = (uint16_t)(a), vo_f[vo_nf][1] = (uint16_t)(b), vo_f[vo_nf][2] = (uint16_t)(c), vo_nf++)
    switch (shape) {
    case OS_DISC: case OS_POLY: {                  /* a fan: Disc round (24), Polygon SIDES */
        const int n = shape == OS_DISC ? 24 : res;
        V(0.0f, 0.0f, 0.0f);
        for (i = 0; i < n; i++)
            V(fm_cosf(FM_TWO_PI * (float)i / (float)n), fm_sinf(FM_TWO_PI * (float)i / (float)n), 0.0f);
        for (i = 0; i < n; i++)
            F(0, 1 + i, 1 + (i + 1) % n);
        break;
    }
    case OS_RING: case OS_STAR: {                  /* Ring: a band 24 round; Star: SIDES points */
        const int n = shape == OS_RING ? 24 : res, m = shape == OS_RING ? n : 2 * n;
        for (i = 0; i < m; i++) {
            const float a = FM_TWO_PI * (float)i / (float)m;
            if (shape == OS_RING) {
                V(fm_cosf(a), fm_sinf(a), 0.0f);
                V(0.72f * fm_cosf(a), 0.72f * fm_sinf(a), 0.0f);
            } else {
                const float r = i & 1 ? 0.42f : 1.0f;
                V(r * fm_cosf(a - FM_PI * 0.5f), r * fm_sinf(a - FM_PI * 0.5f), 0.0f);
            }
        }
        if (shape == OS_RING)
            for (i = 0; i < n; i++) {
                const int a = 2 * i, b = 2 * ((i + 1) % n);
                F(a, b, a + 1);
                F(b, b + 1, a + 1);
            }
        else {
            V(0.0f, 0.0f, 0.0f);
            for (i = 0; i < m; i++)
                F(m, i, (i + 1) % m);
        }
        break;
    }
    case OS_BARS:                                  /* a bar, standing on its foot (vobj_frame stretches it) */
        V(-0.42f, 0.0f, 0.0f), V(0.42f, 0.0f, 0.0f), V(0.42f, -1.0f, 0.0f), V(-0.42f, -1.0f, 0.0f);
        F(0, 1, 2), F(0, 2, 3);
        break;
    case OS_CUBE: {
        static const int8_t CF[12][3] = {{0, 2, 1}, {1, 2, 3}, {4, 5, 6}, {5, 7, 6}, {0, 1, 4}, {1, 5, 4},
                                         {2, 6, 3}, {3, 6, 7}, {0, 4, 2}, {2, 4, 6}, {1, 3, 5}, {3, 7, 5}};
        for (i = 0; i < 8; i++)
            V(i & 1 ? 0.7f : -0.7f, i & 2 ? 0.7f : -0.7f, i & 4 ? 0.7f : -0.7f);
        for (i = 0; i < 12; i++)
            F(CF[i][0], CF[i][1], CF[i][2]);
        break;
    }
    case OS_ICOSA: {
        static const int8_t IF[20][3] = {{0, 11, 5}, {0, 5, 1}, {0, 1, 7}, {0, 7, 10}, {0, 10, 11}, {1, 5, 9},
                                         {5, 11, 4}, {11, 10, 2}, {10, 7, 6}, {7, 1, 8}, {3, 9, 4}, {3, 4, 2},
                                         {3, 2, 6}, {3, 6, 8}, {3, 8, 9}, {4, 9, 5}, {2, 4, 11}, {6, 2, 10},
                                         {8, 6, 7}, {9, 8, 1}};
        const float p = 1.618034f, s = 0.525731f;  /* (1 / sqrt(1 + p^2)) */
        static const int8_t IV[12][3] = {{-1, 1, 0}, {1, 1, 0}, {-1, -1, 0}, {1, -1, 0}, {0, -1, 1}, {0, 1, 1},
                                         {0, -1, -1}, {0, 1, -1}, {1, 0, -1}, {1, 0, 1}, {-1, 0, -1}, {-1, 0, 1}};
        for (i = 0; i < 12; i++) {
            const float a = (float)IV[i][0], b = (float)IV[i][1], c = (float)IV[i][2];
            /* (the golden rectangles: the 1 on one axis, the p on the next) */
            V(s * (i < 4 ? a : i < 8 ? 0.0f : a * p), s * (i < 4 ? b * p : i < 8 ? b : 0.0f),
              s * (i < 4 ? 0.0f : i < 8 ? c * p : c));
        }
        for (i = 0; i < 20; i++)
            F(IF[i][0], IF[i][1], IF[i][2]);
        break;
    }
    case OS_SPHERE: case OS_TORUS: case OS_TUBE: {   /* u round, v along: a grid wrapped */
        const int nu = 2 * res, nv = shape == OS_SPHERE ? res : shape == OS_TORUS ? res : 6,
                  vwrap = shape == OS_TORUS;
        const int rows = vwrap ? nv : nv + 1;
        for (j = 0; j < rows; j++)
            for (i = 0; i < nu; i++) {
                const float u = FM_TWO_PI * (float)i / (float)nu, cu = fm_cosf(u), su = fm_sinf(u);
                if (shape == OS_SPHERE) {
                    const float t = FM_PI * (float)j / (float)nv, st = fm_sinf(t);
                    V(st * cu, fm_cosf(t), st * su);
                } else if (shape == OS_TORUS) {
                    const float t = FM_TWO_PI * (float)j / (float)nv, r = 0.62f + 0.28f * fm_cosf(t);
                    V(r * cu, 0.28f * fm_sinf(t), r * su);
                } else {
                    V(0.55f * cu, -1.0f + 2.0f * (float)j / (float)nv, 0.55f * su);
                }
            }
        for (j = 0; j < nv; j++)
            for (i = 0; i < nu; i++) {
                const int a = j * nu + i, b = j * nu + (i + 1) % nu, c = ((j + 1) % rows) * nu + i,
                          d = ((j + 1) % rows) * nu + (i + 1) % nu;
                if (!vwrap && j + 1 >= rows)
                    continue;
                F(a, c, b);
                F(b, c, d);
            }
        if (shape == OS_TUBE)
            vo_flat = 1;                           /* (open: its inside shows) */
        break;
    }
    default: {                                     /* Terrain, Wave: a floor 12 x 12, its heights the sound's */
        for (j = 0; j < 12; j++)
            for (i = 0; i < 12; i++) {
                float hgt;
                if (shape == OS_TERRAIN)
                    hgt = vo_land[(vo_land_w + 12u - 1u - (unsigned)j) % 12u][i];
                else
                    hgt = (float)vin.wave[(i * 10 + j) % VS_WAVE_N] * (1.0f / 127.0f) * 0.5f;
                V(-1.0f + (float)i * (2.0f / 11.0f), -hgt, -1.0f + (float)j * (2.0f / 11.0f));
            }
        for (j = 0; j < 11; j++)
            for (i = 0; i < 11; i++) {
                const int a = j * 12 + i;
                F(a, a + 12, a + 1);
                F(a + 1, a + 12, a + 13);
            }
        vo_flat = 1;
        break;
    }
    }
#undef V
#undef F
}

static uint16_t vo_shade(uint16_t c, float k)      /* an RGB565 colour times k (0 .. 1.5), to the LCD's order */
{
    int r = (int)((float)(c >> 11) * k), g = (int)((float)(c >> 5 & 63u) * k), b = (int)((float)(c & 31u) * k);
    r = r > 31 ? 31 : r, g = g > 63 ? 63 : g, b = b > 31 ? 31 : b;
    return swap16((uint32_t)(r << 11 | g << 5 | b));
}

static void vobj_frame(float dtk)
{
    const int shape = VPAR(OSHAPE), draw = VPAR(ODRAW), count = VPAR(OCOUNT), arr = VPAR(OARRAY);
    const float size = 0.15f + 0.85f * (float)VPAR(OSIZE) * 0.01f, spread = 0.2f + 1.4f * (float)VPAR(OSPREAD) * 0.01f,
                react = (float)VPAR(OREACT) * 0.01f, pulse = 1.0f + 0.45f * vbeat * (float)VPAR(OPULSE) * 0.01f,
                morph = (float)VPAR(OMORPH) * 0.01f, light = (float)VPAR(OLIGHT) * 0.01f,
                tilt = (float)VPAR(OTILT) * (FM_PI / 100.0f), dist = 3.6f - 2.4f * (float)VPAR(OZOOM) * 0.01f;
    const uint16_t base = vpal[40 + VPAR(OCOLOR) * 215 / 100];
    int res = VPAR(OSIDES), inst, i, k;
    float ty_c, ty_s, tx_c, tx_s, gy_c, gy_s;
    vntri = vnwire = 0;
    vo_add = VPAR(OBLEND) == 1;
    vo_pts = draw == 2;
    {                                              /* Terrain's next row: the bands across, smoothed */
        float *row = vo_land[vo_land_w % 12u];
        for (i = 0; i < 12; i++)                   /* (two of the spectrum's 24 bands a column) */
            row[i] = (float)(vin.spec[2 * i] + vin.spec[2 * i + 1]) * (0.6f / 510.0f);
        vo_land_w++;
    }
    if (shape == OS_OFF)
        return;
    vo_spin += dtk * (float)VPAR(OSPIN) * 0.0012f * (0.3f + 1.7f * (float)VPAR(VSPEED) * 0.01f);
    vo_grp += dtk * (float)VPAR(OSPIN) * 0.0004f;
    vo_spin -= vo_spin > FM_PI ? FM_TWO_PI : vo_spin < -FM_PI ? -FM_TWO_PI : 0.0f;
    vo_grp -= vo_grp > FM_PI ? FM_TWO_PI : vo_grp < -FM_PI ? -FM_TWO_PI : 0.0f;
    for (;;) {                                     /* (coarser while the copies would be too many faces) */
        vo_mesh(shape, res);
        if (res <= 3 || vo_nf * count <= VO_FACES || shape == OS_CUBE || shape == OS_ICOSA || shape <= OS_BARS ||
            shape >= OS_TERRAIN)
            break;
        res--;
    }
    ty_c = fm_cosf(vo_spin), ty_s = fm_sinf(vo_spin);
    tx_c = fm_cosf(tilt), tx_s = fm_sinf(tilt);
    gy_c = fm_cosf(vo_grp), gy_s = fm_sinf(vo_grp);
    for (inst = 0; inst < count; inst++) {
        static float sx[VO_MAXV] VS_POOL, sy[VO_MAXV] VS_POOL, sz[VO_MAXV] VS_POOL;   /* (camera space) */
        const float band = (float)vin.spec[count > 1 ? inst * VS_NSPEC / count : 2] * (1.0f / 210.0f),   /* (its own
                                                    * part of the spectrum, low to high across the copies) */
                    own = (float)inst * 0.37f;
        float px = 0.0f, py = 0.0f, pz = 0.0f, sc = size * pulse * (1.0f + 1.5f * react * band),
              oc = fm_cosf(own), os = fm_sinf(own);
        if (count > 1) {                           /* ARRAY: where this copy is */
            const float q = (float)inst / (float)(count - 1);
            sc *= count <= 4 ? 0.6f : count <= 9 ? 0.42f : 0.3f;
            switch (arr) {
            case 0: {                              /* Grid */
                int side = 1;
                while (side * side < count)
                    side++;
                px = ((float)(inst % side) - 0.5f * (float)(side - 1)) * spread * 1.4f / (float)side * 2.0f;
                py = ((float)(inst / side) - 0.5f * (float)(side - 1)) * spread * 1.4f / (float)side * 2.0f;
                break;
            }
            case 1: {                              /* Ring, facing you */
                const float a = FM_TWO_PI * (float)inst / (float)count;
                px = spread * fm_cosf(a), py = spread * fm_sinf(a);
                break;
            }
            case 2:                                /* Line */
                px = (q * 2.0f - 1.0f) * spread * 1.2f;
                break;
            case 3: {                              /* Spiral */
                const float a = (float)inst * 2.399963f, r = spread * fm_sqrtf((float)(inst + 1) / (float)count);
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
            default: {                             /* Orbit: a ring lying down, going round */
                const float a = FM_TWO_PI * (float)inst / (float)count;
                px = spread * fm_cosf(a), pz = spread * fm_sinf(a);
                break;
            }
            }
        }
        for (i = 0; i < vo_nv; i++) {              /* the vertex: MORPH, its size, its own turn, placed, the group's
                                                    * turn and TILT, then the camera */
            float x = vo_x[i], y = vo_y[i], z = vo_z[i], m = 1.0f;
            if (morph > 0.0f && shape != OS_BARS)
                m += morph * (0.5f * (float)vin.wave[(i * 7 + inst * 13) % VS_WAVE_N] * (1.0f / 127.0f) +
                              0.8f * vband[i % VS_NBAND]);
            if (shape == OS_BARS)
                y *= 0.25f + 2.2f * band * (0.4f + react);
            if (shape >= OS_TERRAIN)
                y *= 1.0f + 2.0f * morph;
            x *= sc * m, y *= sc * (shape >= OS_TERRAIN ? 1.0f : m), z *= sc * m;
            if (count > 1 && shape > OS_BARS) {    /* (each copy turned its own way a little) */
                const float x2 = x * oc + z * os;
                z = -x * os + z * oc, x = x2;
            }
            {                                      /* its spin (round y), then where it is */
                const float x2 = x * ty_c + z * ty_s, z2 = -x * ty_s + z * ty_c;
                x = x2 + px, z = z2 + pz, y += py;
            }
            if (shape <= OS_BARS) {                /* (flat ones turn round the view, not round y) */
                const float ax = vo_x[i] * sc * m, ay = (shape == OS_BARS ? vo_y[i] * (0.25f + 2.2f * band *
                                                         (0.4f + react)) : vo_y[i]) * sc * m;
                const float c = count > 1 ? fm_cosf(vo_spin + own) : ty_c, s = count > 1 ? fm_sinf(vo_spin + own) : ty_s;
                x = (shape == OS_BARS ? ax : ax * c - ay * s) + px;
                y = (shape == OS_BARS ? ay : ax * s + ay * c) + py;
                z = pz;
            }
            {                                      /* the group's turn round y, then TILT round x */
                const float x2 = x * gy_c + z * gy_s, z2 = -x * gy_s + z * gy_c;
                const float y3 = y * tx_c - z2 * tx_s, z3 = y * tx_s + z2 * tx_c;
                sx[i] = x2, sy[i] = y3, sz[i] = z3 + dist;
            }
        }
        for (k = 0; k < vo_nf && vntri < VO_TRI_N; k++) {   /* the faces: culled, lit, projected */
            const int a = vo_f[k][0], b = vo_f[k][1], c = vo_f[k][2];
            float ux, uy, uz, wx, wy, wz, nx, ny, nz, nl, lit;
            int16_t X[3], Y[3];
            int j, behind = 0;
            const int vi[3] = {a, b, c};
            for (j = 0; j < 3; j++) {
                const float zz = sz[vi[j]];
                if (zz < 0.2f) {
                    behind = 1;
                    break;
                }
                X[j] = (int16_t)fm_clampf((120.0f + 150.0f * sx[vi[j]] / zz) * 16.0f, -8000.0f, 8000.0f);
                Y[j] = (int16_t)fm_clampf((120.0f + 150.0f * sy[vi[j]] / zz) * 16.0f, -8000.0f, 8000.0f);
            }
            if (behind)
                continue;
            ux = sx[b] - sx[a], uy = sy[b] - sy[a], uz = sz[b] - sz[a];
            wx = sx[c] - sx[a], wy = sy[c] - sy[a], wz = sz[c] - sz[a];
            nx = uy * wz - uz * wy, ny = uz * wx - ux * wz, nz = ux * wy - uy * wx;
            nl = fm_sqrtf(nx * nx + ny * ny + nz * nz) + 1e-9f;
            lit = fm_fabsf((nx * -0.35f + ny * -0.55f + nz * -0.76f) / nl);   /* (a light up and to the left) */
            lit = (1.0f - light) + light * (0.2f + 1.1f * lit);
            if (draw == 1 || draw == 2 || draw == 3) {   /* Wire, Points, Solid + wire: its edges (or corners) */
                const uint16_t wc = draw == 3 ? swap16(vpal[250]) : vo_shade(base, 1.2f);
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
                vtri[vntri].z = (int16_t)((sz[a] + sz[b] + sz[c]) * 1000.0f);
                vtri[vntri].c = vo_shade(base, lit);
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
    if (VPAR(OBLEND) == 2) {                       /* FEED: their light into the field too, to be fed back */
        for (i = 0; i < vntri; i++) {
            const int o = vtri_ord[i];
            const float cx = (float)(vtri[o].x[0] + vtri[o].x[1] + vtri[o].x[2]) * (1.0f / 96.0f),
                        cy = (float)(vtri[o].y[0] + vtri[o].y[1] + vtri[o].y[2]) * (1.0f / 96.0f);
            vf_disc(cx, cy, 1.6f, 160);
        }
        for (i = 0; i < vnwire; i += 3)
            vf_line((float)vwire[i].x0 * (1.0f / 32.0f), (float)vwire[i].y0 * (1.0f / 32.0f),
                    (float)vwire[i].x1 * (1.0f / 32.0f), (float)vwire[i].y1 * (1.0f / 32.0f), 120);
    }
}

static inline uint16_t vo_mix(uint16_t dst, uint16_t src)   /* Add: the two, each plane up to full (LCD order) */
{
    const uint32_t a = swap16(dst), b = swap16(src);
    uint32_t r = (a >> 11) + (b >> 11), g = (a >> 5 & 63u) + (b >> 5 & 63u), bl = (a & 31u) + (b & 31u);
    r = r > 31u ? 31u : r, g = g > 63u ? 63u : g, bl = bl > 31u ? 31u : bl;
    return swap16(r << 11 | g << 5 | bl);
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
        ytop = (Y[ia] + 15) >> 4, ybot = (Y[ic] + 15) >> 4;
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
                if (vo_add)
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
                    *p = vo_add ? vo_mix(*p, c) : c;
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
