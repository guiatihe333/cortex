/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal, guiatihe333 */
/* CORTEX VIDEO (app.h): the ear (USB audio in, the render), MIDI in, the panel, the screen, the scenes and the link to
 * the computer's mirror. Part of fm1-base's unity build (gfx.c and announce.c in scope). The video synth itself is
 * vsynth.c, the same file the mirror runs. */
#define VS_HAVE_SWAP16
#define VS_NO_STDINC                     /* (the FM-1 has no C library headers: libc.c's memset, memcpy) */
#define VS_POOL PLAT_POOL
#define VS_RAM PLAT_RAM_CODE
#define VS_FAR(fn) PLAT_FAR(fn)
#define VS_TICKS() plat_ticks24()
#include "vsynth.c"
#include "vlink.h"
#include "vpages.h"

static int cv_eq(const char *a, const char *b)   /* (strcmp == 0) */
{
    while (*a && *a == *b)
        a++, b++;
    return *a == *b;
}
static int cv_pre(const char *a, const char *p)  /* (a begins with p) */
{
    while (*p && *a == *p)
        a++, p++;
    return !*p;
}
static void cv_cpy(char *d, const char *s, int max)   /* (at most max - 1 characters, ended) */
{
    int i = 0;
    while (s && s[i] && i < max - 1)
        d[i] = s[i], i++;
    d[i] = 0;
}

/* ------------------------------------------------------------------ the ear --- */
/* the render: what plays in over USB (fm1-base adds it to the output itself: the FM-1 passes it on), heard a sample in
 * four (11 kHz): six bands (one-pole splits at 150, 400, 1 k, 2.5 k, 5 kHz; fast up, slow down, as CORTEX's ear),
 * the peak, the last 128 samples. GAIN: the input's level into it (x 1/16 .. x 16, x 1 at 50) */
static float ear_lp[5], ear_env[VS_NBAND], ear_acc;
static uint8_t ear_ph;
static volatile float ear_band[VS_NBAND], ear_level, ear_gain = 0.5f / 32768.0f;
static volatile int8_t ear_wave[VS_WAVE_N];
static volatile uint32_t ear_wave_w, ear_blocks_in;   /* (blocks that brought sound: the header's IN) */
static float ear_ring[512] PLAT_POOL;            /* the last samples at 11 kHz, for the spectrum (ear_spectrum) */
static volatile uint32_t ear_ring_w;
static void ear_block(const int16_t *s, uint32_t n)
{
    static const float K[5] = {0.0819f, 0.2040f, 0.4337f, 0.7608f, 0.9423f};   /* 1 - e^(-2 pi fc / 11025) */
    const float g = ear_gain;
    float pk = 0.0f;
    uint32_t i;
    int k;
    for (i = 0; i < n; i++) {
        float m = s ? (float)s[i] * g : 0.0f, b[VS_NBAND];
        pk = fm_maxf(pk, fm_fabsf(m));
        ear_acc += m;
        if (++ear_ph < 4u)
            continue;
        ear_ph = 0;
        m = ear_acc * 0.25f;
        ear_acc = 0.0f;
        for (k = 0; k < 5; k++)
            ear_lp[k] += K[k] * (m - ear_lp[k]);
        b[0] = ear_lp[0];
        for (k = 1; k < 5; k++)
            b[k] = ear_lp[k] - ear_lp[k - 1];
        b[5] = m - ear_lp[4];
        for (k = 0; k < VS_NBAND; k++) {
            const float a = fm_fabsf(b[k]);
            ear_env[k] = a > ear_env[k] ? ear_env[k] + (a - ear_env[k]) * 0.3f : ear_env[k] * 0.9995f;
            ear_band[k] = ear_env[k];
        }
        ear_wave[ear_wave_w % VS_WAVE_N] = (int8_t)fm_clampf(m * 160.0f, -127.0f, 127.0f);
        ear_wave_w++;
        ear_ring[ear_ring_w & 511u] = m;
        ear_ring_w++;
    }
    ear_level = fm_maxf(pk, ear_level * fm_expf(-(float)n * (1.0f / (0.25f * 44100.0f))));
}

/* ---------------------------------------------------------------- MIDI in --- */
/* the render takes it (plat_midi_in), the main loop reads it: notes (a ring), the CC, the clock. CC 1 is the react
 * matrix's CC source; CC 14..17 the page's four knobs; a program change a scene */
#define NQ 32u
static volatile uint32_t nq_w, nq_r;
static volatile uint8_t nq[NQ][3];                /* channel, note, velocity */
static volatile uint8_t midi_cc1, midi_vel, midi_root, kq_cc[4], kq_has, pc_q = 255;
static volatile uint32_t clk_ticks, clk_last_ms, clk_beats, midi_in_n;
static volatile uint8_t clk_run;
static void midi_take(void)
{
    uint32_t pkt;
    int port;
    while (plat_midi_in(&pkt, &port)) {
        const uint32_t st = pkt >> 8 & 0xFFu, d1 = pkt >> 16 & 0x7Fu, d2 = pkt >> 24 & 0x7Fu;
        if (st == 0xF8u) {                       /* the clock: 24 a beat */
            clk_ticks++;
            clk_last_ms = plat_ms();
            if (clk_ticks % 24u == 0u)
                clk_beats++;
            continue;
        }
        if (st == 0xFAu) {                       /* Start: from the top */
            clk_ticks = 0, clk_run = 1;
            continue;
        }
        if (st == 0xFCu) {
            clk_run = 0;
            continue;
        }
        if (st >= 0xF0u)
            continue;
        midi_in_n++;
        if ((st & 0xF0u) == 0x90u && d2) {
            if (nq_w - nq_r < NQ) {
                nq[nq_w % NQ][0] = (uint8_t)(st & 15u), nq[nq_w % NQ][1] = (uint8_t)d1, nq[nq_w % NQ][2] = (uint8_t)d2;
                nq_w++;
            }
            midi_vel = (uint8_t)d2, midi_root = (uint8_t)(d1 % 12u);
        } else if ((st & 0xF0u) == 0xB0u) {
            if (d1 == 1u)
                midi_cc1 = (uint8_t)d2;
            else if (d1 >= 14u && d1 <= 17u) {
                kq_cc[d1 - 14u] = (uint8_t)d2;
                kq_has |= (uint8_t)(1u << (d1 - 14u));
            }
        } else if ((st & 0xF0u) == 0xC0u) {
            pc_q = (uint8_t)d1;
        }
    }
}

void app_render(int32_t *lr, uint32_t n, uint32_t gain_q12)
{
    const int16_t *usb, *st;
    const uint32_t k = plat_usb_audio_in(&usb), ks = plat_usb_audio_in_lr(&st);
    uint32_t i;
    (void)gain_q12;                              /* (USB audio full: the render at 4096, MASTER after the USB tap) */
    midi_take();
    /* what the computer plays, as this render's own output (the platform's own mix of it is muted, app_init): so it
     * goes back to the computer on the USB recording too, which the mirror records with the picture. 16-bit to the
     * I2S's 24 (x 256); one block behind what came in (5.8 ms) */
    for (i = 0; i < n; i++) {
        const int on = st && i < ks;
        lr[2u * i] = on ? (int32_t)st[2u * i] * 256 : 0;
        lr[2u * i + 1u] = on ? (int32_t)st[2u * i + 1u] * 256 : 0;
    }
    if (k)
        ear_blocks_in++;
    ear_block(k ? usb : 0, k ? (k < n ? k : n) : n);
}
void app_panic(void) {}

/* ---------------------------------------------------------------- the store --- */
#define SET_MAGIC 0x31585643u            /* "CVX1" */
#define NSCENE 16
typedef struct {
    uint32_t magic, n;
    int16_t par[ST_N];
    uint8_t page, slot, look, scene;
    uint16_t bpm;                        /* the tempo without a clock (the taps'), 40 .. 240 */
    uint8_t sq, smorph, schain;          /* scenes: LAUNCH (0 at once, 1 on the beat, 2 on the bar), MORPH (beats, 0 a
                                          * cut), CHAIN (bars a scene, 0 off) */
    uint8_t kfn[11];                     /* the matrix: keys 17 .. 27 (F#4 .. G5), what each does (MF_*) */
    int8_t kpar[11];                     /* ... its setting (Jump, Toggle) */
    int16_t kval[11];                    /* ... and the value it goes to */
    uint8_t rsv[8];
} set_t;
typedef struct {
    uint32_t magic, n;
    uint32_t used;                       /* bit i: scene i stored */
    uint8_t par[NSCENE][ST_N];           /* (each setting over its low end: every range fits a byte; the store's
                                          * objects hold 3840 bytes) */
} scenes_t;
static set_t set;
static scenes_t scn;
static uint8_t set_dirty, scn_dirty;

/* the store from before STYLE's eleven settings (ST_CCRT .. ST_CSEG, between DELAY and the react slots): eleven
 * fewer (before the camera), or 2026-11-21's four (STYLE, AMOUNT, SATURATION, SEGMENTS: 7 fewer). Carried over, the
 * new ones at their defaults */
#define ST_NEW_AT ST_CCRT
static uint8_t video_ble_rx[VLINK_CODEC_MAX] PLAT_POOL;          /* (a frame coming in: 60 x 60 RGB332, or coded;
                                                  * at the start, the store's old objects read for carrying over) */
#define store_old video_ble_rx
_Static_assert(VLINK_CODEC_MAX >= PLAT_STORE_MAX, "the old objects fit");
static int old_index(int i, int oldn)            /* (an old setting's place now; -1 gone) */
{
    if (i < ST_NEW_AT)
        return i;
    if (oldn == ST_N - 11)
        return i + 11;
    if (i < ST_NEW_AT + 4)                       /* (the night before's: SATURATION and SEGMENTS kept) */
        return i == ST_NEW_AT + 2 ? ST_CSAT : i == ST_NEW_AT + 3 ? ST_CSEG : -1;
    return i + 7;
}
static void set_defaults(void);
static int store_migrate_set(void)
{
    const int got = plat_store_load(OBJ_SET, store_old, PLAT_STORE_MAX);
    uint32_t magic, n;
    int i;
    if (got < 8)
        return 0;
    memcpy(&magic, store_old, 4), memcpy(&n, store_old + 4, 4);
    if (magic != SET_MAGIC || (n != ST_N - 11 && n != ST_N - 7) || got != (int)(sizeof set - 2u * (ST_N - n)))
        return 0;
    set_defaults();
    for (i = 0; i < (int)n; i++) {
        const int k = old_index(i, (int)n);
        if (k >= 0)
            memcpy(&set.par[k], store_old + 8 + 2 * i, 2);
    }
    memcpy(&set.page, store_old + 8 + 2 * n, sizeof set - __builtin_offsetof(set_t, page));
    if (n == ST_N - 11 && set.page >= 21)        /* (past MODULATE: STYLE's three pages before it) */
        set.page = (uint8_t)(set.page + 3);
    else if (n == ST_N - 7 && set.page >= 22)
        set.page = (uint8_t)(set.page + 2);
    set.n = ST_N;
    set_dirty = 1;
    return 1;
}
static int store_migrate_scenes(void)
{
    const int got = plat_store_load(OBJ_SCENES, store_old, PLAT_STORE_MAX);
    uint32_t magic, n, used;
    int s, i;
    if (got < 12)
        return 0;
    memcpy(&magic, store_old, 4), memcpy(&n, store_old + 4, 4), memcpy(&used, store_old + 8, 4);
    if (magic != SET_MAGIC || (n != ST_N - 11 && n != ST_N - 7) || got < (int)(12u + NSCENE * n))
        return 0;
    memset(&scn, 0, sizeof scn);
    scn.magic = SET_MAGIC, scn.n = ST_N, scn.used = used;
    for (s = 0; s < NSCENE; s++) {
        for (i = 0; i < ST_N; i++)               /* (the new ones: their defaults, over their low ends) */
            scn.par[s][i] = (uint8_t)(vs_par_info(i)->def - vs_par_info(i)->lo);
        for (i = 0; i < (int)n; i++) {
            const int k = old_index(i, (int)n);
            if (k >= 0)
                scn.par[s][k] = store_old[12 + s * n + i];
        }
    }
    scn_dirty = 1;
    return 1;
}

static void set_defaults(void)
{
    int k;
    memset(&set, 0, sizeof set);
    set.magic = SET_MAGIC, set.n = ST_N;
    for (k = 0; k < ST_N; k++)
        set.par[k] = vs_par_info(k)->def;
    set.bpm = 120;
    {                                    /* the matrix as it comes: the genome, the hold, the breakers, a few nexts */
        static const uint8_t KF[11] = {1, 2, 3, 4, 5, 6, 7, 8, 11, 10, 13};
        memcpy(set.kfn, KF, sizeof KF);
        memset(set.kpar, 0, sizeof set.kpar);
        memset(set.kval, 0, sizeof set.kval);
    }
}

/* ---------------------------------------------------------------- the looks --- */
/* CORTEX's twenty looks (1.5.20 .. 1.6): PRESET; each sets the picture's settings to their start, then its own */
static const struct {
    const char *name;
    uint8_t n, k[15];
    int16_t v[15];
} LOOKS[] = {
    {"Default", 0, {0}, {0}},
    {"Clean", 12, {ST_VTRAIL, ST_VFEED, ST_VRGB, ST_VHUE, ST_VSYNC, ST_VMUTATE, ST_VCHAOS, ST_VFLOW, ST_VPULSE, ST_AR1S + 2,
                   ST_AR1S + 2 + VS_SLOT_W, ST_AR1S + 2 + 2 * VS_SLOT_W}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {"Clean Camera", 11, {ST_VTRAIL, ST_VMODE, ST_VBARS, ST_VFEED, ST_VRGB, ST_VHUE, ST_VSYNC, ST_VPULSE, ST_AR1S + 2,
                          ST_AR1S + 2 + VS_SLOT_W, ST_AR1S + 2 + 2 * VS_SLOT_W}, {0, VS_LIVE_VIDEO, 2, 0, 0, 0, 0, 0, 0, 0, 0}},
    {"Tunnel", 12, {ST_VMODE, ST_VFEED, ST_VTRAIL, ST_VXTGT, ST_VXZOOM, ST_VXROT, ST_VPAL, ST_VHUE, ST_VMOSH, ST_VSORT, ST_VCORRUPT, ST_VSLICE}, {4, 85, 80, 0, 45, 25, 2, 20, 0, 0, 0, 0}},
    {"LZX Grid", 13, {ST_VMODE, ST_VOFREQ, ST_VOSHAPE, ST_VOFREQ2, ST_VOCOMB, ST_VOSPEED, ST_VFEED, ST_VFOLD, ST_VPAL, ST_VMOSH, ST_VSORT, ST_VCORRUPT, ST_VSLICE}, {8, 45, 3, 45, 1, 25, 15, 55, 3, 0, 0, 0, 0}},
    {"Hydra", 13, {ST_VMODE, ST_VOFREQ, ST_VOSHAPE, ST_VOCOMB, ST_VOWARP, ST_VFEED, ST_VXROT, ST_VXZOOM, ST_VFOLD, ST_VHUE, ST_VMOSH, ST_VSORT, ST_VCORRUPT}, {8, 30, 0, 4, 65, 75, 12, 15, 35, 40, 0, 0, 10}},
    {"Kaleido", 9, {ST_VMODE, ST_VMIRROR, ST_VSIDES, ST_VGEO, ST_VFEED, ST_VPAL, ST_VFOLD, ST_VMOSH, ST_VSORT}, {5, 4, 6, 4, 45, 0, 20, 0, 0}},
    {"Datamosh", 7, {ST_VMODE, ST_VMOSH, ST_VSORT, ST_VCORRUPT, ST_VSLICE, ST_VFEED, ST_VPULSE}, {3, 85, 55, 55, 45, 35, 70}},
    {"Rutt-Etra", 10, {ST_VMODE, ST_VGEO, ST_VFEED, ST_VPAL, ST_VTRAIL, ST_VRGB, ST_VMOSH, ST_VSORT, ST_VCORRUPT, ST_VSLICE}, {1, 6, 10, 4, 45, 10, 0, 0, 0, 0}},
    {"Calm", 15, {ST_VMODE, ST_VFEED, ST_VTRAIL, ST_VSPEED, ST_VMUTATE, ST_VCHAOS, ST_VMOSH, ST_VSORT, ST_VCORRUPT, ST_VSLICE, ST_VRGB, ST_VSYNC, ST_VPAL, ST_VEAR, ST_VPULSE}, {5, 10, 88, 15, 10, 0, 0, 0, 0, 0, 5, 0, 2, 75, 10}},
    {"Vectors", 13, {ST_VMODE, ST_VGEO, ST_VGEO2, ST_VSIDES, ST_VSIDES2, ST_VFEED, ST_VXTGT, ST_VXROT, ST_VTRAIL, ST_VMOSH, ST_VSORT, ST_VCORRUPT, ST_VSLICE}, {6, 2, 9, 5, 7, 60, 2, 10, 70, 0, 0, 0, 0}},
    {"Prism", 6, {ST_VMODE, ST_VMIRROR, ST_VLAYER, ST_VBLEND, ST_VMIX, ST_VPAL}, {2, 3, 7, 1, 50, 1}},
    {"Starfield", 11, {ST_VMODE, ST_VGEO, ST_VFEED, ST_VXZOOM, ST_VXTGT, ST_VPAL, ST_VTRAIL, ST_VMOSH, ST_VSORT, ST_VCORRUPT, ST_VSLICE}, {7, 13, 30, -30, 0, 4, 70, 0, 0, 0, 0}},
    {"Broken TV", 7, {ST_VMODE, ST_VSYNC, ST_VRGB, ST_VSLICE, ST_VCORRUPT, ST_VPOSTER, ST_VPULSE}, {0, 80, 70, 60, 30, 40, 60}},
    {"Aurora", 15, {ST_VMODE, ST_VPAL, ST_VFEED, ST_VTRAIL, ST_VSPEED, ST_VMUTATE, ST_VCHAOS, ST_VEAR, ST_VPULSE, ST_VFLOW, ST_VHUE, ST_VMOSH, ST_VSORT, ST_VCORRUPT, ST_VSLICE}, {5, 2, 10, 88, 15, 10, 0, 75, 10, 25, 30, 0, 0, 0, 0}},
    {"Mandala", 10, {ST_VMODE, ST_VGEO, ST_VSIDES, ST_VMIRROR, ST_VFEED, ST_VPAL, ST_VMOSH, ST_VSORT, ST_VCORRUPT, ST_VSLICE}, {2, 4, 8, 4, 30, 1, 0, 0, 0, 0}},
    {"Phosphor", 11, {ST_VMODE, ST_VGEO, ST_VGEO2, ST_VPAL, ST_VFEED, ST_VTRAIL, ST_VRGB, ST_VMOSH, ST_VSORT, ST_VCORRUPT, ST_VSLICE}, {1, 5, 2, 7, 40, 75, 0, 0, 0, 0, 0}},
    {"Moire", 12, {ST_VMODE, ST_VOFREQ, ST_VOSHAPE, ST_VOFREQ2, ST_VOCOMB, ST_VOSPEED, ST_VPAL, ST_VFEED, ST_VMOSH, ST_VSORT, ST_VCORRUPT, ST_VSLICE}, {8, 70, 5, 72, 2, 10, 4, 0, 0, 0, 0, 0}},
    {"Wormhole", 13, {ST_VMODE, ST_VGEO, ST_VSIDES, ST_VFEED, ST_VXTGT, ST_VXZOOM, ST_VXROT, ST_VTRAIL, ST_VPAL, ST_VMOSH, ST_VSORT, ST_VCORRUPT, ST_VSLICE}, {6, 16, 5, 45, 2, 25, -20, 60, 3, 0, 0, 0, 0}},
    {"VHS", 10, {ST_VMODE, ST_VSYNC, ST_VRGB, ST_VSLICE, ST_VPOSTER, ST_VTRAIL, ST_VPAL, ST_VMOSH, ST_VCORRUPT, ST_VHUE}, {0, 45, 55, 30, 20, 70, 1, 15, 10, 5}},
    {"Synthwave", 11, {ST_VMODE, ST_VGEO, ST_VPAL, ST_VFEED, ST_VXTGT, ST_VXZOOM, ST_VTRAIL, ST_VMOSH, ST_VSORT, ST_VCORRUPT, ST_VSLICE}, {4, 3, 6, 50, 0, 30, 70, 0, 0, 0, 0}},
    {"Ink", 11, {ST_VMODE, ST_VPAL, ST_VKEY, ST_VPOSTER, ST_VFEED, ST_VFLOW, ST_VTRAIL, ST_VMOSH, ST_VSORT, ST_VCORRUPT, ST_VSLICE}, {3, 4, 40, 60, 60, 60, 90, 0, 0, 0, 0}},
    {"Strobe", 9, {ST_VMODE, ST_VSTROBE, ST_VPAL, ST_VFEED, ST_VGEO, ST_VSIDES, ST_VMIRROR, ST_VPULSE, ST_VINVERT}, {7, 70, 5, 40, 7, 6, 4, 90, 0}},
    {"Orb", 8, {ST_VMODE, ST_OFORM, ST_OSIZE, ST_OSPEC, ST_OLIGHT, ST_VFEED, ST_VTRAIL, ST_VPAL}, {5, 2, 50, 55, 80, 30, 70, 0}},
    {"Supershell", 11, {ST_VMODE, ST_OFORM, ST_OSYM, ST_OPINCH, ST_OCURVE, ST_OSYM2, ST_OTWIST, ST_OSPEC, ST_ODRAW, ST_VPAL, ST_VFEED}, {7, 2, 7, 18, 35, 3, 25, 40, 3, 6, 20}},
    {"Cubes", 10, {ST_VMODE, ST_OFORM, ST_OSYM, ST_OSYM2, ST_OPINCH, ST_OCURVE, ST_OCOUNT, ST_OARRAY, ST_OREACT, ST_VPAL}, {7, 2, 4, 4, 90, 90, 27, 6, 70, 5}},
    {"Flower", 10, {ST_VMODE, ST_OFORM, ST_OSYM, ST_OPINCH, ST_OCURVE, ST_ODEPTH, ST_OCOUNT, ST_OSTSCALE, ST_OSTROT, ST_OSTHUE}, {5, 1, 6, 30, 20, 0, 8, -25, 12, 40}},
    {"Torus Ring", 10, {ST_VMODE, ST_OFORM, ST_OHOLE, ST_OCOUNT, ST_OARRAY, ST_OSPREAD, ST_OBLEND, ST_VFEED, ST_VTRAIL, ST_VPAL}, {5, 3, 30, 6, 5, 55, 2, 60, 85, 8}},
    {"Terrain", 9, {ST_VMODE, ST_OFORM, ST_ODRAW, ST_OTILT, ST_OZOOM, ST_OMORPH, ST_VPAL, ST_VFEED, ST_OSPIN}, {4, 4, 1, 30, 55, 50, 6, 40, 10}},
    {"Spectrum", 10, {ST_VMODE, ST_OFORM, ST_OSYM, ST_OPINCH, ST_OCURVE, ST_OCOUNT, ST_OARRAY, ST_OSPREAD, ST_OREACT, ST_VPAL}, {8, 1, 4, 90, 90, 16, 2, 80, 100, 5}},
    {"Clone Spiral", 11, {ST_VMODE, ST_OFORM, ST_OSYM, ST_OCOUNT, ST_OARRAY, ST_OSTROT, ST_OSTHUE, ST_OSTTIME, ST_OSTSCALE, ST_VFEED, ST_VPAL}, {5, 2, 5, 16, 3, 15, 60, 50, 10, 40, 6}},
    {"Kick Shatter", 12, {ST_VMODE, ST_OFORM, ST_OSYM, ST_OPINCH, ST_TR1S, ST_TR1A, ST_TR1N, ST_TR1S + 3, ST_TR1S + 4, ST_TR1S + 5, ST_OSYNC, ST_VPAL}, {7, 2, 5, 40, 1, 0, 90, 2, 3, 70, 3, 8}},
    {"Negative", 8, {ST_VMODE, ST_VINVERT, ST_VPAL, ST_VFEED, ST_VXZOOM, ST_VXROT, ST_VGEO, ST_VTRAIL}, {5, 100, 8, 70, 30, 15, 14, 80}},
    {"Camera Glitch", 9, {ST_VTRAIL, ST_VMODE, ST_VBARS, ST_VMOSH, ST_VCORRUPT, ST_VSLICE, ST_VRGB, ST_VSYNC, ST_VFEED}, {20, VS_LIVE_VIDEO, 2, 70, 45, 35, 30, 30, 0}},
};
#define NLOOK ((int)(sizeof LOOKS / sizeof *LOOKS))
#define PICTURE_LAST ST_TR3N                  /* (a look, a scene, the dice: VMODE .. VINVERT; not the ear, the screen,
                                                  * the react slots: the player's) */

/* ------------------------------------------------------------------- pages --- */
/* (vpages.h: the pages, their groups, the knobs' own kinds) */
#define NMKEY 11                         /* the matrix's keys: 17 .. 27 */
enum { MF_OFF, MF_NUDGE, MF_THROW, MF_NEW, MF_HOLD, MF_STROBE, MF_INVERT, MF_MOSH, MF_FREEZE, MF_BLACK, MF_NVIS,
       MF_NOBJ, MF_NPAL, MF_NLOOK, MF_PLOOK, MF_NSCENE, MF_PSCENE, MF_RSCENE, MF_TAP, MF_CHAIN, MF_JUMP, MF_TOGGLE,
       MF_N };
static const char *const MF_NAME[MF_N] = {"Off", "Nudge", "Throw", "New", "Hold", "Strobe", "Invert", "Mosh",
                                          "Freeze", "Blackout", "Next visual", "Next object", "Next colour",
                                          "Next look", "Prev look", "Next scene", "Prev scene", "Any scene", "Tap",
                                          "Chain", "Jump", "Toggle"};

/* ---------------------------------------------------------------- the link --- */
static struct {
    uint8_t on, rec, want_sync;
    uint32_t pending;                    /* a frame that did not go (the ring busy): sent before the next is made */
    uint32_t last_cmd_ms, seq, seed, sent, waited, syncs, rec_ms;
    int16_t sent_par[ST_N];              /* the settings as the mirror has them */
} lk;
static uint8_t lk_buf[VLINK_SYNC_MAX > VLINK_MAX ? VLINK_SYNC_MAX : VLINK_MAX];
static uint8_t lk_raw[VLINK_SYNC_RAW > VLINK_RAW_MAX ? VLINK_SYNC_RAW : VLINK_RAW_MAX];

static uint8_t video_pixels[1][VLINK_VIDEO_SIDE * VLINK_VIDEO_SIDE] PLAT_POOL;   /* (one: the room the sharp
                                                  * picture's colour took) */
static struct {
    uint32_t shown_ms, part_ms, frames;
    uint8_t front, shown_kind, loading_kind, frame_id, next_part;
} video_rx;

static void receive_video(const uint8_t *p, uint32_t n)
{
    uint8_t packet[3 + VLINK_VIDEO_PART];
    const int selected = vs_get(ST_VMODE) - VS_NMODE + 1;
    if (selected < 1 || selected > 2 || !lk.on ||
        n != (sizeof packet * 8u + 6u) / 7u ||
        vlink_unpack(p, n, packet, sizeof packet) != sizeof packet ||
        packet[0] != selected)
        return;
    if (packet[2] == 0) {
        video_rx.loading_kind = packet[0];
        video_rx.frame_id = packet[1];
        video_rx.next_part = 0;
    }
    if (packet[0] != video_rx.loading_kind || packet[1] != video_rx.frame_id ||
        packet[2] != video_rx.next_part || packet[2] >= VLINK_VIDEO_PARTS)
        return;
    video_rx.part_ms = plat_ms();
    memcpy(video_pixels[0] + packet[2] * VLINK_VIDEO_PART, packet + 3, VLINK_VIDEO_PART);
    if (++video_rx.next_part == VLINK_VIDEO_PARTS) {
        video_rx.front ^= 1u;
        video_rx.shown_kind = packet[0];
        video_rx.shown_ms = video_rx.part_ms;
        video_rx.frames++;
    }
}

/* Cortex BLE preview: source, frame ID, little-endian byte offset, up to 180 RGB332 pixels. */
#define VIDEO_BLE_QUEUE 16u
#define VIDEO_BLE_HEADER 4u
static struct {
    uint8_t size, data[VIDEO_BLE_HEADER + VLINK_BLE_PAYLOAD];
} video_ble_queue[VIDEO_BLE_QUEUE] PLAT_POOL;
static volatile uint8_t video_ble_write, video_ble_read, video_ble_mode;
static uint16_t video_ble_offset;
static uint8_t video_ble_id, video_ble_kind, video_ble_front;
static uint8_t video_ble_pixels[1][VLINK_VIDEO_SIDE * VLINK_VIDEO_SIDE] PLAT_POOL;   /* (one: a frame copied in whole) */
static uint8_t video_sharp[VLINK_HQ_BYTES] PLAT_POOL;            /* (the sharp picture, decoded) */
static uint8_t video_ble_hq[2], video_ble_sharp;
static unsigned video_ble_size;

/* the sharp picture's code (the iPhone's VideoCodec), 2: 2 bytes its length, flags (1: every block), the quantizer's
 * scale (16: JPEG's quality 50), 29 bytes a bit a block (15 x 15, which are in it); then bits, the first the highest,
 * each block in it: a bit (its colour follows: four cells of 30 x 30, 8 bits each, Cb Cr nibbles, steps of 8); its DC
 * against the block before in this frame (signed Exp-Golomb); how many others are not zero (Exp-Golomb); each: the
 * zeros before it in zigzag (Exp-Golomb), its size less one (Exp-Golomb), its sign (a bit). The colour (3): its
 * 4 x 4 cells of the 60 x 60, Cb then Cr, each coded as the light is: a 4 x 4 DCT, JPEG's colour quantizer, DC
 * against the block before, the rest as runs */
static const uint8_t ZZ4[16] = {0, 1, 4, 8, 5, 2, 3, 6, 9, 12, 13, 10, 7, 11, 14, 15};
static const uint8_t QC4[16] = {17, 18, 24, 47, 18, 21, 26, 66, 24, 26, 56, 99, 47, 66, 99, 99};
static float idct4[4][4];
static const uint8_t ZZ[64] = {0, 1, 8, 16, 9, 2, 3, 10, 17, 24, 32, 25, 18, 11, 4, 5, 12, 19, 26, 33, 40, 48,
                               41, 34, 27, 20, 13, 6, 7, 14, 21, 28, 35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23,
                               30, 37, 44, 51, 58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63};
static const uint8_t QBASE[64] = {16, 11, 10, 16, 24, 40, 51, 61, 12, 12, 14, 19, 26, 58, 60, 55,
                                  14, 13, 16, 24, 40, 57, 69, 56, 14, 17, 22, 29, 51, 87, 80, 62,
                                  18, 22, 37, 56, 68, 109, 103, 77, 24, 35, 55, 64, 81, 104, 113, 92,
                                  49, 64, 78, 87, 103, 121, 120, 101, 72, 92, 95, 98, 112, 100, 103, 99};
static float idct_t[8][8];                       /* [x][u]: C(u) / 2 cos((2x + 1) u pi / 16) */
static struct {
    const uint8_t *p;
    uint32_t pos, end, bad;                      /* (in bits) */
} cbits;
static uint32_t cb_get(int n)                    /* n bits (n <= 24) */
{
    uint32_t v = 0;
    if (cbits.pos + (uint32_t)n > cbits.end) {
        cbits.bad = 1;
        return 0;
    }
    while (n--) {
        v = v << 1 | (uint32_t)(cbits.p[cbits.pos >> 3] >> (7 - (cbits.pos & 7)) & 1u);
        cbits.pos++;
    }
    return v;
}
static uint32_t cb_eg(void)                      /* Exp-Golomb, order 0 */
{
    int z = 0;
    while (!cb_get(1)) {
        if (cbits.bad || ++z > 20) {
            cbits.bad = 1;
            return 0;
        }
    }
    return ((1u << z) | cb_get(z)) - 1u;
}
static int cb_seg(void)                          /* signed: 1, -1, 2, -2 .. as 1, 2, 3, 4 .. */
{
    const uint32_t m = cb_eg();
    return m & 1u ? (int)((m + 1u) >> 1) : -(int)(m >> 1);
}
static uint8_t codec_scale;                      /* (diagnostics: the last frame's quantizer scale) */
static int codec_decode(const uint8_t *p, unsigned n)
{
    static const float C[9] = {1.0f, 0.98078528f, 0.92387953f, 0.83146961f, 0.70710678f, 0.55557023f, 0.38268343f,
                               0.19509032f, 0.0f};
    uint8_t *luma = video_sharp, *cb = video_sharp + VLINK_HQ_LUMA, *cr = cb + 3600;
    int step[64], cstep[16], b, k, dc = 0, cdc[2] = {0, 0};
    if (idct_t[0][0] == 0.0f) {
        int x, u;
        for (x = 0; x < 8; x++)
            for (u = 0; u < 8; u++) {
                const int m = ((2 * x + 1) * u) & 31;
                const float c = m <= 8 ? C[m] : m <= 16 ? -C[16 - m] : m <= 24 ? -C[m - 16] : C[32 - m];
                idct_t[x][u] = u ? 0.5f * c : 0.35355339f;
            }
    }
    if (idct4[0][0] == 0.0f) {
        int x, u;
        for (x = 0; x < 4; x++)
            for (u = 0; u < 4; u++) {
                const int m = ((2 * x + 1) * u * 2) & 31;   /* (in 16ths of pi) */
                const float c = m <= 8 ? C[m] : m <= 16 ? -C[16 - m] : m <= 24 ? -C[m - 16] : C[32 - m];
                idct4[x][u] = u ? 0.70710678f * c : 0.5f;
            }
    }
    if (n < 33 || (unsigned)(p[0] | p[1] << 8) != n || !p[3])
        return 0;
    codec_scale = p[3];
    for (k = 0; k < 64; k++) {
        const int s = (QBASE[k] * p[3] + 8) / 16;
        step[k] = s < 1 ? 1 : s;
    }
    for (k = 0; k < 16; k++) {
        const int s = (QC4[k] * p[3] + 8) / 16;
        cstep[k] = s < 1 ? 1 : s;
    }
    cbits.p = p + 33, cbits.pos = 0, cbits.end = (n - 33u) * 8u, cbits.bad = 0;
    for (b = 0; b < 225; b++) {
        const int by = b / 15, bx = b - by * 15;
        float F[64], T[64];
        int pos = 0, x, y, u, nz;
        if (!(p[4 + (b >> 3)] >> (b & 7) & 1u))
            continue;
        if (cb_get(1)) {                         /* (its colour, coded) */
            int comp;
            for (comp = 0; comp < 2; comp++) {
                uint8_t *pl = comp ? cr : cb;
                float G[16], H[16];
                int p4 = 0, n4, x4, y4, u4;
                memset(G, 0, sizeof G);
                cdc[comp] += cb_seg();
                G[0] = (float)(cdc[comp] * cstep[0]);
                n4 = (int)cb_eg();
                while (n4-- > 0) {
                    int v;
                    p4 += 1 + (int)cb_eg();
                    v = 1 + (int)cb_eg();
                    if (cb_get(1))
                        v = -v;
                    if (cbits.bad || p4 > 15)
                        return 0;
                    G[ZZ4[p4]] = (float)(v * cstep[ZZ4[p4]]);
                }
                for (y4 = 0; y4 < 4; y4++)
                    for (x4 = 0; x4 < 4; x4++) {
                        float s = 0.0f;
                        for (u4 = 0; u4 < 4; u4++)
                            s += idct4[x4][u4] * G[y4 * 4 + u4];
                        H[y4 * 4 + x4] = s;
                    }
                for (y4 = 0; y4 < 4; y4++)
                    for (x4 = 0; x4 < 4; x4++) {
                        float s = 128.0f;
                        int v;
                        for (u4 = 0; u4 < 4; u4++)
                            s += idct4[y4][u4] * H[u4 * 4 + x4];
                        v = (int)(s + 0.5f);
                        pl[(by * 4 + y4) * 60 + bx * 4 + x4] = (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v);
                    }
            }
        }
        memset(F, 0, sizeof F);
        dc += cb_seg();
        F[0] = (float)(dc * step[0]);
        nz = (int)cb_eg();
        while (nz-- > 0) {
            int v;
            pos += 1 + (int)cb_eg();
            v = 1 + (int)cb_eg();
            if (cb_get(1))
                v = -v;
            if (cbits.bad || pos > 63)
                return 0;
            F[ZZ[pos]] = (float)(v * step[ZZ[pos]]);
        }
        if (cbits.bad)
            return 0;
        for (y = 0; y < 8; y++)                  /* (rows: the frequencies across to x) */
            for (x = 0; x < 8; x++) {
                float s = 0.0f;
                for (u = 0; u < 8; u++)
                    s += idct_t[x][u] * F[y * 8 + u];
                T[y * 8 + x] = s;
            }
        for (y = 0; y < 8; y++)                  /* (columns: down to y) */
            for (x = 0; x < 8; x++) {
                float s = 128.0f;
                int v;
                for (u = 0; u < 8; u++)
                    s += idct_t[y][u] * T[u * 8 + x];
                v = (int)(s + 0.5f);
                luma[(by * 8 + y) * 120 + bx * 8 + x] = (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v);
            }
    }
    return 1;
}

/* the clean picture's code (flags bit 1; the iPhone's VideoCodec, clean): the light 240 x 240 in 30 x 30 blocks of 8
 * x 8, its colour 60 x 60 in 15 x 15 blocks of 4 x 4, each part apart: 2 bytes its length, flags, the scale, 113
 * bytes a bit a light block, 29 a bit a colour block; then bits: the light blocks in it (DC against the one before,
 * the others as runs), then the colour blocks in it (Cb then Cr, each so). Decoded into the engine's planes, lent
 * (vs_lend: the clean picture is painted as it is, no effects), and the colour into video_sharp's */
static uint8_t clean_on;                         /* (the last sharp frame was a clean one: the engine's planes lent) */
static uint8_t clean_age[900];                   /* (each light block: 255 when it last came, falling: STYLE Motion) */
static uint32_t clean_rnd = 0x2545F491u;
static uint32_t clean_rand(void) { clean_rnd ^= clean_rnd << 13, clean_rnd ^= clean_rnd >> 17, clean_rnd ^= clean_rnd << 5; return clean_rnd; }
static int codec_decode240(const uint8_t *p, unsigned n)
{
    uint8_t *luma = vs_lend(), *cb = video_sharp + VLINK_HQ_LUMA, *cr = cb + 3600;
    int step[64], cstep[16], b, k, dc = 0, cdc[2] = {0, 0};
    if (idct_t[0][0] == 0.0f || idct4[0][0] == 0.0f)
        codec_decode(p, 0);                      /* (only its tables: nothing decoded from 0 bytes) */
    if (n < 146 || (unsigned)(p[0] | p[1] << 8) != n || !p[3])
        return 0;
    codec_scale = p[3];
    for (k = 0; k < 64; k++) {
        const int s = (QBASE[k] * p[3] + 8) / 16;
        step[k] = s < 1 ? 1 : s;
    }
    for (k = 0; k < 16; k++) {
        const int s = (QC4[k] * p[3] + 8) / 16;
        cstep[k] = s < 1 ? 1 : s;
    }
    cbits.p = p + 146, cbits.pos = 0, cbits.end = (n - 146u) * 8u, cbits.bad = 0;
    /* MOSH: a block's news dropped (what was there stays, the motion smears over it), more on the beat; CORRUPT: a
     * block put elsewhere or flattened to its mean; TRAILS: the news blended over what was there */
    const float beat = vs_beat();
    const uint32_t mosh = (uint32_t)((float)vs_eff(ST_VMOSH) * (0.5f + 0.5f * beat) * 655.36f * 0.9f),
                   corrupt = (uint32_t)((float)vs_eff(ST_VCORRUPT) * (0.3f + beat) * 655.36f * 0.15f);
    const int tr = vs_eff(ST_VTRAIL), keep = tr * tr * 220 / 10000;
    for (k = 0; k < 900; k++)
        clean_age[k] = (uint8_t)(clean_age[k] > 24 ? clean_age[k] - 24 : 0);
    for (b = 0; b < 900; b++) {
        int by = b / 30, bx = b - by * 30;
        float F[64], T[64];
        int pos = 0, x, y, u, nz, flat = 0;
        if (!(p[4 + (b >> 3)] >> (b & 7) & 1u))
            continue;
        clean_age[b] = 255;
        memset(F, 0, sizeof F);
        dc += cb_seg();
        F[0] = (float)(dc * step[0]);
        nz = (int)cb_eg();
        while (nz-- > 0) {
            int v;
            pos += 1 + (int)cb_eg();
            v = 1 + (int)cb_eg();
            if (cb_get(1))
                v = -v;
            if (cbits.bad || pos > 63)
                return 0;
            F[ZZ[pos]] = (float)(v * step[ZZ[pos]]);
        }
        if (cbits.bad)
            return 0;
        for (y = 0; y < 8; y++)
            for (x = 0; x < 8; x++) {
                float s = 0.0f;
                for (u = 0; u < 8; u++)
                    s += idct_t[x][u] * F[y * 8 + u];
                T[y * 8 + x] = s;
            }
        for (y = 0; y < 8; y++)
            for (x = 0; x < 8; x++) {
                float s = 128.0f;
                int v;
                for (u = 0; u < 8; u++)
                    s += idct_t[y][u] * T[u * 8 + x];
                v = (int)(s + 0.5f);
                T[y * 8 + x] = (float)(v < 0 ? 0 : v > 255 ? 255 : v);   /* (the block decoded, before it goes) */
            }
        if (mosh && (clean_rand() & 0xFFFFu) < mosh)
            continue;                            /* MOSH: dropped */
        if (corrupt && (clean_rand() & 0xFFFFu) < corrupt) {
            const uint32_t r = clean_rand();
            if (r & 1u) {                        /* CORRUPT: somewhere else */
                by = (int)(r >> 8 & 0xFFu) % 30, bx = (int)(r >> 16 & 0xFFu) % 30;
            } else {
                flat = 1;                        /* ... or its mean */
            }
        }
        if (flat) {
            float m = 0.0f;
            for (k = 0; k < 64; k++)
                m += T[k];
            for (k = 0; k < 64; k++)
                T[k] = m * (1.0f / 64.0f);
        }
        for (y = 0; y < 8; y++)
            for (x = 0; x < 8; x++) {
                uint8_t *q = &luma[(by * 8 + y) * 240 + bx * 8 + x];
                const int v = (int)T[y * 8 + x];
                *q = (uint8_t)(keep ? (*q * keep + v * (256 - keep)) >> 8 : v);   /* TRAILS */
            }
    }
    for (b = 0; b < 225; b++) {
        const int by = b / 15, bx = b - by * 15;
        int comp;
        if (!(p[117 + (b >> 3)] >> (b & 7) & 1u))
            continue;
        for (comp = 0; comp < 2; comp++) {
            uint8_t *pl = comp ? cr : cb;
            float G[16], H[16];
            int p4 = 0, n4, x4, y4, u4;
            memset(G, 0, sizeof G);
            cdc[comp] += cb_seg();
            G[0] = (float)(cdc[comp] * cstep[0]);
            n4 = (int)cb_eg();
            while (n4-- > 0) {
                int v;
                p4 += 1 + (int)cb_eg();
                v = 1 + (int)cb_eg();
                if (cb_get(1))
                    v = -v;
                if (cbits.bad || p4 > 15)
                    return 0;
                G[ZZ4[p4]] = (float)(v * cstep[ZZ4[p4]]);
            }
            if (cbits.bad)
                return 0;
            for (y4 = 0; y4 < 4; y4++)
                for (x4 = 0; x4 < 4; x4++) {
                    float s = 0.0f;
                    for (u4 = 0; u4 < 4; u4++)
                        s += idct4[x4][u4] * G[y4 * 4 + u4];
                    H[y4 * 4 + x4] = s;
                }
            for (y4 = 0; y4 < 4; y4++)
                for (x4 = 0; x4 < 4; x4++) {
                    float s = 128.0f;
                    int v;
                    for (u4 = 0; u4 < 4; u4++)
                        s += idct4[y4][u4] * H[u4 * 4 + x4];
                    v = (int)(s + 0.5f);
                    pl[(by * 4 + y4) * 60 + bx * 4 + x4] = (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v);
                }
        }
    }
    return 1;
}
static void clean_off(void)                      /* (the engine's planes back to it, cleared) */
{
    if (clean_on) {
        clean_on = 0;
        vs_unlend();
    }
}

/* the clean picture painted, with the effects that need no field (a row or a pixel at a time):
 *  - where each pixel comes from: TRANSFORM (zoom, turn, shift; the picture tiled), FLOW (a wave), LENS and SWIRL
 *    (MODULATE's), SEGMENTS (a tunnel; a kaleidoscope of 2 .. 12), FEEDBACK (a zoom blur to the middle, on the beat),
 *    then MIRROR (Mirror, Flip, Quad, Kaleido) and PIXEL's blocks. A map of 60 x 60 points a frame, the pixels between
 *    (in video_sharp's light, unused by the clean picture);
 *  - the light: LAYER Osc over it (OSC's waves, BLEND, MIX), PULSE, CHROMA's map (KEY, COLORAMA's fold, POSTER,
 *    BRIGHT, INVERT, STROBE), SORT's ramps along the row;
 *  - the colour: HUE turning it (above 10), SATURATION, COLORAMA's palette colour;
 *  - STYLE: CRT, VHS, Edges, Emboss, Halftone, LED, Solarize, Swap, Motion (its AMOUNT);
 *  - the row: SYNC's and SLICE's shifts, RGB's split.
 * To the screen through tables (each plane's share, swapped: no clamps). RAM code: no calls */
enum { CS_OFF, CS_CRT, CS_VHS, CS_EDGES, CS_EMBOSS, CS_HALF, CS_LED, CS_SOLAR, CS_SWAP, CS_MOTION };
struct clean_fx {
    const uint8_t *L, *cb, *cr, *lut, *qx, *j0, *wx, *age, *sin256, *hpos;
    const int16_t *mu, *mv;                      /* (the map: 60 x 60, 1/16 pixel; 0: none) */
    const uint16_t *pal, *TR, *TG, *TB;
    int pm, neg, gain, mirror, ps, mapped, style, amt, blur;
    int hc, hs;                                  /* (the colour turned and saturated: 1/256) */
    int osc, blend, mix, oa, ob, ofreq2, ocomb, oshape, ring;   /* (LAYER Osc: phase steps a pixel across and down) */
    int32_t ophase;
    int halfc;                                   /* (Halftone's, LED's cell) */
    int vhs_band, vhs_h;                         /* (VHS: the tracking band's top, height) */
    const int8_t *vhs_jit;
};
static uint16_t clean_tr[768], clean_tg[768], clean_tb[768];
static uint8_t clean_j0[240], clean_wx[240], clean_qx[240], clean_sin[256], clean_hpos[240];
static int8_t clean_jit[240];
static PLAT_RAM_CODE void clean_row(const struct clean_fx *f, int y, int o, int spr, int16_t *ru, int16_t *rv,
                                    int16_t *vb, int16_t *vr, uint8_t *ly, uint16_t *row, uint16_t *out)
{
    const uint16_t *TR = f->TR, *TG = f->TG, *TB = f->TB;
    const uint8_t *lut = f->lut, *L = f->L;
    const int style = f->style, amt = f->amt;
    int x;
    {                                            /* the colour's row between its two nearest 60 x 60 rows; the map's */
        const int fy = y * 64 - 96, i0 = fy < 0 ? 0 : fy >> 8, i1 = i0 + 1 < 60 ? i0 + 1 : 59, wy = fy < 0 ? 0 : fy & 255;
        const uint8_t *b0 = f->cb + i0 * 60, *b1 = f->cb + i1 * 60, *r0 = f->cr + i0 * 60, *r1 = f->cr + i1 * 60;
        for (x = 0; x < 60; x++) {
            vb[x] = (int16_t)((((int)b0[x] - 128) * (256 - wy) + ((int)b1[x] - 128) * wy) >> 8);
            vr[x] = (int16_t)((((int)r0[x] - 128) * (256 - wy) + ((int)r1[x] - 128) * wy) >> 8);
        }
        vb[60] = vb[59], vr[60] = vr[59];
        if (f->mapped) {
            const int16_t *u0 = f->mu + i0 * 60, *u1 = f->mu + i1 * 60, *v0 = f->mv + i0 * 60, *v1 = f->mv + i1 * 60;
            for (x = 0; x < 60; x++) {
                ru[x] = (int16_t)((u0[x] * (256 - wy) + u1[x] * wy) >> 8);
                rv[x] = (int16_t)((v0[x] * (256 - wy) + v1[x] * wy) >> 8);
            }
            ru[60] = ru[59], rv[60] = rv[59];
        }
    }
    {
        const int vhs_in = style == CS_VHS && y >= f->vhs_band && y < f->vhs_band + f->vhs_h;
        const int hcell = f->halfc, hcy = y - f->hpos[y] + hcell / 2, dy = y - 120;
        int32_t ph = f->ophase + y * f->ob, ph2 = f->ophase + y * f->oa;   /* (OSC 2 square to the first) */
        for (x = 0; x < 240; x++, ph += f->oa, ph2 -= f->ob) {
            int px = x, py = y, Y, cb, cr, r, g, bl, j, w;
            if (style == CS_HALF || style == CS_LED) {   /* (the cell's middle stands for it) */
                px = x - f->hpos[x] + hcell / 2, py = hcy;
                px = px > 239 ? 239 : px, py = py > 239 ? 239 : py;
            } else if (f->mapped) {
                const int jj = f->j0[x], ww = f->wx[x];
                px = ((ru[jj] * (256 - ww) + ru[jj + 1] * ww) >> 8) >> 4;
                py = ((rv[jj] * (256 - ww) + rv[jj + 1] * ww) >> 8) >> 4;
                while (px < 0) px += 240;
                while (px >= 240) px -= 240;
                while (py < 0) py += 240;
                while (py >= 240) py -= 240;
            }
            if (f->mirror == 1 || f->mirror >= 3)
                px = px >= 120 ? 239 - px : px;
            if (f->mirror == 2 || f->mirror >= 3)
                py = py >= 120 ? 239 - py : py;
            if (f->mirror == 4 && px > py) {
                const int s = px;
                px = py, py = s;
            }
            px = f->qx[px], py = f->qx[py];
            if (style == CS_VHS)                 /* (VHS: the colour a little behind the light) */
                j = (px >> 2) - 1 - amt / 40, j = j < 0 ? 0 : j, w = 0;
            else
                j = px >> 2, w = 0;
            if (!f->mapped && style != CS_VHS && !f->mirror && f->ps <= 1 && style != CS_HALF && style != CS_LED)
                j = f->j0[x], w = f->wx[x];       /* (straight: the colour between its nearest) */
            cb = (vb[j] * (256 - w) + vb[j + 1] * w) >> 8;
            cr = (vr[j] * (256 - w) + vr[j + 1] * w) >> 8;
            if (py != y) {                       /* (another row: the colour's nearest there) */
                cb = (int)f->cb[(py >> 2) * 60 + (px >> 2)] - 128;
                cr = (int)f->cr[(py >> 2) * 60 + (px >> 2)] - 128;
            }
            Y = L[py * 240 + px];
            if (f->blur) {                       /* FEEDBACK: three more steps to the middle */
                const int kx = ((120 - px) * f->blur) >> 8, ky = ((120 - py) * f->blur) >> 8;
                Y = (Y + L[(py + ky) * 240 + px + kx] + L[(py + 2 * ky) * 240 + px + 2 * kx] +
                     L[(py + 3 * ky) * 240 + px + 3 * kx]) >> 2;
            }
            if (style == CS_EDGES || style == CS_EMBOSS) {
                const int xl = px > 0 ? px - 1 : 0, xr = px < 239 ? px + 1 : 239, yu = py > 0 ? py - 1 : 0,
                          yd = py < 239 ? py + 1 : 239;
                if (style == CS_EDGES) {
                    int e = L[py * 240 + xr] - L[py * 240 + xl], e2 = L[yd * 240 + px] - L[yu * 240 + px];
                    e = (e < 0 ? -e : e) + (e2 < 0 ? -e2 : e2);
                    e = e * (2 + amt / 12);
                    Y = (Y * (100 - amt) / 400) + (e > 255 ? 255 : e);
                } else {
                    Y = 128 + ((L[yd * 240 + xr] - L[yu * 240 + xl]) * (1 + amt / 20));
                    cb = cb * (100 - amt) / 100, cr = cr * (100 - amt) / 100;
                }
                Y = Y < 0 ? 0 : Y > 255 ? 255 : Y;
            }
            if (f->osc) {                        /* LAYER Osc: OSC's waves, BLEND, MIX */
                int ov, ov2;
                if (f->oshape == 5) {            /* (Rings: a zone plate, the distance squared) */
                    const int ddx = x - 120;
                    ov = f->sin256[((((uint32_t)(ddx * ddx + dy * dy) * (uint32_t)f->ring) >> 8) +
                                    (uint32_t)(f->ophase >> 16)) & 255u];
                } else {
                    const int k = (ph >> 16) & 255;
                    ov = f->oshape == 1 ? (k < 128 ? k * 2 : 511 - k * 2) : f->oshape == 2 ? k :
                         f->oshape == 3 ? (k < 128 ? 255 : 0) : f->sin256[k];
                }
                if (f->ofreq2) {
                    ov2 = f->sin256[(ph2 >> 16) & 255];
                    ov = f->ocomb == 1 ? ov * ov2 >> 8 : f->ocomb == 2 ? (ov > ov2 ? ov - ov2 : ov2 - ov) :
                         (ov + ov2) >> 1;
                }
                if (f->ocomb == 4)               /* (Field: the camera's light bends it) */
                    ov = f->sin256[(ov + Y) & 255];
                ov = ov * f->mix >> 8;
                switch (f->blend) {
                case 1: Y = 255 - (((255 - Y) * (255 - ov)) >> 8); break;   /* Screen */
                case 2: Y = Y > ov ? Y : ov; break;                         /* Max */
                case 3: Y = Y > ov ? Y - ov : ov - Y; break;                /* Difference */
                case 4: Y = Y * (ov + 256 - f->mix) >> 8; break;            /* Mask: the waves let it through */
                default: Y = Y + ov; break;                                 /* Add */
                }
                Y = Y > 255 ? 255 : Y;
            }
            if (style == CS_SOLAR)               /* (Solarize: the light over the half turned back) */
                Y = Y < 128 ? Y : Y + ((255 - 2 * Y) * amt) / 100;
            if (vhs_in)                          /* (VHS: the tracking band's noise) */
                Y += (int)((x * 2654435761u + y * 40503u + (uint32_t)f->ophase) >> 26) - 32;
            Y = (Y * f->gain) >> 8;
            ly[x] = (uint8_t)(Y < 0 ? 0 : Y > 255 ? 255 : Y);
            vb[61 + x] = (int16_t)cb, vr[61 + x] = (int16_t)cr;   /* (each pixel's colour, after the row's 61) */
        }
    }
    for (x = 0; x < 240; x++) {
        int Y = lut[ly[x]], cb = vb[61 + x], cr = vr[61 + x], r, g, bl;
        if (!Y) {
            row[x] = 0;
            continue;
        }
        if (f->neg)
            cb = -cb, cr = -cr;
        {                                        /* (HUE's turn, SATURATION) */
            const int c2 = (cb * f->hc - cr * f->hs) >> 8, r2 = (cb * f->hs + cr * f->hc) >> 8;
            cb = c2, cr = r2;
        }
        if (style == CS_MOTION && amt) {         /* (Motion: what moved lit and tinted) */
            const int a = f->age[(y >> 3) * 30 + (x >> 3)] * amt / 100;
            Y = Y + (((255 - Y) * a) >> 9);
            cb = cb * (256 - a) >> 8, cr = cr + (a >> 2);
        }
        r = Y + ((359 * cr) >> 8), g = Y - ((88 * cb + 183 * cr) >> 8), bl = Y + ((454 * cb) >> 8);
        if (f->pm || style == CS_EDGES) {        /* COLORAMA (Edges: the palette's neon) */
            const int pm = style == CS_EDGES ? 256 : f->pm;
            const uint32_t P = f->pal[Y];
            const int pr = (int)(P >> 11) << 3, pg = (int)(P >> 5 & 63u) << 2, pb = (int)(P & 31u) << 3;
            r += ((pr - r) * pm) >> 8, g += ((pg - g) * pm) >> 8, bl += ((pb - bl) * pm) >> 8;
        }
        if (style == CS_SWAP) {                  /* (Swap: the planes round, or red and blue) */
            const int s = r;
            if (amt >= 50)
                r = g, g = bl, bl = s;
            else
                r = bl, bl = s;
        } else if (style == CS_CRT) {            /* (CRT: the scanlines, the phosphors' triads) */
            const int k = x % 3, d = amt * 2;
            if (y & 1)
                r = r * (256 - d) >> 8, g = g * (256 - d) >> 8, bl = bl * (256 - d) >> 8;
            if (k)
                r = r * (256 - d / 2) >> 8;
            if (k != 1)
                g = g * (256 - d / 2) >> 8;
            if (k != 2)
                bl = bl * (256 - d / 2) >> 8;
        } else if (style == CS_HALF || style == CS_LED) {   /* (a round dot in each cell: its size the light's) */
            const int c = f->halfc, dx = f->hpos[x] - c / 2, dy = f->hpos[y] - c / 2,
                      rad = style == CS_LED ? c * c * 3 / 16 : (ly[x] * c * c) >> 9;
            if (dx * dx + dy * dy > rad) {
                row[x] = 0;
                continue;
            }
        }
        r = r < -256 ? -256 : r > 511 ? 511 : r;
        g = g < -256 ? -256 : g > 511 ? 511 : g;
        bl = bl < -256 ? -256 : bl > 511 ? 511 : bl;
        row[x] = (uint16_t)(TR[r + 256] | TG[g + 256] | TB[bl + 256]);
    }
    if (style == CS_VHS)                         /* (VHS: each row's wobble) */
        o += f->vhs_jit[y];
    {                                            /* SYNC's and SLICE's shift, RGB's split */
        int xg = -o, xr, xb;
        while (xg < 0) xg += 240;
        while (xg >= 240) xg -= 240;
        if (!spr) {
            for (x = 0; x < 240; x++) {
                out[x] = row[xg];
                if (++xg == 240) xg = 0;
            }
        } else {
            xr = xg - spr, xb = xg + spr;
            while (xr < 0) xr += 240;
            while (xr >= 240) xr -= 240;
            while (xb < 0) xb += 240;
            while (xb >= 240) xb -= 240;
            for (x = 0; x < 240; x++) {
                out[x] = (uint16_t)((row[xr] & 0x00F8u) | (row[xg] & 0xE007u) | (row[xb] & 0x1F00u));
                if (++xg == 240) xg = 0;
                if (++xr == 240) xr = 0;
                if (++xb == 240) xb = 0;
            }
        }
    }
}
static struct clean_fx cfx;
static float clean_ang, clean_offx, clean_offy, clean_ophase;
static float clean_atan2(float y, float x)       /* (within a hundredth of a radian) */
{
    const float ax = fm_fabsf(x), ay = fm_fabsf(y), mx = fm_maxf(ax, ay), mn = fm_minf(ax, ay);
    float a, s, r;
    if (mx < 1e-6f)
        return 0.0f;
    a = mn / mx, s = a * a;
    r = ((-0.0464964749f * s + 0.15931422f) * s - 0.327622764f) * s * a + a;
    if (ay > ax) r = 1.57079637f - r;
    if (x < 0.0f) r = 3.14159274f - r;
    return y < 0.0f ? -r : r;
}
static void clean_frame_fx(void)                 /* (once a frame: the effects' settings, the map, all moving on) */
{
    const int zoom = vs_eff(ST_VXZOOM), rot = vs_eff(ST_VXROT), sh = vs_eff(ST_VXSHX), tgt = vs_get(ST_VXTGT),
              pulse = vs_eff(ST_VPULSE), fold = vs_eff(ST_VFOLD), flow = vs_eff(ST_VFLOW), lens = vs_eff(ST_MLENS),
              swirl = vs_eff(ST_MSWIRL), seg = vs_eff(ST_CSEG), feed = vs_eff(ST_VFEED), hue = vs_eff(ST_VHUE),
              sat = vs_eff(ST_CSAT);
    const int moving = tgt != 1 && (zoom || rot || sh);
    const float beat = vs_beat(), t = vs_clock();
    int i;
    if (!clean_tr[767]) {
        for (i = 0; i < 768; i++) {
            const int v = i - 256 < 0 ? 0 : i - 256 > 255 ? 255 : i - 256;
            clean_tr[i] = swap16((uint16_t)((v >> 3) << 11));
            clean_tg[i] = swap16((uint16_t)((v >> 2) << 5));
            clean_tb[i] = swap16((uint16_t)(v >> 3));
        }
        for (i = 0; i < 240; i++) {              /* (the colour's column: (x + 0.5) / 4 - 0.5, in 256ths) */
            const int fx = i * 64 - 96;
            clean_j0[i] = (uint8_t)(fx < 0 ? 0 : fx >> 8);
            clean_wx[i] = (uint8_t)(fx < 0 ? 0 : fx & 255);
        }
        for (i = 0; i < 256; i++)
            clean_sin[i] = (uint8_t)(127.5f + 127.5f * fm_sinf((float)i * (FM_TWO_PI / 256.0f)));
    }
    cfx.L = vs_lend(), cfx.cb = video_sharp + VLINK_HQ_LUMA, cfx.cr = cfx.cb + 3600;
    cfx.lut = vs_lut(), cfx.pal = vs_pal(), cfx.qx = clean_qx, cfx.j0 = clean_j0, cfx.wx = clean_wx;
    cfx.TR = clean_tr, cfx.TG = clean_tg, cfx.TB = clean_tb, cfx.age = clean_age, cfx.sin256 = clean_sin;
    cfx.pm = fold >= 40 ? 256 : fold * 256 / 40;
    cfx.neg = vs_eff(ST_VINVERT) >= 50;
    cfx.gain = 256 + (int)(120.0f * beat * (float)pulse * 0.01f);   /* (PULSE: the beat's flash) */
    cfx.mirror = vs_eff(ST_VMIRROR);
    cfx.ps = vs_pixel_size();
    for (i = 0; i < 240; i++)
        clean_qx[i] = (uint8_t)(cfx.ps > 1 ? i / (2 * cfx.ps) * (2 * cfx.ps) : i);
    cfx.style = CS_OFF, cfx.amt = 0;             /* (the styles: post_fx, on every picture) */
    cfx.halfc = 4 + cfx.amt / 10;
    for (i = 0; i < 240; i++)
        clean_hpos[i] = (uint8_t)(i % cfx.halfc);
    cfx.hpos = clean_hpos;
    cfx.blur = feed > 0 ? (int)((float)feed * (0.25f + 0.75f * beat) * 0.25f) : 0;   /* (1/256 of the way a step) */
    {                                            /* HUE turns the camera's colour above 10, SATURATION */
        static float hue_ang;
        const float s = 1.0f;                    /* (SATURATION: post_fx) */
        (void)sat;
        if (hue > 10)
            hue_ang += (float)(hue - 10) * 0.002f;
        cfx.hc = (int)(256.0f * s * fm_cosf(hue_ang)), cfx.hs = (int)(256.0f * s * fm_sinf(hue_ang));
    }
    {                                            /* LAYER Osc over the camera */
        const float an = (float)vs_eff(ST_VOANGLE) * (FM_PI / 100.0f),
                    fr = 1.0f + (float)vs_eff(ST_VOFREQ) * 0.4f;   /* (cycles across the screen) */
        cfx.osc = vs_get(ST_VLAYER) == VS_OSC + 1;
        cfx.blend = vs_get(ST_VBLEND), cfx.mix = vs_eff(ST_VMIX) * 256 / 100;
        cfx.oshape = vs_get(ST_VOSHAPE), cfx.ocomb = vs_get(ST_VOCOMB);
        cfx.ofreq2 = vs_eff(ST_VOFREQ2);
        cfx.oa = (int)(fm_cosf(an) * fr * 65536.0f * 256.0f / 240.0f);
        cfx.ob = (int)(fm_sinf(an) * fr * 65536.0f * 256.0f / 240.0f);
        clean_ophase += (float)vs_eff(ST_VOSPEED) * 0.08f;
        cfx.ring = 1 + vs_eff(ST_VOFREQ) / 4;
        cfx.ophase = (int32_t)(clean_ophase * 65536.0f);
    }
    if (cfx.style == CS_VHS) {                   /* VHS: the rows' wobble, the tracking band rolling down */
        static int band;
        for (i = 0; i < 240; i++)
            clean_jit[i] = (int8_t)(((int)(clean_rand() & 7u) - 3) * cfx.amt / 50);
        band = (band + 3) % 300;
        cfx.vhs_band = band - 30, cfx.vhs_h = 6 + cfx.amt / 6;
        for (i = cfx.vhs_band; i < cfx.vhs_band + cfx.vhs_h; i++)
            if (i >= 0 && i < 240)
                clean_jit[i] = (int8_t)(clean_jit[i] * 3 + 8);
        cfx.vhs_jit = clean_jit;
    }
    cfx.mapped = moving || flow || lens || swirl || seg;
    if (cfx.mapped) {                            /* the map: where each of 60 x 60 points comes from */
        int16_t *mu = (int16_t *)(void *)video_sharp, *mv = mu + 3600;
        const float s = fm_exp2f(-(float)zoom * 0.01f), an = (float)vs_eff(ST_VOANGLE) * (FM_PI / 100.0f),
                    fa = (float)flow * 0.08f * (0.5f + vs_level(2)), sw = (float)swirl * 0.03f, le = (float)lens * 0.008f;
        int gx, gy;
        float ca, sa;
        if (moving) {
            clean_ang += (float)rot * 0.0008f;
            clean_offx += (float)sh * 0.02f * fm_cosf(an), clean_offy += (float)sh * 0.02f * fm_sinf(an);
            while (clean_offx < 0.0f) clean_offx += 240.0f;
            while (clean_offx >= 240.0f) clean_offx -= 240.0f;
            while (clean_offy < 0.0f) clean_offy += 240.0f;
            while (clean_offy >= 240.0f) clean_offy -= 240.0f;
        }
        ca = (moving ? s : 1.0f) * fm_cosf(moving ? clean_ang : 0.0f);
        sa = (moving ? s : 1.0f) * fm_sinf(moving ? clean_ang : 0.0f);
        for (gy = 0; gy < 60; gy++)
            for (gx = 0; gx < 60; gx++) {
                float dx = (float)(gx * 4 + 2) - 120.0f, dy = (float)(gy * 4 + 2) - 120.0f, u, v;
                if (seg == 1) {                  /* SEGMENTS Tunnel: around and in, moving */
                    const float r = fm_sqrtf(dx * dx + dy * dy) + 4.0f, a = clean_atan2(dy, dx);
                    u = a * (240.0f / FM_TWO_PI) + 120.0f, v = 2400.0f / r + t * 20.0f;
                    dx = u - 120.0f, dy = v - 120.0f;
                } else if (seg >= 2) {           /* SEGMENTS 2 .. 12: a kaleidoscope, its wedge folded */
                    const float r = fm_sqrtf(dx * dx + dy * dy), w = FM_TWO_PI / (float)seg;
                    float a = clean_atan2(dy, dx) + FM_PI;
                    a -= w * fm_floorf(a / w);
                    if (a > w * 0.5f)
                        a = w - a;
                    dx = r * fm_cosf(a), dy = r * fm_sinf(a);
                }
                if (le != 0.0f || sw != 0.0f) {  /* LENS (pushed out, pulled in), SWIRL (turned, most in the middle) */
                    const float r2 = (dx * dx + dy * dy) * (1.0f / 14400.0f), k = 1.0f + le * (r2 - 0.4f);
                    const float rr = fm_minf(fm_sqrtf(r2), 1.0f), th = sw * (1.0f - rr) * (1.0f - rr);
                    const float c = fm_cosf(th), si = fm_sinf(th), nx = (dx * c - dy * si) * k, ny = (dx * si + dy * c) * k;
                    dx = nx, dy = ny;
                }
                u = 120.0f + dx * ca - dy * sa + (moving ? clean_offx : 0.0f);
                v = 120.0f + dx * sa + dy * ca + (moving ? clean_offy : 0.0f);
                if (fa != 0.0f) {                /* FLOW: a wave across and down */
                    u += fa * fm_sinf((float)gy * 0.21f + t * 1.3f);
                    v += fa * fm_cosf((float)gx * 0.21f - t * 1.1f);
                }
                mu[gy * 60 + gx] = (int16_t)fm_clampf(u * 16.0f, -32000.0f, 32000.0f);
                mv[gy * 60 + gx] = (int16_t)fm_clampf(v * 16.0f, -32000.0f, 32000.0f);
            }
        cfx.mu = mu, cfx.mv = mv;
    }
}
static void paint_clean_band(int y0, int h)
{
    static int16_t ru[61], rv[61], vb[61 + 240], vr[61 + 240];
    static uint8_t ly[240];
    static uint16_t row[240];
    int y;
    if (y0 == 0)
        clean_frame_fx();
    for (y = 0; y < h; y++) {
        int o, spr;
        vs_row_fx(y0 + y, &o, &spr);
        PLAT_FAR(clean_row)(&cfx, y0 + y, o, spr, ru, rv, vb, vr, ly, row, cv_px + y * 240);
    }
}
static volatile uint32_t video_ble_taken, video_ble_full;   /* (diagnostics: packets queued, refused: the queue full) */
static uint32_t video_ble_bad;                   /* (packets out of order: a frame lost) */
static uint8_t video_ble_shown_kind;
static uint32_t video_ble_part_ms;

int app_ble_load(struct plat_ble_rec *record) { (void)record; return 0; }
int app_ble_save(const struct plat_ble_rec *record) { (void)record; return 0; }

/* BLE IRQ writes to the queue. Frame assembly and rendering stay in the main loop. */
int app_ble_video_write(const uint8_t *data, uint16_t length)
{
    const uint8_t w = video_ble_write, next = (w + 1u) & (VIDEO_BLE_QUEUE - 1u);
    if (!video_ble_mode || length <= VIDEO_BLE_HEADER || length > sizeof video_ble_queue[w].data ||
        (data[0] & 0x7Fu) != video_ble_mode)
        return 0;
    if (next == video_ble_read) {
        video_ble_full++;
        return 0;
    }
    video_ble_taken++;
    memcpy(video_ble_queue[w].data, data, length);
    video_ble_queue[w].size = (uint8_t)length;
    __asm__ volatile("" ::: "memory");
    video_ble_write = next;
    return 1;
}
static void video_ble_poll(uint32_t now)
{
    const int visual = vs_get(ST_VMODE);
    const uint8_t mode = visual >= VS_NMODE ? (uint8_t)(visual - VS_NMODE + 1) : 0;
    unsigned budget = VIDEO_BLE_QUEUE;
    if (mode != video_ble_mode) {
        const int radio_change = (mode == 0) != (video_ble_mode == 0);
        video_ble_mode = mode;
        video_ble_read = video_ble_write;
        video_ble_kind = video_ble_shown_kind = 0;
        if (radio_change)
            plat_ble_enable(mode != 0);
    }
    while (budget-- && video_ble_read != video_ble_write) {
        const uint8_t r = video_ble_read;
        const uint8_t *data = video_ble_queue[r].data;
        const unsigned offset = (unsigned)data[2] | (unsigned)data[3] << 8;
        const unsigned count = video_ble_queue[r].size - VIDEO_BLE_HEADER;
        __asm__ volatile("" ::: "memory");
        if (!offset)                             /* (a frame's size: 60 x 60, or the code's first two bytes) */
            video_ble_size = data[0] & 0x80u ? (count >= 2 ? (unsigned)data[4] | (unsigned)data[5] << 8 : 0)
                                             : VLINK_VIDEO_SIDE * VLINK_VIDEO_SIDE;
        if ((data[0] & 0x7Fu) == mode && video_ble_size <= VLINK_CODEC_MAX && offset + count <= video_ble_size) {
            const unsigned size = video_ble_size;
            if (!offset) {
                video_ble_sharp = (uint8_t)(data[0] >> 7);
                video_ble_kind = mode;
                video_ble_id = data[1];
                video_ble_offset = 0;
            }
            if (offset && !(video_ble_kind == mode && data[1] == video_ble_id && offset == video_ble_offset))
                video_ble_bad++;
            if (video_ble_kind == mode && data[1] == video_ble_id && offset == video_ble_offset) {
                memcpy(video_ble_rx + offset, data + VIDEO_BLE_HEADER, count);
                video_ble_offset = (uint16_t)(offset + count);
                video_ble_part_ms = now;
                if (video_ble_offset == size && video_ble_sharp == data[0] >> 7) {
                    if (video_ble_sharp && size > 2 && (video_ble_rx[2] & 2u)) {
                        clean_on = 1;            /* (the engine idle from now: its planes the picture's) */
                        if (!codec_decode240(video_ble_rx, size))
                            video_ble_bad += 1000u;
                    } else if (video_ble_sharp) {
                        clean_off();
                        if (!codec_decode(video_ble_rx, size))
                            video_ble_bad += 1000u;
                    } else {
                        clean_off();
                        memcpy(video_ble_pixels[0], video_ble_rx, size);
                    }
                    video_ble_hq[video_ble_front ^ 1u] = video_ble_sharp;
                    video_ble_front ^= 1u;
                    /* the frame whole: the app may send the next (CC 105, its number; flow control: no queue
                     * piles up between the phone and the picture, so no delay) */
                    plat_midi_send(0x0Bu | 0xBFu << 8 | 105u << 16 | (uint32_t)(video_ble_id & 127u) << 24, TO_BLE_HOST);
                    video_ble_shown_kind = mode;
                    video_rx.frames++;
                }
            }
        }
        __asm__ volatile("" ::: "memory");
        video_ble_read = (r + 1u) & (VIDEO_BLE_QUEUE - 1u);
    }
}

static void mirror_cmd(const uint8_t *b, uint32_t n);
_Static_assert(VLINK_SYNC_MAX <= 384, "a SYNC fits the SysEx ring (384 bytes) whole: more settings, a SYNC in parts");
int app_sysex(const uint8_t *p, uint32_t n)      /* (the bytes between F0 and F7) */
{
    uint8_t b[8];
    if (n < 5 || p[0] != VLINK_MANU || p[1] != VLINK_ID0 || p[2] != VLINK_ID1)
        return 0;
    if (p[3] == VLINK_VIDEO) {
        receive_video(p + 4, n - 4);
        return 1;
    }
    if (p[3] != VLINK_CMD)
        return 0;
    if (vlink_unpack(p + 4, n - 4, b, sizeof b) < 1)
        return 1;
    lk.last_cmd_ms = plat_ms();
    if (b[0] >= VLINK_SET) {                     /* (the mirror's controls: main loop, as app_frame) */
        mirror_cmd(b, vlink_unpack(p + 4, n - 4, b, sizeof b));
        return 1;
    }
    if (b[0] == VLINK_OFF) {
        lk.on = 0, lk.rec = 0;
        video_rx.shown_kind = video_rx.loading_kind = 0;
    } else if (b[0] == VLINK_RESYNC || !lk.on) {
        lk.on = 1;
        lk.want_sync = 1;
    }
    return 1;
}

/* --------------------------------------------------------------------- UI --- */
static struct {
    uint32_t btn, keys, act_t, msg_until, frame_t, frame_us, frame_n, tap_t[4];
    uint32_t gen_us, paint_us, lcd_us, gap_ms, gap_max, vsp_us, wait_us;   /* (a frame's parts, averaged: generate, paint, the LCD's
                                                       * waits; the time between frames, its most) */
    char msg[2][24];
    uint8_t hold, mutate, strobe_key, hide_now, ntap;
    float phase;                         /* the tempo's beats (the clock's, else the taps') */
    uint32_t phase_t;
    uint8_t mkey;                        /* the MATRIX page's key (0 .. 10: 17 .. 27) */
    uint8_t map_on, map_cur, last_page;  /* the page map (SEL held, SELECT / ALGO turned); the page before (HOME) */
    uint8_t glo_used, home_done;         /* (GLO held: a knob reset; HOME held: the bars hidden) */
    uint32_t glo_t, home_t;
    uint8_t tslot;                       /* the TRIGGERS page's trigger (0 .. 2) */
    int8_t pend;                         /* a scene waiting for its beat or bar (-1 none) */
    int32_t beat_was;                    /* the beat the last frame was in */
    uint8_t morphing, chain_on;
    float m_t0, m_len;                   /* MORPH: from beat m_t0, m_len beats */
    int16_t m_from[ST_N], m_to[ST_N];
    int32_t chain_bar;                   /* CHAIN: the bar its scene began */
    uint16_t kheld, kon;                 /* the matrix's keys held; Toggles on */
    int16_t ksave[NMKEY];                /* what a Jump, a Toggle or a breaker put back */
    int8_t ksave_par[NMKEY];
} ui;

static void cat(char *d, const char *s)
{
    while (*d)
        d++;
    while (*s)
        *d++ = *s++;
    *d = 0;
}
void app_say(const char *a, const char *b)
{
    char t[52];
    cv_cpy(ui.msg[0], a, 24);
    cv_cpy(ui.msg[1], b, 24);
    ui.msg_until = plat_ms() + 1400u;
    if (ann_on()) {
        t[0] = 0;
        cat(t, ui.msg[0]);
        cat(t, " ");
        cat(t, ui.msg[1]);
        ann_say(t, 0, ANN_NOW);
    }
}
static void say_int(const char *a, int v)
{
    char t[12];
    vs_itoa(v, t);
    app_say(a, t);
}

static void scene_go(int i);
/* UNDO (ENV): the settings as they were before the last gestures (a look, a scene, the dice, a knob turned after a
 * pause, the mirror's), eight back */
#define NUNDO 8
static int16_t undo_par[NUNDO][ST_N];
static uint8_t undo_n, undo_w;
static uint32_t undo_knob_t;
static int8_t undo_knob = -100;
static void undo_mark(void)
{
    int k;
    for (k = 0; k < ST_N; k++)
        undo_par[undo_w % NUNDO][k] = (int16_t)vs_get(k);
    undo_w++;
    undo_n = (uint8_t)(undo_n < NUNDO ? undo_n + 1 : NUNDO);
}
static void undo_knob_mark(int k)                /* (a knob: once a gesture, a new one after 1.5 s or another knob) */
{
    if (k != undo_knob || plat_ms() - undo_knob_t > 1500u)
        undo_mark();
    undo_knob = (int8_t)k;
    undo_knob_t = plat_ms();
}
static void par_set(int k, int v)                /* a setting from the panel (the mirror gets it with the next frame) */
{
    vs_set(k, v);
    set.par[k] = (int16_t)vs_get(k);
    set_dirty = 1;
    if (k == ST_VGAIN)
        ear_gain = (0.5f / 32768.0f) * fm_exp2f((float)(set.par[k] - 50) * 0.08f);
}
static int knob_par(int i)                       /* the page's knob i: its setting (the React page: the slot's) */
{
    int k = PAGES[set.page].k[i];
    if (k >= ST_AR1S && cv_pre(PAGES[set.page].name, "REACT"))
        k += VS_SLOT_W * set.slot;
    if (k >= ST_TR1S && k < ST_TR1S + 3 && cv_pre(PAGES[set.page].name, "TRIGGERS"))
        k += 3 * ui.tslot;
    return k;
}
static void knob_text(int i, char *name, char *val)
{
    const int k = knob_par(i);
    char b[12];
    if (k == K_SLOT) {
        cv_cpy(name, "Slot", 16);
        vs_itoa(set.slot + 1, val);
    } else if (k == K_TEMPO) {
        cv_cpy(name, clk_ticks && plat_ms() - clk_last_ms < 2000u ? "Clock" : "Tempo", 16);
        vs_itoa(set.bpm, val);
    } else if (k == K_NONE) {
        cv_cpy(name, "Frame", 16);
        vs_itoa((int)(ui.frame_us / 1000u), val);
        cat(val, " ms");
    } else if (k == K_TSLOT) {
        cv_cpy(name, "Trigger", 16);
        vs_itoa(ui.tslot + 1, val);
    } else if (k == K_SCN) {
        cv_cpy(name, "Scene", 16);
        vs_itoa(set.scene + 1, val);
        if (!(scn.used >> set.scene & 1u))
            cat(val, " empty");
    } else if (k == K_SQ) {
        static const char *const SQ[3] = {"Now", "Beat", "Bar"};
        cv_cpy(name, "Launch", 16);
        cv_cpy(val, SQ[set.sq % 3u], 16);
    } else if (k == K_SMORPH) {
        cv_cpy(name, "Morph", 16);
        if (set.smorph)
            vs_itoa(set.smorph, val), cat(val, set.smorph == 1 ? " beat" : " beats");
        else
            cv_cpy(val, "Cut", 16);
    } else if (k == K_SCHAIN) {
        cv_cpy(name, "Chain", 16);
        if (set.schain)
            vs_itoa(set.schain, val), cat(val, set.schain == 1 ? " bar" : " bars"), cat(val, ui.chain_on ? "" : " (off)");
        else
            cv_cpy(val, "Off", 16);
    } else if (k == K_MKEY) {
        cv_cpy(name, "Key", 16);
        vs_itoa(17 + ui.mkey, val);
    } else if (k == K_MFN) {
        cv_cpy(name, "Does", 16);
        cv_cpy(val, MF_NAME[set.kfn[ui.mkey] % MF_N], 16);
    } else if (k == K_MPAR) {
        cv_cpy(name, "Setting", 16);
        cv_cpy(val, set.kfn[ui.mkey] >= MF_JUMP ? vs_par_info(set.kpar[ui.mkey])->name : "-", 16);
    } else if (k == K_MVAL) {
        cv_cpy(name, "To", 16);
        if (set.kfn[ui.mkey] >= MF_JUMP)
            cv_cpy(val, vs_par_text(set.kpar[ui.mkey], set.kval[ui.mkey], b), 16);
        else
            cv_cpy(val, "-", 16);
    } else {
        const char *nm = vs_par_info(k)->name;
        int j = 0;
        while (nm[j] && j < 11) {               /* ("Source 3" as "Source") */
            if (nm[j] == ' ' && nm[j + 1] >= '1' && nm[j + 1] <= '8' && !nm[j + 2] && k >= ST_TR1S)
                break;
            name[j] = nm[j];
            j++;
        }
        name[j] = 0;
        cv_cpy(val, vs_par_text(k, vs_get(k), b), 16);
    }
}
static void knob_turn(int i, int d)
{
    const int k = knob_par(i);
    char n[16], v[16];
    if (k == K_SLOT) {
        set.slot = (uint8_t)((set.slot + VS_NSLOT + (d > 0 ? 1 : -1)) % VS_NSLOT);
    } else if (k == K_TEMPO) {
        set.bpm = (uint16_t)clamp(set.bpm + d, 40, 240);
        set_dirty = 1;
    } else if (k == K_NONE) {
        return;
    } else if (k == K_TSLOT) {
        ui.tslot = (uint8_t)((ui.tslot + 3 + (d > 0 ? 1 : -1)) % 3);
    } else if (k == K_SCN) {
        set.scene = (uint8_t)((set.scene + NSCENE + (d > 0 ? 1 : -1)) % NSCENE);
        if (scn.used >> set.scene & 1u)
            scene_go(set.scene);
    } else if (k == K_SQ) {
        set.sq = (uint8_t)clamp(set.sq + (d > 0 ? 1 : -1), 0, 2);
    } else if (k == K_SMORPH) {
        set.smorph = (uint8_t)clamp(set.smorph + (d > 0 ? 1 : -1), 0, 16);
    } else if (k == K_SCHAIN) {
        set.schain = (uint8_t)clamp(set.schain + (d > 0 ? 1 : -1), 0, 16);
        ui.chain_on = set.schain != 0;
        ui.chain_bar = (int32_t)fm_floorf(ui.phase * 0.25f);
    } else if (k == K_MKEY) {
        ui.mkey = (uint8_t)((ui.mkey + NMKEY + (d > 0 ? 1 : -1)) % NMKEY);
    } else if (k == K_MFN) {
        set.kfn[ui.mkey] = (uint8_t)((set.kfn[ui.mkey] + MF_N + (d > 0 ? 1 : -1)) % MF_N);
        if (set.kfn[ui.mkey] >= MF_JUMP && set.kpar[ui.mkey] < ST_VMODE)
            set.kpar[ui.mkey] = ST_VFEED, set.kval[ui.mkey] = 100;
    } else if (k == K_MPAR) {
        if (set.kfn[ui.mkey] < MF_JUMP)
            return;
        set.kpar[ui.mkey] = (int8_t)(ST_VMODE + (set.kpar[ui.mkey] - ST_VMODE + (VS_TGT_LAST + 1) + (d > 0 ? 1 : -1)) %
                                                    (VS_TGT_LAST + 1));
        set.kval[ui.mkey] = (int16_t)vs_get(set.kpar[ui.mkey]);
    } else if (k == K_MVAL) {
        const vs_par_t *p;
        if (set.kfn[ui.mkey] < MF_JUMP)
            return;
        p = vs_par_info(set.kpar[ui.mkey]);
        set.kval[ui.mkey] = (int16_t)clamp(set.kval[ui.mkey] + (p->hi - p->lo >= 100 && (d > 1 || d < -1) ? 2 * d : d),
                                           p->lo, p->hi);
    } else {
        const vs_par_t *p = vs_par_info(k);
        const int span = p->hi - p->lo, fine = ui.btn >> B_SEL & 1u,
                  step = fine ? d : span >= 100 && (d > 1 || d < -1) ? 3 * d : span >= 100 ? 2 * d : d;
        undo_knob_mark(k);
        par_set(k, vs_get(k) + step);
    }
    if (k < 0 && k != K_SLOT && k != K_MKEY && k != K_TSLOT)
        set_dirty = 1;
    knob_text(i, n, v);
    app_say(n, v);
}
static void look_apply(int i)
{
    int k, j;
    undo_mark();
    for (k = ST_VMODE; k <= PICTURE_LAST; k++)
        par_set(k, vs_par_info(k)->def);
    for (j = 0; j < LOOKS[i].n; j++)
        par_set(LOOKS[i].k[j], LOOKS[i].v[j]);
    set.look = (uint8_t)i;
    app_say("LOOK", LOOKS[i].name);
}
static void scene_store(int i)
{
    int k;
    for (k = 0; k < ST_N; k++)
        scn.par[i][k] = (uint8_t)(vs_get(k) - vs_par_info(k)->lo);
    scn.used |= 1u << i;
    scn_dirty = 1;
    set.scene = (uint8_t)i;
    say_int("STORED SCENE", i + 1);
}
static int scene_par(int k)                      /* a setting a scene holds: the picture's and the react slots (not the
                                                  * ear's, not the screen's) */
{
    return (k >= ST_VMODE && k <= PICTURE_LAST) || k >= ST_AR1S;
}
static void scene_start(int i)                   /* now: cut to it, or MORPH towards it */
{
    int k;
    undo_mark();
    ui.pend = -1;
    set.scene = (uint8_t)i;
    if (!set.smorph) {
        ui.morphing = 0;
        for (k = 0; k < ST_N; k++)
            if (scene_par(k))
                par_set(k, scn.par[i][k] + vs_par_info(k)->lo);
    } else {                                     /* (the switches at once; every range on its way, set.smorph beats) */
        for (k = 0; k < ST_N; k++) {
            const vs_par_t *p = vs_par_info(k);
            ui.m_from[k] = (int16_t)vs_get(k);
            ui.m_to[k] = (int16_t)(scn.par[i][k] + vs_par_info(k)->lo);
            if (scene_par(k) && p->hi - p->lo < 10)
                par_set(k, ui.m_to[k]);
        }
        ui.morphing = 1;
        ui.m_t0 = ui.phase;
        ui.m_len = (float)set.smorph;
    }
    say_int(ui.morphing ? "MORPH TO SCENE" : "SCENE", i + 1);
}
static void scene_go(int i)                      /* a scene asked for: at once, or on the next beat or bar (LAUNCH) */
{
    if (!(scn.used >> i & 1u)) {
        say_int("EMPTY SCENE", i + 1);
        return;
    }
    if (set.sq == 0) {
        scene_start(i);
        return;
    }
    ui.pend = (int8_t)i;
    say_int(set.sq == 1 ? "NEXT BEAT: SCENE" : "NEXT BAR: SCENE", i + 1);
}
static void scene_recall(int i) { scene_go(i); }
static int scene_next(int from, int dir)         /* the next stored scene that way (from itself if none other) */
{
    int k, i = from;
    for (k = 0; k < NSCENE; k++) {
        i = (i + NSCENE + dir) % NSCENE;
        if (scn.used >> i & 1u)
            return i;
    }
    return from;
}
static void scenes_frame(void)                   /* a frame: the beat's and the bar's launches, MORPH, CHAIN */
{
    const int32_t beat = (int32_t)fm_floorf(ui.phase), bar = (int32_t)fm_floorf(ui.phase * 0.25f);
    const int on_beat = beat != ui.beat_was, on_bar = on_beat && (beat & 3) == 0;
    int k;
    ui.beat_was = beat;
    if (ui.pend >= 0 && ((set.sq == 1 && on_beat) || (set.sq == 2 && on_bar)))
        scene_start(ui.pend);
    if (ui.chain_on && set.schain && on_bar && bar - ui.chain_bar >= set.schain) {
        ui.chain_bar = bar;
        if (scn.used & (scn.used - 1u))          /* (two scenes at least) */
            scene_start(scene_next(set.scene, 1));
    }
    if (ui.morphing) {
        float t = (ui.phase - ui.m_t0) / ui.m_len;
        t = t < 0.0f ? 0.0f : t > 1.0f ? 1.0f : t;
        t = t * t * (3.0f - 2.0f * t);           /* (eased at both ends) */
        for (k = 0; k < ST_N; k++)
            if (scene_par(k) && ui.m_from[k] != ui.m_to[k]) {
                const int v = ui.m_from[k] + (int)((float)(ui.m_to[k] - ui.m_from[k]) * t + (ui.m_to[k] > ui.m_from[k] ? 0.5f : -0.5f));
                if (v != vs_get(k))
                    par_set(k, v);
            }
        if (t >= 1.0f)
            ui.morphing = 0;
    }
}
static uint32_t dice_r = 0x9E3779B9u;
static int dice(int lo, int hi)
{
    dice_r ^= dice_r << 13, dice_r ^= dice_r >> 17, dice_r ^= dice_r << 5;
    return lo + (int)((dice_r >> 8) % (uint32_t)(hi - lo + 1));
}
static void dice_page(void)                      /* GLO: the page's settings rolled (the switches too, within reason) */
{
    int i;
    undo_mark();
    for (i = 0; i < 4; i++) {
        const int k = knob_par(i);
        const vs_par_t *p;
        if (k < 0 || k == ST_VBARS || k == ST_VDELAY || k == ST_VGAIN || k == ST_VBRIGHT)
            continue;
        p = vs_par_info(k);
        par_set(k, dice(p->lo, p->hi > 100 ? 100 : p->hi));
    }
    app_say("DICE", PAGES[set.page].name);
}
static void page_go(int d)
{
    char t[40], n[16], v[16];
    int i;
    if (d % NPAGE)
        ui.last_page = set.page;
    set.page = (uint8_t)((set.page + NPAGE + d) % NPAGE);
    t[0] = 0;
    for (i = 0; i < 4; i++) {                    /* (a screen reader hears the page whole) */
        knob_text(i, n, v);
        if (i)
            cat(t, ", ");
        cat(t, v);
    }
    app_say(PAGES[set.page].name, t);
}
static void map_move(int d, int by_group)        /* the page map's cursor: a page, or a group's first page */
{
    if (!ui.map_on)
        ui.map_on = 1, ui.map_cur = set.page;
    if (by_group) {
        int g = (PAGES[ui.map_cur].group + GR_N + d) % GR_N, p;
        for (p = 0; p < NPAGE && PAGES[p].group != g; p++)
            ;
        ui.map_cur = (uint8_t)p;
    } else {
        ui.map_cur = (uint8_t)((ui.map_cur + NPAGE + d) % NPAGE);
    }
}
static void undo(void)
{
    int k;
    if (!undo_n) {
        app_say("UNDO", "NOTHING TO UNDO");
        return;
    }
    undo_w--, undo_n--;
    ui.morphing = 0;
    for (k = 0; k < ST_N; k++)
        if (vs_get(k) != undo_par[undo_w % NUNDO][k])
            par_set(k, undo_par[undo_w % NUNDO][k]);
    undo_knob = -100;
    say_int("UNDO", undo_n);
}
/* the iPhone app's remote (Live Video, Video import): Control Changes on channel 16, over USB and Bluetooth MIDI;
 * the app answers 102 (its recording on / off), 103 (a photo), 104 (its camera or clip started / stopped) */
enum { PHONE_REC = 102, PHONE_PHOTO = 103, PHONE_RUN = 104 };
static void phone_cmd(int cc)
{
    plat_midi_send(0x0Bu | 0xBFu << 8 | (uint32_t)cc << 16 | 127u << 24, TO_USB | TO_BLE);
}
static int video_mode(void) { return vs_get(ST_VMODE) >= VS_NMODE; }

/* the photos kept on the FM-1: the 60 x 60 picture as it came (RGB332), one a sector of PLAT_XDATA0 (four), the
 * oldest written over; a sector: "CVP1", its number (the newest the highest), the pixels */
#define PHOTO_BASE PLAT_XDATA0_LO
#define PHOTO_N 4
#define PHOTO_MAGIC 0x31505643u
#define PHOTO_PIX (VLINK_VIDEO_SIDE * VLINK_VIDEO_SIDE)
_Static_assert(8 + PHOTO_PIX <= 0xF00 && PHOTO_BASE + PHOTO_N * 0x1000u <= PLAT_XDATA0_HI, "photos in XDATA0");
static uint8_t photo_view;                       /* 0: the live picture; 1 .. : a kept photo (the newest first) */
static const uint8_t *photo_at(int slot, uint32_t *seq)
{
    const uint8_t *f = plat_data(PHOTO_BASE + (uint32_t)slot * 0x1000u);
    uint32_t m, s;
    if (!f)
        return 0;
    memcpy(&m, f, 4), memcpy(&s, f + 4, 4);
    if (m != PHOTO_MAGIC)
        return 0;
    if (seq)
        *seq = s;
    return f + 8;
}
static int photo_count(void)
{
    int k, n = 0;
    for (k = 0; k < PHOTO_N; k++)
        n += photo_at(k, 0) != 0;
    return n;
}
static const uint8_t *photo_nth(int nth)         /* (0: the newest) */
{
    int order[PHOTO_N], n = 0, k, j;
    uint32_t seq[PHOTO_N];
    for (k = 0; k < PHOTO_N; k++)
        if (photo_at(k, &seq[n]))
            order[n++] = k;
    for (k = 1; k < n; k++)                      /* (newest first) */
        for (j = k; j > 0 && seq[j] > seq[j - 1]; j--) {
            const uint32_t s = seq[j]; const int o = order[j];
            seq[j] = seq[j - 1], order[j] = order[j - 1];
            seq[j - 1] = s, order[j - 1] = o;
        }
    return nth < n ? photo_at(order[nth], 0) : 0;
}
static int photo_keep(const uint8_t *pix)
{
    static uint8_t buf[8 + PHOTO_PIX] PLAT_POOL;
    uint32_t best = 0xFFFFFFFFu, top = 0, s, m = PHOTO_MAGIC;
    int k, slot = 0;
    for (k = 0; k < PHOTO_N; k++) {
        if (!photo_at(k, &s)) {
            if (best) slot = k, best = 0;
            continue;
        }
        if (s > top) top = s;
        if (s < best) slot = k, best = s;
    }
    memcpy(buf, &m, 4);
    s = top + 1;
    memcpy(buf + 4, &s, 4);
    memcpy(buf + 8, pix, PHOTO_PIX);
    return plat_data_erase(PHOTO_BASE + (uint32_t)slot * 0x1000u) == 0 &&
           plat_data_write(PHOTO_BASE + (uint32_t)slot * 0x1000u, buf, sizeof buf) == 0;
}
static const uint8_t *live_image(void);
/* EDIT: a photo of the FM-1's picture (the effects in it), taken as the next frame is painted: 60 x 60 kept on the
 * FM-1, 120 x 120 sent to the iPhone app (VLINK_PHOTO, a frame a row) over USB and over Bluetooth (its stream) */
static uint8_t photo_req, photo_id;
static uint8_t photo_frames[VLINK_PHOTO_SIDE][VLINK_PHOTO_MAX] PLAT_POOL;
static uint8_t photo_small[PHOTO_PIX] PLAT_POOL;
static uint16_t photo_len;
static uint8_t photo_usb = VLINK_PHOTO_SIDE;
static volatile uint8_t photo_ble = VLINK_PHOTO_SIDE;
static volatile uint16_t photo_off;
static int photo_ble_peek(uint32_t *pkt)
{
    const uint8_t r = photo_ble;
    const uint8_t *f;
    unsigned n;
    if (r >= VLINK_PHOTO_SIDE)
        return 0;
    f = photo_frames[r] + photo_off;
    n = photo_len - photo_off;
    if (n > 3)
        *pkt = 0x04u | (uint32_t)f[0] << 8 | (uint32_t)f[1] << 16 | (uint32_t)f[2] << 24;
    else if (n == 3)
        *pkt = 0x07u | (uint32_t)f[0] << 8 | (uint32_t)f[1] << 16 | (uint32_t)f[2] << 24;
    else if (n == 2)
        *pkt = 0x06u | (uint32_t)f[0] << 8 | (uint32_t)f[1] << 16;
    else
        *pkt = 0x05u | (uint32_t)f[0] << 8;
    return 1;
}
static void photo_ble_pop(void)
{
    const unsigned n = photo_len - photo_off;
    if (photo_ble >= VLINK_PHOTO_SIDE)
        return;
    if (n > 3) {
        photo_off = (uint16_t)(photo_off + 3);
    } else {
        photo_off = 0;
        photo_ble = (uint8_t)(photo_ble + 1);
    }
}
static int photo_ble_mid(void) { return photo_ble < VLINK_PHOTO_SIDE && photo_off > 0; }
static const struct plat_ble_stream photo_stream = {photo_ble_peek, photo_ble_pop, photo_ble_mid};
static void phone_photo(void)
{
    if (!live_image()) {
        app_say("PHOTO", "NO PICTURE YET");
        return;
    }
    photo_req = 1;
}
static void photo_row(int y, const uint16_t *px)  /* (a screen row painted: its share of the photo) */
{
    if (!(y & 1)) {
        uint8_t raw[VLINK_PHOTO_RAW];
        int x;
        raw[0] = photo_id, raw[1] = (uint8_t)(y >> 1);
        for (x = 0; x < VLINK_PHOTO_SIDE; x++) {
            const uint16_t c = px[2 * x];        /* (the screen's: high byte first in memory) */
            memcpy(raw + 2 + 2 * x, &c, 2);
        }
        photo_len = (uint16_t)vlink_frame(VLINK_PHOTO, raw, sizeof raw, photo_frames[y >> 1]);
    }
    if (!(y & 3)) {
        int x;
        for (x = 0; x < 60; x++) {
            const uint32_t c = swap16(px[4 * x]);
            photo_small[(y >> 2) * 60 + x] = (uint8_t)((c >> 8 & 0xE0u) | (c >> 6 & 0x1Cu) | (c >> 3 & 0x03u));
        }
    }
}
static void photo_done(void)
{
    const int kept = photo_keep(photo_small);
    photo_usb = 0;
    photo_off = 0;
    __asm__ volatile("" ::: "memory");
    photo_ble = 0;
    photo_id = (uint8_t)((photo_id + 1) & 127u);
    app_say("PHOTO", kept ? "FM-1 + IPHONE" : "SENT, NOT KEPT");
}
static void photo_send(void)                     /* (the main loop: the rows to USB as the ring takes them) */
{
    while (photo_usb < VLINK_PHOTO_SIDE && plat_sysex_room() >= photo_len)
        if (plat_sysex_send(photo_frames[photo_usb++], photo_len) < 0)
            photo_usb = VLINK_PHOTO_SIDE;
}
static void photo_next(void)                     /* FX in the video modes: the kept photos, then live again */
{
    const int n = photo_count();
    char t[16] = "PHOTO ";
    photo_view = (uint8_t)(n ? (photo_view + 1) % (n + 1) : 0);
    if (!photo_view) {
        app_say("LIVE", n ? "FX: THE PHOTOS" : "NO PHOTOS KEPT");
        return;
    }
    t[6] = (char)('0' + photo_view), t[7] = 0;
    cat(t, " OF ");
    { char d[2] = {(char)('0' + n), 0}; cat(t, d); }
    app_say(t, "FX: NEXT");
}

static void rec_toggle(void)
{
    if (video_mode()) {                          /* the iPhone records (and the mirror, if it is open) */
        phone_cmd(PHONE_REC);
        if (lk.on) {
            lk.rec = (uint8_t)!lk.rec;
            lk.rec_ms = plat_ms();
        }
        app_say("REC", lk.on ? (lk.rec ? "IPHONE + PC ON" : "IPHONE + PC OFF") : "IPHONE");
        return;
    }
    if (!lk.on) {
        app_say("NO MIRROR", "OPEN IT ON THE PC");
        return;
    }
    lk.rec = (uint8_t)!lk.rec;
    lk.rec_ms = plat_ms();
    app_say("REC", lk.rec ? "ON THE PC" : "STOPPED");
}
static void save_all(void)
{
    int ok = plat_store_save(OBJ_SET, &set, sizeof set) == 0;
    if (ok && scn_dirty)
        ok = plat_store_save(OBJ_SCENES, &scn, sizeof scn) == 0;
    if (ok)
        set_dirty = scn_dirty = 0;
    app_say("SAVE", ok ? "DONE" : "FAILED");
}
static void tap(void)                            /* FX: the tempo by hand (four taps' average) */
{
    const uint32_t now = plat_ms();
    if (ui.ntap && now - ui.tap_t[(ui.ntap - 1) & 3] > 2000u)
        ui.ntap = 0;
    ui.tap_t[ui.ntap & 3] = now;
    ui.ntap++;
    if (ui.ntap >= 2) {
        const int m = ui.ntap < 4 ? ui.ntap : 4;
        const uint32_t span = ui.tap_t[(ui.ntap - 1) & 3] - ui.tap_t[(ui.ntap - m) & 3];
        if (span)
            set.bpm = (uint16_t)clamp((int)(60000u * (uint32_t)(m - 1) / span), 40, 240);
        ui.phase = (float)(int)(ui.phase + 0.5f);   /* (the tap is a beat) */
    }
    say_int("TEMPO", set.bpm);
}
/* the matrix (keys 17 .. 27): what a key does (set.kfn), pressed and let go. The breakers and Jump hold their setting
 * while the key is down and put it back after; Toggle on one press, back on the next. Held, a key learns: turn a knob
 * while it is down and it becomes a Jump to where the knob went (the knob's setting comes back when it is let go) */
static void mk_hold(int m, int par, int val)     /* the setting par to val while key m is down (or Toggle on) */
{
    ui.ksave_par[m] = (int8_t)par;
    ui.ksave[m] = (int16_t)vs_get(par);
    par_set(par, val);
}
static void mk_release(int m)                    /* ... and back */
{
    if (ui.ksave_par[m] >= 0) {
        par_set(ui.ksave_par[m], ui.ksave[m]);
        ui.ksave_par[m] = -1;
    }
}
static void mk_down(int m)
{
    char t[16];
    switch (set.kfn[m]) {
    case MF_NUDGE: ui.mutate = 1; app_say("MUTATE", "A NUDGE"); break;
    case MF_THROW: ui.mutate = 2; app_say("MUTATE", "A THROW"); break;
    case MF_NEW: ui.mutate = 3; app_say("VISUAL", "NEW STRUCTURE"); break;
    case MF_HOLD: ui.hold = (uint8_t)!ui.hold; app_say("STRUCTURE", ui.hold ? "HELD" : "FREE"); break;
    case MF_STROBE: ui.strobe_key = 1; break;
    case MF_INVERT: mk_hold(m, ST_VINVERT, 100); break;
    case MF_MOSH: mk_hold(m, ST_VMOSH, 90); break;
    case MF_FREEZE: mk_hold(m, ST_VTRAIL, 100); break;
    case MF_BLACK: mk_hold(m, ST_VBRIGHT, 0); break;
    case MF_NVIS: par_set(ST_VMODE, (vs_get(ST_VMODE) + 1) % VS_NVISUAL); app_say("VISUAL", VS_MODE_NAME[vs_get(ST_VMODE)]); break;
    case MF_NOBJ: par_set(ST_OFORM, (vs_get(ST_OFORM) + 1) % 6); app_say("FORM", vs_par_text(ST_OFORM, vs_get(ST_OFORM), t)); break;
    case MF_NPAL: par_set(ST_VPAL, (vs_get(ST_VPAL) + 1) % VS_NPAL); app_say("COLOUR", vs_par_text(ST_VPAL, vs_get(ST_VPAL), t)); break;
    case MF_NLOOK: look_apply((set.look + 1) % NLOOK); break;
    case MF_PLOOK: look_apply((set.look + NLOOK - 1) % NLOOK); break;
    case MF_NSCENE: scene_go(scene_next(set.scene, 1)); break;
    case MF_PSCENE: scene_go(scene_next(set.scene, -1)); break;
    case MF_RSCENE: scene_go(scene_next(set.scene, 1 + dice(0, NSCENE - 2))); break;
    case MF_TAP: tap(); break;
    case MF_CHAIN:
        ui.chain_on = (uint8_t)(!ui.chain_on && set.schain);
        ui.chain_bar = (int32_t)fm_floorf(ui.phase * 0.25f);
        app_say("CHAIN", ui.chain_on ? "ON" : set.schain ? "OFF" : "SET ITS BARS (SCENES)");
        break;
    case MF_JUMP: mk_hold(m, set.kpar[m], set.kval[m]); break;
    case MF_TOGGLE:
        if (ui.kon >> m & 1u) {
            mk_release(m);
            ui.kon &= (uint16_t)~(1u << m);
        } else {
            mk_hold(m, set.kpar[m], set.kval[m]);
            ui.kon |= (uint16_t)(1u << m);
        }
        app_say(vs_par_info(set.kpar[m])->name, ui.kon >> m & 1u ? "ON" : "OFF");
        break;
    default: break;
    }
}
static void mk_up(int m)
{
    const int f = set.kfn[m];
    if (f == MF_STROBE)
        ui.strobe_key = 0;
    else if (f != MF_TOGGLE)
        mk_release(m);
}
static void mk_learn(int m, int knob, int d)     /* a knob turned with key m held: the key a Jump to there */
{
    const int k = knob_par(knob);
    char t[16];
    if (k < 0)
        return;
    if (set.kfn[m] != MF_JUMP || set.kpar[m] != k) {   /* (a new setting: the old one back, this one held now) */
        mk_release(m);
        set.kfn[m] = MF_JUMP;
        set.kpar[m] = (int8_t)k;
        ui.ksave_par[m] = (int8_t)k;
        ui.ksave[m] = (int16_t)vs_get(k);
    }
    {
        const vs_par_t *p = vs_par_info(k);
        par_set(k, vs_get(k) + (p->hi - p->lo >= 100 && (d > 1 || d < -1) ? 2 * d : d));
    }
    set.kval[m] = (int16_t)vs_get(k);
    set_dirty = 1;
    vs_itoa(17 + m, t);
    cat(t, ": JUMP");
    app_say(t, vs_par_info(k)->name);
}
static void key_down(int k)
{
    if (k < NSCENE) {                            /* the first sixteen keys: scenes (SEL held: stored) */
        if (ui.btn >> B_SEL & 1u)
            scene_store(k);
        else
            scene_go(k);
        return;
    }
    if (k - NSCENE < NMKEY) {
        ui.kheld |= (uint16_t)(1u << (k - NSCENE));
        mk_down(k - NSCENE);
    }
}

static void input(void)
{
    const uint32_t btn = plat_buttons(), keys = plat_keys(), now = plat_ms();
    uint32_t k, bit;
    int32_t e;
    for (k = 0; k < NB; k++) {
        bit = 1u << k;
        if (!(btn & bit) || (ui.btn & bit))
            continue;
        ui.act_t = now;
        switch (k) {
        case B_REC: rec_toggle(); break;
        case B_PLAY:
            if (video_mode()) {
                phone_cmd(PHONE_RUN);
                app_say("VIDEO", "IPHONE: START / STOP");
                break;
            }
            ui.hold = (uint8_t)!ui.hold; app_say("STRUCTURE", ui.hold ? "HELD" : "FREE"); break;
        case B_EDIT:
            if (video_mode()) {
                phone_photo();
                break;
            }
            ui.mutate = 3; app_say("VISUAL", "NEW STRUCTURE"); break;
        case B_LFO: ui.mutate = 1; app_say("MUTATE", "A NUDGE"); break;
        case B_ENV: undo(); break;
        case B_GLO: ui.glo_t = now, ui.glo_used = 0; break;
        case B_SAVE: save_all(); break;
        case B_FX: if (video_mode()) photo_next(); else tap(); break;
        case B_HOME: ui.home_t = now, ui.home_done = 0; break;
        case B_OCTDN: page_go(-1); break;
        case B_OCTUP: page_go(1); break;
        case B_ARP: look_apply((set.look + NLOOK - 1) % NLOOK); break;
        case B_SEQ: look_apply((set.look + 1) % NLOOK); break;
        default: break;
        }
    }
    if (!(btn >> B_GLO & 1u) && (ui.btn >> B_GLO & 1u) && !ui.glo_used)
        dice_page();                             /* GLO let go, no knob reset meanwhile: the dice */
    if ((btn >> B_HOME & 1u) && !ui.home_done && now - ui.home_t >= 500u) {
        ui.hide_now = (uint8_t)!ui.hide_now;     /* HOME held: the bars hidden or shown */
        ui.home_done = 1;
    }
    if (!(btn >> B_HOME & 1u) && (ui.btn >> B_HOME & 1u) && !ui.home_done) {
        const int was = set.page;                /* HOME tapped: the page before, and back */
        page_go(ui.last_page - set.page);
        ui.last_page = (uint8_t)was;
    }
    if (!(btn >> B_SEL & 1u) && (ui.btn >> B_SEL & 1u) && ui.map_on) {
        ui.map_on = 0;                           /* SEL let go: to the page the map points at */
        if (ui.map_cur != set.page)
            page_go(ui.map_cur - set.page);
    }
    for (k = 0; k < NKEYS; k++) {
        bit = 1u << k;
        if ((keys & bit) && !(ui.keys & bit)) {
            ui.act_t = now;
            ui.map_on = 0;                       /* (SEL + a key stores a scene: no map) */
            key_down((int)k);
        }
    }
    for (k = NSCENE; k < NSCENE + NMKEY; k++)  /* the matrix's keys let go */
        if (!(keys >> k & 1u) && (ui.keys >> k & 1u)) {
            ui.kheld &= (uint16_t)~(1u << (k - NSCENE));
            mk_up((int)(k - NSCENE));
        }
    for (k = 0; k < NE; k++) {
        if ((e = plat_enc((int)k)) == 0)
            continue;
        ui.act_t = now;
        if (ui.kheld && k >= EN_K1) {            /* (a matrix key held: it learns this knob) */
            int m = 0;
            while (!(ui.kheld >> m & 1u))
                m++;
            mk_learn(m, (int)(k - EN_K1), (int)e);
        } else if ((btn >> B_GLO & 1u) && k >= EN_K1) {   /* GLO + a knob: back to its start */
            const int p = knob_par((int)(k - EN_K1));
            ui.glo_used = 1;
            if (p >= 0) {
                char t[16];
                undo_knob_mark(p);
                par_set(p, vs_par_info(p)->def);
                app_say(vs_par_info(p)->name, vs_par_text(p, vs_get(p), t));
            }
        } else if ((k == EN_SELECT || k == EN_ALGO) && (btn >> B_SEL & 1u))
            map_move(e > 0 ? 1 : -1, k == EN_ALGO);   /* (SEL held: the page map, SELECT a page, ALGO a group) */
        else if (k == EN_SELECT)
            page_go(e > 0 ? 1 : -1);
        else if (k == EN_PRESET)
            look_apply((set.look + NLOOK + (e > 0 ? 1 : -1)) % NLOOK);
        else if (k == EN_ALGO)
            scene_recall((set.scene + NSCENE + (e > 0 ? 1 : -1)) % NSCENE);
        else
            knob_turn((int)(k - EN_K1), (int)e);
    }
    if (kq_has) {                                /* CC 14 .. 17: the page's knobs, across their ranges */
        const uint8_t h = kq_has;
        kq_has = 0;
        for (k = 0; k < 4; k++)
            if (h >> k & 1u) {
                const int p = knob_par((int)k);
                if (p >= 0) {
                    const vs_par_t *i = vs_par_info(p);
                    par_set(p, i->lo + (kq_cc[k] * (i->hi - i->lo) + 63) / 127);
                }
            }
    }
    if (pc_q != 255) {
        scene_recall(pc_q % NSCENE);
        pc_q = 255;
    }
    ui.btn = btn;
    ui.keys = keys;
    {
        uint32_t lit = (lk.rec ? 1u << B_REC : 0u) | (ui.hold ? 1u << B_PLAY : 0u) |
                       (set_dirty || scn_dirty ? 1u << B_SAVE : 0u), klit = scn.used & 0xFFFFu;
        if ((now / 250u) & 1u && lk.rec)
            lit &= ~(1u << B_REC);               /* (REC blinks while recording) */
        if (ui.pend >= 0 && (now / 125u) & 1u)   /* (a scene waiting for its beat: blinking) */
            klit ^= 1u << ui.pend;
        for (k = 0; k < NMKEY; k++)              /* (the matrix: what is on, lit) */
            if ((set.kfn[k] == MF_HOLD && ui.hold) || (set.kfn[k] == MF_TOGGLE && (ui.kon >> k & 1u)) ||
                (set.kfn[k] == MF_CHAIN && ui.chain_on))
                klit |= 1u << (NSCENE + k);
        plat_glow(1);
        plat_leds(btn | lit, keys | klit);
    }
}
static void video_ble_poll(uint32_t now);
void app_input_only(void) { input(); video_ble_poll(plat_ms()); }   /* (the video between frames too: answered sooner) */

/* ----------------------------------------------------------- the spectrum --- */
/* a frame's look at the last 256 samples (11 kHz: 43 Hz a bin, up to 5.5 kHz): a Hann window, an FFT, 24 bands log
 * spaced (in dB, -60 .. 0 dB to 0 .. 255); the drums by their bands' rise (kick: the lows, snare: the middle, hat: the
 * top and the air above 5 kHz), each held off a while after it hits; the brightness (the spectrum's centroid) */
#define FFT_N 256
static float fft_re[FFT_N] PLAT_POOL, fft_im[FFT_N] PLAT_POOL, fft_win[FFT_N] PLAT_POOL, fft_cs[FFT_N / 2] PLAT_POOL,
             fft_sn[FFT_N / 2] PLAT_POOL;
static uint8_t fft_edge[VS_NSPEC + 1], fft_ok;
static float dr_prev[3], dr_avg[3], dr_env[3];
static uint32_t dr_t[3];
static void fft_init(void)
{
    int i;
    for (i = 0; i < FFT_N; i++)
        fft_win[i] = 0.5f - 0.5f * fm_cosf(FM_TWO_PI * (float)i / (float)FFT_N);
    for (i = 0; i < FFT_N / 2; i++)
        fft_cs[i] = fm_cosf(FM_TWO_PI * (float)i / (float)FFT_N), fft_sn[i] = -fm_sinf(FM_TWO_PI * (float)i / (float)FFT_N);
    for (i = 0; i <= VS_NSPEC; i++) {            /* (bin 1 .. 127, log spaced, each band a bin wider at least) */
        int e = (int)(fm_exp2f((float)i / (float)VS_NSPEC * 6.99f) + 0.5f);
        if (i && e <= fft_edge[i - 1])
            e = fft_edge[i - 1] + 1;
        fft_edge[i] = (uint8_t)(e > 127 ? 127 : e);
    }
    fft_ok = 1;
}
static void fft_run(void)                        /* in place, radix 2 */
{
    int i, j = 0, len;
    for (i = 1; i < FFT_N; i++) {                /* (bit reversal) */
        int bit = FFT_N >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j |= bit;
        if (i < j) {
            float t = fft_re[i];
            fft_re[i] = fft_re[j], fft_re[j] = t;
            t = fft_im[i], fft_im[i] = fft_im[j], fft_im[j] = t;
        }
    }
    for (len = 2; len <= FFT_N; len <<= 1) {
        const int half = len >> 1, step = FFT_N / len;
        for (i = 0; i < FFT_N; i += len)
            for (j = 0; j < half; j++) {
                const float wr = fft_cs[j * step], wi = fft_sn[j * step];
                float *ar = &fft_re[i + j], *ai = &fft_im[i + j], *br = &fft_re[i + j + half], *bi = &fft_im[i + j + half];
                const float tr = *br * wr - *bi * wi, ti = *br * wi + *bi * wr;
                *br = *ar - tr, *bi = *ai - ti;
                *ar += tr, *ai += ti;
            }
    }
}
static void ear_spectrum(vs_in_t *in)
{
    const uint32_t w = ear_ring_w, now = plat_ms();
    float mag[FFT_N / 2], cs = 0.0f, ct = 0.0f, grp[3] = {0.0f, 0.0f, 0.0f};
    int i, b;
    if (!fft_ok)
        fft_init();
    for (i = 0; i < FFT_N; i++) {
        fft_re[i] = ear_ring[(w - FFT_N + (uint32_t)i) & 511u] * fft_win[i];
        fft_im[i] = 0.0f;
    }
    fft_run();
    for (i = 1; i < FFT_N / 2; i++) {
        mag[i] = fm_sqrtf(fft_re[i] * fft_re[i] + fft_im[i] * fft_im[i]) * (1.0f / 64.0f);   /* (a sine's amplitude:
                                                  * N / 2, the window's half) */
        cs += mag[i] * (float)i, ct += mag[i];
    }
    for (b = 0; b < VS_NSPEC; b++) {
        float m = 0.0f, db;
        for (i = fft_edge[b]; i < fft_edge[b + 1] || i == fft_edge[b]; i++)
            m = fm_maxf(m, mag[i]);
        db = 20.0f * fm_log2f(m + 1e-6f) * 0.30103f;   /* (log10 through log2) */
        in->spec[b] = (uint8_t)fm_clampf((db + 60.0f) * (255.0f / 60.0f), 0.0f, 255.0f);
        grp[b < 6 ? 0 : b < 18 ? 1 : 2] += (float)in->spec[b] * (b < 6 ? 1.0f / 6.0f : b < 18 ? 1.0f / 12.0f : 1.0f / 6.0f);
    }
    grp[2] = grp[2] * 0.6f + fm_minf(ear_band[5] * 2000.0f, 255.0f) * 0.4f;   /* (the hat: the air above 5 kHz too) */
    for (i = 0; i < 3; i++) {                    /* the drums: a rise well over its own recent rises */
        static const uint32_t HOLD[3] = {110u, 90u, 60u};
        const float rise = fm_maxf(grp[i] - dr_prev[i], 0.0f);
        dr_prev[i] = grp[i];
        if (rise > dr_avg[i] * 2.2f + 6.0f && now - dr_t[i] > HOLD[i]) {
            dr_env[i] = 1.0f;
            dr_t[i] = now;
        } else {
            dr_env[i] *= 0.72f;
        }
        dr_avg[i] += (rise - dr_avg[i]) * 0.1f;
    }
    in->kick = (uint8_t)(dr_env[0] * 255.0f), in->snare = (uint8_t)(dr_env[1] * 255.0f), in->hat = (uint8_t)(dr_env[2] * 255.0f);
    in->bright = (uint8_t)fm_clampf(ct > 1e-5f ? (fm_log2f(cs / ct) * (255.0f / 7.0f)) : 0.0f, 0.0f, 255.0f);
}

/* -------------------------------------------------------------- a frame --- */
static vs_in_t fin;
static uint32_t clk_beats_seen;
static void frame_input(void)
{
    const uint32_t now = plat_ms();
    uint32_t i, w;
    memset(&fin, 0, sizeof fin);
    fin.ms = now;
    fin.level = ear_level;
    for (i = 0; i < VS_NBAND; i++)
        fin.bands[i] = ear_band[i];
    w = ear_wave_w;
    for (i = 0; i < VS_WAVE_N; i++)              /* (the oldest first) */
        fin.wave[i] = ear_wave[(w + i) % VS_WAVE_N];
    while (nq_r != nq_w && fin.nnote < VS_NNOTE) {
        const volatile uint8_t *q = nq[nq_r % NQ];
        fin.note[fin.nnote].voice = (uint8_t)(q[0] % VS_NVOX);
        fin.note[fin.nnote].note = q[1];
        fin.note[fin.nnote].vel = q[2];
        fin.nnote++;
        nq_r++;
    }
    fin.root = midi_root, fin.cc = midi_cc1, fin.vel = midi_vel;
    if (clk_ticks && now - clk_last_ms < 2000u) {   /* the tempo: the MIDI clock's while it runs (a clock came), else
                                                    * the taps' */
        ui.phase = (float)clk_ticks / 24.0f;
        if (clk_beats != clk_beats_seen) {
            clk_beats_seen = clk_beats;
            fin.clock_beat = 1;
        }
    } else {
        ui.phase += (float)(now - ui.phase_t) * (float)set.bpm * (1.0f / 60000.0f);
    }
    ui.phase_t = now;
    fin.phase = ui.phase;
    fin.hold = ui.hold;
    fin.strobe = ui.strobe_key;
    fin.mutate = ui.mutate;
    ui.mutate = 0;
    ear_spectrum(&fin);
}
/* the link, a frame: SYNC when the mirror asks (both engines reset), else the frame's input and what changed. 0: the
 * frame must wait (the SysEx ring is full: the mirror would lose it) */
static int link_frame_ok(void)
{
    if (lk.on && plat_ms() - lk.last_cmd_ms > 3000u) {   /* no keep-alive: the mirror is gone */
        lk.on = lk.rec = 0;
        video_rx.shown_kind = video_rx.loading_kind = 0;
        app_say("MIRROR", "GONE");
    }
    if (!lk.on)
        return 1;
    if (lk.want_sync) {
        uint32_t n;
        if (plat_sysex_room() < VLINK_SYNC_MAX)  /* (the ring holds 384 bytes: the SYNC, then the frames as it drains) */
            return 0;
        lk.pending = 0;                          /* (a frame not yet out: the SYNC starts anew without it) */
        lk.seed = plat_ms() * 2654435761u + 1u;
        memcpy(lk.sent_par, vpar, sizeof lk.sent_par);
        n = vlink_sync_raw(lk_raw, lk.seed, lk.sent_par);
        n = vlink_frame(VLINK_SYNC, lk_raw, n, lk_buf);
        if (plat_sysex_send(lk_buf, n) != 0)
            return 0;
        vs_reset(lk.seed);
        lk.want_sync = 0, lk.seq = 0, lk.syncs++;
        app_say("MIRROR", "LINKED");
        return 0;                                /* (the first frame once the SYNC has gone out) */
    }
    if (lk.pending) {                            /* (the last frame first: the mirror needs every one, in order) */
        if (plat_sysex_send(lk_buf, lk.pending) != 0) {
            lk.waited++;
            return 0;
        }
        lk.pending = 0;
        lk.seq++, lk.sent++;
    }
    if (plat_sysex_room() < VLINK_MAX) {
        lk.waited++;
        return 0;
    }
    return 1;
}
static void link_send(uint32_t sum)
{
    uint8_t ck[VLINK_NCHG];
    int16_t cv[VLINK_NCHG];
    uint32_t nc = 0, n;
    int k;
    if (!lk.on)
        return;
    for (k = 0; k < ST_N && nc < VLINK_NCHG; k++)
        if (lk.sent_par[k] != vpar[k]) {
            ck[nc] = (uint8_t)k, cv[nc] = vpar[k];
            nc++;
        }
    n = vlink_frame_raw(lk_raw, lk.seq, lk.rec, sum, &fin, ck, cv, nc);
    n = vlink_frame(VLINK_FRAME, lk_raw, n, lk_buf);
    for (k = 0; k < (int)nc; k++)                /* (the engine has run with them: they are the mirror's now) */
        lk.sent_par[ck[k]] = cv[k];
    if (plat_sysex_send(lk_buf, n) == 0)
        lk.seq++, lk.sent++;
    else
        lk.pending = n;                          /* (the ring busy: it goes before the next frame, the same bytes) */
}
/* what changed in the settings that the frame needs (vs_set already holds them): changes that could not go out this
 * frame wait in vpar against sent_par. But the engine must not run with settings the mirror lacks: hold back what
 * does not fit */
static void link_hold_back(int16_t *saved, uint8_t *held)
{
    int k, n = 0;
    for (k = 0; k < ST_N; k++) {
        held[k] = 0;
        if (lk.on && lk.sent_par[k] != vpar[k] && ++n > VLINK_NCHG) {
            saved[k] = vpar[k];
            vpar[k] = lk.sent_par[k];
            held[k] = 1;
        }
    }
}

static uint32_t ear_blocks_seen_t(void);
#define HDR_H 28
#define KNB_Y 172
#define KNB_H 68
static void dim(uint16_t *p, uint32_t n)         /* to 3/8 (the bars read over the picture) */
{
    uint32_t i;
    for (i = 0; i < n; i++) {
        const uint16_t c = swap16(p[i]);
        p[i] = swap16((uint16_t)(((c >> 2) & 0x39E7u) + ((c >> 3) & 0x18E3u)));
    }
}

/* the middle's overlay (over the picture, its rows dimmed): the page map while SEL is held. y0: the band's first
 * screen row (drawn in screen rows) */
static void draw_overlay(int y0, int h)
{
    const uint32_t now = plat_ms();
    int i;
    (void)now;
    if (!ui.map_on)
        return;
    dim(cv_px, 240u * (uint32_t)h);
    cv_oy = -y0;                                 /* (screen rows from here) */
    if (ui.map_on) {                             /* the groups at the left, the cursor's group's pages at the right */
        const int g = PAGES[ui.map_cur].group;
        int row = 0;
        for (i = 0; i < GR_N; i++) {
            const int on = i == g;
            if (on)
                cv_rect(4, 34 + i * 19, 84, 17, C_HI);
            cv_text(8, 36 + i * 19, &FONT_XS, GROUP_NAME[i], on ? C_BLACK : C_GRAY);
        }
        for (i = 0; i < NPAGE; i++)
            if (PAGES[i].group == g) {
                const int on = i == ui.map_cur;
                if (on)
                    cv_rect(96, 34 + row * 16, 140, 15, C_WHITE);
                cv_text(100, 35 + row * 16, &FONT_XS, PAGES[i].name, on ? C_BLACK : i == set.page ? C_HI : C_GRAY);
                row++;
            }
    }
    cv_oy = 0;
}
static uint16_t src_colour(int src)              /* a react source's colour (the knobs' arcs show what moves them) */
{
    switch (src) {
    case SRC_BASS: case SRC_LOWS: case SRC_KICK: return RGB(240, 70, 50);
    case SRC_MID: case SRC_MIDS: case SRC_SNARE: return RGB(250, 190, 40);
    case SRC_HIGH: case SRC_HIGHS: case SRC_HAT: case SRC_BRIGHT: return RGB(60, 200, 250);
    case SRC_BEAT: case SRC_NOTES: case SRC_VEL: return RGB(255, 255, 255);
    case SRC_LFO: case SRC_TEMPO: case SRC_BAR: return RGB(90, 230, 110);
    default: return RGB(200, 120, 255);
    }
}
static void arc(int cx, int cy, int r, float a0, float a1, uint16_t c)   /* an arc, 3 px thick (angles in turns) */
{
    const int n = (int)((a1 - a0) * 60.0f) + 1;
    int i;
    for (i = 0; i <= n; i++) {
        const float a = (a0 + (a1 - a0) * (float)i / (float)n) * FM_TWO_PI;
        cv_rect(cx + (int)((float)r * fm_cosf(a)) - 1, cy + (int)((float)r * fm_sinf(a)) - 1, 3, 3, c);
    }
}
static void draw_header(void)
{
    char t[32];
    const uint32_t now = plat_ms();
    int32_t x;
    if (now < ui.msg_until) {
        x = cv_text(6, 6, &FONT_XS, ui.msg[0], C_WHITE);
        cv_text(x + 6, 6, &FONT_XS, ui.msg[1], C_HI);
        return;
    }
    x = cv_text(6, 6, &FONT_XS, PAGES[set.page].name, C_WHITE);
    {                                            /* (its group, and which of the group's pages) */
        const int g = PAGES[set.page].group;
        int p, i = 0, n = 0;
        char u[8];
        for (p = 0; p < NPAGE; p++)
            if (PAGES[p].group == g) {
                n++;
                if (p <= set.page)
                    i = n;
            }
        t[0] = 0;
        cat(t, GROUP_NAME[g]);
        cat(t, " ");
        vs_itoa(i, u);
        cat(t, u);
        cat(t, "/");
        vs_itoa(n, u);
        cat(t, u);
    }
    x = cv_text(x + 6, 6, &FONT_XS, t, C_DIM);
    if (now - ear_blocks_seen_t() < 500u)
        x = cv_text(x + 8, 6, &FONT_XS, "IN", C_HI);
    if (lk.on) {
        x = cv_text(x + 8, 6, &FONT_XS, "PC", RGB(80, 220, 120));
        if (lk.rec) {
            const uint32_t s = (now - lk.rec_ms) / 1000u;
            char m[12];
            cv_rect(x + 8, 9, 8, 8, RGB(230, 40, 40));
            vs_itoa((int)(s / 60u), m);
            cat(m, s % 60u < 10u ? ":0" : ":");
            {
                char u[4];
                vs_itoa((int)(s % 60u), u);
                cat(m, u);
            }
            cv_text(x + 20, 6, &FONT_XS, m, RGB(230, 40, 40));
        }
    }
    {   /* the ear at the right: the spectrum (24 bands), the drums' lights, the tempo's */
        int i;
        for (i = 0; i < VS_NSPEC; i++) {
            const int h = fin.spec[i] * 16 / 255;
            cv_rect(166 + i * 3, 23 - h, 2, h, i < 6 ? RGB(240, 70, 50) : i < 18 ? RGB(250, 190, 40) : RGB(60, 200, 250));
        }
        cv_rect(150, 5, 5, 5, fin.kick > 120 ? RGB(240, 70, 50) : C_LINE);
        cv_rect(150, 12, 5, 5, fin.snare > 120 ? RGB(250, 190, 40) : C_LINE);
        cv_rect(150, 19, 5, 5, fin.hat > 120 ? RGB(60, 200, 250) : C_LINE);
        cv_rect(158, 5, 4, 19, ui.phase - fm_floorf(ui.phase) < 0.2f ? C_WHITE : C_LINE);   /* (the tempo) */
    }
}
static void draw_knobs(void)
{
    int i;
    for (i = 0; i < 4; i++) {
        char n[16], v[16];
        const int k = knob_par(i), x = i * 60;
        knob_text(i, n, v);
        cv_text(x + (60 - text_w(&FONT_XS, n)) / 2, 4, &FONT_XS, n, C_GRAY);
        cv_text(x + (60 - text_w(&FONT_XS, v)) / 2, 52, &FONT_XS, v, C_WHITE);
        if (k >= 0) {                            /* an arc from 7 to 5 o'clock: the value; past it, in the colour of
                                                  * what moves it, where its react slots carry it now */
            const vs_par_t *p = vs_par_info(k);
            const float span = (float)(p->hi - p->lo ? p->hi - p->lo : 1), a0 = 0.375f, len = 0.75f,
                        fv = (float)(vs_get(k) - p->lo) / span, fe = (float)(vs_eff(k) - p->lo) / span;
            int s;
            uint16_t mc = C_WHITE;
            arc(x + 30, 33, 12, a0, a0 + len, C_LINE);
            arc(x + 30, 33, 12, a0, a0 + len * fv, C_HI);
            for (s = 0; s < VS_NSLOT; s++) {      /* (the first slot on it, or on All, gives the colour) */
                const int b = ST_AR1S + s * VS_SLOT_W, tg = vs_get(b + 1);
                if (vs_get(b) && vs_get(b + 2) && (tg == k - ST_VMODE + 1 || (tg >= VS_NTGT && p->hi - p->lo >= 10))) {
                    mc = src_colour(vs_get(b));
                    break;
                }
            }
            if (fe != fv)
                arc(x + 30, 33, 8, a0 + len * fm_minf(fv, fe), a0 + len * fm_maxf(fv, fe), mc);
        }
    }
}
static uint32_t ear_seen_t, ear_seen_n;
static uint32_t ear_blocks_seen_t(void)
{
    if (ear_blocks_in != ear_seen_n) {
        ear_seen_n = ear_blocks_in;
        ear_seen_t = plat_ms();
    }
    return ear_seen_t;
}
static uint16_t external_palette[256];
static uint32_t external_rgb[256];          /* (the same, 8 bits a plane: 0xRRGGBB, for the smooth picture) */

/* Grade the incoming 3:3:2 colours on the FM-1. The Cortex palette keeps the source in colour. */
static void prepare_video_palette(void)
{
    const unsigned levels = 8u - 6u * (unsigned)VPAR(VPOSTER) / 100u;
    const unsigned gain = (unsigned)VPAR(VBRIGHT) + 50u;
    const unsigned key = 2u * (unsigned)VPAR(VKEY);
    unsigned pixel;
    for (pixel = 0; pixel < 256; ++pixel) {
        unsigned r = (pixel >> 5) * 255u / 7u, g = (pixel >> 2 & 7u) * 255u / 7u, b = (pixel & 3u) * 85u;
        const unsigned luma = (r * 54u + g * 183u + b * 19u) >> 8;
        if (luma < key) {
            external_palette[pixel] = 0;
            external_rgb[pixel] = 0;
            continue;
        }
        if (VPAR(VPAL)) {
            const uint16_t c = vpal[luma];
            r = (c >> 11) * 255u / 31u;
            g = (c >> 5 & 63u) * 255u / 63u;
            b = (c & 31u) * 255u / 31u;
        }
        if (VPAR(VPOSTER)) {
            r = ((r * (levels - 1u) + 127u) / 255u) * 255u / (levels - 1u);
            g = ((g * (levels - 1u) + 127u) / 255u) * 255u / (levels - 1u);
            b = ((b * (levels - 1u) + 127u) / 255u) * 255u / (levels - 1u);
        }
        r = r * gain / 100u;
        g = g * gain / 100u;
        b = b * gain / 100u;
        r = r > 255u ? 255u : r;
        g = g > 255u ? 255u : g;
        b = b > 255u ? 255u : b;
        r = (r * (256u - vinv) + (255u - r) * vinv) >> 8;
        g = (g * (256u - vinv) + (255u - g) * vinv) >> 8;
        b = (b * (256u - vinv) + (255u - b) * vinv) >> 8;
        external_palette[pixel] = swap16(RGB(r, g, b));
        external_rgb[pixel] = r << 16 | g << 8 | b;
    }
}

/* the camera's picture smooth: each screen pixel between the four nearest of the 60 x 60 (bilinear, a quarter-pixel's
 * precision), its colour graded (the palette above), dithered into the screen's 5:6:5 (no bands). MIRROR folds the
 * screen first; PIXEL asks for the blocks (paint_video_band's old path) */
static inline unsigned blend4(uint32_t a, uint32_t b, uint32_t c, uint32_t d, int wx, int wy, int s)
{                                                /* (one plane of four colours, between them by wx, wy in 256ths) */
    const unsigned top = ((a >> s & 255u) * (256u - (unsigned)wx) + (b >> s & 255u) * (unsigned)wx) >> 8,
                   bot = ((c >> s & 255u) * (256u - (unsigned)wx) + (d >> s & 255u) * (unsigned)wx) >> 8;
    return (top * (256u - (unsigned)wy) + bot * (unsigned)wy) >> 8;
}
static void paint_video_smooth(int y0, int h, const uint8_t *image)
{
    static const uint8_t BAYER[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
    const int size = VLINK_VIDEO_SIDE, mirror = VPAR(VMIRROR);
    int y, x;
    for (y = 0; y < h; ++y) {
        int Y = y + y0;
        uint16_t *p = cv_px + y * 240;
        if (mirror >= 2 && Y >= 120)
            Y = 239 - Y;
        for (x = 0; x < 240; ++x) {
            int X = x, sy = Y, fx, fy, ix, iy, wx, wy, ix1, iy1;
            uint32_t a, b, c, d;
            unsigned r, g, bl;
            if (mirror != 0 && mirror != 2 && X >= 120)
                X = 239 - X;
            if (mirror == 4 && X > sy) {
                const int s = X;
                X = sy, sy = s;
            }
            fx = X * 64 - 96, fy = sy * 64 - 96;   /* (the source's coordinate in 256ths: (2 x + 1) / 8 - 1 / 2) */
            ix = fx < 0 ? 0 : fx >> 8, wx = fx < 0 ? 0 : fx & 255;
            iy = fy < 0 ? 0 : fy >> 8, wy = fy < 0 ? 0 : fy & 255;
            ix1 = ix + 1 < size ? ix + 1 : size - 1, iy1 = iy + 1 < size ? iy + 1 : size - 1;
            a = external_rgb[image[iy * size + ix]], b = external_rgb[image[iy * size + ix1]];
            c = external_rgb[image[iy1 * size + ix]], d = external_rgb[image[iy1 * size + ix1]];
            r = blend4(a, b, c, d, wx, wy, 16), g = blend4(a, b, c, d, wx, wy, 8), bl = blend4(a, b, c, d, wx, wy, 0);
            {
                const unsigned k = BAYER[Y & 3][x & 3];
                r = r + (k >> 1) > 255u ? 255u : r + (k >> 1);
                g = g + (k >> 2) > 255u ? 255u : g + (k >> 2);
                bl = bl + (k >> 1) > 255u ? 255u : bl + (k >> 1);
            }
            p[x] = swap16((uint16_t)((r >> 3) << 11 | (g >> 2) << 5 | (bl >> 3)));
        }
    }
}

static void paint_video_band(int y0, int h, const uint8_t *image)
{
    const int size = VLINK_VIDEO_SIDE, mirror = VPAR(VMIRROR), block = 1 + VPAR(VPIXEL) / 15;
    int y, x;
    if (!image) {
        memset(cv_px, 0, (uint32_t)h * 240u * sizeof *cv_px);
        return;
    }
    if (block == 1) {                            /* (no PIXEL: smooth) */
        paint_video_smooth(y0, h, image);
        return;
    }
    for (y = 0; y < h; ++y) {
        int sy = (y + y0) / 4;
        if (mirror >= 2 && sy >= size / 2)
            sy = size - 1 - sy;
        sy = sy / block * block;
        for (x = 0; x < size; ++x) {
            int sx = x, row = sy;
            uint16_t *p = cv_px + y * 240 + x * 4;
            if (mirror != 0 && mirror != 2 && sx >= size / 2)
                sx = size - 1 - sx;
            sx = sx / block * block;
            if (mirror == 4 && sx > row) {
                const int t = sx;
                sx = row;
                row = t;
            }
            p[0] = p[1] = p[2] = p[3] = external_palette[image[row * size + sx]];
        }
    }
}

static int preview_state(int source, uint32_t now)
{
    const int ble_recent = video_ble_kind == source && now - video_ble_part_ms < 1500u;
    if (video_ble_shown_kind == source && ble_recent)
        return 4;
    if (lk.on && video_rx.shown_kind == source && now - video_rx.shown_ms < 1500u)
        return 3;
    if (ble_recent)
        return 2;
    if (!lk.on)
        return 0;
    if (video_rx.loading_kind == source && now - video_rx.part_ms < 1500u)
        return 2;                         /* the source is sending parts */
    return 1;                             /* session, but no recent matching video */
}

static const char *preview_message(int source, int state)
{
    switch (state) {
    case 0: return "Aguardando Bluetooth ou USB";
    case 1: return source == 1 ? "USB: abra a camera" : "USB: importe um video";
    case 2: return "Video: quadro incompleto";
    case 4: return "Bluetooth: video ativo";
    default: return "USB: video ativo";
    }
}

static int live_hq;                              /* (the picture live_image gave is the sharp one) */
static const uint8_t *live_image(void)
{
    const int source = video_mode() ? vs_get(ST_VMODE) - VS_NMODE + 1 : 0;
    const int preview = source ? preview_state(source, plat_ms()) : 0;
    live_hq = preview == 4 && video_ble_hq[video_ble_front];
    return preview == 4 ? (live_hq ? video_sharp : video_ble_pixels[0]) :
           preview == 3 ? video_pixels[0] : 0;
}

#define VS_SW16P(v) ((uint16_t)(((v) >> 8 & 0xFFu) | ((v) & 0xFFu) << 8))

/* STYLE, on the finished picture (every visual), a band at a time, before the bars: VHS (the rows wobbling, the colour
 * bleeding sideways, a tracking band rolling down), EDGES (neon lines, the palette's colour), EMBOSS (a relief),
 * SOLARIZE, SWAP (red and blue, or the planes round), SATURATION, MOTION (the clean camera's blocks that moved, lit),
 * CRT (scanlines, the phosphors' triads), HALFTONE (dots the light's size), LED (round lamps). Each its own amount,
 * together. RAM code: no calls */
struct post_fx {
    int crt, vhs, edge, emboss, half, led, solar, swap, motion, sat, clean;
    int hc, lc;                                  /* (HALFTONE's, LED's cell) */
    const uint8_t *hpos, *lpos, *age;            /* (x mod the cells; the clean camera's blocks' ages) */
    const int8_t *jit;                           /* (VHS: each row's wobble) */
    const uint16_t *pal;
    int vband, vh;                               /* (VHS: the tracking band) */
    uint32_t seed;
};
static struct post_fx pfx;
static uint8_t post_hpos[240], post_lpos[240];
static int8_t post_jit[240];
static PLAT_RAM_CODE void post_row(const struct post_fx *f, int y, uint16_t *px, const uint16_t *below,
                                   uint8_t *prevL, uint8_t *curL, uint8_t *nextL, uint16_t *orig)
{
    int x;
    for (x = 0; x < 240; x++)                    /* (the row as painted; its light, and the one below's) */
        orig[x] = px[x];
    for (x = 0; x < 240; x++) {
        const uint32_t c = VS_SW16P(below[x]);
        nextL[x] = (uint8_t)((77u * ((c >> 11) << 3) + 150u * ((c >> 5 & 63u) << 2) + 29u * ((c & 31u) << 3)) >> 8);
    }
    for (x = 0; x < 240; x++) {
        int xs = x, r, g, b, Y;
        uint32_t c;
        if (f->vhs) {                            /* VHS: the row's wobble */
            xs = x - f->jit[y];
            while (xs < 0) xs += 240;
            while (xs >= 240) xs -= 240;
        }
        c = VS_SW16P(orig[xs]);
        r = (int)(c >> 11) << 3, g = (int)(c >> 5 & 63u) << 2, b = (int)(c & 31u) << 3;
        if (f->vhs) {                            /* ... the colour bleeding: red behind, blue ahead; the band's noise */
            const int k = 1 + f->vhs / 25;
            const uint32_t cr = VS_SW16P(orig[xs - k < 0 ? 0 : xs - k]), cb = VS_SW16P(orig[xs + k > 239 ? 239 : xs + k]);
            r = (r + ((int)(cr >> 11) << 3)) >> 1, b = (b + ((int)(cb & 31u) << 3)) >> 1;
            if (y >= f->vband && y < f->vband + f->vh) {
                const int n = (int)(((uint32_t)x * 2654435761u + (uint32_t)y * 40503u + f->seed) >> 25) - 64;
                r += n, g += n, b += n;
            }
        }
        if (f->edge) {                           /* EDGES: the light's steps, the palette's neon */
            const int xl = x > 0 ? x - 1 : 0, xr = x < 239 ? x + 1 : 239;
            int e = curL[xr] - curL[xl], e2 = nextL[x] - prevL[x];
            e = ((e < 0 ? -e : e) + (e2 < 0 ? -e2 : e2)) * (2 + f->edge / 12);
            e = e > 255 ? 255 : e;
            {
                const uint32_t P = f->pal[e];
                const int keep = 256 - f->edge * 2;
                r = ((r * keep) >> 8) + ((((int)(P >> 11) << 3) * e) >> 8);
                g = ((g * keep) >> 8) + ((((int)(P >> 5 & 63u) << 2) * e) >> 8);
                b = ((b * keep) >> 8) + ((((int)(P & 31u) << 3) * e) >> 8);
            }
        }
        if (f->emboss) {                         /* EMBOSS: lit from the top left */
            const int xl = x > 0 ? x - 1 : 0, xr = x < 239 ? x + 1 : 239;
            int v = 128 + (nextL[xr] - prevL[xl]) * (1 + f->emboss / 20);
            const int a = f->emboss * 256 / 100;
            v = v < 0 ? 0 : v > 255 ? 255 : v;
            r += ((v - r) * a) >> 8, g += ((v - g) * a) >> 8, b += ((v - b) * a) >> 8;
        }
        if (f->solar) {                          /* SOLARIZE: what is over the half turned back */
            if (r > 128) r += ((255 - 2 * r) * f->solar) / 100;
            if (g > 128) g += ((255 - 2 * g) * f->solar) / 100;
            if (b > 128) b += ((255 - 2 * b) * f->solar) / 100;
        }
        if (f->swap) {                           /* SWAP: red and blue; past 50 the planes round */
            const int s = r;
            if (f->swap >= 50)
                r = g, g = b, b = s;
            else
                r = b, b = s;
        }
        if (f->sat != 100) {                     /* SATURATION */
            Y = (77 * r + 150 * g + 29 * b) >> 8;
            r = Y + (r - Y) * f->sat / 100, g = Y + (g - Y) * f->sat / 100, b = Y + (b - Y) * f->sat / 100;
        }
        if (f->motion && f->clean) {             /* MOTION: what moved lit and warmed */
            const int a = f->age[(y >> 3) * 30 + (x >> 3)] * f->motion / 100;
            r += ((255 - r) * a) >> 8, b -= (b * a) >> 9;
        }
        if (f->crt) {                            /* CRT: the scanlines, the triads */
            const int k = x % 3, d = f->crt * 2;
            if (y & 1)
                r = r * (256 - d) >> 8, g = g * (256 - d) >> 8, b = b * (256 - d) >> 8;
            if (k)
                r = r * (256 - d / 2) >> 8;
            if (k != 1)
                g = g * (256 - d / 2) >> 8;
            if (k != 2)
                b = b * (256 - d / 2) >> 8;
        }
        if (f->half || f->led) {                 /* HALFTONE, LED: a round dot a cell (the larger of the two) */
            const int led = f->led > f->half, c = led ? f->lc : f->hc, pos = led ? f->lpos[x] : f->hpos[x];
            const int dx = pos - c / 2, dy = (led ? f->lpos[y] : f->hpos[y]) - c / 2;
            const int cx = x - pos + c / 2 > 239 ? 239 : x - pos + c / 2;
            const int L = curL[cx], rad = led ? c * c * 3 / 16 : (L * c * c) >> 9;
            if (dx * dx + dy * dy > rad) {
                const int a = (led ? f->led : f->half) * 256 / 100;
                r -= (r * a) >> 8, g -= (g * a) >> 8, b -= (b * a) >> 8;
            }
        }
        r = r < 0 ? 0 : r > 255 ? 255 : r;
        g = g < 0 ? 0 : g > 255 ? 255 : g;
        b = b < 0 ? 0 : b > 255 ? 255 : b;
        {
            const uint32_t v = (uint32_t)((r >> 3) << 11 | (g >> 2) << 5 | (b >> 3));
            px[x] = (uint16_t)VS_SW16P(v);
        }
    }
    for (x = 0; x < 240; x++)                    /* (this row's light: the next row's above) */
        prevL[x] = curL[x], curL[x] = nextL[x];
}
static int post_frame(void)                      /* (once a frame: STYLE's settings; 0: none on) */
{
    int i;
    pfx.crt = vs_eff(ST_CCRT), pfx.vhs = vs_eff(ST_CVHS), pfx.edge = vs_eff(ST_CEDGE);
    pfx.emboss = vs_eff(ST_CEMBOSS), pfx.half = vs_eff(ST_CHALF), pfx.led = vs_eff(ST_CLED);
    pfx.solar = vs_eff(ST_CSOLAR), pfx.swap = vs_eff(ST_CSWAP), pfx.motion = vs_eff(ST_CMOTION);
    pfx.sat = vs_eff(ST_CSAT), pfx.clean = clean_on;
    if (!(pfx.crt | pfx.vhs | pfx.edge | pfx.emboss | pfx.half | pfx.led | pfx.solar | pfx.swap |
          (pfx.motion && clean_on)) && pfx.sat == 100)
        return 0;
    pfx.pal = vs_pal(), pfx.age = clean_age;
    pfx.hc = 4 + pfx.half / 10, pfx.lc = 4 + pfx.led / 10;
    for (i = 0; i < 240; i++)
        post_hpos[i] = (uint8_t)(i % pfx.hc), post_lpos[i] = (uint8_t)(i % pfx.lc);
    pfx.hpos = post_hpos, pfx.lpos = post_lpos;
    if (pfx.vhs) {                               /* VHS: the rows' wobble, the tracking band rolling down */
        static int band;
        for (i = 0; i < 240; i++)
            post_jit[i] = (int8_t)(((int)(clean_rand() & 7u) - 3) * pfx.vhs / 50);
        band = (band + 3) % 300;
        pfx.vband = band - 30, pfx.vh = 6 + pfx.vhs / 6;
        for (i = pfx.vband; i < pfx.vband + pfx.vh; i++)
            if (i >= 0 && i < 240)
                post_jit[i] = (int8_t)(post_jit[i] * 3 + 8);
        pfx.seed = clean_rand();
    }
    pfx.jit = post_jit;
    return 1;
}
static void post_band(int y0, int h)             /* (the band's rows, top to bottom; at its edges the row itself) */
{
    static uint8_t prevL[240], curL[240], nextL[240];
    static uint16_t orig[240];
    int y, x;
    for (x = 0; x < 240; x++) {
        const uint32_t c = VS_SW16P(cv_px[x]);
        curL[x] = (uint8_t)((77u * ((c >> 11) << 3) + 150u * ((c >> 5 & 63u) << 2) + 29u * ((c & 31u) << 3)) >> 8);
        prevL[x] = curL[x];
    }
    for (y = 0; y < h; y++)
        PLAT_FAR(post_row)(&pfx, y0 + y, cv_px + y * 240, cv_px + (y + 1 < h ? y + 1 : y) * 240, prevL, curL, nextL, orig);
}

static void draw_frame(void)                     /* the picture, the bars over it (unless hidden) */
{
    static const int16_t BY[4] = {0, HDR_H, HDR_H + 72, KNB_Y}, BH[4] = {HDR_H, 72, 72, KNB_H};
    const uint32_t idle = ui.map_on ? 0u : plat_ms() - ui.act_t;
    const int bars = vs_get(ST_VBARS),
              hide = ui.hide_now || (plat_ms() >= ui.msg_until &&
                                     (bars == 2 ? idle > 1500u : bars == 1 ? idle > (uint32_t)vs_get(ST_VDELAY) * 1000u : 0));
    const int source = vs_get(ST_VMODE) >= VS_NMODE ? vs_get(ST_VMODE) - VS_NMODE + 1 : 0;
    const int preview = source ? preview_state(source, plat_ms()) : 0;
    const uint8_t *kept = source && photo_view ? photo_nth(photo_view - 1) : 0,
                  *live = preview == 4 || preview == 3 ? live_image() : 0,
                  *image = kept ? kept : live;   /* (live: through the engine, its effects; a kept photo as it is) */
    const int snap = photo_req && live && !kept;
    static int post_on;
    int b;
    uint32_t t = plat_ticks24(), lw, pt = 0, tv = 0, tw = 0;
    if (image)
        prepare_video_palette();
    lcd_sync();
    lw = plat_ticks24() - t;
    for (b = 0; b < 4; b++) {
        t = plat_ticks24();
        cv_begin_pingpong(240, (uint32_t)BH[b], (uint32_t)b);
        tw += plat_ticks24() - t;                /* (the band's buffer free: the LCD's transfer before) */
        {
            const uint32_t t1 = plat_ticks24();
            if (source && (kept || !live))
                paint_video_band(BY[b], BH[b], kept);
            else if (clean_on && source)
                paint_clean_band(BY[b], BH[b]);
            else
                vs_paint(BY[b], BH[b], cv_px);
            if (b == 0)
                post_on = !kept && post_frame();
            if (post_on)
                post_band(BY[b], BH[b]);
            tv += plat_ticks24() - t1;
        }
        if (snap) {
            int y;
            for (y = 0; y < BH[b]; y++)
                photo_row(BY[b] + y, cv_px + y * 240);
        }
        if (!hide && b == 0) {
            dim(cv_px, 240u * HDR_H);
            draw_header();
        } else if (!hide && b == 3) {
            dim(cv_px, 240u * KNB_H);
            draw_knobs();
        } else if (b == 1 || b == 2) {
            if (b == 1 && source && !image && !ui.map_on)
                cv_text(12, 24, &FONT_XS, preview_message(source, preview), C_GRAY);
            draw_overlay(BY[b], BH[b]);
        }
        pt += plat_ticks24() - t;
        t = plat_ticks24();
        cv_blit(0, (uint32_t)BY[b]);
        lw += plat_ticks24() - t;
        if (b < 3)
            video_ble_poll(plat_ms());           /* (and between the bands: a frame whole is answered at once) */
    }
    ui.paint_us += ((int32_t)(pt / 24u) - (int32_t)ui.paint_us) / 8;
    ui.vsp_us += ((int32_t)(tv / 24u) - (int32_t)ui.vsp_us) / 8;
    ui.wait_us += ((int32_t)(tw / 24u) - (int32_t)ui.wait_us) / 8;
    ui.lcd_us += ((int32_t)(lw / 24u) - (int32_t)ui.lcd_us) / 8;
    if (snap) {
        photo_req = 0;
        photo_done();
    }
}

/* the whole screen in words (a screen reader asks; fm1_remote.py screen): the page, its knobs, and the frame's timing,
 * which is how the FM-1 is measured from the computer */
#define SPEAK_N 480
static void cat_n(char *d, const char *s)        /* (cat, but never past SPEAK_N: the words can grow) */
{
    int n = 0;
    while (d[n] && n < SPEAK_N - 1)
        n++;
    while (*s && n < SPEAK_N - 1)
        d[n++] = *s++;
    d[n] = 0;
}
static void speak(void)
{
    static char t[SPEAK_N];                      /* (not on the stack: 1.x's 200 bytes overflowed it, a crash) */
    char n[16], v[16], u[12];
    int i;
    t[0] = 0;
    cat_n(t, APP_NAME ". ");
    if (vs_get(ST_VMODE) >= VS_NMODE) {          /* (first: a screen reader's message is cut near 240 characters) */
        const int source = vs_get(ST_VMODE) - VS_NMODE + 1;
        char u2[12];
        cat_n(t, "Video frames ");
        vs_itoa((int)video_rx.frames, u2);
        cat_n(t, u2);
        cat_n(t, ", ");
        cat_n(t, preview_message(source, preview_state(source, plat_ms())));
        {
            uint32_t v[7];
            plat_ble_link_info(v);
            cat_n(t, ". BLE packets ");
            vs_itoa((int)video_ble_taken, u2), cat_n(t, u2);
#ifndef PLAT_HOST
            {
                extern uint32_t ble_hw_rx_data, ble_hw_rx_md, ble_hw_multi_evt, ble_hw_most_evt;
                cat_n(t, " md ");
                vs_itoa((int)ble_hw_rx_md, u2), cat_n(t, u2);
                cat_n(t, "/");
                vs_itoa((int)ble_hw_rx_data, u2), cat_n(t, u2);
                cat_n(t, " multi ");
                vs_itoa((int)ble_hw_multi_evt, u2), cat_n(t, u2);
                cat_n(t, " most ");
                vs_itoa((int)ble_hw_most_evt, u2), cat_n(t, u2);
            }
            {
                extern void ble_hw_app_stats(uint32_t v[4]);
                uint32_t h[4];
                ble_hw_app_stats(h);
                cat_n(t, " ev ");
                vs_itoa((int)h[0], u2), cat_n(t, u2);
                cat_n(t, " rep ");
                vs_itoa((int)h[1], u2), cat_n(t, u2);
                cat_n(t, " crc ");
                vs_itoa((int)h[2], u2), cat_n(t, u2);
                cat_n(t, " isr ");
                vs_itoa((int)h[3], u2), cat_n(t, u2);
            }
#endif
            cat_n(t, " lost ");
            vs_itoa((int)video_ble_bad, u2), cat_n(t, u2);
            cat_n(t, " q ");
            vs_itoa((int)codec_scale, u2), cat_n(t, u2);
            cat_n(t, " interval ");
            vs_itoa((int)v[2], u2), cat_n(t, u2);
            cat_n(t, " mtu ");
            vs_itoa((int)v[1], u2), cat_n(t, u2);
        }
        cat_n(t, ". ");
    }
    cat_n(t, PAGES[set.page].name);
    for (i = 0; i < 4; i++) {
        knob_text(i, n, v);
        cat_n(t, ". ");
        cat_n(t, n);
        cat_n(t, " ");
        cat_n(t, v);
    }
#define SPK(label, val) (cat_n(t, label), vs_itoa((int)(val), u), cat_n(t, u))
    SPK(". Frame us ", ui.frame_us);
    SPK(", generate ", ui.gen_us);
    SPK(", paint ", ui.paint_us);
    SPK(" (picture ", ui.vsp_us);
    {
        extern uint32_t vs_prof[4];
        SPK(" r", vs_prof[0] / 24u);
        SPK(" g", vs_prof[1] / 24u);
        SPK(" o", vs_prof[2] / 24u);
        SPK(" p", vs_prof[3] / 24u);
    }
    SPK(", lcd ", ui.lcd_us);
    SPK(", every ms ", ui.gap_ms);
    SPK(", most ", ui.gap_max);
    SPK(". Frames ", ui.frame_n);
    SPK(". Link ", lk.on);
    SPK(", sent ", lk.sent);
    SPK(", waited ", lk.waited);
    SPK(", syncs ", lk.syncs);
    SPK(". Audio blocks ", ear_blocks_in);
#undef SPK
    ann_say(t, 0, ANN_NOW);
}

static void mirror_cmd(const uint8_t *b, uint32_t n)   /* a setting turned, a scene, undo: from the computer */
{
    char t[16];
    if (b[0] == VLINK_SET && n >= 4) {
        const int k = b[1], v = (int16_t)(b[2] | b[3] << 8);
        if (k < ST_N) {
            undo_knob_mark(k + 1000);            /* (the mirror's gestures: undone as the knobs' are) */
            par_set(k, v);
            app_say(vs_par_info(k)->name, vs_par_text(k, vs_get(k), t));
        }
    } else if (b[0] == VLINK_SCENE && n >= 2) {
        scene_go(b[1] % NSCENE);
    } else if (b[0] == VLINK_UNDO) {
        undo();
    } else if (b[0] == VLINK_LOOK && n >= 2) {
        look_apply(b[1] % NLOOK);
    }
}

/* ------------------------------------------------------------------ the loop --- */
void app_init(void)
{
    int k;
    if (plat_store_load(OBJ_SET, &set, sizeof set) != (int)sizeof set || set.magic != SET_MAGIC || set.n != ST_N)
        if (!store_migrate_set())
            set_defaults();
    if (plat_store_load(OBJ_SCENES, &scn, sizeof scn) != (int)sizeof scn || scn.magic != SET_MAGIC || scn.n != ST_N)
        if (!store_migrate_scenes()) {
            memset(&scn, 0, sizeof scn);
            scn.magic = SET_MAGIC, scn.n = ST_N;
        }
    set.page = (uint8_t)(set.page % NPAGE), set.slot = (uint8_t)(set.slot % VS_NSLOT);
    set.look = (uint8_t)(set.look % NLOOK), set.scene = (uint8_t)(set.scene % NSCENE);
    set.bpm = (uint16_t)clamp(set.bpm, 40, 240);
    set.sq = (uint8_t)(set.sq % 3u), set.smorph = (uint8_t)(set.smorph > 16 ? 16 : set.smorph);
    set.schain = (uint8_t)(set.schain > 16 ? 16 : set.schain);
    for (k = 0; k < NMKEY; k++) {
        set.kfn[k] = (uint8_t)(set.kfn[k] % MF_N);
        if (set.kpar[k] < 0 || set.kpar[k] > VS_TGT_LAST)
            set.kpar[k] = ST_VFEED;
        ui.ksave_par[k] = -1;
    }
    ui.pend = -1;
    for (k = 0; k < ST_N; k++)
        par_set(k, set.par[k]);
    set_dirty = 0;
    vs_reset(0x2545F491u);
    plat_usb_audio_full(1);                      /* the USB recording at full level, MASTER for the speaker alone */
    plat_usb_play_gain(0);                       /* the computer's audio: played by the render itself (app_render) */
}
void app_ui_init(void) { ui.act_t = plat_ms(); plat_ble_stream(&photo_stream); }

void app_frame(void)
{
    const uint32_t now = plat_ms();
    input();
    video_ble_poll(now);
    if (now - ui.frame_t >= 30u) {               /* every second pass of the main loop (16 ms): ~31 frames a second when
                                                  * a frame takes under 30 ms; the motion goes by the clock (vsynth.c) */
        int16_t saved[ST_N];
        uint8_t held[ST_N];
        uint32_t t0, sum;
        int k;
        if (!link_frame_ok())
            return;                              /* (the ring full: the next pass) */
        t0 = plat_ticks24();
        if (ui.frame_t) {
            const uint32_t g = now - ui.frame_t;
            ui.gap_ms += ((int32_t)g - (int32_t)ui.gap_ms) / 8;
            ui.gap_max = g > ui.gap_max ? g : ui.gap_max;
        }
        ui.frame_t = now;
        scenes_frame();
        frame_input();
        link_hold_back(saved, held);
        if (clean_on && !video_mode())
            clean_off();                         /* (a synthesis again: the engine's planes back) */
        if (!clean_on) {
            const uint8_t *cam = photo_view ? 0 : live_image();
            vs_camera(cam, live_hq);
            vs_frame(&fin);
        } else {
            vs_frame_light(&fin);                /* (the clean picture: the clock, the ear, CHROMA; nothing drawn) */
        }
        ui.gen_us += ((int32_t)((plat_ticks24() - t0) / 24u) - (int32_t)ui.gen_us) / 8;
        sum = lk.on && !clean_on && !(video_mode() && !photo_view && live_image()) ? vs_sum() : 0u;   /* (0: the camera's,
                                                  * which the mirror has not) */
        photo_send();
        link_send(sum);
        for (k = 0; k < ST_N; k++)               /* (what did not fit goes with the next frame) */
            if (held[k])
                vpar[k] = saved[k];
        draw_frame();
        ui.frame_us += ((int32_t)((plat_ticks24() - t0) / 24u) - (int32_t)ui.frame_us) / 8;
        ui.frame_n++;
    }
    if (ann_on() && ann_where())
        speak();
    ann_tick(now);
}

/* -------------------------------------------------------------- the host --- */
static int num(const char *s)                     /* ("par12": 12) */
{
    int v = 0;
    while (*s >= '0' && *s <= '9')
        v = v * 10 + (*s++ - '0');
    return v;
}
int app_expect(const char *what, int *got)
{
    if (cv_eq(what, "page"))
        *got = set.page;
    else if (cv_pre(what, "par"))
        *got = vs_get(num(what + 3));
    else if (cv_eq(what, "link"))
        *got = lk.on;
    else if (cv_eq(what, "rec"))
        *got = lk.rec;
    else if (cv_eq(what, "frames"))
        *got = (int)ui.frame_n;
    else if (cv_eq(what, "sent"))
        *got = (int)lk.sent;
    else if (cv_eq(what, "held"))
        *got = ui.hold;
    else if (cv_eq(what, "scene"))
        *got = set.scene;
    else if (cv_eq(what, "scenes"))
        *got = (int)scn.used;
    else if (cv_eq(what, "bpm"))
        *got = set.bpm;
    else if (cv_eq(what, "look"))
        *got = set.look;
    else
        return 0;
    return 1;
}
