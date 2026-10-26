/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal */
/* The instrument (amb.h). One compilation unit, -O2. The voices (pad, bells, bass) are FM6: JIANT FM's
 * 6-operator FM engine, msfa (Dexed's core) in integer C, fm6_core.c (Apache-2.0); the pulse is AMB-1's own. */
#include "amb.h"
#include "fastmath.h"
#include "fm6_tables.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#include "fm6_core.c"
#pragma GCC diagnostic pop

#if defined EN_HOST || defined PLAT_HOST
#define EN_POOL
#else
#define EN_POOL __attribute__((section(".pool")))
#endif
#define BARRIER() __asm__ volatile("" ::: "memory")

const char *const EN_NOTE_NAME[12] = {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};
const char *const EN_MODE_NAME[EN_NMODES] = {"Ionian", "Dorian", "Phrygian", "Lydian", "Mixolyd.", "Aeolian",
                                             "Pent maj", "Pent min"};
#define B(a, b, c, d, e, f, g) (1u << (a) | 1u << (b) | 1u << (c) | 1u << (d) | 1u << (e) | 1u << (f) | 1u << (g))
const uint16_t EN_MODE_POOL[EN_NMODES] = {
    B(0, 2, 4, 5, 7, 9, 11), B(0, 2, 3, 5, 7, 9, 10), B(0, 1, 3, 5, 7, 8, 10), B(0, 2, 4, 6, 7, 9, 11),
    B(0, 2, 4, 5, 7, 9, 10), B(0, 2, 3, 5, 7, 8, 10), B(0, 2, 4, 7, 9, 9, 9),  B(0, 3, 5, 7, 10, 10, 10)};
#undef B
const uint8_t EN_LOOP_LEN[EN_NLOOP] = {13, 17, 23, 29};
const uint8_t EN_DRUM_N[EN_NDRUM] = {16, 12, 7};

static const en_param_t PARAMS[P_NPARAMS] = {
    {"Voice 1", 0, 100, 60},  {"Voice 2", 0, 100, 65},  {"Voice 3", 0, 100, 45},  {"Pulse", 0, 100, 40},
    {"Density", 0, 100, 40},  {"Evolve", 0, 100, 35},   {"Drift", 0, 100, 25},    {"Hits", 0, 100, 35},
    {"Echo", 0, 100, 45},     {"Echo time", 0, 5, 3},   {"Reverb", 0, 100, 65},   {"Size", 0, 100, 80},
    {"Bright", 0, 100, 40},   {"Swell", 0, 100, 60},    {"V2 tone", 0, 100, 50}, {"Motion", 0, 100, 50},
    {"Tune", -50, 50, 0},     {"MIDI out", 0, 1, 1},    {"Tempo", 30, 120, 72},   {"Root", 0, 11, 2},
    {"Mode", 0, EN_NMODES - 1, 1}, {"V2 oct", -2, 2, 0},   {"V1 patch", 0, EN_NSRC - 1, 0},
    {"V3 oct", -1, 1, 0},     {"Glide", 0, 100, 20},    {"Fifth", 0, 100, 35},
    {"Swing", 0, 100, 0},     {"Hats", 0, 100, 50},
    {"V2 patch", 0, EN_NSRC - 1, EN_FACT_BELL}, {"V3 patch", 0, EN_NSRC - 1, EN_FACT_BASS},
#define MS_PARAMS(w, n)                                                                                            \
    {w " mod" n " depth", 0, 100, 0}, {w " mod" n " rate", 0, 100, 31},                                            \
    {w " mod" n " shape", 0, EN_NMODSHAPE - 1, 0}, {w " mod" n " target", 0, EN_NMODDEST - 1, 0},                 \
    {w " mod" n " spread", 0, 100, 100}, {w " mod" n " fade", 0, 100, 0},                                          \
    {w " mod" n " sync", 0, EN_NMODSYNC - 1, 0}, {w " mod" n " smooth", 0, 100, 0}
#define VOX_PARAMS(w)                                                                                              \
    {w " own", 0, 1, 0}, {w " index", -24, 24, 0}, MS_PARAMS(w, "1"), MS_PARAMS(w, "2"), MS_PARAMS(w, "3"),        \
    MS_PARAMS(w, "4"), MS_PARAMS(w, "5")
    VOX_PARAMS("V1"), VOX_PARAMS("V2"), VOX_PARAMS("V3"),
#define PL_PARAMS(w, role, dens, pace, gate)                                                                       \
    {w " env", 0, EN_NENV - 1, ENV_PATCH}, {w " gate", 5, 100, gate}, {w " role", 0, EN_NROLE - 1, role},          \
    {w " octave", -2, 2, 0}, {w " density", 0, 100, dens}, {w " pace", 0, 100, pace}, {w " glide", 0, 100, 20},    \
    {w " attack", 0, 100, 5}, {w " decay", 0, 100, 40}, {w " sustain", 0, 100, 70}, {w " release", 0, 100, 40}
    PL_PARAMS("V1", ROLE_CHORD, 100, 60, 70), PL_PARAMS("V2", ROLE_LOOPS, 40, 60, 25),
    PL_PARAMS("V3", ROLE_BASS, 35, 0, 80),
#undef PL_PARAMS
#undef VOX_PARAMS
#undef MS_PARAMS
};
_Static_assert(EN_NSLOT == 5, "VOX_PARAMS: five modulators");
_Static_assert(PL_N == 11, "PL_PARAMS: eleven a voice");
const uint8_t EN_VOX_PATCH[EN_NVOX] = {P_PADTONE, P_BELLPATCH, P_BASSPATCH};
static const char *const DEST_NAME[MOD_OP] = {"Timbre", "Ratio", "Metal", "Feedback", "Decay", "Pitch", "Space",
                                              "All", "Level", "LFO PM"};
static const char *const OPT_NAME[OPT_N] = {"Level", "Ratio", "Fine", "Atk", "Dec", "Sus", "Rel"};
const char *const EN_MODSHAPE_NAME[EN_NMODSHAPE] = {"Sine", "Triangle", "Saw up", "Saw down", "Square", "S&H",
                                                   "Walk", "Chaos", "Env"};
static const char *const SYNC_NAME[EN_NMODSYNC] = {"Free", "Note", "Tempo"};
static const char *const PATCH_NAME[EN_NFACT] = {
    "Rhodes", "Voices", "Glass", "Strings", "Warm", "Halo", "Bell", "Vibes", "Marimba", "Tubular", "Kalimba", "Celesta",
    "Sine", "Round", "Pick", "Sub", "Reed", "Fretless"};
static const char *const ENV_NAME[EN_NENV] = {"AMB", "Patch", "ADSR"};
static const char *const ROLE_NAME[EN_NROLE] = {"Chord", "Loops", "Bass"};
/* format 6's FM Ratio: the modulators' frequency times these (log2), harmonic, then three inharmonic */
#define NRATIO 9
static const float RATIO_MUL[NRATIO] = {0.0f, 1.0f, 1.5849625f, 2.0f, 2.3219281f, 2.8073549f, 0.4956951f,
                                        1.4646683f, 1.8073549f};
/* SYNC Tempo: RATE as a note value, a cycle of so many beats */
#define NDIV 12
static const char *const DIV_NAME[NDIV] = {"8 bars", "4 bars", "2 bars", "1 bar", "1/2.", "1/2", "1/4.", "1/4",
                                           "1/8.", "1/8", "1/8t", "1/16"};
static const float DIV_BEATS[NDIV] = {32.0f, 16.0f, 8.0f, 4.0f, 3.0f, 2.0f, 1.5f, 1.0f, 0.75f, 0.5f,
                                      0.33333333f, 0.25f};
static int div_of(int v) { return v * NDIV / 101; }
const en_param_t *en_param_info(int i) { return (i >= 0 && i < P_NPARAMS) ? &PARAMS[i] : &PARAMS[0]; }

static void copy9(const char *s, char *b)       /* a name of up to 9 characters */
{
    int k = 0;
    while (s[k] && k < 9) {
        b[k] = s[k];
        k++;
    }
    b[k] = 0;
}
void en_moddest_text(int d, char *b)
{
    d = (unsigned)d < EN_NMODDEST ? d : 0;
    if (d < MOD_OP) {
        copy9(DEST_NAME[d], b);
        return;
    }
    b[0] = 'O', b[1] = 'P', b[2] = (char)('1' + (d - MOD_OP) / OPT_N), b[3] = ' ';
    copy9(OPT_NAME[(d - MOD_OP) % OPT_N], b + 4);
    b[9] = 0;
}

void en_mod_legacy(int16_t *par, int depth, int rate, int chaos, int dest)
{
    int v;
    for (v = 0; v < EN_NVOX; v++) {             /* Chaos blended a wave (0), a walk (50) and the map (100) */
        par[P_MS(v, 0, MS_DEPTH)] = (int16_t)depth;
        par[P_MS(v, 0, MS_RATE)] = (int16_t)rate;
        par[P_MS(v, 0, MS_SHAPE)] = (int16_t)(chaos < 30 ? SHAPE_SINE : chaos < 85 ? SHAPE_WALK : SHAPE_CHAOS);
        par[P_MS(v, 0, MS_DEST)] = (int16_t)dest;
    }
}

/* the pieces: eight recipes for the whole garden (the FM-1's PRESETS, the web app's cards). Tune and
 * MIDI out are taken from here by neither: they belong to the player, not the piece. The recipe's last four are
 * the modulation as format 5 had it, one for all three voices (en_mod_legacy: each voice's first modulator); the
 * patches their own first (the pad's PT), the other modulators off */
/*                     Pad Bel Bas Pul  Den Evo Dri Hit  Ech ET Rev Siz  Bri Swe Ton Mot  Tun MI Tem Rt Md Oc  PT  BO Gli Fif  Swi Hat  MD MR Cha MT
 * (PT: the pad's colour, 0 Rhodes, 1 Voices; BO: the bass's octave; MD MR Cha MT: the modulation's depth, rate,
 * chaos and target, 0 Timbre 1 Ratio 2 Metal 3 Feedback 4 Decay 5 Pitch 6 Space 7 All) */
#define REC_N (P_BELLPATCH + 4)
static const struct {
    const char *name;
    int8_t v[REC_N];
} PIECES[EN_NPIECES] = {
    {"Garden",       {60, 65, 45, 40, 40, 35, 25, 35, 45, 3, 65, 80, 40, 60, 50, 50, 0, 1, 72, 2, 1, 0, 0, 0, 20, 35, 0, 50, 20, 27, 25, 0}},
    {"Departures",   {35, 75, 0, 0, 35, 30, 40, 0, 30, 3, 70, 85, 20, 55, 5, 40, 0, 1, 60, 1, 0, -1, 0, 0, 30, 25, 0, 40, 15, 16, 10, 5}},
    {"Voices",       {85, 0, 25, 0, 30, 30, 50, 0, 20, 5, 80, 90, 67, 70, 30, 55, 0, 1, 56, 5, 5, 0, 1, -1, 45, 30, 0, 40, 35, 23, 40, 3}},
    {"Afternoon",    {50, 65, 35, 25, 55, 45, 20, 30, 50, 2, 60, 75, 60, 50, 60, 55, 0, 1, 84, 7, 3, 0, 0, 0, 15, 40, 15, 65, 25, 39, 35, 6}},
    {"Night Bus",    {45, 55, 55, 60, 45, 50, 15, 55, 55, 1, 55, 70, 35, 40, 75, 70, 0, 1, 96, 9, 5, 0, 0, 0, 10, 45, 30, 70, 35, 47, 60, 2}},
    {"Shoreline",    {40, 60, 30, 0, 30, 40, 40, 0, 85, 4, 55, 95, 30, 80, 0, 85, 0, 1, 64, 4, 6, 0, 0, 0, 40, 20, 0, 30, 40, 19, 20, 4}},
    {"Glass",        {35, 80, 30, 20, 60, 55, 25, 25, 45, 2, 65, 80, 55, 45, 90, 50, 0, 1, 76, 3, 7, 1, 0, 1, 10, 30, 10, 85, 45, 31, 70, 1}},
    {"Tides",        {55, 60, 40, 35, 45, 75, 10, 40, 50, 3, 65, 85, 60, 60, 55, 95, 0, 1, 70, 10, 4, 0, 1, 0, 25, 55, 10, 50, 50, 31, 50, 7}},
};

const char *en_piece_name(int i) { return PIECES[(unsigned)i < EN_NPIECES ? i : 0].name; }
void en_piece(int i, int16_t *par, uint16_t *pool)
{
    int p;
    const int8_t *r;
    i = (unsigned)i < EN_NPIECES ? i : 0;
    r = PIECES[i].v;
    for (p = 0; p < P_NPARAMS; p++)
        par[p] = p < P_BELLPATCH ? r[p] : PARAMS[p].def;
    en_mod_legacy(par, r[P_BELLPATCH], r[P_BELLPATCH + 1], r[P_BELLPATCH + 2], r[P_BELLPATCH + 3]);
    en_upgrade(par);
    *pool = EN_MODE_POOL[r[P_MODE]];
}

void en_upgrade(int16_t *par)
{
    int v;
    for (v = 0; v < EN_NVOX; v++) {             /* each voice the layer it was, played as it is now */
        par[P_PL(v, PL_ROLE)] = (int16_t)v;
        par[P_PL(v, PL_GLIDE)] = par[P_GLIDE];
    }
    par[P_PL(VOX_PAD, PL_OCT)] = 0;
    par[P_PL(VOX_PAD, PL_DENS)] = 100;
    par[P_PL(VOX_PAD, PL_PACE)] = par[P_SWELL];
    par[P_PL(VOX_BELLS, PL_OCT)] = par[P_BOCT];
    par[P_PL(VOX_BELLS, PL_DENS)] = par[P_DENSITY];
    par[P_PL(VOX_BELLS, PL_PACE)] = 60;
    par[P_PL(VOX_BASS, PL_OCT)] = par[P_BASSOCT];
    par[P_PL(VOX_BASS, PL_DENS)] = par[P_FIFTH];
    par[P_PL(VOX_BASS, PL_PACE)] = 0;
}

static const char *const ETIME_NAME[6] = {"1/4.", "1/2", "1/2.", "1 bar", "2 bars", "4 bars"};
static const float ETIME_BEATS[6] = {1.5f, 2.0f, 3.0f, 4.0f, 8.0f, 16.0f};

static char *itoa_s(int v, char *b)
{
    char t[8];
    int n = 0, neg = v < 0;
    if (neg)
        v = -v;
    do {
        t[n++] = (char)('0' + v % 10);
        v /= 10;
    } while (v && n < 6);
    if (neg)
        *b++ = '-';
    while (n)
        *b++ = t[--n];
    *b = 0;
    return b;
}

static float drift_secs(int v) { return 20.0f * fm_exp2f((float)v * 0.039069f); }   /* 20 s .. 5 min */
static float mod_secs(int v) { return 60.0f * fm_exp2f((float)v * -0.09f); }         /* 60 s .. 0.12 s */
static float fade_secs(int v) { return 8.0f * (float)(v * v) * 0.0001f; }            /* 0 .. 8 s */
static float smooth_secs(int v) { return 0.002f + 0.498f * (float)(v * v) * 0.0001f; }   /* 2 ms .. 0.5 s */

static int16_t par[P_NPARAMS];                  /* the engine's own copy (the render's) */

static char *tenths(int ds, char *b)            /* tenths as "2.7", ten and over as "12" */
{
    b = itoa_s(ds / 10, b);
    if (ds < 100) {
        *b++ = '.';
        *b++ = (char)('0' + ds % 10);
    }
    *b = 0;
    return b;
}

void en_param_text(int i, int v, char *b)
{
    static const char *const ONOFF[2] = {"Off", "On"};
    const char *s = 0;
    int vox = i >= P_VOX && i < P_PLAY, vv = vox ? (i - P_VOX) / VX_N : -1, vk = vox ? (i - P_VOX) % VX_N : -1,
        ms = vk >= VX_SLOT ? (vk - VX_SLOT) / MS_N : -1, mk = vk >= VX_SLOT ? (vk - VX_SLOT) % MS_N : -1;
    if (i == P_ETIME)
        s = ETIME_NAME[(unsigned)v < 6u ? v : 0];
    else if (i == P_MIDI)
        s = ONOFF[v ? 1 : 0];
    else if (i == P_PADTONE || i == P_BELLPATCH || i == P_BASSPATCH) {
        if ((unsigned)v < EN_NFACT) {
            s = PATCH_NAME[v];
        } else {                                 /* a bank's: "B01-05" (the screen shows its name) */
            v -= EN_NFACT;
            *b++ = 'B';
            *b++ = (char)('0' + (v / 32 + 1) / 10);
            *b++ = (char)('0' + (v / 32 + 1) % 10);
            *b++ = '-';
            *b++ = (char)('0' + (v % 32 + 1) / 10);
            *b++ = (char)('0' + (v % 32 + 1) % 10);
            *b = 0;
            return;
        }
    } else if (vk == VX_OWN)
        s = v ? "Edited" : "As loaded";
    else if (mk == MS_DEST) {
        en_moddest_text(v, b);
        return;
    } else if (mk == MS_SHAPE)
        s = EN_MODSHAPE_NAME[(unsigned)v < EN_NMODSHAPE ? v : 0];
    else if (mk == MS_SYNC)
        s = SYNC_NAME[(unsigned)v < EN_NMODSYNC ? v : 0];
    else if (mk == MS_RATE && par[P_MS(vv, ms, MS_SYNC)] == SYNC_TEMPO)
        s = DIV_NAME[div_of(v < 0 ? 0 : v > 100 ? 100 : v)];
    else if (mk == MS_FADE && v == 0)
        s = "Off";
    else if (i >= P_PLAY && (i - P_PLAY) % PL_N == PL_ENV)
        s = ENV_NAME[(unsigned)v < EN_NENV ? v : 0];
    else if (i >= P_PLAY && (i - P_PLAY) % PL_N == PL_ROLE)
        s = ROLE_NAME[(unsigned)v < EN_NROLE ? v : 0];
    else if (i >= P_PLAY && (i - P_PLAY) % PL_N == PL_PACE && par[i - PL_PACE + PL_ROLE] != ROLE_CHORD) {
        static const char *const STEP[2][5] = {{"1/16", "1/8", "1/4", "1/2", "1/2"},
                                               {"Chord", "1 bar", "1/2", "1/4", "1/8"}};
        const int loops = par[i - PL_PACE + PL_ROLE] == ROLE_LOOPS;
        s = STEP[!loops][loops ? v / 25 : v < 20 ? 0 : v < 40 ? 1 : v < 60 ? 2 : v < 80 ? 3 : 4];
    } else if (i == P_MODE)
        s = EN_MODE_NAME[(unsigned)v < EN_NMODES ? v : 0];
    else if (i == P_ROOT)
        s = EN_NOTE_NAME[(unsigned)v < 12u ? v : 0];
    if (s) {
        copy9(s, b);
        return;
    }
    if (i == P_DRIFT) {
        int sec = (int)(drift_secs(v) + 0.5f);
        if (sec < 60) {
            b = itoa_s(sec, b);
            *b++ = 's';
            *b = 0;
        } else {
            b = itoa_s(sec / 60, b);
            *b++ = ':';
            *b++ = (char)('0' + sec % 60 / 10);
            *b++ = (char)('0' + sec % 10);
            *b = 0;
        }
        return;
    }
    if (mk == MS_RATE || mk == MS_FADE) {        /* a cycle: 60 s .. 0.12 s; the fade: to 8 s */
        b = tenths((int)((mk == MS_RATE ? mod_secs(v) : fade_secs(v)) * 10.0f + 0.5f), b);
        *b++ = 's';
        *b = 0;
        return;
    }
    if (i >= P_PLAY && (i - P_PLAY) % PL_N == PL_PACE) {   /* a chord's breath, seconds a cycle */
        b = itoa_s(4 + (12 * v + 50) / 100, b);
        *b++ = 's';
        *b = 0;
        return;
    }
    if (i >= P_PLAY && (i - P_PLAY) % PL_N == PL_GLIDE) {
        b = itoa_s(20 + 12 * v, b);
        *b++ = 'm';
        *b++ = 's';
        *b = 0;
        return;
    }
    if (i >= P_PLAY && (i - P_PLAY) % PL_N == PL_GATE) {
        b = itoa_s(v, b);
        *b++ = '%';
        *b = 0;
        return;
    }
    if (mk == MS_SMOOTH) {                       /* 2 ms .. 500 ms */
        b = itoa_s((int)(smooth_secs(v) * 1000.0f + 0.5f), b);
        *b++ = 'm';
        *b++ = 's';
        *b = 0;
        return;
    }
    if (vk == VX_INDEX) {
        if (v > 0)
            *b++ = '+';
        b = itoa_s(v, b);
        *b++ = 'd';
        *b++ = 'B';
        *b = 0;
        return;
    }
    if (i == P_SWING) {                          /* as a swing percentage: 50% (straight) .. 75% */
        b = itoa_s(50 + v / 4, b);
        *b++ = '%';
        *b = 0;
        return;
    }
    if ((i == P_TUNE || i == P_BOCT || i == P_BASSOCT || (i >= P_PLAY && (i - P_PLAY) % PL_N == PL_OCT)) && v > 0)
        *b++ = '+';
    itoa_s(v, b);
}

/* ------------------------------------------------------- the pure parts --- */
uint32_t en_euclid(int k, int n, int rot)
{
    /* Bjorklund's algorithm (as Toussaint's "The Euclidean Algorithm Generates Traditional Musical
     * Rhythms"): k groups "x" and n - k groups ".", the remainders folded onto the heads until at most
     * one remains. Groups are bit strings, LSB first: (bits, length). */
    uint32_t gb[32], m = 0;
    uint8_t gl[32];
    int ng, heads, tails, i, pos = 0;
    if (n <= 0 || n > 32 || k <= 0)
        return 0;
    if (k > n)
        k = n;
    for (i = 0; i < n; i++) {
        gb[i] = i < k ? 1u : 0u;
        gl[i] = 1;
    }
    ng = n;
    heads = k;
    tails = n - k;
    while (tails > 1 && heads > 0) {
        int pairs = heads < tails ? heads : tails;
        for (i = 0; i < pairs; i++) {           /* a tail onto each head */
            gb[i] |= gb[ng - pairs + i] << gl[i];
            gl[i] = (uint8_t)(gl[i] + gl[ng - pairs + i]);
        }
        if (heads > tails) {                    /* the unpaired heads become the new tails */
            tails = heads - pairs;
            heads = pairs;
        } else {
            tails -= pairs;
            heads = pairs;
        }
        ng = heads + tails;
    }
    for (i = 0; i < ng; i++) {
        uint32_t j;
        for (j = 0; j < gl[i]; j++)
            if (gb[i] >> j & 1u)
                m |= 1u << (((pos + (int)j + rot) % n + n) % n);
        pos += gl[i];
    }
    return m;
}

int en_scale(uint32_t pool, int *steps)
{
    int i, m = 0;
    pool |= 1u;
    for (i = 0; i < 12; i++)
        if (pool >> i & 1u)
            steps[m++] = i;
    return m;
}

int en_chord_tones(uint32_t pool, int root, int deg, int *pcs)
{
    static const uint8_t OFF[7] = {0, 2, 4, 6, 1, 3, 5};   /* stacked thirds, then what's left */
    int st[12], m = en_scale(pool, st), want, n = 0, i, j;
    want = m >= 6 ? 4 : 3;
    deg = ((deg % m) + m) % m;
    for (i = 0; i < 7 && n < want; i++) {
        int pc = (root + st[(deg + OFF[i]) % m]) % 12, dup = 0;
        for (j = 0; j < n; j++)
            dup |= pcs[j] == pc;
        if (!dup)
            pcs[n++] = pc;
    }
    return n;
}

static int nearest(int from, int pc, int lo, int hi)    /* the note of class pc nearest from, in lo..hi */
{
    int n = from + ((pc - from) % 12 + 12) % 12;          /* at or above */
    if (n - from > 6)
        n -= 12;
    while (n < lo)
        n += 12;
    while (n > hi)
        n -= 12;
    return n;
}

void en_voice_lead(const int *from, const int *pcs, int nt, int *to, int lo, int hi)
{
    static const uint8_t PERM[24][4] = {
        {0, 1, 2, 3}, {0, 1, 3, 2}, {0, 2, 1, 3}, {0, 2, 3, 1}, {0, 3, 1, 2}, {0, 3, 2, 1},
        {1, 0, 2, 3}, {1, 0, 3, 2}, {1, 2, 0, 3}, {1, 2, 3, 0}, {1, 3, 0, 2}, {1, 3, 2, 0},
        {2, 0, 1, 3}, {2, 0, 3, 1}, {2, 1, 0, 3}, {2, 1, 3, 0}, {2, 3, 0, 1}, {2, 3, 1, 0},
        {3, 0, 1, 2}, {3, 0, 2, 1}, {3, 1, 0, 2}, {3, 1, 2, 0}, {3, 2, 0, 1}, {3, 2, 1, 0}};
    int t[4], p, v, best = 1 << 30;
    for (v = 0; v < 4; v++)
        t[v] = pcs[v < nt ? v : 0];                        /* three tones: the root doubled */
    for (p = 0; p < 24; p++) {
        int c = 0, n[4], a, b;
        for (v = 0; v < 4; v++) {
            n[v] = nearest(from[v], t[PERM[p][v]], lo, hi);
            c += n[v] > from[v] ? n[v] - from[v] : from[v] - n[v];
        }
        for (a = 0; a < 4; a++)                            /* no two voices on one note */
            for (b = a + 1; b < 4; b++)
                if (n[a] == n[b])
                    c += 40;
        if (c < best) {
            best = c;
            for (v = 0; v < 4; v++)
                to[v] = n[v];
        }
    }
}

const char *en_chord_name(const int *pcs, int n, char *buf)
{
    uint32_t iv = 0;
    int i, k = 0, third, fifth;
    const char *q = "", *r = EN_NOTE_NAME[pcs[0] % 12];
    for (i = 1; i < n; i++)
        iv |= 1u << ((pcs[i] - pcs[0] + 12) % 12);
    third = iv >> 4 & 1u ? 4 : iv >> 3 & 1u ? 3 : 0;
    fifth = iv >> 7 & 1u ? 7 : iv >> 6 & 1u ? 6 : iv >> 8 & 1u ? 8 : 0;
    if (third == 4) {
        q = fifth == 8 ? "aug" : iv >> 11 & 1u ? "M7" : iv >> 10 & 1u ? "7" : iv >> 9 & 1u ? "6"
            : iv >> 2 & 1u ? "add9" : "";
    } else if (third == 3) {
        if (fifth == 6)
            q = iv >> 10 & 1u ? "m7b5" : iv >> 9 & 1u ? "dim7" : "dim";
        else
            q = iv >> 10 & 1u ? "m7" : iv >> 11 & 1u ? "mM7" : iv >> 9 & 1u ? "m6" : iv >> 2 & 1u ? "madd9" : "m";
    } else {
        q = iv >> 5 & 1u ? (iv >> 10 & 1u ? "7sus4" : "sus4") : iv >> 2 & 1u ? "sus2" : "5";
    }
    while (*r)
        buf[k++] = *r++;
    while (*q)
        buf[k++] = *q++;
    buf[k] = 0;
    return buf;
}

/* ------------------------------------------------------------ commands --- */
enum { C_SET, C_POOL, C_PLAY, C_NEXT, C_FREEZE, C_SEED, C_PANIC, C_CLOCK, C_REGROW, C_PBYTE, C_PDONE, C_PLOAD };
_Static_assert(P_NPARAMS <= 256, "C_SET carries a parameter's index in 8 bits");
_Static_assert(FP_SIZE <= 256, "C_PBYTE carries a patch byte's offset in 8 bits");
#define QN 1024u                              /* a piece (project_apply) posts every parameter and patch byte at once */
static uint32_t q[QN];
static volatile uint32_t q_w, q_r;
static void post(uint32_t c, int a, int b)
{
    uint32_t w = q_w;
    if (w - q_r >= QN)
        return;                                   /* full: dropped (never, at the UI's rate) */
    q[w % QN] = c << 24 | ((uint32_t)a & 0xFFu) << 16 | ((uint32_t)b & 0xFFFFu);
    BARRIER();
    q_w = w + 1;
}
void en_set(int p, int v) { post(C_SET, p, v); }
void en_pool(uint32_t pool) { post(C_POOL, 0, (int)(pool & 0xFFFu)); }
void en_play(int on) { post(C_PLAY, on, 0); }
void en_next(void) { post(C_NEXT, 0, 0); }
void en_freeze(int on) { post(C_FREEZE, on, 0); }
void en_seed(uint32_t s) { post(C_SEED, 0, (int)(s & 0xFFFFu)); }
void en_panic(void) { post(C_PANIC, 0, 0); }
void en_clock(int msg) { post(C_CLOCK, msg, 0); }
void en_regrow(void) { post(C_REGROW, 0, 0); }
void en_patch_byte(int voice, int off, int v) { post(C_PBYTE, voice, off << 8 | (v & 0xFF)); }
void en_patch_done(int voice, int fresh) { post(C_PDONE, voice, fresh); }
/* each voice's patch mailbox: the UI writes it whole between two counts (odd while writing), the engine copies it on
 * C_PLOAD and keeps the copy only if the count stood still and even (a write it raced is followed by its own C_PLOAD) */
static struct {
    volatile uint32_t seq;
    uint8_t p[FP_SIZE];
} pbox[EN_NVOX];
void en_patch_load(int voice, const uint8_t *p, int fresh)
{
    int k;
    if ((unsigned)voice >= EN_NVOX)
        return;
    pbox[voice].seq++;
    BARRIER();
    for (k = 0; k < FP_SIZE; k++)
        pbox[voice].p[k] = p[k];
    BARRIER();
    pbox[voice].seq++;
    post(C_PLOAD, voice, fresh);
}

/* -------------------------------------------------------------- state --- */
volatile uint8_t en_playing, en_frozen, en_ext;
volatile uint8_t en_chord[EN_NPAD], en_chord_root, en_chord_n;
volatile float en_drift, en_pad_level, en_bass_level, en_tempo;
volatile uint8_t en_loop_pos[EN_NLOOP];
volatile int8_t en_loop_note[EN_NLOOP][EN_LOOP_MAX];
volatile float en_loop_glow[EN_NLOOP];
volatile uint8_t en_drum_pos[EN_NDRUM];
volatile uint32_t en_drum_pat[EN_NDRUM];
volatile float en_drum_glow[EN_NDRUM];
volatile uint32_t en_bars;
volatile float en_arc;
volatile uint8_t en_pulse_rest;
volatile uint8_t en_pad_note[EN_NPAD];
volatile float en_pad_amp[EN_NPAD];

static uint32_t pool = 0xAADu;                  /* Ionian until told */
static int nsteps, steps[12];                   /* the scale */
static int deg;                                 /* the chord's scale degree */
static float tunefac = 1.0f;
static uint32_t rng = 0x9E3779B9u;

static uint32_t rnd(void)                       /* Marsaglia's xorshift (Xorshift RNGs, 2003): shifts 13, 17, 5 */
{
    uint32_t x = rng;
    x = x ^ (x << 13);
    x = x ^ (x >> 17);
    rng = x ^ (x << 5);
    return rng;
}
static float frand(void) { return (float)(rnd() >> 8) * (1.0f / 16777216.0f); }
static int irand(int n) { return n > 0 ? (int)((rnd() >> 8) % (uint32_t)n) : 0; }

static void midi(uint32_t st, uint32_t d1, uint32_t d2)
{
    if (par[P_MIDI])
        en_midi_out(st, d1, d2);
}

static float mtof(float n) { return 440.0f * tunefac * fm_exp2f((n - 69.0f) * (1.0f / 12.0f)); }

/* a sine table: 1024 points a turn, plus one */
#define SN 1024
static float sint[SN + 1];
static inline float sine(float ph)              /* ph in turns, any value >= 0 */
{
    float x = (ph - (float)(int)ph) * (float)SN;
    int i = (int)x;
    float f = x - (float)i;
    return sint[i] + (sint[i + 1] - sint[i]) * f;
}

/* ------------------------------------------------- slow random motion --- */
#define NMOD 9                                  /* 0..5 the pads', 6..8 the echo's */
#define NMOD_PAD 6
static struct {
    float v, tgt, rate;
    uint32_t left;                              /* samples until a new target */
} mod[NMOD];

static void mods_step(uint32_t n)
{
    int i;
    for (i = 0; i < NMOD; i++) {
        if (mod[i].left <= n) {
            float secs = 7.0f + 33.0f * frand();
            mod[i].tgt = frand() * 2.0f - 1.0f;
            mod[i].left = (uint32_t)(secs * EN_SR);
            mod[i].rate = (float)n / (secs * EN_SR * 0.5f);
        } else {
            mod[i].left -= n;
        }
        mod[i].v += (mod[i].tgt - mod[i].v) * fm_minf(mod[i].rate, 1.0f);
    }
}
static float motion;                            /* 0..1: Motion */

/* ------------------------------------------------------------- FM6 voices --- */
/* Every note is an fm6_note_t of a 155-byte patch (fm6_core.c FP_*), rendered a block of FM6_N (32) samples at
 * a time into fm_buf, which the layers read sample by sample (fm_pull): a note starts at the next block, at most
 * 0.7 ms late. Each voice plays its own patch (vpat), sent whole from the main loop (vstage, applied on
 * en_patch_load): one of AMB-1's eighteen (en_factory: pad_bank, bell_bank, bass_bank) or a DX7 voice of a bank,
 * edited operator by operator or not. One carrier at full is 2^25 out of fm6_core: FM_SCALE makes it 1.0, AMB-1's
 * level for a voice; the voice's gain the patch's own (patch_gain). */
#define FM_SCALE (1.0f / 33554432.0f)
#define FM_DB (16777216.0f / 6.0206f)          /* a level offset of the modulators, Q24 log2 a dB */
enum { FB_PADL, FB_PADR, FB_BELLL, FB_BELLR, FB_BELLE, FB_BASSL, FB_BASSR, FB_N };
static float fm_buf[FB_N][FM6_N];
static uint32_t fm_pos = FM6_N;
static uint8_t vpat[EN_NVOX][FP_SIZE + 1];      /* each voice's patch, as the notes play it */
static uint8_t vstage[EN_NVOX][FP_SIZE + 1];    /* its bytes as they arrive, into vpat when applied */

static int32_t fm_logfreq(float note)          /* log2 Hz, Q24, of a MIDI pitch (a fraction too) and Tune */
{
    int32_t k = (int32_t)note;
    float fr = note - (float)k + (float)par[P_TUNE] * 0.01f;
    return FM6_LOGF0 + k * 1398101 + (int32_t)(fr * 1398101.33f);   /* (1 << 24) / 12 a semitone */
}

/* the patch's TRANSPOSE (the DX7's: 24 is C3, as played), in semitones */
static int32_t fm_trnsp(const uint8_t *p) { return (int32_t)p[FP_TRNSP] - 24; }

/* a note's next block into x (float, its DC blocked as JIANT's FM6 does: FM puts sidebands at 0 Hz); 0: every
 * carrier silent (x untouched) */
static int fm_voice(fm6_note_t *n, const uint8_t *p, float note, int32_t lfo_v, int32_t lfo_d, float bloom_db,
                    int32_t fb_add, const int32_t *dt, const int32_t *ol, float *x)
{
    int32_t bus[FM6_N], fb = (int32_t)(p[FP_FB] & 7u) * 256 + fb_add;   /* feedback in 1/256 steps */
    uint32_t i;
    if (!fm6_note_compute(n, p, bus, lfo_v, lfo_d, fm_logfreq(note + (float)fm_trnsp(p)), p[FP_ALG] & 31u,
                          fb < 0 ? 0 : fb > 9 * 256 ? 9 * 256 : fb, dt, (int32_t)(bloom_db * FM_DB), ol))
        return 0;
    for (i = 0; i < FM6_N; i++) {
        int32_t s = bus[i] - n->dc;
        n->dc += s >> 10;
        x[i] = (float)s * FM_SCALE;
    }
    return 1;
}

/* a note-on of patch p (fm6_note_init) at MIDI note `note` transposed as the patch says: its key scaling and rate
 * scaling as a DX7 has them, its pitch then from fm_voice (which transposes it the same) */
static void fm_note_on(fm6_note_t *n, const uint8_t *p, int32_t note, int32_t vel, int fresh)
{
    fm6_note_init(n, p, note + fm_trnsp(p), vel, fresh);
}

static float breath(float t)                    /* rise, hang, fall, a short rest */
{
    if (t < 0.4f)
        return 0.5f - 0.5f * fm_cosf(FM_PI * t * 2.5f);
    if (t < 0.55f)
        return 1.0f;
    if (t < 0.9f)
        return 0.5f + 0.5f * fm_cosf(FM_PI * (t - 0.55f) * (1.0f / 0.35f));
    return 0.0f;
}

/* --------------------------------------------------------------- the voices --- */
/* Three voices, alike: each plays its own patch in a ROLE of three (amb.h PL_*), set the same way (its level, ROLE,
 * OCTAVE, DENSITY, PACE, GLIDE, ENVELOPE, GATE and an ADSR):
 *   Chord  breathing notes, as the sung tape loops of Music for Airports' "2/1": up to four (DENSITY), each swelling
 *          in, hanging and fading on its own cycle (unequal lengths, PACE's), taking its note of the chord as it
 *          starts, so the chord forms and dissolves rather than sitting there; the breath and the arc bloom it
 *   Loops  four loops of 13, 17, 23 and 29 steps (a step PACE's, 1/16 .. 1/2), their notes grown and pruned by DENSITY
 *          and Evolve, each struck as its step comes round, each loop its shade of the patch
 *   Bass   the chord's root, one note gliding (GLIDE), leaning on the fifth halfway (DENSITY), struck each chord or, by
 *          PACE, each bar .. eighth
 * ENVELOPE: AMB-1's own (the breath; the loops let go after a beat; the bass held, faded in), the patch's (each note
 * struck and let go at GATE: its envelopes as a DX7 plays them) or the ADSR (the same, its carriers' envelope the four
 * knobs). Each role sends where AMB-1's layer did: a chord to the pad's dark bus, the loops to the bells' (the most
 * echo), the bass to the mix. A voice changing its role fades out (20 ms), changes, and fades in. */
#define NPOLY EN_NPAD                           /* a voice's notes */
typedef struct {
    fm6_note_t fm;
    fm6_lfo_t lfo;
    uint8_t vp[FP_SIZE + 1];                    /* the note's own copy of its voice's patch, made as it strikes */
    float note, target;                         /* MIDI: the note sounding, the next one (a chord, the bass) */
    float t, rate, peak;                        /* a chord: t 0..1 through the breath (per block), its pace, strength */
    float amp, tgt, bloom;                      /* its amplitude, ramped to tgt; its modulators' level (dB) */
    float pan, gl, gr, pl, pr;                  /* its place; the gains it had last block (ramped from) */
    float hf;                                   /* a chord: a handoff's fade (1 normal) */
    float vel, out, lv;                         /* the loops: the strike's velocity, the patch's level, MOD's LEVEL */
    int32_t wait, held;                         /* the loops: samples until it rings (a hand), until it is let go */
    uint8_t midi_note, hand, gen, up;           /* hand: 1 fading out of the old key, 2 into the new; gen: the patch it
                                                 * sings (its voice's gen as it started); up: its key let go */
    uint8_t on, rung, loop;                     /* the loops: sounding, started, its loop */
} vnote_t;
typedef struct {
    vnote_t n[NPOLY];
    uint8_t role, gen;                          /* the role playing (PL_ROLE once faded); a new patch: gen one more */
    float vol, gain, tg, fo;                    /* Level; the patch's level; the transport's fade; the role's fade */
    float glide_k;                              /* 1 / GLIDE's time, in samples */
    uint8_t pc, rest, strike, xs;               /* the bass: its pitch class; sitting this chord out; struck next
                                                 * block; fading out for a new patch */
    int rs;                                     /* the bass: blocks to a new patch's restrike (the knob at rest) */
    int32_t held;                               /* the bass: samples until its key is let go; 0 let go */
    float xf;                                   /* the bass: the restrike's fade */
    int8_t loop_note[EN_NLOOP][EN_LOOP_MAX];    /* the loops: each loop's notes (scale steps from the chord), -1 rest */
    uint8_t loop_vel[EN_NLOOP][EN_LOOP_MAX], loop_pos[EN_NLOOP], loop_rest[EN_NLOOP];
    uint8_t loop_patch[EN_NLOOP][FP_SIZE + 1];  /* each loop's shade of the patch */
} voice_t;
static voice_t vc[EN_NVOX];
#define PL(v, k) (par[P_PL(v, PL_##k)])
#define GATE_F(v) ((float)PL(v, GATE) * 0.01f)
#define GEN(v, k) ((v) * NPOLY + (k))           /* a note's MOD generator */
static float pad_gate, bright, btone;           /* the transport (1 playing); the pieces' Bright and Bell tone */
static const float PAD_PAN[EN_NPAD] = {-0.55f, -0.18f, 0.18f, 0.55f};
static const float BREATH[EN_NPAD] = {1.0f, 1.21f, 1.43f, 1.67f};    /* unequal cycles, as 2/1's 12..20 s loops */
static const float START_T[EN_NPAD] = {0.0f, 0.9f, 0.82f, 0.95f};    /* the first breaths overlap */
static float arcm;                              /* the slow arc, -1 sparse .. +1 full, times Motion */
static const uint8_t LOOP_BASE[EN_NLOOP] = {72, 67, 79, 62};     /* each loop's register */
static const float LOOP_PAN[EN_NLOOP] = {-0.6f, 0.35f, 0.7f, -0.25f};
/* the bells' modulator ratios (1, 2, 3, 3.5, 4.77), as FM6 coarse and fine: coarse * (1 + fine / 100) */
static const uint8_t RATIO_FC[5] = {1, 2, 3, 3, 4}, RATIO_FF[5] = {0, 0, 0, 17, 19};
#define BASS_RS 138                             /* a new patch for the bass: struck anew once the knob rests (0.1 s) */
static int first_role(int r)                    /* the first voice of role r (the garden shows it); -1 none */
{
    int v;
    for (v = 0; v < EN_NVOX; v++)
        if (vc[v].role == r)
            return v;
    return -1;
}
static int loops_step(int v)                    /* the loops' step, sixteenths: 1/16, 1/8, 1/4, 1/2 */
{
    const int p = PL(v, PACE);
    return p < 25 ? 1 : p < 50 ? 2 : p < 75 ? 4 : 8;
}
static int bass_step(int v)                     /* the bass struck again each so many sixteenths; 0: each chord */
{
    const int p = PL(v, PACE);
    return p < 20 ? 0 : p < 40 ? 16 : p < 60 ? 8 : p < 80 ? 4 : 2;
}

static void midi(uint32_t st, uint32_t d1, uint32_t d2);
static const uint8_t *mod_patch(int g, const uint8_t *src, uint8_t *dst);
static void gen_note(int g);
/* the ADSR: the carriers' envelope from the four knobs (attack, decay to the sustain's level, release), as DX7 rates
 * (99 at once .. 24 over seconds) and levels; the modulators keep the patch's */
static void adsr_patch(int v, uint8_t *p)
{
    const uint32_t car = fm6_carriers(p[FP_ALG] & 31u);
    const int a = PL(v, A), d = PL(v, D), su = PL(v, S), r = PL(v, R);
    uint32_t j;
    for (j = 0; j < 6u; j++) {
        uint8_t *o = p + j * FP_OP;
        if (!(car >> j & 1u))
            continue;
        o[FP_R1] = (uint8_t)(99 - a * 3 / 4);
        o[FP_R1 + 1] = (uint8_t)(99 - d * 3 / 4);
        o[FP_R1 + 2] = 99;
        o[FP_R1 + 3] = (uint8_t)(99 - r * 3 / 4);
        o[FP_L1] = 99;
        o[FP_L1 + 1] = o[FP_L1 + 2] = (uint8_t)(su * 99 / 100);
        o[FP_L1 + 3] = 0;
    }
}
/* note k of voice v struck: its own copy of src (MOD's envelopes, the ADSR), its LFO again (Key sync: from the top) */
static void note_strike(int v, int k, const uint8_t *src, int32_t note, int32_t vel, int fresh)
{
    vnote_t *n = &vc[v].n[k];
    gen_note(GEN(v, k));
    mod_patch(GEN(v, k), src, n->vp);
    if (PL(v, ENV) == ENV_ADSR)
        adsr_patch(v, n->vp);
    fm_note_on(&n->fm, n->vp, note, vel, fresh);
    fm6_lfo_reset(&n->lfo, n->vp);
    fm6_lfo_key(&n->lfo);
    n->up = 0;
}
/* a sounding note takes its voice's edited patch now (fm6_note_refresh: no restart) */
static void note_refresh(int v, int k, const uint8_t *src)
{
    vnote_t *n = &vc[v].n[k];
    mod_patch(GEN(v, k), src, n->vp);
    if (PL(v, ENV) == ENV_ADSR)
        adsr_patch(v, n->vp);
    fm6_note_refresh(&n->fm, n->vp);
    fm6_lfo_reset(&n->lfo, n->vp);
}

/* ------------------------------------------------------------------ a chord --- */
static int chord_n(int v) { return 1 + (PL(v, DENS) * 3 + 50) / 100; }   /* its voices sounding, 1..4 */
static void chord_breathe(int v, int i)         /* a new breath: its length, its strength, its note */
{
    vnote_t *n = &vc[v].n[i];
    float secs = (4.0f + 0.12f * (float)PL(v, PACE)) * BREATH[i] * (0.85f + 0.3f * frand());
    n->rate = 32.0f / (secs * EN_SR);
    n->peak = 0.6f + 0.4f * frand();
    if (en_playing && n->midi_note)
        midi(0x80u | (uint32_t)v, n->midi_note, 0);
    n->note = n->target;
    n->gen = vc[v].gen;
    n->midi_note = (uint8_t)n->target;
    /* the note struck anew (between breaths, or as a handoff fades in: from silence then, phases from 0) */
    note_strike(v, i, vpat[v], (int32_t)n->note, 100, n->amp < 0.01f);
    if (en_playing)
        midi(0x90u | (uint32_t)v, n->midi_note, (uint32_t)(50.0f + 40.0f * n->peak));
}

/* a new key (piece, root, scale) or patch: the notes still singing the old one fade out in about a second and
 * breathe again on the new chord; a note at rest just takes its new one. Chord drift doesn't do this: there the old
 * notes are meant to hang over */
#define HAND_STEP (32.0f / (1.0f * EN_SR))
static void chord_handoff(int v)
{
    int i;
    for (i = 0; i < NPOLY; i++) {
        vnote_t *n = &vc[v].n[i];
        if (n->amp > 0.002f && ((float)n->midi_note != n->target || n->gen != vc[v].gen))
            n->hand = 1;
        else if (!n->hand)
            n->note = n->target;
    }
}

/* a chord change: a note the new chord doesn't have fades out in about a second and breathes in on its new note; a
 * note the chords share holds to the end of its breath */
static int chord_pcs[4], chord_nt;
static void chord_leave(int v)
{
    int i, k;
    for (i = 0; i < NPOLY; i++) {
        vnote_t *n = &vc[v].n[i];
        int pc = n->midi_note % 12, shared = 0;
        for (k = 0; k < chord_nt; k++)
            shared |= chord_pcs[k] == pc;
        if (n->amp > 0.002f && !shared && !n->hand)
            n->hand = 1;
        else if (n->amp <= 0.002f && !n->hand)
            n->note = n->target;
    }
}

static void chord_coefs(int v, int i)           /* every FM6 block (32 samples): the breath, the bloom */
{
    vnote_t *n = &vc[v].n[i];
    float br;
    if (n->hand == 1) {                         /* out of the old key */
        n->hf -= HAND_STEP;
        if (n->hf <= 0.0f) {
            n->hf = 0.0f;
            n->hand = 2;
            n->t = 0.15f;                       /* a breath already rising, on the new chord */
            chord_breathe(v, i);
        }
    } else if (n->hand == 2) {                  /* into the new */
        n->hf += HAND_STEP * 0.6f;
        if (n->hf >= 1.0f) {
            n->hf = 1.0f;
            n->hand = 0;
        }
    }
    n->t += n->rate;
    if (n->t >= 1.0f) {
        n->t -= 1.0f;
        chord_breathe(v, i);
    }
    br = breath(n->t);
    if (PL(v, ENV) != ENV_AMB) {                /* the patch's envelopes (or the ADSR): struck at the breath, let go
                                                 * at GATE of it (its sounding part, 0.9), the level steady */
        if (!n->up && n->t >= GATE_F(v) * 0.9f) {
            fm6_note_key(&n->fm, n->vp, 0);
            n->up = 1;
        }
        br = 0.7f;
        n->tgt = n->peak * vc[v].tg * n->hf;
    } else {
        n->tgt = br * n->peak * vc[v].tg * n->hf;
    }
    /* the bloom: the modulators about -15 .. +12 dB from the patch, with Bright, the breath, the arc and a slow drift
     * (as the vowel the Voices had moved) */
    n->bloom = -10.0f + 9.0f * bright * 0.01f + 6.0f * br + 3.0f * arcm + 2.0f * motion * mod[(i + 2) % NMOD_PAD].v;
}

/* ------------------------------------------------- the voices' modulation --- *//* ------------------------------------------------- the voices' modulation --- */
/* MOD (amb.h): each voice has EN_NSLOT modulators; in each, a generator for each sounding voice, stepped once an FM6
 * block (0.7 ms: the FM6 core ramps an operator's gain across its block, so the index moves without steps), set by the
 * modulator's own settings (mslot_t). SHAPE what it makes:
 *   sine, triangle, saw up, saw down, square   one a cycle, at the generator's own pace (RATE times its spread)
 *   S&H     a random value a cycle, held
 *   walk    a random value at each cycle's end, glided to over the cycle (a raised cosine)
 *   chaos   the logistic map x' = r x (1 - x) at r 3.95, four steps a cycle, slewed a twentieth of a step (8 .. 150
 *           ms): a jump of the index is heard as a jump, never as a click
 *   env     from each note's start, 1 falling to 0 over a cycle (squared, as a decay), then 0
 * Then SPREAD (each voice its own, or the layer's first for all: a crossfade), SMOOTH (a one-pole glide, 2 ms at
 * least: a square's edge never clicks), FADE (from each note's start) and DEPTH. What reaches the voice is shaped for
 * what it moves (mod_of, the five modulators summed): the index in dB (as the ear hears it), a ratio in harmonic steps
 * upward (with hysteresis: a value sitting on a step's edge does not chatter) glided 15 ms, the feedback one-sided
 * (more of it, from the patch's own), pitch and place slewed further (15 and 30 ms: a chaotic jump of pitch is a
 * glitch, not a wobble), the level and a carrier's level one-sided down. The arc deepens it when the garden is full
 * and calms it when sparse. */
#define NGEN EN_NMOD
_Static_assert(EN_NMOD == EN_NVOX * NPOLY, "a generator a note");
volatile float en_mod[EN_NSLOT][EN_NMOD];
typedef struct {
    float ph, seg, from, to, lx, ls, hold;      /* the cycle 0..1; the map's step 0..1; the walk's ends; the map; S&H */
    float pace;                                 /* this generator's pace: 0.7 .. 1.3 of RATE */
    float vs, fade;                             /* what it makes, smoothed; the fade since its note, 0..1 */
    float pitch, pan, ratio, fine;              /* the slewed pitch (semitones), place, ratio (octaves), an operator's
                                                 * fine pitch (semitones) */
    int8_t step;                                /* the ratio's step now (RATIO_LOG2) */
} gen_t;
static gen_t gen[EN_NSLOT][NGEN];
static uint8_t gen_vox[NGEN];                   /* each generator's voice (VOX_*) */
static const uint8_t VOX_GEN0[EN_NVOX] = {0, NPOLY, 2 * NPOLY};   /* each voice's first generator */
typedef struct {                                /* a modulator, as set */
    float depth, rate, deff;                    /* 0..1; Hz (a cycle a second at pace 1); the depth as the arc makes it */
    float spread, fade_k, slew;                 /* 0..1; the fade's step a block (1: none); SMOOTH's coefficient */
    uint8_t shape, dest, sync;
} mslot_t;
typedef struct {
    mslot_t s[EN_NSLOT];
    float index;                                /* INDEX: dB on the modulators */
} vox_t;
static vox_t vx[EN_NVOX];
static uint32_t mod_rng = 0x2545F491u;          /* its own: the garden's notes and chords stay as they were */
static float mrand(void)                        /* xorshift, as rnd() */
{
    uint32_t x = mod_rng;
    x ^= x << 13;
    x ^= x >> 17;
    mod_rng = x ^= x << 5;
    return (float)(x >> 8) * (1.0f / 16777216.0f);
}
/* RATIO's steps: the modulators' ratio times these (log2): x1 (the patch's own) .. x5, harmonics all. Never below:
 * a modulator at half the ratio puts sidebands at half the frequency, and the voice sounds an octave down */
#define RATIO_LO 0
#define RATIO_HI 4
static const float RATIO_LOG2[RATIO_HI - RATIO_LO + 1] = {0.0f, 1.0f, 1.5849625f, 2.0f, 2.3219281f};
#define MOD_DT ((float)FM6_N / EN_SR)

static void mod_init(void)
{
    int i, v, s;
    for (s = 0; s < EN_NSLOT; s++)
        for (i = 0; i < NGEN; i++) {
            gen_t *g = &gen[s][i];
            float k = (float)(i + 11 * s) * 0.618034f;   /* the golden ratio: paces apart, none repeating another */
            g->pace = 0.7f + 0.6f * (k - (float)(int)k);
            g->ph = mrand();
            g->seg = mrand();
            g->from = mrand() * 2.0f - 1.0f;
            g->to = mrand() * 2.0f - 1.0f;
            g->hold = mrand() * 2.0f - 1.0f;
            g->lx = 0.2f + 0.6f * mrand();
            g->ls = g->pitch = g->pan = g->ratio = g->fine = g->vs = 0.0f;
            g->fade = 1.0f;
            g->step = 0;
            en_mod[s][i] = 0.0f;
        }
    for (i = 0; i < NGEN; i++)
        gen_vox[i] = (uint8_t)(i / NPOLY);
    for (v = 0; v < EN_NVOX; v++)
        for (s = 0; s < EN_NSLOT; s++)
            vx[v].s[s].fade_k = vx[v].s[s].slew = 1.0f;
}

static float tempo_bpm(void);
static void vox_rate(int v, int s)              /* RATE, by SYNC: free (60 s .. 0.12 s) or a note value at the tempo */
{
    int r = par[P_MS(v, s, MS_RATE)];
    vx[v].s[s].rate = par[P_MS(v, s, MS_SYNC)] == SYNC_TEMPO
                          ? tempo_bpm() * (1.0f / 60.0f) / DIV_BEATS[div_of(r)]
                          : 1.0f / mod_secs(r);
}

static void gen_note(int g)                     /* generator g's voice starts a note: the fades from 0; SYNC Note and
                                                 * the envelope start the shape again */
{
    int s;
    for (s = 0; s < EN_NSLOT; s++) {
        const mslot_t *x = &vx[gen_vox[g]].s[s];
        gen_t *q = &gen[s][g];
        q->fade = x->fade_k >= 1.0f ? 1.0f : 0.0f;
        if (x->sync == SYNC_NOTE || x->shape == SHAPE_ENV) {
            q->ph = q->seg = 0.0f;
            q->from = q->vs;
            q->to = mrand() * 2.0f - 1.0f;
            q->hold = mrand() * 2.0f - 1.0f;
        }
    }
}

static float gen_shape(const gen_t *g, const mslot_t *x)   /* a generator's shape now, -1 .. 1 (the envelope 0 .. 1) */
{
    const float ph = g->ph;
    switch (x->shape) {
    case SHAPE_TRI: return ph < 0.5f ? 4.0f * ph - 1.0f : 3.0f - 4.0f * ph;
    case SHAPE_UP: return 2.0f * ph - 1.0f;
    case SHAPE_DOWN: return 1.0f - 2.0f * ph;
    case SHAPE_SQUARE: return ph < 0.5f ? 1.0f : -1.0f;
    case SHAPE_SH: return g->hold;
    case SHAPE_WALK: return g->from + (g->to - g->from) * (0.5f - 0.5f * fm_cosf(FM_PI * ph));
    case SHAPE_CHAOS: return g->ls;
    case SHAPE_ENV: return (1.0f - ph) * (1.0f - ph);
    default: return sine(ph);
    }
}

static void mod_step(void)                      /* once an FM6 block */
{
    const float dt = MOD_DT;
    int i, v, s;
    for (v = 0; v < EN_NVOX; v++)
        for (s = 0; s < EN_NSLOT; s++)
            vx[v].s[s].deff = fm_clampf(vx[v].s[s].depth * (1.0f + 0.35f * arcm), 0.0f, 1.0f);
    for (s = 0; s < EN_NSLOT; s++) {
        float lead = 0.0f;
        for (i = 0; i < NGEN; i++) {
            const mslot_t *x = &vx[gen_vox[i]].s[s];
            gen_t *g = &gen[s][i];
            float f = x->rate * (x->sync == SYNC_TEMPO ? 1.0f : g->pace), w, d, q, tau;
            if (x->deff <= 0.0f && g->vs == 0.0f) {   /* off: nothing to make (the phase waits) */
                en_mod[s][i] = 0.0f;
                g->pitch = g->pan = g->ratio = g->fine = 0.0f;
                g->step = 0;
                continue;
            }
            if (x->shape == SHAPE_ENV) {         /* once from each note, then held at its end */
                g->ph = fm_minf(g->ph + f * dt, 1.0f);
            } else {
                g->ph += f * dt;
                if (g->ph >= 1.0f) {             /* a cycle's end: S&H's next value, the walk's next end */
                    g->ph -= 1.0f;
                    g->hold = mrand() * 2.0f - 1.0f;
                    g->from = g->to;
                    g->to = fm_clampf(g->from * 0.5f + (mrand() * 2.0f - 1.0f) * 0.8f, -1.0f, 1.0f);
                }
            }
            if (x->shape == SHAPE_CHAOS) {       /* the map: four steps a cycle */
                g->seg += 4.0f * f * dt;
                if (g->seg >= 1.0f) {
                    g->seg -= 1.0f;
                    g->lx = 3.95f * g->lx * (1.0f - g->lx);
                    if (g->lx < 0.001f || g->lx > 0.999f)   /* (off the attractor: back on, somewhere) */
                        g->lx = 0.2f + 0.6f * mrand();
                }
                tau = fm_clampf(0.25f / (20.0f * f), 0.008f, 0.15f);   /* a twentieth of a step */
                g->ls += (g->lx * 2.0f - 1.0f - g->ls) * fm_minf(dt / tau, 1.0f);
            }
            w = gen_shape(g, x);
            if (i == VOX_GEN0[gen_vox[i]])       /* the layer's first: what SPREAD 0 gives all of them */
                lead = w;
            w = lead + (w - lead) * x->spread;
            if (x->deff <= 0.0f)                 /* (turned off: back to rest, gliding) */
                w = 0.0f;
            g->vs += (w - g->vs) * x->slew;      /* SMOOTH */
            if (fm_fabsf(g->vs) < 1e-6f && x->deff <= 0.0f)
                g->vs = 0.0f;
            g->fade = fm_minf(g->fade + x->fade_k, 1.0f);   /* FADE */
            d = fm_clampf(g->vs, -1.0f, 1.0f) * x->deff * g->fade;
            en_mod[s][i] = d;
            g->pitch += (0.3f * d - g->pitch) * (dt / 0.015f);   /* +-30 cents, 15 ms: a vibrato at the top RATE
                                                                   * (8.5 Hz) loses 1 dB, a jump rounds */
            g->fine += (0.5f * d - g->fine) * (dt / 0.015f);    /* an operator's: +-50 cents */
            g->pan += (0.8f * d - g->pan) * (dt / 0.03f);       /* 30 ms */
            q = (d + x->deff) * 2.0f;            /* one-sided, as FEEDBACK: x1 at the bottom, x5 at full depth */
            if (q > (float)g->step + 0.65f && g->step < RATIO_HI)              /* hysteresis: 0.15 past the edge */
                g->step++;
            else if (q < (float)g->step - 0.65f && g->step > RATIO_LO)
                g->step--;
            g->ratio += (RATIO_LOG2[g->step - RATIO_LO] - g->ratio) * (dt / 0.015f);   /* 15 ms */
        }
    }
}

/* what voice generator g's five modulators and its voice's INDEX do to it now (mod_of): dB on the modulators, the
 * feedback added (1/256 steps), a pitch offset (semitones), a pan offset, a gain (LEVEL), the LFO's extra pitch depth
 * (semitones at the LFO's peak); and by operator, into dt a pitch (Q24 log2) and into ol a level (Q24 log2) */
typedef struct {
    float bloom, pitch, pan, lvl, lfopm;
    int32_t fb;
} mod_t;
static int mod_on(const mslot_t *x, int dest)
{
    return x->dest == dest || (x->dest == MOD_ALL && dest < MOD_LEVEL && dest != MOD_RATIO);
}
static void mod_of(int g, const uint8_t *p, mod_t *m, int32_t *dt, int32_t *ol)
{
    const vox_t *v = &vx[gen_vox[g]];
    const uint32_t car = fm6_carriers(p[FP_ALG] & 31u);
    int s, j;
    m->bloom = v->index;
    m->pitch = m->pan = m->lfopm = 0.0f;
    m->lvl = 1.0f;
    m->fb = 0;
    for (j = 0; j < 6; j++)
        dt[j] = ol[j] = 0;
    for (s = 0; s < EN_NSLOT; s++) {
        const mslot_t *x = &v->s[s];
        const gen_t *q = &gen[s][g];
        const float d = en_mod[s][g], k = x->dest == MOD_ALL ? 0.7f : 1.0f, up = (d + x->deff) * 0.5f;   /* 0..deff */
        int32_t metal = 0, ratio = 0, nm = 0;
        if (d == 0.0f && x->deff <= 0.0f && q->pitch == 0.0f && q->pan == 0.0f && q->ratio == 0.0f && q->fine == 0.0f)
            continue;
        if (mod_on(x, MOD_TIMBRE))
            m->bloom += 12.0f * k * d;
        if (mod_on(x, MOD_PITCH))
            m->pitch += k * q->pitch;
        if (mod_on(x, MOD_SPACE))
            m->pan += k * q->pan;
        if (x->dest == MOD_LEVEL)
            m->lvl *= 1.0f - 0.68f * up;        /* to 0.32: -10 dB at full depth */
        if (x->dest == MOD_LFOPM)
            m->lfopm += 0.3f * up;
        if (mod_on(x, MOD_FEEDBACK))            /* one-sided, up to 9 steps: as loud as the operator itself */
            m->fb += (int32_t)(up * k * 9.0f * 256.0f);
        if (mod_on(x, MOD_METAL))
            metal = (int32_t)(80.0f * k * d * 13981.0f);   /* 13981: a cent, Q24 log2 */
        if (x->dest == MOD_RATIO)
            ratio = (int32_t)(q->ratio * 16777216.0f);
        if (metal || ratio)
            for (j = 0; j < 6; j++)             /* the modulators: their ratio, then apart, one up, the next down
                                                 * (counted: algorithm 5's are operators 6, 4 and 2, all even) */
                if (!(car >> j & 1u))
                    dt[j] += ratio + ((nm++ & 1) ? -metal : metal);
        if (x->dest >= MOD_OP) {                 /* an operator's own: operator n is the patch's (6 - n)th */
            const int o = (x->dest - MOD_OP) / OPT_N, kind = (x->dest - MOD_OP) % OPT_N, kk = 5 - o;
            if (kind == OPT_LEVEL)              /* a carrier only down (never louder than the patch), a modulator
                                                 * either way: +-12 dB */
                ol[kk] += (int32_t)((car >> kk & 1u ? -24.0f * up : 12.0f * d) * FM_DB);
            else if (kind == OPT_RATIO)
                dt[kk] += (int32_t)(q->ratio * 16777216.0f);
            else if (kind == OPT_FINE)
                dt[kk] += (int32_t)(q->fine * 1398101.33f);   /* a semitone, Q24 log2 */
        }
    }
}

/* a note's own copy of its patch, as it starts: MOD's DECAY (the modulators' decay rate +-20, positive: the
 * brightness fades sooner; the carriers' +-6, the ring) and each operator's envelope as its modulators move it
 * (rates +-30, + slower; sustain +-30), by generator g's five */
static int clamp99(int v, int lo) { return v < lo ? lo : v > 99 ? 99 : v; }
static const uint8_t *mod_patch(int g, const uint8_t *src, uint8_t *dst)
{
    const vox_t *v = &vx[gen_vox[g]];
    uint32_t car = fm6_carriers(src[FP_ALG] & 31u), j;
    int s, dm = 0, dc = 0, any = 0, eo[6][4];
    for (j = 0; j < FP_SIZE + 1u; j++)
        dst[j] = src[j];
    for (j = 0; j < 6u; j++)
        eo[j][0] = eo[j][1] = eo[j][2] = eo[j][3] = 0;
    for (s = 0; s < EN_NSLOT; s++) {
        const mslot_t *x = &v->s[s];
        const float d = en_mod[s][g];
        if (d == 0.0f)
            continue;
        if (mod_on(x, MOD_DECAY)) {
            const float k = x->dest == MOD_ALL ? 0.7f : 1.0f;
            dm += (int)(20.0f * k * d);
            dc += (int)(6.0f * k * d);
            any = 1;
        }
        if (x->dest >= MOD_OP) {
            const int kind = (x->dest - MOD_OP) % OPT_N, kk = 5 - (x->dest - MOD_OP) / OPT_N;
            if (kind >= OPT_ATK) {
                eo[kk][kind - OPT_ATK] += (int)(30.0f * d);
                any = 1;
            }
        }
    }
    if (!any)
        return dst;
    for (j = 0; j < 6u; j++) {
        uint8_t *o = dst + j * FP_OP;
        o[FP_R1] = (uint8_t)clamp99(o[FP_R1] - eo[j][0], 1);              /* + slower */
        o[FP_R1 + 1] = (uint8_t)clamp99(o[FP_R1 + 1] - eo[j][1] + ((car >> j & 1u) ? dc : dm), 1);
        o[FP_R1 + 2] = (uint8_t)clamp99(o[FP_R1 + 2] - eo[j][1], 1);
        o[FP_L1 + 2] = (uint8_t)clamp99(o[FP_L1 + 2] + eo[j][2], 0);
        o[FP_R1 + 3] = (uint8_t)clamp99(o[FP_R1 + 3] - eo[j][3], 1);
    }
    return dst;
}

/* a voice's block (amb.h VX_*, MS_*) into the engine */
static void loops_patches(int v);
static void vox_apply(int v, int k, int val)
{
    mslot_t *x;
    float t;
    int i, s;
    if (k == VX_OWN) {
        if (vc[v].role == ROLE_LOOPS)            /* Bell tone's own patch, or the voice's */
            loops_patches(v);
        return;
    }
    if (k == VX_INDEX) {
        vx[v].index = (float)val;
        return;
    }
    s = (k - VX_SLOT) / MS_N;
    x = &vx[v].s[s];
    switch ((k - VX_SLOT) % MS_N) {
    case MS_DEPTH: x->depth = (float)val * 0.01f; break;
    case MS_RATE: case MS_SYNC:
        x->sync = (uint8_t)par[P_MS(v, s, MS_SYNC)];
        vox_rate(v, s);
        break;
    case MS_SHAPE: x->shape = (uint8_t)val; break;
    case MS_DEST: x->dest = (uint8_t)val; break;
    case MS_SPREAD: x->spread = (float)val * 0.01f; break;
    case MS_FADE:
        t = fade_secs(val);
        x->fade_k = t > 0.0f ? MOD_DT / t : 1.0f;
        if (x->fade_k >= 1.0f)                   /* off: at once, not at the next note */
            for (i = 0; i < NGEN; i++)
                if (gen_vox[i] == v)
                    gen[s][i].fade = 1.0f;
        break;
    case MS_SMOOTH: x->slew = fm_minf(MOD_DT / smooth_secs(val), 1.0f); break;
    default: break;
    }
}

/* ----------------------------------------------------------------- pulse --- */
typedef struct {
    float ph, inc, inc_end, amp, adec, idx, x1, x2, att, mph[4], pend;
    int32_t wait;                               /* swing: samples until a hit that waits (pend: its velocity) */
    uint8_t on, note;
} drum_t;
static drum_t drum[EN_NDRUM];
static uint8_t drum_pos[EN_NDRUM];
static uint32_t drum_pat[EN_NDRUM];
static float pulse_vol, swing, hat_f = 0.9f, hat_open = 0.42f;
static float step_len, pulse_left;               /* a sixteenth, in samples; to the next MIDI clock pulse */
static uint32_t noise = 22222u;
static const uint8_t DRUM_KMAX[EN_NDRUM] = {5, 7, 3};
static const uint8_t DRUM_GM[EN_NDRUM] = {36, 42, 46};          /* kick, closed hat, open hat */
/* the hats' metal: four square waves at the 808's inharmonic ratios (Hz / EN_SR) */
static const float HAT_INC[4] = {304.4f / EN_SR, 369.6f / EN_SR, 522.7f / EN_SR, 800.0f / EN_SR};

/* ------------------------------------------------------------------ space --- */
static float echo_send, rev_send, echo_len, echo_len_t, echo_fb = 0.55f;
static float rev_in_gain = 1.0f;                /* falls to 0 while frozen */

/* -------------------------------------------------------------- harmony --- */
static void scale_set(void)
{
    nsteps = en_scale(pool, steps);
}

static int bell_note(int v, int l, int off)     /* a loop slot's note, from the chord's degree */
{
    int d = deg + off, oct = d / nsteps, s = steps[d % nsteps], base, root = par[P_ROOT], n;
    base = LOOP_BASE[l] - ((LOOP_BASE[l] - root) % 12 + 12) % 12;   /* the root at or below the register */
    n = base + s + 12 * oct + 12 * PL(v, OCT);
    while (n > 103)
        n -= 12;
    while (n < 36)
        n += 12;
    return n;
}

static void chord_retarget(int v)               /* the chord's tones to a Chord voice's four, in its register */
{
    int from[4], to[4], i, o = 12 * PL(v, OCT);
    for (i = 0; i < NPOLY; i++)
        from[i] = (int)(vc[v].n[i].target + 0.5f);
    en_voice_lead(from, chord_pcs, chord_nt, to, 48 + o, 77 + o);
    for (i = 0; i < NPOLY; i++)                 /* each takes its note at its next breath */
        vc[v].n[i].target = (float)to[i];
}
static void pad_retarget(void)                  /* the chord's tones, to every Chord voice */
{
    int v, i, f;
    chord_nt = en_chord_tones(pool, par[P_ROOT], deg, chord_pcs);
    for (v = 0; v < EN_NVOX; v++)
        if (vc[v].role == ROLE_CHORD)
            chord_retarget(v);
    f = first_role(ROLE_CHORD);
    for (i = 0; i < EN_NPAD; i++)               /* (the garden shows the first Chord voice's, or the chord's tones) */
        en_chord[i] = (uint8_t)(f >= 0 ? (int)vc[f].n[i].target : 60 + chord_pcs[i % chord_nt]);
    en_chord_root = (uint8_t)chord_pcs[0];
    en_chord_n = (uint8_t)chord_nt;
}

static void bass_to(int v, int pc)
{
    voice_t *c = &vc[v];
    int n = 36 + pc % 12 + 12 * PL(v, OCT);     /* C2..B2, octaves down or up */
    c->n[0].target = (float)n;
    c->pc = (uint8_t)(pc % 12);
    if (en_playing && c->n[0].midi_note != n) {
        midi(0x80u | (uint32_t)v, c->n[0].midi_note, 0);
        midi(0x90u | (uint32_t)v, (uint32_t)n, 80);
    }
    if (c->n[0].midi_note != n)                  /* a new note for MOD (its fade, SYNC Note, the envelope) */
        gen_note(GEN(v, 0));
    c->strike = 1;                               /* (heard with the patch's envelopes: each chord struck) */
    c->n[0].midi_note = (uint8_t)n;
}
static void key_moved(void)                     /* a new key or scale: the chords hand off, the basses to the root */
{
    int v;
    pad_retarget();
    for (v = 0; v < EN_NVOX; v++)
        if (vc[v].role == ROLE_CHORD)
            chord_handoff(v);
        else if (vc[v].role == ROLE_BASS)
            bass_to(v, chord_pcs[0]);
}

static float drift_left, drift_total, evolve;   /* evolve: Evolve, 0..1 */
static uint8_t change_pending, fifth_done;

static void drift_reset(void)
{
    drift_total = drift_secs(par[P_DRIFT]) * (0.7f + 0.6f * frand()) * EN_SR;
    drift_left = drift_total;
    fifth_done = 0;
}

static void chord_change(void)                  /* to another degree: the near, strong moves first */
{
    static const float W7[7] = {4.0f, 2.0f, 1.0f, 3.0f, 3.0f, 3.0f, 0.4f};
    float w[12], sum = 0.0f, r;
    int i, v;
    for (i = 0; i < nsteps; i++) {
        w[i] = i == deg ? 0.0f : nsteps == 7 ? W7[i] : i == 0 ? 3.0f : 1.5f;
        sum += w[i];
    }
    r = frand() * sum;
    for (i = 0; i < nsteps; i++) {
        r -= w[i];
        if (r <= 0.0f && w[i] > 0.0f)
            break;
    }
    if (i >= nsteps)
        i = deg == 0 ? (nsteps > 1 ? 1 : 0) : 0;
    deg = i;
    pad_retarget();
    for (v = 0; v < EN_NVOX; v++)
        if (vc[v].role == ROLE_CHORD) {
            chord_leave(v);
        } else if (vc[v].role == ROLE_BASS) {
            bass_to(v, chord_pcs[0]);
            /* Evolve: the bass sometimes sits a chord out, more often when the arc is sparse */
            vc[v].rest = frand() < evolve * 0.3f + 0.25f * fm_maxf(-arcm, 0.0f);
        }
    drift_reset();
}

/* ------------------------------------------------------------- the loops --- */
static int rand_offset(void)                    /* scale steps above the chord's degree */
{
    if (frand() < 0.72f) {                      /* a chord tone, sometimes an octave up */
        int o = 2 * irand(chord_nt);
        return o + (frand() < 0.3f ? nsteps : 0);
    }
    return 1 + 2 * irand(3);                    /* a passing note: the 2nd, 4th or 6th */
}

static int loop_count(int v, int l)
{
    int i, c = 0;
    for (i = 0; i < EN_LOOP_LEN[l]; i++)
        c += vc[v].loop_note[l][i] >= 0;
    return c;
}

static int loop_target(int v)                   /* 0..5 notes: DENSITY, swelled and thinned by the arc */
{
    float d = (float)PL(v, DENS) * 0.05f * (1.0f + 0.8f * arcm);
    return (int)(fm_clampf(d, 0.0f, 5.0f) + frand());
}

static void loop_add(int v, int l)
{
    int len = EN_LOOP_LEN[l], i, s;
    for (i = 0; i < 8; i++) {
        s = irand(len);
        if (vc[v].loop_note[l][s] < 0) {
            vc[v].loop_note[l][s] = (int8_t)rand_offset();
            vc[v].loop_vel[l][s] = (uint8_t)(50 + irand(50));
            return;
        }
    }
}

static int loop_pick(int v, int l)              /* a random sounding slot, -1 none */
{
    int len = EN_LOOP_LEN[l], i, s = irand(len);
    for (i = 0; i < len; i++, s = (s + 1) % len)
        if (vc[v].loop_note[l][s] >= 0)
            return s;
    return -1;
}

static void loop_publish(int v, int l)          /* (the garden shows the first Loops voice's) */
{
    int i;
    if (first_role(ROLE_LOOPS) != v)
        return;
    for (i = 0; i < EN_LOOP_MAX; i++)
        en_loop_note[l][i] = i < EN_LOOP_LEN[l] ? vc[v].loop_note[l][i] : -1;
}

static void loop_regrow(int v, int l)
{
    int i, n = loop_target(v);
    for (i = 0; i < EN_LOOP_MAX; i++)
        vc[v].loop_note[l][i] = -1;
    for (i = 0; i < n; i++)
        loop_add(v, l);
    loop_publish(v, l);
}

static void loop_pass(int v, int l)             /* the end of a pass: grow, prune, evolve */
{
    voice_t *c = &vc[v];
    int n = loop_count(v, l), t = loop_target(v), s;
    for (; n < t; n++)                          /* DENSITY: to its count by the end of a pass */
        loop_add(v, l);
    for (; n > t && (s = loop_pick(v, l)) >= 0; n--)
        c->loop_note[l][s] = -1;
    c->loop_rest[l] = frand() < evolve * 0.15f + 0.15f * fm_maxf(-arcm, 0.0f);
    if (frand() < evolve * 0.7f && (s = loop_pick(v, l)) >= 0) {
        if (frand() < 0.5f) {
            c->loop_note[l][s] = (int8_t)rand_offset();          /* another note */
        } else {
            int d = irand(EN_LOOP_LEN[l]);                       /* the same note, elsewhere */
            if (c->loop_note[l][d] < 0) {
                c->loop_note[l][d] = c->loop_note[l][s];
                c->loop_vel[l][d] = c->loop_vel[l][s];
                c->loop_note[l][s] = -1;
            }
        }
    }
    loop_publish(v, l);
}

/* ------------------------------------------------------------ the patches --- */
/* operator op (1..6) of patch p: rates, levels, output level, coarse and fine ratio, detune (7: none), velocity
 * sensitivity, rate scaling. Breakpoint C3, no level scaling, no amplitude modulation, ratio mode */
static void fm_op(uint8_t *p, int op, int r1, int r2, int r3, int r4, int l1, int l2, int l3, int l4, int ol, int fc,
                  int ff, int det, int kvs, int rs)
{
    uint8_t *o = p + (6 - op) * FP_OP;
    int k;
    for (k = 0; k < FP_OP; k++)
        o[k] = 0;
    o[FP_R1] = (uint8_t)r1, o[FP_R1 + 1] = (uint8_t)r2, o[FP_R1 + 2] = (uint8_t)r3, o[FP_R1 + 3] = (uint8_t)r4;
    o[FP_L1] = (uint8_t)l1, o[FP_L1 + 1] = (uint8_t)l2, o[FP_L1 + 2] = (uint8_t)l3, o[FP_L1 + 3] = (uint8_t)l4;
    o[FP_BP] = 39;
    o[FP_KVS] = (uint8_t)kvs;
    o[FP_RS] = (uint8_t)rs;
    o[FP_OL] = (uint8_t)(ol < 0 ? 0 : ol > 99 ? 99 : ol);
    o[FP_FC] = (uint8_t)fc;
    o[FP_FF] = (uint8_t)ff;
    o[FP_DET] = (uint8_t)det;
}

/* the voice's part: algorithm (1..32), feedback, LFO (speed, delay, pitch depth, wave, pitch sensitivity); no
 * pitch envelope, no transpose, a name */
static void fm_voice_par(uint8_t *p, int alg, int fb, int lfs, int lfd, int lpmd, int lfw, int lpms, const char *name)
{
    int k;
    for (k = 0; k < 4; k++) {
        p[FP_PR1 + k] = 99;
        p[FP_PL1 + k] = 50;
    }
    p[FP_ALG] = (uint8_t)(alg - 1);
    p[FP_FB] = (uint8_t)fb;
    p[FP_OKS] = 1;
    p[FP_LFS] = (uint8_t)lfs;
    p[FP_LFD] = (uint8_t)lfd;
    p[FP_LPMD] = (uint8_t)lpmd;
    p[FP_LAMD] = 0;
    p[FP_LKS] = 0;
    p[FP_LFW] = (uint8_t)lfw;
    p[FP_LPMS] = (uint8_t)lpms;
    p[FP_TRNSP] = 24;
    for (k = 0; k < 10; k++)
        p[FP_NAME + k] = (uint8_t)(*name ? *name++ : ' ');
    p[FP_SIZE] = 0;
}

static void fm_off(uint8_t *p, int op) { fm_op(p, op, 99, 99, 99, 99, 0, 0, 0, 0, 0, 1, 0, 7, 0, 0); }   /* silent */

/* the pad's bank, held down (the breath is its amplitude): algorithm 5 (three pairs, the sixth operator with
 * feedback for MOD), the carriers 1, 3 and 5. Each a level of its own (PAD_GAIN, measured: analysis/patchlev.c) */
static void pad_bank(int sel, uint8_t *p)
{
    switch (sel) {
    case 1:                                      /* Voices: two carriers ~2.4 cents apart, one fed back (a soft saw),
                                                  * the third harmonic's region a little (the vowel), vibrato */
        fm_op(p, 1, 60, 30, 25, 50, 99, 97, 95, 0, 94, 1, 0, 5, 0, 1);
        fm_op(p, 2, 60, 30, 25, 50, 99, 92, 90, 0, 70, 1, 0, 7, 2, 1);
        fm_op(p, 3, 55, 30, 25, 50, 99, 95, 92, 0, 80, 3, 0, 7, 0, 1);
        fm_op(p, 4, 55, 30, 25, 50, 99, 90, 85, 0, 58, 1, 0, 7, 2, 1);
        fm_op(p, 5, 60, 30, 25, 50, 99, 97, 95, 0, 94, 1, 0, 9, 0, 1);
        fm_op(p, 6, 60, 30, 25, 50, 99, 92, 90, 0, 68, 1, 0, 9, 1, 1);
        fm_voice_par(p, 5, 6, 33, 50, 5, 4, 3, "AMB VOICES");
        break;
    case 2:                                      /* Glass: the fundamental under an inharmonic 3.5, an octave and a
                                                  * seventh harmonic a little, two carriers apart (a slow chorus) */
        fm_op(p, 1, 60, 30, 25, 50, 99, 95, 92, 0, 86, 1, 0, 9, 0, 1);
        fm_op(p, 2, 60, 30, 25, 50, 99, 85, 80, 0, 52, 1, 0, 7, 1, 1);
        fm_op(p, 3, 60, 30, 25, 50, 99, 95, 92, 0, 72, 2, 0, 5, 0, 1);
        fm_op(p, 4, 60, 30, 25, 50, 99, 85, 80, 0, 50, 7, 0, 9, 1, 1);
        fm_op(p, 5, 60, 30, 25, 50, 99, 97, 95, 0, 96, 1, 0, 7, 0, 1);
        fm_op(p, 6, 60, 30, 25, 50, 99, 80, 70, 0, 62, 3, 50, 7, 2, 1);
        fm_voice_par(p, 5, 1, 30, 50, 3, 4, 3, "AMB GLASS");
        break;
    case 3:                                      /* Strings: three detuned pairs at the unison, fed back (saws), an
                                                  * octave above, vibrato */
        fm_op(p, 1, 50, 30, 25, 50, 99, 97, 95, 0, 70, 2, 0, 7, 0, 1);
        fm_op(p, 2, 50, 30, 25, 50, 99, 92, 90, 0, 48, 2, 0, 7, 1, 1);
        fm_op(p, 3, 50, 30, 25, 50, 99, 97, 95, 0, 92, 1, 0, 9, 0, 1);
        fm_op(p, 4, 50, 30, 25, 50, 99, 92, 90, 0, 70, 1, 0, 9, 1, 1);
        fm_op(p, 5, 50, 30, 25, 50, 99, 97, 95, 0, 96, 1, 0, 5, 0, 1);
        fm_op(p, 6, 50, 30, 25, 50, 99, 92, 90, 0, 78, 1, 0, 5, 1, 1);
        fm_voice_par(p, 5, 6, 35, 60, 5, 4, 3, "AMB STRING");
        break;
    case 4:                                      /* Warm: the fundamental, its sub-octave and octave, barely modulated */
        fm_op(p, 1, 60, 30, 25, 50, 99, 97, 95, 0, 66, 2, 0, 7, 0, 1);
        fm_op(p, 2, 60, 30, 25, 50, 99, 90, 88, 0, 40, 2, 0, 7, 1, 1);
        fm_op(p, 3, 60, 30, 25, 50, 99, 97, 95, 0, 84, 0, 0, 7, 0, 1);
        fm_op(p, 4, 60, 30, 25, 50, 99, 90, 88, 0, 50, 1, 0, 7, 1, 1);
        fm_op(p, 5, 60, 30, 25, 50, 99, 97, 95, 0, 97, 1, 0, 7, 0, 1);
        fm_op(p, 6, 60, 30, 25, 50, 99, 90, 88, 0, 60, 1, 0, 7, 1, 1);
        fm_voice_par(p, 5, 1, 0, 0, 0, 4, 0, "AMB WARM");
        break;
    case 5:                                      /* Halo: the fundamental by 1.41, its octave by 2.76, a high fourth
                                                  * harmonic by 5: a shimmer of inharmonic sidebands */
        fm_op(p, 1, 55, 30, 25, 50, 99, 95, 92, 0, 56, 4, 0, 9, 0, 1);
        fm_op(p, 2, 55, 30, 25, 50, 99, 85, 80, 0, 40, 5, 0, 7, 1, 1);
        fm_op(p, 3, 55, 30, 25, 50, 99, 95, 92, 0, 74, 2, 0, 5, 0, 1);
        fm_op(p, 4, 55, 30, 25, 50, 99, 85, 80, 0, 55, 2, 38, 7, 1, 1);
        fm_op(p, 5, 55, 30, 25, 50, 99, 97, 95, 0, 96, 1, 0, 7, 0, 1);
        fm_op(p, 6, 55, 30, 25, 50, 99, 85, 80, 0, 68, 1, 41, 7, 1, 1);
        fm_voice_par(p, 5, 0, 20, 40, 2, 4, 3, "AMB HALO");
        break;
    default:                                     /* Rhodes: a modulator at the carrier's pitch (the sixth, with
                                                  * feedback for MOD), its octave a little */
        fm_off(p, 1);
        fm_off(p, 2);
        fm_op(p, 3, 80, 45, 30, 60, 99, 94, 90, 0, 74, 2, 0, 7, 0, 1);
        fm_op(p, 4, 85, 40, 30, 60, 99, 85, 80, 0, 40, 1, 0, 7, 2, 1);
        fm_op(p, 5, 80, 40, 30, 60, 99, 96, 94, 0, 99, 1, 0, 7, 0, 1);
        fm_op(p, 6, 85, 35, 25, 60, 99, 90, 85, 0, 68, 1, 0, 7, 2, 1);
        fm_voice_par(p, 5, 0, 0, 0, 0, 4, 0, "AMB RHODES");
        break;
    }
}
static const float PAD_GAIN[EN_PADPATCHES] = {1.48f, 1.55f, 1.74f, 1.76f, 1.66f, 1.72f};

/* the bass's bank, held down, every carrier sustained (the bass is one note, gliding): algorithm 5 */
static void bass_bank(int sel, uint8_t *p)
{
    fm_off(p, 1);
    fm_off(p, 2);
    fm_off(p, 4);
    switch (sel) {
    case 1:                                      /* Round: the sine fattened by its own pitch, the sub-octave */
        fm_op(p, 3, 70, 99, 99, 50, 99, 99, 99, 0, 86, 0, 0, 7, 0, 0);
        fm_op(p, 5, 70, 99, 99, 50, 99, 99, 99, 0, 99, 1, 0, 7, 0, 0);
        fm_op(p, 6, 70, 99, 99, 50, 99, 99, 99, 0, 62, 1, 0, 7, 0, 0);
        fm_voice_par(p, 5, 0, 0, 0, 0, 4, 0, "AMB ROUND");
        break;
    case 2:                                      /* Pick: fed back, its brightness falling from the strike */
        fm_op(p, 3, 70, 99, 99, 50, 99, 99, 99, 0, 82, 0, 0, 7, 0, 0);
        fm_op(p, 4, 99, 60, 40, 50, 99, 60, 55, 0, 50, 1, 0, 7, 0, 0);
        fm_op(p, 5, 70, 99, 99, 50, 99, 99, 99, 0, 99, 1, 0, 7, 0, 0);
        fm_op(p, 6, 99, 50, 40, 50, 99, 75, 70, 0, 74, 1, 0, 7, 0, 0);
        fm_voice_par(p, 5, 5, 0, 0, 0, 4, 0, "AMB PICK");
        break;
    case 3:                                      /* Sub: the sub-octave first, the fundamental under it, a breath */
        fm_op(p, 3, 70, 99, 99, 50, 99, 99, 99, 0, 78, 1, 0, 7, 0, 0);
        fm_op(p, 5, 70, 99, 99, 50, 99, 99, 99, 0, 99, 0, 0, 7, 0, 0);
        fm_op(p, 6, 70, 99, 99, 50, 99, 99, 99, 0, 35, 1, 0, 7, 0, 0);
        fm_voice_par(p, 5, 0, 0, 0, 0, 4, 0, "AMB SUB");
        break;
    case 4:                                      /* Reed: modulated an octave up and fed back: hollow */
        fm_op(p, 3, 70, 99, 99, 50, 99, 99, 99, 0, 76, 0, 0, 7, 0, 0);
        fm_op(p, 4, 70, 99, 99, 50, 99, 99, 99, 0, 42, 1, 0, 7, 0, 0);
        fm_op(p, 5, 70, 99, 99, 50, 99, 99, 99, 0, 99, 1, 0, 7, 0, 0);
        fm_op(p, 6, 70, 99, 99, 50, 99, 99, 99, 0, 70, 2, 0, 7, 0, 0);
        fm_voice_par(p, 5, 3, 0, 0, 0, 4, 0, "AMB REED");
        break;
    case 5:                                      /* Fretless: the modulator swelling in after the note */
        fm_op(p, 3, 60, 99, 99, 50, 99, 99, 99, 0, 80, 0, 0, 7, 0, 0);
        fm_op(p, 5, 60, 99, 99, 50, 99, 99, 99, 0, 99, 1, 0, 7, 0, 0);
        fm_op(p, 6, 40, 60, 99, 50, 99, 85, 85, 0, 66, 1, 0, 7, 0, 0);
        fm_voice_par(p, 5, 1, 0, 0, 0, 4, 0, "AMB FRETLS");
        break;
    default:                                     /* the sine, its sub-octave (-4.4 dB), the second harmonic (-16 dB) */
        fm_op(p, 1, 70, 99, 99, 50, 99, 99, 99, 0, 78, 2, 0, 7, 0, 0);
        fm_op(p, 3, 70, 99, 99, 50, 99, 99, 99, 0, 93, 0, 0, 7, 0, 0);
        fm_op(p, 5, 70, 99, 99, 50, 99, 99, 99, 0, 99, 1, 0, 7, 0, 0);
        fm_op(p, 6, 70, 99, 99, 50, 99, 99, 99, 0, 45, 1, 0, 7, 0, 0);
        fm_voice_par(p, 5, 0, 0, 0, 0, 4, 0, "AMB BASS");
        break;
    }
}
static const float BASS_GAIN[EN_BASSPATCHES] = {1.0f, 1.175f, 1.148f, 1.148f, 0.923f, 1.161f};


/* each loop its own bell: index, decay and ratio a little apart, so the four can be told apart */
static const float LOOP_IDX[EN_NLOOP] = {1.0f, 0.55f, 1.5f, 0.8f};
static const int8_t LOOP_RAT[EN_NLOOP] = {0, 0, 1, -1};
static const int8_t LOOP_R2[EN_NLOOP] = {31, 28, 34, 27};      /* the decay (t60 ~4.2 s at C4 x 1, 1.35, 0.75, 1.6) */
static const int8_t LOOP_OL[EN_NLOOP] = {0, -7, 5, -3};        /* LOOP_IDX in output level steps (0.75 dB) */

/* the bells' bank past the first (bell_tone makes that one from Bell tone): struck and ringing out, algorithm 5,
 * every envelope 99 0 0 0 (R3 99: at silence the note ends at once); the first pair silent */
static void bell_bank(int sel, uint8_t *p)
{
    fm_off(p, 1);
    fm_off(p, 2);
    switch (sel) {
    case 1:                                      /* Vibes: a soft fourth harmonic, a second carrier apart, warm */
        fm_op(p, 3, 90, 29, 99, 30, 99, 0, 0, 0, 80, 1, 0, 9, 0, 2);
        fm_op(p, 4, 99, 33, 99, 30, 99, 0, 0, 0, 52, 1, 0, 7, 2, 2);
        fm_op(p, 5, 90, 31, 99, 30, 99, 0, 0, 0, 99, 1, 0, 7, 0, 2);
        fm_op(p, 6, 99, 40, 99, 30, 99, 0, 0, 0, 62, 4, 0, 7, 3, 2);
        fm_voice_par(p, 5, 0, 0, 0, 0, 4, 0, "AMB VIBES");
        break;
    case 2:                                      /* Marimba: wood, short: the fourth harmonic's knock, fast decays */
        fm_off(p, 4);
        fm_op(p, 3, 99, 50, 99, 40, 99, 0, 0, 0, 62, 4, 0, 7, 0, 2);
        fm_op(p, 5, 99, 42, 99, 40, 99, 0, 0, 0, 99, 1, 0, 7, 0, 2);
        fm_op(p, 6, 99, 62, 99, 40, 99, 0, 0, 0, 74, 4, 0, 7, 3, 2);
        fm_voice_par(p, 5, 0, 0, 0, 0, 4, 0, "AMB MARIMB");
        break;
    case 3:                                      /* Tubular: 3.5 and a 2.76 partial, long */
        fm_op(p, 3, 85, 22, 99, 25, 99, 0, 0, 0, 72, 2, 38, 7, 0, 2);
        fm_op(p, 4, 99, 26, 99, 25, 99, 0, 0, 0, 45, 1, 0, 7, 2, 2);
        fm_op(p, 5, 85, 24, 99, 25, 99, 0, 0, 0, 99, 1, 0, 7, 0, 2);
        fm_op(p, 6, 99, 26, 99, 25, 99, 0, 0, 0, 70, 3, 50, 7, 3, 2);
        fm_voice_par(p, 5, 2, 0, 0, 0, 4, 0, "AMB TUBE");
        break;
    case 4:                                      /* Kalimba: a tine, plucked: a high seventh, a sixth partial, short */
        fm_off(p, 4);
        fm_op(p, 3, 99, 58, 99, 40, 99, 0, 0, 0, 56, 6, 0, 7, 0, 2);
        fm_op(p, 5, 99, 40, 99, 35, 99, 0, 0, 0, 99, 1, 0, 7, 0, 2);
        fm_op(p, 6, 99, 56, 99, 35, 99, 0, 0, 0, 66, 7, 0, 7, 3, 2);
        fm_voice_par(p, 5, 0, 0, 0, 0, 4, 0, "AMB KALIMB");
        break;
    default:                                     /* Celesta: at the unison, fed back (a bright edge), a fourth harmonic */
        fm_op(p, 3, 95, 45, 99, 30, 99, 0, 0, 0, 52, 4, 0, 7, 0, 2);
        fm_op(p, 4, 99, 45, 99, 30, 99, 0, 0, 0, 40, 1, 0, 7, 2, 2);
        fm_op(p, 5, 95, 35, 99, 30, 99, 0, 0, 0, 99, 1, 0, 7, 0, 2);
        fm_op(p, 6, 99, 45, 99, 30, 99, 0, 0, 0, 68, 1, 0, 7, 3, 2);
        fm_voice_par(p, 5, 4, 0, 0, 0, 4, 0, "AMB CELEST");
        break;
    }
}
static const float BELL_GAIN[EN_BELLPATCHES] = {1.0f, 0.88f, 1.10f, 0.79f, 1.16f, 0.92f};   /* between rms and peak: struck */

/* the bells' first patch, from Bell tone (0..1), for loop l: 0 a round, nearly pure tone; 1 a long metallic clang.
 * Algorithm 5: the bell (6 -> 5: the modulator with feedback, for MOD), a faint inharmonic partial (2.76, 3), the first
 * pair silent. The modulator's level is the index (0.15 + 6.5 tone^2 radians, as AMB-1's 2-operator bell: full output
 * is 4 pi, an output level step 0.75 dB), its decay as the index's (0.35 .. 2.95 s), the strike from soft (~50 ms) to
 * hard (~4 ms) by half way; the low bells ring longer (rate scaling), velocity brightens them (the modulators'
 * sensitivity). R3 99: at silence the note ends at once (a slower R3 holds a stage that does not move, msfa's
 * ACCURATE_ENVELOPE, for seconds) */
static void bell_tone(int l, float tone, int tone_v, uint8_t *p)
{
    float idx = (0.15f + 6.5f * tone * tone) * LOOP_IDX[l];
    int ri = tone_v * 5 / 101 + LOOP_RAT[l], mol, r1, mr2;
    ri = ri < 0 ? 0 : ri > 4 ? 4 : ri;
    mol = 99 - (int)(20.0f * 0.30103f * fm_log2f(4.0f * FM_PI / idx) / 0.75f + 0.5f);
    r1 = 53 + (int)(36.0f * fm_minf(tone * 2.0f, 1.0f));
    mr2 = 53 - (int)(19.0f * tone);
    fm_off(p, 1);
    fm_off(p, 2);
    fm_op(p, 3, r1, LOOP_R2[l] + 8, 99, 30, 99, 0, 0, 0, 62, 2, 38, 7, 0, 2);
    fm_off(p, 4);
    fm_op(p, 5, r1, LOOP_R2[l], 99, 30, 99, 0, 0, 0, 99, 1, 0, 7, 0, 2);
    fm_op(p, 6, 99, mr2, 99, 30, 99, 0, 0, 0, mol, RATIO_FC[ri], RATIO_FF[ri], 7, 3, 2);
    fm_voice_par(p, 5, 0, 0, 0, 0, 4, 0, "AMB BELL");
}

/* a Loops voice's four patches, one a loop. AMB-1's own first bell, as loaded, is Bell tone's (bell_tone), each loop
 * its shade; any other patch, or that one edited, is the voice's (vpat), each loop shaded as the first is: the
 * modulators' level by LOOP_IDX and Bell tone (+-9 dB), the carriers' decay by LOOP_R2 */
static void loops_patches(int v)
{
    int l;
    if (par[EN_VOX_PATCH[v]] == EN_FACT_BELL && !par[P_VX(v, VX_OWN)]) {
        for (l = 0; l < EN_NLOOP; l++)
            bell_tone(l, btone, par[P_BTONE], vc[v].loop_patch[l]);
        return;
    }
    for (l = 0; l < EN_NLOOP; l++) {
        uint8_t *p = vc[v].loop_patch[l];
        uint32_t car, j;
        int tol = (int)((btone - 0.5f) * 24.0f);
        for (j = 0; j < FP_SIZE + 1u; j++)
            p[j] = vpat[v][j];
        car = fm6_carriers(p[FP_ALG] & 31u);
        for (j = 0; j < 6u; j++) {
            uint8_t *o = p + j * FP_OP;
            if (!o[FP_OL])
                continue;
            if (car >> j & 1u)
                o[FP_R1 + 1] = (uint8_t)clamp99(o[FP_R1 + 1] + LOOP_R2[l] - 31, 1);
            else
                o[FP_OL] = (uint8_t)clamp99(o[FP_OL] + LOOP_OL[l] + tol, 0);
        }
    }
}

/* ----------------------------------------------------- a voice's patch --- */
_Static_assert((int)EP_R1 == (int)FP_R1 && (int)EP_OL == (int)FP_OL && (int)EP_DET == (int)FP_DET &&
                   (int)EP_OP == (int)FP_OP && (int)EP_ALG == (int)FP_ALG && (int)EP_LPMS == (int)FP_LPMS &&
                   (int)EP_NAME == (int)FP_NAME && (int)EN_PATCH == (int)FP_SIZE,
               "amb.h's patch layout is fm6_core's");

void en_factory(int voice, int sel, int tone, uint8_t *p)   /* (any voice any of the eighteen) */
{
    (void)voice;
    sel = (unsigned)sel < EN_NFACT ? sel : 0;
    if (sel < EN_FACT_BELL)
        pad_bank(sel, p);
    else if (sel >= EN_FACT_BASS)
        bass_bank(sel - EN_FACT_BASS, p);
    else if (sel > EN_FACT_BELL)
        bell_bank(sel - EN_FACT_BELL, p);
    else
        bell_tone(0, (float)tone * 0.01f, tone, p);
    p[FP_SIZE] = 0;
}

uint8_t en_patch_max(int off)
{
    static const uint8_t OPMAX[FP_OP] = {99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 3, 3, 7, 3, 7, 99, 1, 31, 99, 14};
    static const uint8_t VMAX[FP_NAME - FP_PR1] = {99, 99, 99, 99, 99, 99, 99, 99, 31, 7, 1, 99, 99, 99, 99, 1, 5, 7,
                                                   48};
    if (off < 0 || off >= FP_SIZE)
        return 0;
    if (off < 6 * FP_OP)
        return OPMAX[off % FP_OP];
    return off < FP_NAME ? VMAX[off - FP_PR1] : 127;
}

/* a DX7 bank's voice (VMEM, 128 bytes, the 32-voice bulk dump's) into the edit buffer's layout (VCED: one byte a
 * parameter): six operators of 17 bytes (OP6 first), the pitch envelope, the packed bytes, the name. An empty slot
 * (erased flash, or all zeros) is not a voice; a value past its range is held to it */
int en_unpack(const uint8_t *m, uint8_t *p)
{
    int o, k;
    for (o = 0; o < 6; o++) {
        const uint8_t *s = m + o * 17;
        uint8_t *d = p + o * FP_OP;
        for (k = 0; k < 11; k++)                 /* R1..R4, L1..L4, BP, LD, RD */
            d[k] = s[k];
        d[FP_LC] = s[11] & 3u;
        d[FP_RC] = (uint8_t)(s[11] >> 2 & 3u);
        d[FP_RS] = s[12] & 7u;
        d[FP_DET] = (uint8_t)(s[12] >> 3 & 15u);
        d[FP_AMS] = s[13] & 3u;
        d[FP_KVS] = (uint8_t)(s[13] >> 2 & 7u);
        d[FP_OL] = s[14];
        d[FP_MODE] = s[15] & 1u;
        d[FP_FC] = (uint8_t)(s[15] >> 1 & 31u);
        d[FP_FF] = s[16];
    }
    for (k = 0; k < 8; k++)                      /* PR1..PR4, PL1..PL4 */
        p[FP_PR1 + k] = m[102 + k];
    p[FP_ALG] = m[110] & 31u;
    p[FP_FB] = m[111] & 7u;
    p[FP_OKS] = (uint8_t)(m[111] >> 3 & 1u);
    p[FP_LFS] = m[112];
    p[FP_LFD] = m[113];
    p[FP_LPMD] = m[114];
    p[FP_LAMD] = m[115];
    p[FP_LKS] = m[116] & 1u;
    p[FP_LFW] = (uint8_t)(m[116] >> 1 & 7u);
    p[FP_LPMS] = (uint8_t)(m[116] >> 4 & 7u);
    p[FP_TRNSP] = m[117];
    for (k = 0; k < 10; k++)
        p[FP_NAME + k] = m[118 + k];
    p[FP_SIZE] = 0;
    {
        int any = 0;
        for (k = 0; k < 128; k++) {              /* erased flash (0xFF), or no voice at all (zeros) */
            if (m[k] > 127u)
                return 0;
            any |= m[k];
        }
        if (!any)
            return 0;
    }
    for (k = 0; k < FP_SIZE; k++)                /* a value past its range (old banks' stray 127s): held to it, as a
                                                  * DX7 plays it; a name's control characters as spaces */
        if (p[k] > en_patch_max(k))
            p[k] = en_patch_max(k);
    for (k = 0; k < 10; k++)
        if (p[FP_NAME + k] < 32u)
            p[FP_NAME + k] = ' ';
    return 1;
}

/* format 6's FM edits into a patch: the modulators' ratio times RATIO_MUL, the feedback, every operator's envelope */
void en_bake(uint8_t *p, int ratio, int fb, int atk, int dec, int sus, int rel)
{
    uint32_t car = fm6_carriers(p[FP_ALG] & 31u), j;
    float mul = fm_exp2f(RATIO_MUL[(unsigned)ratio < NRATIO ? ratio : 0]);
    for (j = 0; j < 6u; j++) {
        uint8_t *o = p + j * FP_OP;
        if (ratio && !(car >> j & 1u) && !o[FP_MODE]) {   /* coarse * (1 + fine / 100), coarse 0 being 0.5 */
            float f = (o[FP_FC] ? (float)o[FP_FC] : 0.5f) * (1.0f + (float)o[FP_FF] * 0.01f) * mul;
            int c = f < 1.0f ? 0 : f >= 31.0f ? 31 : (int)f;
            int ff = (int)((f / (c ? (float)c : 0.5f) - 1.0f) * 100.0f + 0.5f);
            o[FP_FC] = (uint8_t)c;
            o[FP_FF] = (uint8_t)(ff < 0 ? 0 : ff > 99 ? 99 : ff);
        }
        o[FP_R1] = (uint8_t)clamp99(o[FP_R1] - atk, 1);
        o[FP_R1 + 1] = (uint8_t)clamp99(o[FP_R1 + 1] - dec, 1);
        o[FP_R1 + 2] = (uint8_t)clamp99(o[FP_R1 + 2] - dec, 1);
        o[FP_L1 + 2] = (uint8_t)clamp99(o[FP_L1 + 2] + sus, 0);
        o[FP_R1 + 3] = (uint8_t)clamp99(o[FP_R1 + 3] - rel, 1);
    }
    fb += p[FP_FB];
    p[FP_FB] = (uint8_t)(fb < 0 ? 0 : fb > 7 ? 7 : fb);
}

/* a patch's level out: AMB-1's own eighteen measured (analysis/patchlev.c), each in the role it was made for; a bank's
 * from its carriers, so a voice of six carriers at full is as loud as one */
static float patch_gain(int v, int src, const uint8_t *p)
{
    static const float BASE[EN_NROLE] = {1.48f, 1.0f, 1.0f};
    uint32_t car = fm6_carriers(p[FP_ALG] & 31u), j;
    float a = 0.0f;
    if ((unsigned)src < EN_NFACT)
        return src < EN_FACT_BELL ? PAD_GAIN[src] : src < EN_FACT_BASS ? BELL_GAIN[src - EN_FACT_BELL]
                                                                       : BASS_GAIN[src - EN_FACT_BASS];
    for (j = 0; j < 6u; j++)                     /* each carrier's amplitude: 0.75 dB an output level step */
        if (car >> j & 1u)
            a += fm_exp2f((float)((int)p[j * FP_OP + FP_OL] - 99) * 0.1245f);
    return BASE[vc[v].role < EN_NROLE ? vc[v].role : 0] / fm_maxf(a, 1.0f);
}

/* the staged bytes into the voice's patch: a new one (fresh: a chord crosses over, the bass is struck anew, the loops
 * from their next strike) or an edit (heard at once on the notes sounding: their copies made again, refreshed) */
static void patch_apply(int v, int fresh)
{
    voice_t *c = &vc[v];
    uint32_t j;
    int i;
    for (j = 0; j < FP_SIZE + 1u; j++)
        vpat[v][j] = vstage[v][j];
    c->gain = patch_gain(v, par[EN_VOX_PATCH[v]], vpat[v]);
    if (c->role == ROLE_CHORD) {
        if (fresh) {
            c->gen++;
            chord_handoff(v);
            return;
        }
        for (i = 0; i < NPOLY; i++)
            if (c->n[i].amp > 0.0f)
                note_refresh(v, i, vpat[v]);
    } else if (c->role == ROLE_LOOPS) {
        loops_patches(v);
        if (fresh)
            return;
        for (i = 0; i < NPOLY; i++)
            if (c->n[i].on && c->n[i].rung)
                note_refresh(v, i, c->loop_patch[c->n[i].loop]);
    } else if (fresh) {
        c->rs = BASS_RS;
    } else {
        note_refresh(v, 0, vpat[v]);
    }
}

/* a voice taking up its role (at the start, or faded out of another): its notes from silence, the role's own state */
static void loop_regrow(int v, int l);
static void role_enter(int v)
{
    voice_t *c = &vc[v];
    int i, l;
    midi(0xB0u | (uint32_t)v, 123, 0);
    for (i = 0; i < NPOLY; i++) {
        vnote_t *n = &c->n[i];
        n->amp = n->tgt = 0.0f;
        n->on = n->rung = n->hand = 0;
        n->up = 1;
        n->hf = 1.0f;
        n->midi_note = 0;
        n->pan = PAD_PAN[i];
        n->gl = n->gr = n->pl = n->pr = 0.0f;
        n->t = START_T[i];
        n->note = n->target = (float)(50 + 7 * i + 12 * PL(v, OCT));
    }
    c->gain = patch_gain(v, par[EN_VOX_PATCH[v]], vpat[v]);
    c->rest = c->strike = c->xs = 0;
    c->rs = 0;
    c->held = 0;
    c->xf = 1.0f;
    if (c->role == ROLE_CHORD) {
        if (nsteps)
            chord_retarget(v);
        for (i = 0; i < NPOLY; i++) {
            c->n[i].note = c->n[i].target;
            c->n[i].gen = c->gen;
            if (en_playing)
                chord_breathe(v, i);
            else
                note_strike(v, i, vpat[v], (int32_t)c->n[i].note, 100, 1);
        }
    } else if (c->role == ROLE_LOOPS) {
        loops_patches(v);
        for (l = 0; l < EN_NLOOP; l++) {
            c->loop_pos[l] = 0;
            c->loop_rest[l] = 0;
            if (nsteps)
                loop_regrow(v, l);
        }
    } else {
        c->n[0].midi_note = 0;
        if (nsteps)
            bass_to(v, chord_pcs[0]);
        else
            c->n[0].target = 36.0f;
        c->n[0].note = c->n[0].target;
        note_strike(v, 0, vpat[v], (int32_t)c->n[0].note, 100, 1);
        c->strike = en_playing;
    }
    if (nsteps)
        pad_retarget();                          /* (the garden: who shows the chord may have changed) */
}

static float fm_gain(const fm6_note_t *n, const uint8_t *p)   /* a note's loudest carrier now, 1.0 at full */
{
    uint32_t car = fm6_carriers(p[FP_ALG] & 31u), k;
    int32_t g = 0;
    for (k = 0; k < 6u; k++)
        if ((car >> k & 1u) && n->op[k].gain_out > g)
            g = n->op[k].gain_out;
    return (float)g * FM_SCALE;
}

static void bell_voice(int v, int l, int n, float vel, int32_t wait)
{
    voice_t *c = &vc[v];
    int i, best = 0;
    float lo = 9.0f, p;
    vnote_t *b;
    for (i = 0; i < NPOLY; i++) {               /* a free note, or the quietest (its carrier's gain) */
        const vnote_t *q = &c->n[i];
        float a = q->on ? (q->rung ? fm_gain(&q->fm, q->vp) * q->vel : 2.0f) : -1.0f;
        if (a < lo) {
            lo = a;
            best = i;
        }
    }
    b = &c->n[best];
    if (b->on)
        midi(0x80u | (uint32_t)v, b->midi_note, 0);
    b->on = 1;
    b->rung = 0;
    b->midi_note = (uint8_t)n;
    b->loop = (uint8_t)l;
    b->vel = vel;
    p = (LOOP_PAN[l] + 1.0f) * (FM_PI / 4.0f);
    b->gl = fm_cosf(p);
    b->gr = fm_sinf(p);
    b->wait = wait;
    if (first_role(ROLE_LOOPS) == v)
        en_loop_glow[l] = 1.0f;
    midi(0x90u | (uint32_t)v, (uint32_t)n, (uint32_t)(vel * 110.0f));
}

static void bell_ring(int v, int l, int slot)
{
    int n = bell_note(v, l, vc[v].loop_note[l][slot]);
    float vel = (float)vc[v].loop_vel[l][slot] * 0.01f;
    int32_t wait = irand(2600);                 /* a human hand: up to 60 ms late */
    bell_voice(v, l, n, vel, wait);
    if (n + 12 <= 103 && frand() < 0.08f + 0.12f * fm_maxf(arcm, 0.0f))
        bell_voice(v, l, n + 12, vel * 0.4f, wait + 300 + irand(2000));   /* now and then, its octave */
}

/* ---------------------------------------------------------------- pulse --- */
static const uint8_t DRUM_ROT[EN_NDRUM] = {0, 2, 3};
static int drum_k[EN_NDRUM], drum_kb[EN_NDRUM], drum_rot[EN_NDRUM], pulse_rest;   /* rest: bars left */

static void drum_publish(int d)
{
    drum_pat[d] = en_euclid(drum_k[d], EN_DRUM_N[d], drum_rot[d]);
    en_drum_pat[d] = drum_pat[d];
}

static void drums_pattern(void)                 /* Hits: the patterns' home */
{
    int d;
    for (d = 0; d < EN_NDRUM; d++) {
        drum_kb[d] = drum_k[d] = (par[P_HITS] * DRUM_KMAX[d] + 50) / 100;
        drum_rot[d] = DRUM_ROT[d];
        drum_publish(d);
    }
}

static void drums_evolve(void)                  /* a bar: a pattern turns, or gains or loses a hit */
{
    int d = irand(EN_NDRUM), n = EN_DRUM_N[d];
    if (!drum_kb[d])
        return;
    if (frand() < 0.5f) {
        drum_rot[d] = (drum_rot[d] + (frand() < 0.5f ? 1 : n - 1)) % n;
    } else {
        int k = drum_k[d] + (frand() < 0.5f ? 1 : -1);
        if (k >= drum_kb[d] - 1 && k <= drum_kb[d] + 1 && k >= 1 && k <= n)
            drum_k[d] = k;
    }
    drum_publish(d);
}

static void drum_fire(int d, float vel)
{
    drum_t *v = &drum[d];
    v->on = 1;
    v->amp = vel;
    v->att = 0.0f;
    switch (d) {
    case 0: {                                   /* thump: the chord's root, low, falling into place */
        float f = mtof((float)(36 + chord_pcs[0]));
        v->note = DRUM_GM[0];
        v->inc = f * 2.2f * (1.0f / EN_SR);
        v->inc_end = f * (1.0f / EN_SR);
        v->adec = fm_expf(-6.9f / (0.45f * EN_SR));
        break;
    }
    case 1:                                     /* closed hat: short; it chokes the open one */
        v->note = DRUM_GM[1];
        v->adec = fm_expf(-6.9f / (0.06f * EN_SR));
        drum[2].adec = fm_expf(-6.9f / (0.02f * EN_SR));
        break;
    default:                                    /* open hat: a long, soft "tss" */
        v->note = DRUM_GM[2];
        v->adec = fm_expf(-6.9f / (hat_open * EN_SR));
        break;
    }
    en_drum_glow[d] = 1.0f;
    midi(0x99, v->note, (uint32_t)(vel * 100.0f));
    midi(0x89, v->note, 0);
}

static void drum_hit(int d, float vel, int off)  /* off: an off sixteenth, which Swing delays */
{
    int32_t w = off ? (int32_t)(swing * step_len) : 0;
    if (w > 0) {
        drum[d].wait = w;
        drum[d].pend = vel;
    } else {
        drum_fire(d, vel);
    }
}

static int32_t drums_wait(void)                 /* samples to the next hit that waits; 0: none */
{
    int32_t m = 0;
    int d;
    for (d = 0; d < EN_NDRUM; d++)
        if (drum[d].wait > 0 && (!m || drum[d].wait < m))
            m = drum[d].wait;
    return m;
}

static void drums_wait_step(int32_t k)          /* k samples went by: hits whose wait is over */
{
    int d;
    for (d = 0; d < EN_NDRUM; d++)
        if (drum[d].wait > 0 && (drum[d].wait -= k) <= 0) {
            drum[d].wait = 0;
            drum_fire(d, drum[d].pend);
        }
}

/* the grid: one call a sixteenth (MIDI clock: 6 ticks) */
static uint32_t sixteenth;
static void tick(void)
{
    int d, l, v;
    const float pa = fm_clampf(1.0f + 1.3f * arcm, 0.3f, 1.15f);   /* the arc: the pulse fades (never out) */
    if ((sixteenth & 15u) == 0u) {              /* a bar: Evolve's rests and turns */
        if (pulse_rest > 0)
            pulse_rest--;
        else if (frand() < evolve * 0.015f)
            pulse_rest = 2 + irand(3);           /* a short breath: 2 to 4 bars */
        en_pulse_rest = (uint8_t)(pulse_rest > 0);
        if (frand() < evolve * 0.25f)
            drums_evolve();
    }
    for (d = 0; d < EN_NDRUM; d++) {
        if (drum_pat[d] >> drum_pos[d] & 1u && !pulse_rest)
            drum_hit(d, ((drum_pos[d] == 0 ? 0.95f : 0.6f) + 0.25f * frand()) * pa, (int)(sixteenth & 1u));
        en_drum_pos[d] = drum_pos[d];
        if (++drum_pos[d] >= EN_DRUM_N[d])
            drum_pos[d] = 0;
    }
    for (v = 0; v < EN_NVOX; v++) {
        voice_t *c = &vc[v];
        int st;
        if (c->role == ROLE_LOOPS && sixteenth % (uint32_t)loops_step(v) == 0u) {   /* the loops: PACE's step */
            for (l = 0; l < EN_NLOOP; l++) {
                int s = c->loop_pos[l];
                if (c->loop_note[l][s] >= 0 && !c->loop_rest[l])
                    bell_ring(v, l, s);
                if (first_role(ROLE_LOOPS) == v)
                    en_loop_pos[l] = (uint8_t)s;
                if (++c->loop_pos[l] >= EN_LOOP_LEN[l]) {
                    c->loop_pos[l] = 0;
                    loop_pass(v, l);
                }
            }
        } else if (c->role == ROLE_BASS && PL(v, ENV) != ENV_AMB && (st = bass_step(v)) > 0 &&
                   sixteenth % (uint32_t)st == 0u) {
            c->strike = 1;                       /* the bass struck again, PACE's step */
        }
    }
    if ((sixteenth & 15u) == 0u && change_pending && !en_frozen) {   /* chords move on the bar */
        change_pending = 0;
        chord_change();
    }
    sixteenth++;
    en_bars = sixteenth;
}

/* ------------------------------------------------- reverb: Dattorro's plate --- */
/* J. Dattorro, "Effect Design, Part 1", JAES 1997 (as in FoMni): a predelay, four input diffusers,
 * then a figure-eight tank of two halves, each a modulated allpass, a delay, a damping lowpass, an
 * allpass and a delay, feeding the other half. Stereo out from seven taps a side. The paper's lengths
 * are for 29761 Hz; DS() scales them to 44.1 kHz. */
#define DS(n) (((n) * 14818 + 5000) / 10000)          /* 44100 / 29761 */
#define PRE_N 1323                                   /* 30 ms */
#define EXC DS(16)
enum { L_PRE, L_IN1, L_IN2, L_IN3, L_IN4, L_APL, L_D1L, L_AP2L, L_D2L, L_APR, L_D1R, L_AP2R, L_D2R, L_N };
static const int DL_LEN[L_N] = {PRE_N, DS(142), DS(107), DS(379), DS(277), DS(672) + EXC + 2, DS(4453), DS(1800),
                                DS(3720), DS(908) + EXC + 2, DS(4217), DS(2656), DS(3163)};
#define DL_TOTAL (PRE_N + DS(142) + DS(107) + DS(379) + DS(277) + DS(672) + DS(4453) + DS(1800) + DS(3720) + DS(908) + \
                  DS(4217) + DS(2656) + DS(3163) + 2 * (EXC + 2))
static float pl_buf[DL_TOTAL] EN_POOL;
typedef struct {
    float *b;
    int n, i;
} dl_t;
static dl_t dl[L_N];
static float pl_bw, pl_damp_l, pl_damp_r, pl_decay = 0.5f, pl_decay_t = 0.5f, pl_dd2 = 0.5f, lfo_s, lfo_c = 1.0f;

static inline float tap(const dl_t *d, int k)
{
    int j = d->i - k;
    return d->b[j < 0 ? j + d->n : j];
}
static inline void push(dl_t *d, float x)
{
    d->b[d->i] = x;
    if (++d->i >= d->n)
        d->i = 0;
}
static inline float allpass(dl_t *d, int len, float x, float g)
{
    float z = tap(d, len), v = x - g * z;
    push(d, fm_flush(v));
    return z + g * v;
}
static inline float allpass_mod(dl_t *d, float len, float x, float g)
{
    int k = (int)len;
    float f = len - (float)k, z = tap(d, k) + (tap(d, k + 1) - tap(d, k)) * f, v = x - g * z;
    push(d, fm_flush(v));
    return z + g * v;
}

static inline void reverb(float in, float *ol, float *or_)
{
    float x, a, b, fb_l = tap(&dl[L_D2R], DS(3163)), fb_r = tap(&dl[L_D2L], DS(3720));
    push(&dl[L_PRE], in);
    x = tap(&dl[L_PRE], PRE_N);
    pl_bw += 0.6f * (x - pl_bw);
    x = allpass(&dl[L_IN1], DS(142), pl_bw, 0.75f);
    x = allpass(&dl[L_IN2], DS(107), x, 0.75f);
    x = allpass(&dl[L_IN3], DS(379), x, 0.625f);
    x = allpass(&dl[L_IN4], DS(277), x, 0.625f);
    {
        float s = lfo_s + 0.0001f * lfo_c, c = lfo_c - 0.0001f * s;
        lfo_s = s;
        lfo_c = c;
    }
    a = allpass_mod(&dl[L_APL], (float)DS(672) + (float)EXC * lfo_s, x + pl_decay * fb_l, -0.7f);
    push(&dl[L_D1L], a);
    b = tap(&dl[L_D1L], DS(4453));
    pl_damp_l += 0.45f * (b - pl_damp_l);
    b = allpass(&dl[L_AP2L], DS(1800), pl_damp_l * pl_decay, pl_dd2);
    push(&dl[L_D2L], b);
    a = allpass_mod(&dl[L_APR], (float)DS(908) + (float)EXC * lfo_c, x + pl_decay * fb_r, -0.7f);
    push(&dl[L_D1R], a);
    b = tap(&dl[L_D1R], DS(4217));
    pl_damp_r += 0.45f * (b - pl_damp_r);
    b = allpass(&dl[L_AP2R], DS(2656), pl_damp_r * pl_decay, pl_dd2);
    push(&dl[L_D2R], b);
    *ol = 0.6f * (tap(&dl[L_D1R], DS(266)) + tap(&dl[L_D1R], DS(2974)) - tap(&dl[L_AP2R], DS(1913)) +
                  tap(&dl[L_D2R], DS(1996)) - tap(&dl[L_D1L], DS(1990)) - tap(&dl[L_AP2L], DS(187)) -
                  tap(&dl[L_D2L], DS(1066)));
    *or_ = 0.6f * (tap(&dl[L_D1L], DS(353)) + tap(&dl[L_D1L], DS(3627)) - tap(&dl[L_AP2L], DS(1228)) +
                   tap(&dl[L_D2L], DS(2673)) - tap(&dl[L_D1R], DS(2111)) - tap(&dl[L_AP2R], DS(335)) -
                   tap(&dl[L_D2R], DS(121)));
}

static void plate_init(void)
{
    float *p = pl_buf;
    int k, i;
    for (k = 0; k < L_N; k++) {
        dl[k].b = p;
        dl[k].n = DL_LEN[k];
        dl[k].i = 0;
        p += DL_LEN[k];
    }
    for (i = 0; i < DL_TOTAL; i++)
        pl_buf[i] = 0.0f;
    pl_bw = pl_damp_l = pl_damp_r = 0.0f;
    lfo_s = 0.0f;
    lfo_c = 1.0f;
}

/* ------------------------------------------------- echo: tape and Echoplex --- */
/* After Discreet Music's chain: an Echoplex (a short, smearing echo, ~160 ms, fed back) and a tape loop
 * (long: up to ~5.6 s, synced to the tempo), each repeat a little darker and thinner, with a little
 * wow. The tape runs at a quarter rate (11 kHz, 16-bit, mono): it loses its highs anyway. Two heads:
 * the first plays the loop's length, the second a part of it (3/4 at first). With Motion the echo
 * evolves: each head wanders across the field, the feedback leans now on one head, now on the other
 * (the repeats fall in changing patterns, left and right), and now and then the second head slides to
 * another part of the loop, as a head moved by hand (the repeats bend a little in pitch on the way).
 * The Echoplex has a second tap, for the right. */
#define TAPE_N 62000                             /* 5.6 s at 11.025 kHz */
#define TAPE_SR 11025.0f
static int16_t tape[TAPE_N] EN_POOL;
static int tape_w, tape_ph;
static float tape_acc, tape_lp, tape_hp, tape_p1, tape_p2, tape_c1, tape_c2;
static const float HEAD2[5] = {0.75f, 0.6667f, 0.625f, 0.5f, 0.375f};   /* the second head's parts */
static float head2 = 0.75f, head2_t = 0.75f, head2_v;   /* its place (of the loop), target, speed */
static float fb_lean;                            /* 0..1: how much of the feedback is the second head's */
static float hg[4] = {1.0f, 0.0f, 0.0f, 1.0f};   /* the heads' pan gains: 1 left, 1 right, 2 left, 2 right */
static uint32_t head_left;                       /* samples until the second head may move */
#define PLEX_N 7100                              /* 160 ms at 44.1 kHz */
#define PLEX_TAP 2700                            /* the right's tap: ~100 ms */
static int16_t plex[PLEX_N];
static int plex_w;
static float plex_lp, plex_lp2;

static inline float tape_read(float d)           /* d quarter-rate samples ago */
{
    int k = (int)d, i0, i1;
    float f = d - (float)k, a, c;
    i0 = tape_w - k;
    i1 = i0 - 1;
    if (i0 < 0)
        i0 += TAPE_N;
    if (i1 < 0)
        i1 += TAPE_N;
    a = (float)tape[i0];
    c = (float)tape[i1];
    return (a + (c - a) * f) * (1.0f / 12000.0f);
}
static inline int16_t s16(float x)
{
    x *= 12000.0f;
    return (int16_t)(x > 32767.0f ? 32767.0f : x < -32767.0f ? -32767.0f : x);
}

static void echo_time(void)
{
    int i = par[P_ETIME];
    float beat, t;
    if (tempo_bpm() <= 0.0f)
        return;                                  /* not yet (en_init applies in order) */
    beat = 60.0f / tempo_bpm();
    t = ETIME_BEATS[i] * beat;
    while (t * TAPE_SR > (float)(TAPE_N - 8) && i > 0)
        t = ETIME_BEATS[--i] * beat;             /* too long for the tape: the next shorter */
    if (t * TAPE_SR > (float)(TAPE_N - 8))
        t = (float)(TAPE_N - 8) / TAPE_SR;
    echo_len_t = t * TAPE_SR;
    if (echo_len <= 0.0f)
        echo_len = echo_len_t;
}

/* once a chunk: where the heads sit in the field, how the feedback leans, the second head's slide */
static void heads_step(uint32_t n)
{
    float p, want, vmax;
    int i;
    for (i = 0; i < 2; i++) {
        p = fm_clampf((i ? 0.7f : -0.7f) + 0.9f * motion * mod[6 + i].v, -1.0f, 1.0f);
        p = (p + 1.0f) * (FM_PI / 4.0f);
        hg[2 * i] = fm_cosf(p);
        hg[2 * i + 1] = fm_sinf(p);
    }
    fb_lean = 0.25f * motion * (1.0f + mod[8].v);
    if (head_left <= n) {
        head_left = (uint32_t)((20.0f + 40.0f * frand()) * EN_SR);
        if (en_playing && !en_frozen && frand() < motion)
            head2_t = HEAD2[(int)(frand() * 4.999f)];
    } else {
        head_left -= n;
    }
    /* its speed (a part of the loop per tape sample): at most a bend of ~14 cents, easing in and out */
    vmax = 0.008f / fm_maxf(echo_len, 100.0f);
    want = fm_clampf((head2_t - head2) * (1.0f / (2.0f * TAPE_SR)), -vmax, vmax);
    head2_v += (want - head2_v) * fm_minf((float)n * (1.0f / EN_SR), 1.0f);
}

/* one sample of both: the input (a mono send) to the tape's heads (el, er) and the Echoplex (xl, xr) */
static inline void echoes(float in, float *el, float *er, float *xl, float *xr)
{
    float y = (float)plex[plex_w] * (1.0f / 12000.0f);         /* the Echoplex: 160 ms, fed back, darker */
    int t = plex_w - PLEX_TAP;
    plex_lp += 0.35f * (y - plex_lp);
    plex_lp2 += 0.35f * ((float)plex[t < 0 ? t + PLEX_N : t] * (1.0f / 12000.0f) - plex_lp2);
    plex[plex_w] = s16(in + 0.55f * plex_lp);
    if (++plex_w >= PLEX_N)
        plex_w = 0;
    *xl = plex_lp;
    *xr = plex_lp2;
    tape_acc += in + 0.3f * plex_lp;
    if (++tape_ph >= 4) {                        /* the tape, a quarter of the time */
        float d, h1, h2, fb;
        tape_ph = 0;
        echo_len += (echo_len_t - echo_len) * 0.001f;
        head2 += head2_v;
        d = echo_len * (1.0f + 0.003f * motion * mod[5].v);   /* wow */
        if (d > (float)(TAPE_N - 4))
            d = (float)(TAPE_N - 4);
        h1 = tape_read(d);
        h2 = tape_read(d * head2);
        tape_lp += 0.55f * (h1 + fb_lean * (h2 - h1) - tape_lp);   /* each time round: darker, thinner */
        tape_hp += 0.03f * (tape_lp - tape_hp);
        fb = tape_lp - tape_hp;
        if (++tape_w >= TAPE_N)
            tape_w = 0;
        tape[tape_w] = s16(tape_acc * 0.25f + echo_fb * fb);
        tape_acc = 0.0f;
        tape_p1 = tape_c1;
        tape_p2 = tape_c2;
        tape_c1 = h1;
        tape_c2 = h2;
    }
    {   /* between the tape's samples: a line from the last to the next; then to the field */
        float f = (float)tape_ph * 0.25f;
        float h1 = tape_p1 + (tape_c1 - tape_p1) * f, h2 = tape_p2 + (tape_c2 - tape_p2) * f;
        *el = hg[0] * h1 + hg[2] * h2;
        *er = hg[1] * h1 + hg[3] * h2;
    }
}

/* ------------------------------------------------------------ parameters --- */
static int pulse;
static uint32_t ext_age;
/* an external MIDI clock's tempo: measured a beat (24 pulses) at a time, by the samples rendered (the pulses arrive at
 * a chunk's start: a beat's count is off by a chunk at most, under 1 %), half towards each new beat's; 0 none */
static uint32_t smp_clock, ext_t0, ext_n;
static float ext_bpm;
static float tempo_bpm(void) { return en_ext && ext_bpm > 0.0f ? ext_bpm : (float)par[P_TEMPO]; }

static void tempo_set(void) { step_len = EN_SR * 60.0f / (tempo_bpm() * 4.0f); }
static void tempo_changed(void)                 /* what follows the tempo: the step, the echo, SYNC Tempo's rates */
{
    int v, s;
    en_tempo = tempo_bpm();
    tempo_set();
    echo_time();
    for (v = 0; v < EN_NVOX; v++)
        for (s = 0; s < EN_NSLOT; s++)
            vox_rate(v, s);
}
static void ext_tick(void)                      /* an external clock's pulse */
{
    if (++ext_n < 24u)
        return;
    {
        const uint32_t dt = smp_clock - ext_t0;
        ext_n = 0;
        ext_t0 = smp_clock;
        if (dt > (uint32_t)(EN_SR * 60.0f / 300.0f) && dt < (uint32_t)(EN_SR * 60.0f / 20.0f)) {
            const float b = EN_SR * 60.0f / (float)dt, nb = ext_bpm > 0.0f ? ext_bpm + (b - ext_bpm) * 0.5f : b,
                        old = ext_bpm;
            ext_bpm = nb;
            if (fm_fabsf(nb - old) > 0.3f)
                tempo_changed();
        }
    }
}

static void play_apply(int v, int k, int val)   /* how a voice plays (amb.h PL_*) into the engine */
{
    voice_t *c = &vc[v];
    switch (k) {
    case PL_OCT:                                 /* heard now: a chord hands off, the bass moves */
        if (!nsteps)
            break;
        if (c->role == ROLE_CHORD) {
            chord_retarget(v);
            chord_handoff(v);
            pad_retarget();
        } else if (c->role == ROLE_BASS) {
            bass_to(v, c->pc);
        }
        break;
    case PL_GLIDE: c->glide_k = 1.0f / ((0.02f + 0.012f * (float)val) * EN_SR); break;   /* 20 ms .. 1.2 s */
    default: break;                              /* (the rest at the next note, the role in fm_block) */
    }
}

static void apply(int p, int v)
{
    const en_param_t *pi = &PARAMS[p];
    int k;
    if (v < pi->lo)
        v = pi->lo;
    if (v > pi->hi)
        v = pi->hi;
    par[p] = (int16_t)v;
    switch (p) {
    case P_PAD: vc[VOX_PAD].vol = (float)(v * v) * 0.0001f; break;
    case P_BELLS: vc[VOX_BELLS].vol = (float)(v * v) * 0.0001f; break;
    case P_BASS: vc[VOX_BASS].vol = (float)(v * v) * 0.0001f; break;
    case P_PULSE: pulse_vol = (float)(v * v) * 0.0001f; break;
    case P_HITS: drums_pattern(); break;
    case P_SWING: swing = (float)v * 0.005f; break;      /* the off sixteenths up to half a step late */
    case P_HATS:
        hat_f = 0.55f + 0.0070f * (float)v;               /* the high-pass: ~4 kHz dark .. ~10 kHz bright */
        hat_open = 0.6f - 0.0035f * (float)v;             /* the open hat: long and soft .. short and crisp */
        break;
    case P_DRIFT:
        if (drift_total > 0.0f) {                /* keep the progress, at the new pace */
            float frac = drift_left / drift_total;
            drift_total = drift_secs(v) * EN_SR;
            drift_left = frac * drift_total;
        }
        break;
    case P_ECHO: echo_send = (float)(v * v) * 0.0001f; break;
    case P_ETIME: echo_time(); break;
    case P_REVERB: rev_send = (float)(v * v) * 0.0001f; break;
    case P_SIZE:
        pl_decay_t = 0.6f + 0.375f * (float)v * 0.01f;     /* to 0.975: a long hall */
        echo_fb = 0.5f + 0.32f * (float)v * 0.01f;
        break;
    case P_BRIGHT: bright = (float)v; break;
    case P_EVOLVE: evolve = (float)v * 0.01f; break;
    case P_BTONE:
        btone = (float)v * 0.01f;
        for (k = 0; k < EN_NVOX; k++)
            if (vc[k].role == ROLE_LOOPS)
                loops_patches(k);
        break;
    case P_PADTONE: case P_BELLPATCH: case P_BASSPATCH:   /* a source of the voice's own: made here (a bank's: its
                                                  * bytes follow, en_patch_byte) */
        if (v < EN_NFACT) {
            k = p == P_PADTONE ? VOX_PAD : p == P_BELLPATCH ? VOX_BELLS : VOX_BASS;
            en_factory(k, v, par[P_BTONE], vstage[k]);
            patch_apply(k, 1);
        }
        break;
    case P_MOTION: motion = (float)v * 0.01f; break;
    case P_TUNE: tunefac = fm_exp2f((float)v * (1.0f / 1200.0f)); break;
    case P_TEMPO: tempo_changed(); break;
    case P_ROOT:
        if (nsteps)
            key_moved();
        break;
    case P_MIDI:
        if (!v)
            en_midi_out(0xB0, 123, 0);
        break;
    default:
        if (p >= P_VOX && p < P_PLAY)            /* a voice's own (FM, its envelopes, MOD) */
            vox_apply((p - P_VOX) / VX_N, (p - P_VOX) % VX_N, v);
        else if (p >= P_PLAY)
            play_apply((p - P_PLAY) / PL_N, (p - P_PLAY) % PL_N, v);
        break;
    }
}

static void pool_set(uint32_t p)
{
    pool = (p | 1u) & 0xFFFu;
    scale_set();
    if (deg >= nsteps)
        deg = 0;
    key_moved();
}

static void regrow(void)                        /* a new piece: fresh loops, home chord, patterns home */
{
    int l, v;
    for (v = 0; v < EN_NVOX; v++) {
        vc[v].rest = 0;
        if (vc[v].role == ROLE_LOOPS)
            for (l = 0; l < EN_NLOOP; l++) {
                loop_regrow(v, l);
                vc[v].loop_rest[l] = 0;
            }
    }
    deg = 0;
    key_moved();
    pulse_rest = 0;
    en_pulse_rest = 0;
    drums_pattern();
    drift_reset();
    change_pending = 0;
}

static void notes_off(void)
{
    int i;
    for (i = 0; i < 4; i++)
        en_midi_out(0xB0 | (uint32_t)i, 123, 0);
    en_midi_out(0xB9, 123, 0);
}

static void quiet(void)
{
    int i, k, v;
    for (v = 0; v < EN_NVOX; v++)
        for (i = 0; i < NPOLY; i++)
            vc[v].n[i].on = 0, vc[v].n[i].amp = 0.0f;
    for (i = 0; i < EN_NDRUM; i++)
        drum[i].on = 0, drum[i].wait = 0;
    for (i = 0; i < FB_N; i++)                  /* the FM block already made: quiet too */
        for (k = 0; k < FM6_N; k++)
            fm_buf[i][k] = 0.0f;
    notes_off();
}

static float arc, arc_tgt;
static uint32_t arc_left;

void en_init(void)
{
    int i, v;
    for (i = 0; i <= SN; i++)
        sint[i] = fm_sinf((float)i * (FM_TWO_PI / (float)SN));
    for (v = 0; v < EN_NVOX; v++) {             /* (an init again: no note of before rings on) */
        __builtin_memset(&vc[v], 0, sizeof vc[v]);
        vc[v].role = (uint8_t)v;
        vc[v].fo = 1.0f;
        vc[v].glide_k = 1.0f / (0.26f * EN_SR);
    }
    rng = 0x9E3779B9u;                           /* the same garden from every init (PLAY stirs it: en_seed), so a */
    mod_rng = 0x2545F491u;                       /* test can hear one setting against another on the same notes */
    en_playing = en_frozen = 0;                  /* (resting: PLAY starts it from the top) */
    en_ext = 0;                                  /* (no external clock until its pulses come) */
    ext_bpm = 0.0f;
    ext_n = 0;
    pad_gate = 0.0f;
    arc = arc_tgt = 0.0f;
    arc_left = 0;
    sixteenth = 0;
    pulse = 0;
    change_pending = 0;
    pulse_rest = 0;
    for (i = 0; i < EN_NDRUM; i++)
        drum[i].on = 0, drum[i].wait = 0;
    fm_pos = FM6_N;
    mod_init();
    plate_init();
    for (i = 0; i < TAPE_N; i++)
        tape[i] = 0;
    for (i = 0; i < PLEX_N; i++)
        plex[i] = 0;
    tape_w = plex_w = tape_ph = 0;
    head2 = head2_t = 0.75f;
    head2_v = 0.0f;
    echo_len = 0.0f;
    q_r = q_w = 0;
    for (i = 0; i < NMOD; i++) {
        mod[i].v = mod[i].tgt = 0.0f;
        mod[i].left = 0;
    }
    arcm = 0.0f;
    for (i = 0; i < P_NPARAMS; i++)              /* (the patches built as their parameters arrive) */
        apply(i, PARAMS[i].def);
    pool_set(EN_MODE_POOL[par[P_MODE]]);
    for (v = 0; v < EN_NVOX; v++) {             /* each voice its role, from silence */
        vc[v].role = (uint8_t)PL(v, ROLE);
        role_enter(v);
    }
    drums_pattern();
    drift_reset();
    pl_decay = pl_decay_t;
}

/* ------------------------------------------------------------ transport --- */
static void clock_pulse(void)
{
    if (pulse == 0)
        tick();
    if (!en_ext)
        midi(0xF8, 0, 0);
    if (++pulse >= 6)
        pulse = 0;
}

static void play_start(int from_top)
{
    int i, v;
    if (from_top) {
        pulse = 0;
        sixteenth = 0;
        for (i = 0; i < EN_NDRUM; i++)
            drum_pos[i] = 0;
        for (v = 0; v < EN_NVOX; v++)
            for (i = 0; i < EN_NLOOP; i++)
                vc[v].loop_pos[i] = 0;
    }
    pulse_left = 0.0f;
    en_playing = 1;
    pad_gate = 1.0f;
    for (v = 0; v < EN_NVOX; v++) {
        voice_t *c = &vc[v];
        if (c->role == ROLE_CHORD) {
            for (i = 0; i < NPOLY; i++)          /* from silence: the breaths start over, overlapping */
                if (c->n[i].amp < 1e-4f) {
                    c->n[i].t = START_T[i];
                    c->n[i].midi_note = 0;
                    chord_breathe(v, i);
                }
        } else if (c->role == ROLE_BASS) {
            if (c->n[0].amp < 1e-4f)
                c->n[0].note = c->n[0].target;
            c->strike = 1;
            midi(0x90u | (uint32_t)v, c->n[0].midi_note, 80);
        }
    }
    if (!en_ext)
        midi(from_top ? 0xFA : 0xFB, 0, 0);
}

static void play_stop(void)
{
    int d;
    for (d = 0; d < EN_NDRUM; d++)
        drum[d].wait = 0;                        /* a swung hit still waiting stays unplayed */
    en_playing = 0;
    pad_gate = 0.0f;
    if (!en_ext)
        midi(0xFC, 0, 0);
    notes_off();
}

static void drain(void)
{
    while (q_r != q_w) {
        uint32_t c = q[q_r % QN];
        int a = (int)(c >> 16 & 0xFFu), b = (int)(int16_t)(c & 0xFFFFu);
        BARRIER();
        q_r++;
        switch (c >> 24) {
        case C_SET:
            if (a < P_NPARAMS)
                apply(a, b);
            break;
        case C_PBYTE: {                          /* (b: offset << 8 | value, unsigned) */
            uint32_t off = (c >> 8) & 0xFFu, val = c & 0xFFu;
            if (a < EN_NVOX && off < FP_SIZE)
                vstage[a][off] = (uint8_t)(val > en_patch_max((int)off) ? en_patch_max((int)off) : val);
            break;
        }
        case C_PDONE:
            if (a < EN_NVOX)
                patch_apply(a, b);
            break;
        case C_PLOAD:                            /* the mailbox's newest patch */
            if (a < EN_NVOX) {
                uint8_t t[FP_SIZE];
                uint32_t s1 = pbox[a].seq, k;
                BARRIER();
                for (k = 0; k < FP_SIZE; k++)
                    t[k] = pbox[a].p[k];
                BARRIER();
                if (!(s1 & 1u) && pbox[a].seq == s1) {
                    for (k = 0; k < FP_SIZE; k++)
                        vstage[a][k] = t[k] > en_patch_max((int)k) ? en_patch_max((int)k) : t[k];
                    patch_apply(a, b);
                }
            }
            break;
        case C_POOL: pool_set((uint32_t)b & 0xFFFu); break;
        case C_PLAY:
            if (a && !en_playing)
                play_start(1);
            else if (!a && en_playing)
                play_stop();
            break;
        case C_NEXT:
            if (en_playing)
                change_pending = 1;
            else
                chord_change();
            break;
        case C_FREEZE: en_frozen = (uint8_t)(a != 0); break;
        case C_SEED:
            rng ^= (uint32_t)(b & 0xFFFF) * 2654435761u;
            if (!rng)
                rng = 1u;
            break;
        case C_CLOCK:
            en_ext = 1;
            ext_age = 0;
            if (a == EN_CLK_TICK) {
                ext_tick();
                if (en_playing)
                    clock_pulse();
            } else if (a == EN_CLK_START) {
                ext_n = 0;
                ext_t0 = smp_clock;
                play_start(1);
            } else if (a == EN_CLK_CONTINUE) {
                if (!en_playing)
                    play_start(0);
            } else if (en_playing) {
                play_stop();
            }
            break;
        case C_PANIC: quiet(); break;
        case C_REGROW: regrow(); break;
        }
    }
}

/* --------------------------------------------------------------- render --- */
#define BLK 256
static float mixl[BLK], mixr[BLK], rsend[BLK], esend[BLK];

/* the pad's bus: a 12 dB low-pass, darker than it sounds on paper (Brian Eno darkened everything: the
 * Synthi's filter below half, then tape). No chorus: a modulated delay against pure tones beats (on
 * the FM-1's speaker above all), and that read as pulsing. The width comes from the voices' places,
 * the echo's two heads and the plate. */
static float padl[BLK], padr[BLK];
static float plp[4];

static void pad_bus(uint32_t n)
{
    const float fc = fm_clampf(700.0f * fm_exp2f(bright * 0.03f + 0.6f * arcm), 300.0f, 9000.0f);
    const float c = 1.0f - fm_expf(-FM_TWO_PI * fc * (1.0f / EN_SR));
    uint32_t j;
    for (j = 0; j < n; j++) {
        float l, r;
        plp[0] += c * (padl[j] - plp[0]);
        plp[1] += c * (plp[0] - plp[1]);
        plp[2] += c * (padr[j] - plp[2]);
        plp[3] += c * (plp[2] - plp[3]);
        l = plp[1];
        r = plp[3];
        mixl[j] += l * 0.8f;
        mixr[j] += r * 0.8f;
        rsend[j] += (l + r) * 0.6f;
        esend[j] += (l + r) * 0.2f;
    }
    for (j = 0; j < 4; j++)
        plp[j] = fm_flush(plp[j]);
}

/* the FM6 blocks: each voice's notes, at its patch's level, moved by its five modulators (mod_of). Every note runs
 * its patch's LFO; MOD's LFO PM adds to its vibrato */
static float lfo_pm(int32_t lv, float pm) { return pm * (float)(lv - (1 << 23)) * (1.0f / 8388608.0f); }

static void chord_block(int v)                  /* a Chord voice's block: its breaths, to the pad's bus */
{
    voice_t *c = &vc[v];
    const int nn = chord_n(v), amb = PL(v, ENV) == ENV_AMB, pub = first_role(ROLE_CHORD) == v;
    float x[FM6_N], lvl = 0.0f;
    int32_t dtb[6], olb[6];
    mod_t m;
    int i;
    uint32_t j;
    c->tg += (pad_gate - c->tg) *               /* (the patch's envelopes: in at once, they shape it) */
             fm_minf((float)FM6_N / ((pad_gate <= 0.0f ? 5.0f : amb ? 2.0f : 0.01f) * EN_SR), 1.0f);
    for (i = 0; i < NPOLY; i++) {
        vnote_t *n = &c->n[i];
        float amp = n->amp, g, gl, gr, pn;
        int32_t lv, ld;
        if (amp < 1e-5f && (c->tg < 1e-4f || i >= nn)) {
            n->amp = 0.0f;
            if (pub)
                en_pad_amp[i] = 0.0f;
            continue;
        }
        chord_coefs(v, i);
        if (i >= nn)                             /* (past DENSITY's count: fading out) */
            n->tgt = 0.0f;
        lv = fm6_lfo_sample(&n->lfo);
        ld = fm6_lfo_delay(&n->lfo);
        mod_of(GEN(v, i), n->vp, &m, dtb, olb);
        g = c->vol * 0.5f * c->gain * c->fo * m.lvl;   /* (LEVEL ramped with the place, below) */
        pn = fm_clampf(n->pan + 0.35f * motion * mod[(i + 3) % NMOD_PAD].v + m.pan, -1.0f, 1.0f);
        gl = fm_cosf((pn + 1.0f) * (FM_PI / 4.0f)) * g;
        gr = fm_sinf((pn + 1.0f) * (FM_PI / 4.0f)) * g;
        if (fm_voice(&n->fm, n->vp, n->note + 0.08f * motion * mod[i].v + m.pitch + lfo_pm(lv, m.lfopm), lv, ld,
                     n->bloom + m.bloom, m.fb, dtb, olb, x)) {
            const float dl = (gl - n->gl) * (1.0f / FM6_N), dr = (gr - n->gr) * (1.0f / FM6_N);
            float pl = n->gl, pr = n->gr;         /* the place ramped across the block (no zipper as it moves) */
            for (j = 0; j < FM6_N; j++) {        /* a slow drift of pitch above, as tape */
                amp += (n->tgt - amp) * 0.01f;
                pl += dl;
                pr += dr;
                fm_buf[FB_PADL][j] += x[j] * amp * pl;
                fm_buf[FB_PADR][j] += x[j] * amp * pr;
            }
        } else {
            for (j = 0; j < FM6_N; j++)
                amp += (n->tgt - amp) * 0.01f;
        }
        n->amp = amp;
        n->gl = gl;
        n->gr = gr;
        if (pub) {
            en_pad_note[i] = (uint8_t)n->note;
            en_pad_amp[i] = amp;
        }
        lvl = fm_maxf(lvl, amp);
    }
    if (pub)
        en_pad_level = lvl;
}

static void loops_block(int v)                  /* a Loops voice's block: its strikes, to the bells' bus */
{
    voice_t *c = &vc[v];
    float x[FM6_N];
    int32_t dtb[6], olb[6];
    mod_t m;
    int i;
    uint32_t j;
    for (i = 0; i < NPOLY; i++) {
        vnote_t *n = &c->n[i];
        const uint8_t *p = n->vp;
        int32_t lv, ld;
        if (!n->on)
            continue;
        if (n->wait > 0) {                      /* (to the block: up to 0.7 ms off a hand's 0..60 ms) */
            n->wait -= FM6_N;
            if (n->wait > 0)
                continue;
            n->wait = 0;
        }
        if (!n->rung) {                         /* the strike: its own copy of its loop's shade, its patch's level */
            note_strike(v, i, c->loop_patch[n->loop], n->midi_note, (int32_t)(n->vel * 127.0f), 1);
            n->pl = n->gl;
            n->pr = n->gr;
            n->out = c->gain;
            n->lv = 1.0f;
            if (PL(v, ENV) != ENV_AMB) {         /* the patch's envelopes (or the ADSR): held GATE of a bar */
                n->held = (int32_t)(step_len * 16.0f * GATE_F(v));
            } else {                             /* AMB-1's: a bell held at a sustain let go after a beat */
                uint32_t car = fm6_carriers(p[FP_ALG] & 31u), j2, sus = 0;
                for (j2 = 0; j2 < 6u; j2++)
                    sus |= (car >> j2 & 1u) && p[j2 * FP_OP + FP_L1 + 2] && p[j2 * FP_OP + FP_OL];
                n->held = sus ? (int32_t)(step_len * 4.0f) : 0;
            }
            n->rung = 1;
        } else if (n->held > 0) {
            n->held -= FM6_N;
            if (n->held <= 0)
                fm6_note_key(&n->fm, p, 0);
        }
        lv = fm6_lfo_sample(&n->lfo);
        ld = fm6_lfo_delay(&n->lfo);
        mod_of(GEN(v, i), p, &m, dtb, olb);
        if (fm_voice(&n->fm, p, (float)n->midi_note + m.pitch + lfo_pm(lv, m.lfopm), lv, ld, m.bloom, m.fb, dtb, olb,
                     x)) {
            const float g = c->vol * 0.53f * n->vel * n->out * c->fo;   /* (AMB-1's bells' level, measured) */
            float gl = n->gl, gr = n->gr;
            if (m.pan != 0.0f) {                 /* SPACE: the note wanders from its loop's place */
                float a = (fm_clampf(LOOP_PAN[n->loop] + m.pan, -1.0f, 1.0f) + 1.0f) * (FM_PI / 4.0f);
                gl = fm_cosf(a);
                gr = fm_sinf(a);
            }
            {
                const float dl = (gl - n->pl) * (1.0f / FM6_N), dr = (gr - n->pr) * (1.0f / FM6_N),
                            dv = (m.lvl - n->lv) * (1.0f / FM6_N);
                float pl = n->pl, pr = n->pr, lg = n->lv;
                for (j = 0; j < FM6_N; j++) {
                    float sm;
                    lg += dv;
                    sm = x[j] * g * lg;
                    pl += dl;
                    pr += dr;
                    fm_buf[FB_BELLL][j] += sm * pl;
                    fm_buf[FB_BELLR][j] += sm * pr;
                    fm_buf[FB_BELLE][j] += sm;
                }
            }
            n->pl = gl;
            n->pr = gr;
            n->lv = m.lvl;
        }
        if (fm6_note_done(&n->fm, p, p[FP_ALG] & 31u)) {   /* rung out */
            n->on = 0;
            midi(0x80u | (uint32_t)v, n->midi_note, 0);
        }
    }
}

static void bass_block(int v)                   /* a Bass voice's block: its note, gliding, to the mix */
{
    voice_t *c = &vc[v];
    vnote_t *n = &c->n[0];
    const int env = PL(v, ENV) != ENV_AMB;      /* (the patch's envelopes: the level steady, the patch shapes it) */
    float x[FM6_N];
    int32_t dtb[6], olb[6];
    mod_t m;
    uint32_t j;
    if (c->rs > 0 && --c->rs == 0)               /* a new patch, the knob at rest: out, struck, in */
        c->xs = 1;
    if (c->xs && (c->xf <= 0.0f || n->amp < 1e-4f)) {
        note_strike(v, 0, vpat[v], (int32_t)n->note, 100, 1);
        c->xs = 0;
        c->held = (int32_t)(drift_left * GATE_F(v));
    }
    if (env) {                                   /* each chord (or PACE's step) struck, let go at GATE of it */
        if (c->strike && en_playing) {
            if (c->rest) {                       /* (sitting this chord out: let go now) */
                fm6_note_key(&n->fm, n->vp, 0);
                c->held = 0;
            } else {
                const int st = bass_step(v);
                note_strike(v, 0, vpat[v], (int32_t)n->target, 100, n->amp < 1e-4f);
                c->held = (int32_t)((st ? (float)st * step_len : drift_total) * GATE_F(v));
            }
        }
        if (c->held > 0 && (c->held -= FM6_N) <= 0)
            fm6_note_key(&n->fm, n->vp, 0);
    }
    c->strike = 0;
    if (n->amp < 1e-5f && !en_playing) {
        n->amp = 0.0f;
    } else {
        const float g = c->vol * 0.42f * 0.75f * c->gain * c->fo,
                    tgt = en_playing && (env || !c->rest) ? 1.0f : 0.0f,
                    rate = tgt > 0.0f ? 1.0f / ((env ? 0.003f : 2.5f) * EN_SR) : 1.0f / (4.0f * EN_SR),
                    xt = c->xs ? 0.0f : 1.0f;
        float amp = n->amp, gl, gr, xf = c->xf;
        int32_t lv = fm6_lfo_sample(&n->lfo), ld = fm6_lfo_delay(&n->lfo);
        n->note += (n->target - n->note) * fm_minf((float)FM6_N * c->glide_k, 1.0f);   /* GLIDE */
        mod_of(GEN(v, 0), n->vp, &m, dtb, olb);
        gl = (1.0f - fm_maxf(m.pan, 0.0f) * 0.5f) * m.lvl;   /* SPACE: the bass leans a little, never far */
        gr = (1.0f + fm_minf(m.pan, 0.0f) * 0.5f) * m.lvl;
        if (fm_voice(&n->fm, n->vp, n->note + m.pitch + lfo_pm(lv, m.lfopm), lv, ld, m.bloom, m.fb, dtb, olb, x)) {
            const float dl = (gl - n->gl) * (1.0f / FM6_N), dr = (gr - n->gr) * (1.0f / FM6_N);
            float pl = n->gl, pr = n->gr;
            for (j = 0; j < FM6_N; j++) {
                amp += (tgt - amp) * rate;
                xf = fm_clampf(xf + (xt > xf ? 1.0f : -1.0f) * (1.0f / 441.0f), 0.0f, 1.0f);   /* 10 ms */
                pl += dl;
                pr += dr;
                fm_buf[FB_BASSL][j] += x[j] * amp * xf * g * pl;
                fm_buf[FB_BASSR][j] += x[j] * amp * xf * g * pr;
            }
        } else {
            for (j = 0; j < FM6_N; j++)
                amp += (tgt - amp) * rate;
            xf = xt;
        }
        n->amp = amp;
        c->xf = xf;
        n->gl = gl;
        n->gr = gr;
    }
    if (first_role(ROLE_BASS) == v)
        en_bass_level = n->amp;
}

/* one FM6 block of every voice into fm_buf, each by its role; a voice changing role fades out, changes, fades in */
static void fm_block(void)
{
    int v, i;
    uint32_t j;
    for (i = 0; i < FB_N; i++)
        for (j = 0; j < FM6_N; j++)
            fm_buf[i][j] = 0.0f;
    mod_step();
    if (first_role(ROLE_CHORD) < 0)
        en_pad_level = 0.0f;
    if (first_role(ROLE_BASS) < 0)
        en_bass_level = 0.0f;
    for (v = 0; v < EN_NVOX; v++) {
        voice_t *c = &vc[v];
        const int want = PL(v, ROLE);
        if (want != c->role) {
            c->fo -= (float)FM6_N / (0.02f * EN_SR);
            if (c->fo <= 0.0f) {
                c->fo = 0.0f;
                c->role = (uint8_t)want;
                role_enter(v);
            }
        } else if (c->fo < 1.0f) {
            c->fo = fm_minf(c->fo + (float)FM6_N / (0.02f * EN_SR), 1.0f);
        }
        if (c->role == ROLE_CHORD)
            chord_block(v);
        else if (c->role == ROLE_LOOPS)
            loops_block(v);
        else
            bass_block(v);
    }
}

/* this chunk's n samples of the FM6 voices, a block made whenever the last is used up: the pad to its bus, the
 * bells and the bass to the mix and the sends */
static void fm_pull(uint32_t n)
{
    uint32_t j;
    for (j = 0; j < n; j++) {
        float b, bl, br;
        if (fm_pos >= FM6_N) {
            fm_block();
            fm_pos = 0;
        }
        padl[j] = fm_buf[FB_PADL][fm_pos];
        padr[j] = fm_buf[FB_PADR][fm_pos];
        bl = fm_buf[FB_BASSL][fm_pos];
        br = fm_buf[FB_BASSR][fm_pos];
        b = 0.5f * (bl + br);
        mixl[j] += fm_buf[FB_BELLL][fm_pos] + bl;
        mixr[j] += fm_buf[FB_BELLR][fm_pos] + br;
        rsend[j] += fm_buf[FB_BELLE][fm_pos] * 0.7f + b * 0.08f;
        esend[j] += fm_buf[FB_BELLE][fm_pos] * 0.85f;
        fm_pos++;
    }
}

/* a hat: the metal and noise, high-passed (Chamberlin SVF, ~4 to 10 kHz by Hats); no pitch, little echo */
static void render_hat(drum_t *v, uint32_t n, float g, float pl, float pr)
{
    uint32_t j;
    int k;
    float x1, x2, amp, att;
    if (!v->on)
        return;
    x1 = v->x1, x2 = v->x2, amp = v->amp, att = v->att;
    for (j = 0; j < n; j++) {
        float in = 0.0f, hp, x;
        for (k = 0; k < 4; k++) {
            v->mph[k] += HAT_INC[k];
            if (v->mph[k] >= 1.0f)
                v->mph[k] -= 1.0f;
            in += v->mph[k] < 0.5f ? 0.25f : -0.25f;
        }
        noise = noise * 1664525u + 1013904223u;
        in = 0.5f * in + 0.6f * (float)(int32_t)noise * (1.0f / 2147483648.0f);
        hp = in - x2 - 0.8f * x1;
        x1 += hat_f * hp;
        x2 += hat_f * x1;
        x = hp * amp * att * g;
        mixl[j] += x * pl;
        mixr[j] += x * pr;
        rsend[j] += x * 0.2f;
        esend[j] += x * 0.08f;
        amp *= v->adec;
        att += (1.0f - att) * 0.3f;
    }
    v->x1 = fm_flush(x1);
    v->x2 = fm_flush(x2);
    v->amp = amp;
    v->att = att;
    v->on = amp > 0.0005f;
}

static void render_drums(uint32_t n)
{
    uint32_t j;
    const float g = pulse_vol * 1.1f;
    drum_t *v;
    v = &drum[0];                                /* thump: a sine falling to its note */
    if (v->on) {
        float ph = v->ph, inc = v->inc, amp = v->amp, att = v->att;
        for (j = 0; j < n; j++) {
            float x = sine(ph) * amp * att * g;
            mixl[j] += x;
            mixr[j] += x;
            rsend[j] += x * 0.1f;
            ph += inc;
            if (ph >= 1.0f)
                ph -= 1.0f;
            inc += (v->inc_end - inc) * 0.0012f;
            amp *= v->adec;
            att += (1.0f - att) * 0.02f;
        }
        v->ph = ph;
        v->inc = inc;
        v->amp = amp;
        v->att = att;
        v->on = amp > 0.0005f;
    }
    render_hat(&drum[1], n, g * 0.2f, 0.6f, 1.0f);    /* closed: a little right */
    render_hat(&drum[2], n, g * 0.14f, 1.0f, 0.7f);   /* open: a little left */
}

static void render_chunk(int32_t *out, uint32_t n, float gain)
{
    uint32_t j;
    const float rg = rev_send * rev_in_gain * (1.0f - 0.25f * arcm), eg = echo_send;
    for (j = 0; j < n; j++)
        mixl[j] = mixr[j] = rsend[j] = esend[j] = 0.0f;
    fm_pull(n);
    pad_bus(n);
    render_drums(n);
    for (j = 0; j < n; j++) {
        float wl, wr, l, r, el, er, xl, xr;
        echoes(esend[j] * eg, &el, &er, &xl, &xr);
        reverb((rsend[j] * rg + (el + er + xl + xr) * 0.35f * rev_send) * 0.8f, &wl, &wr);
        l = (mixl[j] + 0.75f * el + 0.3f * xl + 1.3f * wl) * 0.36f * gain;
        r = (mixr[j] + 0.75f * er + 0.3f * xr + 1.3f * wr) * 0.36f * gain;
        out[2 * j] = (int32_t)(fm_tanhf(l) * 8300000.0f);
        out[2 * j + 1] = (int32_t)(fm_tanhf(r) * 8300000.0f);
    }
}

/* the arc: a slow tide, minutes long, between sparse (-1) and full (+1); Motion sets how far it goes */

static void control(uint32_t n)                 /* once a chunk: the slow things */
{
    int i;
    mods_step(n);
    heads_step(n);
    if (arc_left <= n) {
        float r = frand() * 2.0f - 1.0f;
        arc_tgt = r * (0.6f + 0.4f * frand()) + (r > 0.0f ? 0.25f : -0.25f);   /* away from the middle */
        arc_left = (uint32_t)((50.0f + 100.0f * frand()) * EN_SR);
    } else {
        arc_left -= n;
    }
    if (en_playing)
        arc += (fm_clampf(arc_tgt, -1.0f, 1.0f) - arc) * (float)n * (1.0f / (35.0f * EN_SR));
    arcm = arc * motion;
    en_arc = arcm;
    pl_decay += ((en_frozen ? 0.9995f : pl_decay_t) - pl_decay) * 0.002f;
    pl_dd2 = fm_clampf(pl_decay + 0.15f, 0.25f, 0.5f);
    rev_in_gain += ((en_frozen ? 0.0f : 1.0f) - rev_in_gain) * 0.01f;
    if (en_playing && !en_frozen && !change_pending) {
        drift_left -= (float)n;
        if (!fifth_done && drift_left < 0.5f * drift_total) {
            fifth_done = 1;                      /* halfway: a bass may lean on the fifth (its DENSITY) */
            for (i = 0; i < EN_NVOX; i++)
                if (vc[i].role == ROLE_BASS && frand() < (float)PL(i, DENS) * 0.01f && chord_nt >= 3)
                    bass_to(i, chord_pcs[2]);
        }
        if (drift_left <= 0.0f)
            change_pending = 1;
    }
    en_drift = drift_total > 0.0f ? 1.0f - fm_clampf(drift_left / drift_total, 0.0f, 1.0f) : 0.0f;
    for (i = 0; i < EN_NLOOP; i++)
        en_loop_glow[i] *= 0.985f;
    for (i = 0; i < EN_NDRUM; i++)
        en_drum_glow[i] *= 0.95f;
}

void en_render(int32_t *out, uint32_t n, uint32_t gain_q12)
{
    const float gain = (float)gain_q12 * (1.0f / 4096.0f) * 2.0f;
    drain();
    if (en_ext && ++ext_age > 100u) {           /* the clock gone: the Tempo knob's again */
        en_ext = 0;
        if (ext_bpm > 0.0f) {
            ext_bpm = 0.0f;
            tempo_changed();
        }
    }
    smp_clock += n;
    while (n) {
        uint32_t k = n > BLK ? BLK : n;
        int timed = en_playing && !en_ext;
        if (timed) {
            if (pulse_left <= 0.0f) {
                clock_pulse();
                pulse_left += step_len * (1.0f / 6.0f);
            }
            if ((float)k > pulse_left)
                k = (uint32_t)pulse_left + 1u;
        }
        if (k > n)
            k = n;
        {
            int32_t w = drums_wait();            /* a swung hit lands on its sample */
            if (w > 0 && (uint32_t)w < k)
                k = (uint32_t)w;
        }
        if (timed)
            pulse_left -= (float)k;
        control(k);
        render_chunk(out, k, gain);
        drums_wait_step((int32_t)k);
        out += 2 * k;
        n -= k;
    }
}
