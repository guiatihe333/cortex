/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal, guiatihe333 */
/* CORTEX VIDEO's video synth (vsynth.h): the settings, the palettes, the frame and its reset around CORTEX's visual
 * (vsynth_core.inc). The FM-1 includes it in its unity build with VS_POOL / VS_RAM / VS_FAR set to the platform's;
 * the mirror and the tests compile it as it is. */
#ifndef VS_NO_STDINC
#include <string.h>
#endif
#include "fastmath.h"
#include "vsynth.h"

#ifndef VS_POOL
#define VS_POOL
#endif
#ifndef VS_RAM
#define VS_RAM
#endif
#ifndef VS_FAR
#define VS_FAR(fn) (fn)
#endif

/* ------------------------------------------------------------ settings --- */
static const vs_par_t VPARS[ST_N] = {
    {"Visual", 0, VS_NMODE - 1, 5}, {"Colour", 0, VS_NPAL - 1, 0}, {"Trails", 0, 100, 60}, {"React", 0, 100, 50},
    {"Feedback", 0, 100, 25},  {"RGB", 0, 100, 20},       {"Hue", 0, 100, 10},     {"Sync", 0, 100, 10},
    {"Geometry", 0, 16, 0},    {"Sides", 3, 12, 5},       {"Mirror", 0, 4, 0},     {"Speed", 0, 100, 50},
    {"Layer", 0, 9, 0},        {"Blend", 0, 4, 0},        {"Mix", 0, 100, 60},     {"Pulse", 0, 100, 50},
    {"Morph", 0, 100, 0},      {"Mutate", 0, 100, 30},    {"Flow", 0, 100, 20},    {"Chaos", 0, 100, 30},
    {"Mosh", 0, 100, 0},       {"Sort", 0, 100, 0},       {"Corrupt", 0, 100, 0},  {"Slice", 0, 100, 0},
    {"Colorama", 0, 100, 0},   {"Poster", 0, 100, 0},     {"Pixel", 0, 100, 0},    {"Key", 0, 100, 0},
    {"Freq", 0, 100, 40},      {"Wave", 0, 7, 0},         {"Angle", 0, 100, 0},    {"Scroll", -100, 100, 20},
    {"Freq 2", 0, 100, 0},     {"Combine", 0, 4, 1},      {"Warp", 0, 100, 30},    {"Osc sync", 0, 2, 0},
    {"Zoom", -100, 100, 0},    {"Rotate", -100, 100, 0},  {"Shift", -100, 100, 0}, {"Target", 0, 2, 0},
    {"Geometry 2", 0, 16, 0},  {"Sides 2", 3, 12, 6},     {"Size 2", 10, 200, 60},
    {"Strobe", 0, 100, 0},     {"Invert", 0, 100, 0},
    {"Object", 0, 12, 0},      {"Size", 0, 100, 45},      {"Draw", 0, 3, 0},       {"Tint", 0, 100, 60},
    {"Spin", -100, 100, 25},   {"Tilt", 0, 100, 20},      {"Zoom", 0, 100, 40},    {"Morph", 0, 100, 0},
    {"Count", 1, 16, 1},       {"Array", 0, 5, 1},        {"Spread", 0, 100, 50},  {"React", 0, 100, 40},
    {"Detail", 3, 12, 8},      {"Blend", 0, 2, 0},        {"Light", 0, 100, 70},   {"Pulse", 0, 100, 40},
    {"Bright", 0, 100, 50},    {"Ear", 0, 100, 30},       {"Gain", 0, 100, 50},
    {"Bars", 0, 2, 1},         {"Delay", 1, 15, 4},
    {"Source 1", 0, SRC_N - 1, SRC_BASS},  {"Target 1", 0, VS_NTGT, ST_VFEED + 1},  {"Depth 1", -100, 100, 40},
    {"Mode 1", 0, 3, 0},  {"Lag 1", 0, 100, 0},
    {"Source 2", 0, SRC_N - 1, SRC_HIGH},  {"Target 2", 0, VS_NTGT, ST_VRGB + 1},  {"Depth 2", -100, 100, 35},
    {"Mode 2", 0, 3, 0},  {"Lag 2", 0, 100, 0},
    {"Source 3", 0, SRC_N - 1, SRC_KICK},  {"Target 3", 0, VS_NTGT, ST_VXZOOM + 1},  {"Depth 3", -100, 100, 30},
    {"Mode 3", 0, 3, 0},  {"Lag 3", 0, 100, 0},
    {"Source 4", 0, SRC_N - 1, 0},  {"Target 4", 0, VS_NTGT, 0},  {"Depth 4", -100, 100, 30},
    {"Mode 4", 0, 3, 0},  {"Lag 4", 0, 100, 0},
    {"Source 5", 0, SRC_N - 1, 0},  {"Target 5", 0, VS_NTGT, 0},  {"Depth 5", -100, 100, 30},
    {"Mode 5", 0, 3, 0},  {"Lag 5", 0, 100, 0},
    {"Source 6", 0, SRC_N - 1, 0},  {"Target 6", 0, VS_NTGT, 0},  {"Depth 6", -100, 100, 30},
    {"Mode 6", 0, 3, 0},  {"Lag 6", 0, 100, 0},
    {"Source 7", 0, SRC_N - 1, 0},  {"Target 7", 0, VS_NTGT, 0},  {"Depth 7", -100, 100, 30},
    {"Mode 7", 0, 3, 0},  {"Lag 7", 0, 100, 0},
    {"Source 8", 0, SRC_N - 1, 0},  {"Target 8", 0, VS_NTGT, 0},  {"Depth 8", -100, 100, 30},
    {"Mode 8", 0, 3, 0},  {"Lag 8", 0, 100, 0},
};
_Static_assert(sizeof VPARS / sizeof *VPARS == ST_N, "a setting each");
const vs_par_t *vs_par_info(int k) { return (unsigned)k < (unsigned)ST_N ? &VPARS[k] : &VPARS[0]; }

static void vs_itoa(int v, char *b)
{
    char t[8];
    int n = 0;
    unsigned u = (unsigned)(v < 0 ? -v : v);
    do
        t[n++] = (char)('0' + u % 10u);
    while ((u /= 10u) && n < 7);
    if (v < 0)
        *b++ = '-';
    while (n)
        *b++ = t[--n];
    *b = 0;
}
static const char *const VS_MODE_NAME[VS_NMODE] = {"Bloom", "Ridge", "Prism", "Swarm", "Tunnel", "Plasma",
                                                   "Spiral", "XOR", "Osc"};
static const char *const VS_SRC_NAME[SRC_N] = {"Off", "Level", "Bass", "Mid", "High", "Beat", "Lows", "Mids", "Highs",
                                               "Notes", "Root", "LFO", "Random", "Tempo", "Bar", "CC", "Velocity",
                                               "Kick", "Snare", "Hat", "Bright"};
const char *vs_par_text(int k, int v, char *buf)
{
    static const char *const VG[17] = {"Off", "Polygon", "Lissajous", "Grid", "Mandala", "Scope", "Rutt", "Star",
                                       "Rings", "Spirograph", "Rose", "Cube", "Tunnel", "Starfield", "Circle",
                                       "Lattice", "Spiral"};
    static const char *const VP[VS_NPAL] = {"Cortex", "Ember", "Ice", "Acid", "Mono", "Neon", "Vapor", "Phosphor",
                                            "Thermal", "Gold"};
    static const char *const VW[8] = {"Sine", "Tri", "Saw", "Square", "Noise", "Rings", "Polygon", "Spiral"};
    static const char *const VC[5] = {"Add", "Multiply", "Diff", "Warp", "Field"};
    static const char *const VT[3] = {"Feedback", "Lines", "Both"};
    static const char *const VS[3] = {"Free", "Notes", "Beat"};
    static const char *const VB[5] = {"Add", "Screen", "Max", "Diff", "Mask"};
    static const char *const VB2[3] = {"Show", "Auto", "Hide"};
    static const char *const VR[5] = {"Off", "Mirror", "Flip", "Quad", "Kaleido"};
    const vs_par_t *p = vs_par_info(k);
    v = v < p->lo ? p->lo : v > p->hi ? p->hi : v;
    switch (k) {
    case ST_VMODE: return VS_MODE_NAME[v];
    case ST_VLAYER: return v ? VS_MODE_NAME[v - 1] : "Off";
    case ST_VGEO: case ST_VGEO2: return VG[v];
    case ST_VPAL: return VP[v];
    case ST_VOSHAPE: return VW[v];
    case ST_VOCOMB: return VC[v];
    case ST_VXTGT: return VT[v];
    case ST_VOSYNC: return VS[v];
    case ST_VBLEND: return VB[v];
    case ST_VBARS: return VB2[v];
    case ST_VMIRROR: return VR[v];
    case ST_OSHAPE: {
        static const char *const OS[13] = {"Off", "Disc", "Ring", "Polygon", "Star", "Bars", "Cube", "Sphere", "Torus",
                                           "Icosa", "Tube", "Terrain", "Wave"};
        return OS[v];
    }
    case ST_ODRAW: {
        static const char *const OD[4] = {"Solid", "Wire", "Points", "Solid+wire"};
        return OD[v];
    }
    case ST_OARRAY: {
        static const char *const OA[6] = {"Grid", "Ring", "Line", "Spiral", "Scatter", "Orbit"};
        return OA[v];
    }
    case ST_OBLEND: {
        static const char *const OB[3] = {"Over", "Add", "Feed"};
        return OB[v];
    }
    default: break;
    }
    if (k >= ST_AR1S && (k - ST_AR1S) % VS_SLOT_W == 0)
        return VS_SRC_NAME[v];
    if (k >= ST_AR1S && (k - ST_AR1S) % VS_SLOT_W == 3) {
        static const char *const SM[4] = {"Direct", "Invert", "Beat hold", "Gate"};
        return SM[v];
    }
    if (k >= ST_AR1S && (k - ST_AR1S) % VS_SLOT_W == 1)
        return v == 0 ? "Off" : v >= VS_NTGT ? "All" : VPARS[ST_VMODE + v - 1].name;
    vs_itoa(v, buf);
    if (k == ST_VDELAY) {
        char *e = buf;
        while (*e)
            e++;
        e[0] = ' ', e[1] = 's', e[2] = 0;
    }
    return buf;
}

/* --------------------------------------------------------------- state --- */
static vs_in_t vin;                               /* this frame's input */
static float vin_vox[VS_NVOX];                    /* (the lows, the mids, the highs, as CORTEX's voices were) */
static int16_t vpar[ST_N];                        /* the settings as set (the knobs) */
static uint16_t *vs_out;                          /* vs_paint's rows */
static int vinv, vflash;                          /* INVERT, STROBE this frame (0 .. 256) */
static float vvel_p, vslot[VS_NSLOT], vslot_h[VS_NSLOT];   /* (each react slot's value, smoothed; held) */
#define en_master_level vin.level
#define en_bands vin.bands
#define en_wave vin.wave
#define en_wave_w 0u                              /* (vin.wave is in order, the oldest first) */
#define en_vox_level vin_vox
#define en_chord_root vin.root
#ifndef VS_HAVE_SWAP16                            /* (the FM-1 build has gfx.c's) */
static inline uint16_t swap16(uint32_t c) { return (uint16_t)(((c >> 8) & 0xFFu) | ((c & 0xFFu) << 8)); }
#endif
static void vis_palette(void);
static void vis_mutate(float amount);
static float vdtk = 1.0f;                         /* this frame's time in 50 ms steps (CORTEX's frame): motion by the clock */
static uint32_t vlast_ms;
#include "vsynth_core.inc"
#include "vobj.inc"
#include "vhd.inc"

/* COLOUR: five stops each (RGB565): CORTEX's own night, CORTEX's Ember, Ice, Acid, Mono, then CORTEX VIDEO's */
#define C565(r, g, b) ((uint16_t)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3)))
static const uint16_t VS_PAL[VS_NPAL][5] = {
    {0x0000, C565(42, 10, 94), C565(184, 50, 90), C565(255, 179, 71), C565(255, 246, 214)},     /* Cortex */
    {0x0000, 0x6000, 0xC800, 0xFC00, 0xFFE0},                                                     /* Ember */
    {0x0000, 0x0010, 0x041F, 0x07FF, 0xFFFF},                                                     /* Ice */
    {0x0000, 0x4810, 0xA014, 0x87E0, 0xEFF0},                                                     /* Acid */
    {0x0000, C565(64, 64, 64), C565(144, 144, 144), C565(208, 208, 208), 0xFFFF},                 /* Mono */
    {0x0000, C565(26, 0, 255), C565(255, 0, 200), C565(0, 255, 240), 0xFFFF},                     /* Neon */
    {0x0000, C565(84, 19, 136), C565(247, 6, 207), C565(45, 226, 230), C565(249, 248, 113)},      /* Vapor */
    {0x0000, C565(0, 51, 0), C565(0, 170, 34), C565(102, 255, 102), C565(224, 255, 224)},         /* Phosphor */
    {0x0000, C565(59, 15, 112), C565(222, 73, 104), C565(254, 159, 109), C565(252, 253, 191)},    /* Thermal */
    {0x0000, C565(43, 26, 0), C565(160, 106, 0), C565(255, 209, 102), C565(255, 251, 230)},       /* Gold */
};
static void vis_palette(void)                    /* COLOUR's stops, turned by HUE, into 256 steps */
{
    const int pi = VPAR(VPAL), hue = VPAR(VHUE) + (vhue_kick > 0.0f), key = hue ? -2 : pi;
    uint16_t st[5];
    int i;
    if (key == vpal_key && key != -2)
        return;
    vpal_key = key;
    for (i = 0; i < 5; i++)
        st[i] = VS_PAL[(unsigned)pi < VS_NPAL ? pi : 0][i];
    if (hue) {                                   /* HUE: each stop turned round the grey axis, faster as it rises */
        const float a = vt * (float)hue * 0.03f + vhue_kick, c = fm_cosf(a), sn = fm_sinf(a), k = (1.0f - c) / 3.0f,
                    q = sn * 0.57735f;           /* (Rodrigues about (1,1,1)/sqrt 3) */
        for (i = 0; i < 5; i++) {
            const float r = (float)(st[i] >> 11) / 31.0f, gg = (float)(st[i] >> 5 & 63u) / 63.0f,
                        b = (float)(st[i] & 31u) / 31.0f;
            const float r2 = r * (c + k) + gg * (k - q) + b * (k + q), g2 = r * (k + q) + gg * (c + k) + b * (k - q),
                        b2 = r * (k - q) + gg * (k + q) + b * (c + k);
            st[i] = (uint16_t)((uint16_t)(fm_clampf(r2, 0.0f, 1.0f) * 31.0f + 0.5f) << 11 |
                               (uint16_t)(fm_clampf(g2, 0.0f, 1.0f) * 63.0f + 0.5f) << 5 |
                               (uint16_t)(fm_clampf(b2, 0.0f, 1.0f) * 31.0f + 0.5f));
        }
    }
    for (i = 0; i < 256; i++) {                  /* (four spans of 64 steps each, every one its own: no bands) */
        const uint16_t a = st[i >> 6], b = st[(i >> 6) + 1 < 5 ? (i >> 6) + 1 : 4];
        const int t = i & 63;
        vpal[i] = (uint16_t)((((a >> 11) * (64 - t) + (b >> 11) * t + 32) >> 6) << 11 |
                             ((((a >> 5) & 63u) * (64 - t) + ((b >> 5) & 63u) * t + 32) >> 6) << 5 |
                             (((a & 31u) * (64 - t) + (b & 31u) * t + 32) >> 6));
    }
}

/* ------------------------------------------------------------ the API --- */
void vs_set(int k, int v)
{
    const vs_par_t *p;
    if ((unsigned)k >= (unsigned)ST_N)
        return;
    p = &VPARS[k];
    vpar[k] = (int16_t)(v < p->lo ? p->lo : v > p->hi ? p->hi : v);
}
int vs_get(int k) { return (unsigned)k < (unsigned)ST_N ? vpar[k] : 0; }
int vs_eff(int k) { return (unsigned)k < (unsigned)ST_N ? veff[k] : 0; }
float vs_beat(void) { return vbeat; }
float vs_band(int k) { return (unsigned)k < VS_NBAND ? vband[k] : 0.0f; }

void vs_reset(uint32_t seed)                     /* every moving part as at the first frame (the settings stay) */
{
    int k;
    vis_ok = 0;                                  /* (the field and the shapes cleared at the next frame) */
    memset(vf, 0, sizeof vf);
    memset(vtmp, 0, sizeof vtmp);
    vnseg = 0, vpal_key = -1, vhist_w = 0;
    memset(vtear, 0, sizeof vtear);
    memset(vchroma, 0, sizeof vchroma);
    memset(vorb, 0, sizeof vorb);
    memset(vband, 0, sizeof vband);
    memset(vlv, 0, sizeof vlv);
    memset(vpeak, 0, sizeof vpeak);
    memset(vprev, 0, sizeof vprev);
    vt = vbass_prev = vgain = vflux_avg = vbeat = vhue_kick = 0.0f;
    vbeat_n = vbeat_t = 0;
    memset(veff, 0, sizeof veff);
    vnote_p = vchord_p = vsh = vvel_p = 0.0f;
    vchord_was = 255;
    for (k = 0; k < VG_N; k++)
        vg[k] = vg_to[k] = 0.5f;
    vcur = vnext = vgeo = vlast = -1, vgeo_now = vrutt = 0;
    memset(vlut, 0, sizeof vlut);
    vmw = vmorph_left = 0.0f;
    vamp = 256;
    vseen = 0;
    vrng = seed ? seed : 0x2545F491u;
    vo_note = 60.0f, vo_note_to = 60;
    geo_was = -1, ga = gx = gy = 0.0f;
    memset(vmv, 0, sizeof vmv);
    memset(vmv_life, 0, sizeof vmv_life);
    memset(vchan, 0, sizeof vchan);
    vslice_left = 0;
    vo_ph1 = vo_ph2 = 0, vo_beat_was = 0;
    memset(vpixel_x, 0, sizeof vpixel_x);
    memset(vrow_o, 0, sizeof vrow_o);
    vdst = vf;
    vinv = vflash = 0;
    vdtk = 1.0f, vlast_ms = 0;
    memset(vslot, 0, sizeof vslot);
    memset(vslot_h, 0, sizeof vslot_h);
    vntri = vnwire = 0, vo_spin = vo_grp = 0.0f, vo_land_w = 0;
    memset(vo_land, 0, sizeof vo_land);
    memset(&vin, 0, sizeof vin);
    memset(vin_vox, 0, sizeof vin_vox);
}

void vs_frame(const vs_in_t *in)
{
    int k;
    vin = *in;
    for (k = 0; k < VS_NVOX; k++)                /* (CORTEX's three voices' orbs: the lows, the mids, the highs) */
        vin_vox[k] = 1.2f * fm_maxf(vin.bands[2 * k], vin.bands[2 * k + 1]);
    if (vin.nnote)
        vvel_p = (float)vin.note[vin.nnote - 1 < VS_NNOTE ? vin.nnote - 1 : VS_NNOTE - 1].vel * (1.0f / 127.0f);
    if (vin.mutate)
        vis_mutate(vin.mutate == 1 ? 0.15f : vin.mutate == 2 ? 0.5f : 1.0f);
    vdtk = vlast_ms ? fm_clampf((float)(vin.ms - vlast_ms) * (1.0f / 50.0f), 0.25f, 3.0f) : 1.0f;
    vlast_ms = vin.ms ? vin.ms : 1u;
    vis_step(vpar[ST_VMODE]);
    vobj_frame(vdtk);
    {
        const float fl = fm_maxf(vin.strobe ? 1.0f : 0.0f, (float)VPAR(VSTROBE) * 0.01f * vbeat);
        vflash = (int)(fl * 256.0f);
        vinv = VPAR(VINVERT) * 256 / 100;
    }
}

void vs_paint(int y0, int h, uint16_t *out)
{
    vs_out = out;
    if (h > 240)
        h = 240;
    vis_band(y0, h);
    vobj_paint(y0, h, out);
}

uint32_t vs_sum(void)                            /* (FNV-1a over the field, then the randoms and the clock) */
{
    uint32_t s = 2166136261u;
    const uint8_t *p = &vf[0][0];
    unsigned i;
    for (i = 0; i < sizeof vf; i++)
        s = (s ^ p[i]) * 16777619u;
    s = (s ^ vrng) * 16777619u;
    {
        union {
            float f;
            uint32_t u;
        } c;
        c.f = vt;
        s = (s ^ c.u) * 16777619u;
    }
    return s;
}
