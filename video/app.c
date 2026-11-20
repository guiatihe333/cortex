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
    int16_t par[NSCENE][ST_N];
} scenes_t;
static set_t set;
static scenes_t scn;
static uint8_t set_dirty, scn_dirty;

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
    {"Orb", 9, {ST_VMODE, ST_OSHAPE, ST_OSIZE, ST_OMORPH, ST_OLIGHT, ST_VFEED, ST_VTRAIL, ST_VPAL, ST_OBLEND}, {5, 7, 55, 60, 80, 30, 70, 0, 0}},
    {"Cubes", 10, {ST_VMODE, ST_OSHAPE, ST_OCOUNT, ST_OARRAY, ST_OSPREAD, ST_OREACT, ST_ODRAW, ST_VFEED, ST_VPAL, ST_OSPIN}, {7, 6, 9, 0, 60, 70, 3, 20, 5, 40}},
    {"Terrain", 9, {ST_VMODE, ST_OSHAPE, ST_ODRAW, ST_OTILT, ST_OZOOM, ST_OMORPH, ST_VPAL, ST_VFEED, ST_OSPIN}, {4, 11, 1, 30, 55, 50, 6, 40, 10}},
    {"Torus Ring", 10, {ST_VMODE, ST_OSHAPE, ST_OCOUNT, ST_OARRAY, ST_OSPREAD, ST_OSIZE, ST_OBLEND, ST_VFEED, ST_VTRAIL, ST_VPAL}, {5, 8, 6, 5, 55, 55, 2, 60, 85, 8}},
    {"Spectrum", 9, {ST_VMODE, ST_OSHAPE, ST_OCOUNT, ST_OARRAY, ST_OSPREAD, ST_OREACT, ST_OSPIN, ST_VPAL, ST_VFEED}, {8, 5, 12, 2, 80, 100, 0, 5, 15}},
    {"Negative", 8, {ST_VMODE, ST_VINVERT, ST_VPAL, ST_VFEED, ST_VXZOOM, ST_VXROT, ST_VGEO, ST_VTRAIL}, {5, 100, 8, 70, 30, 15, 14, 80}},
};
#define NLOOK ((int)(sizeof LOOKS / sizeof *LOOKS))
#define PICTURE_LAST ST_OPULSE                  /* (a look, a scene, the dice: VMODE .. VINVERT; not the ear, the screen,
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

static void mirror_cmd(const uint8_t *b, uint32_t n);
int app_sysex(const uint8_t *p, uint32_t n)      /* (the bytes between F0 and F7) */
{
    uint8_t b[8];
    if (n < 5 || p[0] != VLINK_MANU || p[1] != VLINK_ID0 || p[2] != VLINK_ID1 || p[3] != VLINK_CMD)
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
    } else if (b[0] == VLINK_RESYNC || !lk.on) {
        lk.on = 1;
        lk.want_sync = 1;
    }
    return 1;
}

/* --------------------------------------------------------------------- UI --- */
static struct {
    uint32_t btn, keys, act_t, msg_until, frame_t, frame_us, frame_n, tap_t[4];
    uint32_t gen_us, paint_us, lcd_us, gap_ms, gap_max;   /* (a frame's parts, averaged: generate, paint, the LCD's
                                                       * waits; the time between frames, its most) */
    char msg[2][24];
    uint8_t hold, mutate, strobe_key, hide_now, ntap;
    float phase;                         /* the tempo's beats (the clock's, else the taps') */
    uint32_t phase_t;
    uint8_t mkey;                        /* the MATRIX page's key (0 .. 10: 17 .. 27) */
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
            if (nm[j] == ' ' && nm[j + 1] >= '1' && nm[j + 1] <= '8' && !nm[j + 2])
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
        const int span = p->hi - p->lo, step = span >= 100 && (d > 1 || d < -1) ? 2 * d : d;
        undo_knob_mark(k);
        par_set(k, vs_get(k) + step);
    }
    if (k < 0 && k != K_SLOT && k != K_MKEY)
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
        scn.par[i][k] = (int16_t)vs_get(k);
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
                par_set(k, scn.par[i][k]);
    } else {                                     /* (the switches at once; every range on its way, set.smorph beats) */
        for (k = 0; k < ST_N; k++) {
            const vs_par_t *p = vs_par_info(k);
            ui.m_from[k] = (int16_t)vs_get(k);
            ui.m_to[k] = scn.par[i][k];
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
static void group_go(int d)                      /* SEL + SELECT: the next group's first page */
{
    int g = (PAGES[set.page].group + GR_N + d) % GR_N, p;
    for (p = 0; p < NPAGE && PAGES[p].group != g; p++)
        ;
    page_go(p - set.page);
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
static void rec_toggle(void)
{
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
    case MF_NVIS: par_set(ST_VMODE, (vs_get(ST_VMODE) + 1) % VS_NMODE); app_say("VISUAL", VS_MODE_NAME[vs_get(ST_VMODE)]); break;
    case MF_NOBJ: par_set(ST_OSHAPE, (vs_get(ST_OSHAPE) + 1) % 13); app_say("OBJECT", vs_par_text(ST_OSHAPE, vs_get(ST_OSHAPE), t)); break;
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
        case B_PLAY: ui.hold = (uint8_t)!ui.hold; app_say("STRUCTURE", ui.hold ? "HELD" : "FREE"); break;
        case B_EDIT: ui.mutate = 3; app_say("VISUAL", "NEW STRUCTURE"); break;
        case B_LFO: ui.mutate = 1; app_say("MUTATE", "A NUDGE"); break;
        case B_ENV: undo(); break;
        case B_GLO: dice_page(); break;
        case B_SAVE: save_all(); break;
        case B_FX: tap(); break;
        case B_HOME: ui.hide_now = (uint8_t)!ui.hide_now; break;
        case B_OCTDN: page_go(-1); break;
        case B_OCTUP: page_go(1); break;
        case B_ARP: look_apply((set.look + NLOOK - 1) % NLOOK); break;
        case B_SEQ: look_apply((set.look + 1) % NLOOK); break;
        default: break;
        }
    }
    for (k = 0; k < NKEYS; k++) {
        bit = 1u << k;
        if ((keys & bit) && !(ui.keys & bit)) {
            ui.act_t = now;
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
        } else if (k == EN_SELECT && (btn >> B_SEL & 1u))
            group_go(e > 0 ? 1 : -1);
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
void app_input_only(void) { input(); }

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
    {   /* the beat and the bands, small, at the right */
        int i;
        for (i = 0; i < VS_NBAND; i++) {
            const int h = (int)(fm_minf(vs_band(i), 1.0f) * 16.0f);
            cv_rect(200 + i * 6, 22 - h, 4, h, C_HI);
        }
        if (vs_beat() > 0.5f)
            cv_rect(190, 8, 6, 6, C_WHITE);
    }
}
static void draw_knobs(void)
{
    int i;
    for (i = 0; i < 4; i++) {
        char n[16], v[16];
        const int k = knob_par(i), x = i * 60;
        knob_text(i, n, v);
        cv_text(x + 4, 8, &FONT_XS, n, C_GRAY);
        cv_text(x + 4, 28, &FONT_XS, v, C_WHITE);
        if (k >= 0) {
            const vs_par_t *p = vs_par_info(k);
            const int w = (vs_get(k) - p->lo) * 50 / (p->hi - p->lo ? p->hi - p->lo : 1);
            cv_rect(x + 4, 54, 50, 3, C_LINE);
            cv_rect(x + 4, 54, w, 3, C_HI);
            {
                const int we = (vs_eff(k) - p->lo) * 50 / (p->hi - p->lo ? p->hi - p->lo : 1);
                if (we != w)                     /* (where its react slots carry it now) */
                    cv_rect(x + 4 + (we < 0 ? 0 : we > 50 ? 50 : we), 51, 2, 9, C_WHITE);
            }
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
static void draw_frame(void)                     /* the picture, the bars over it (unless hidden) */
{
    static const int16_t BY[4] = {0, HDR_H, HDR_H + 72, KNB_Y}, BH[4] = {HDR_H, 72, 72, KNB_H};
    const uint32_t idle = plat_ms() - ui.act_t;
    const int bars = vs_get(ST_VBARS),
              hide = ui.hide_now || (plat_ms() >= ui.msg_until &&
                                     (bars == 2 ? idle > 1500u : bars == 1 ? idle > (uint32_t)vs_get(ST_VDELAY) * 1000u : 0));
    int b;
    uint32_t t = plat_ticks24(), lw, pt = 0;
    lcd_sync();
    lw = plat_ticks24() - t;
    for (b = 0; b < 4; b++) {
        t = plat_ticks24();
        cv_begin_pingpong(240, (uint32_t)BH[b], (uint32_t)b);
        vs_paint(BY[b], BH[b], cv_px);
        if (!hide && b == 0) {
            dim(cv_px, 240u * HDR_H);
            draw_header();
        } else if (!hide && b == 3) {
            dim(cv_px, 240u * KNB_H);
            draw_knobs();
        }
        pt += plat_ticks24() - t;
        t = plat_ticks24();
        cv_blit(0, (uint32_t)BY[b]);
        lw += plat_ticks24() - t;
    }
    ui.paint_us += ((int32_t)(pt / 24u) - (int32_t)ui.paint_us) / 8;
    ui.lcd_us += ((int32_t)(lw / 24u) - (int32_t)ui.lcd_us) / 8;
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
        set_defaults();
    if (plat_store_load(OBJ_SCENES, &scn, sizeof scn) != (int)sizeof scn || scn.magic != SET_MAGIC || scn.n != ST_N) {
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
void app_ui_init(void) { ui.act_t = plat_ms(); }

void app_frame(void)
{
    const uint32_t now = plat_ms();
    input();
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
        vs_frame(&fin);
        ui.gen_us += ((int32_t)((plat_ticks24() - t0) / 24u) - (int32_t)ui.gen_us) / 8;
        sum = lk.on ? vs_sum() : 0u;
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
