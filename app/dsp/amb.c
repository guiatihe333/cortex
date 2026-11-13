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

static const en_param_t PARAMS[P_NPARAMS] = {
    {"Voice 1", 0, 100, 60},  {"Voice 2", 0, 100, 65},  {"Voice 3", 0, 100, 45},  {"Pulse", 0, 100, 40},
    {"Density", 0, 100, 40},  {"Evolve", 0, 100, 35},   {"Drift", 0, 100, 25},    {"Hits", 0, 100, 35},
    {"Echo", 0, 100, 45},     {"Echo time", 0, 5, 3},   {"Reverb", 0, 100, 65},   {"Size", 0, 100, 80},
    {"Bright", 0, 100, 40},   {"Swell", 0, 100, 60},    {"V2 tone", 0, 100, 50}, {"Motion", 0, 100, 50},
    {"Tune", -50, 50, 0},     {"MIDI out", 0, 1, 1},    {"Tempo", 30, 240, 72},   {"Root", 0, 11, 2},
    {"Mode", 0, EN_NMODES - 1, 1}, {"V2 oct", -2, 2, 0},   {"V1 patch", 0, EN_NSRC - 1, 0},
    {"V3 oct", -1, 1, 0},     {"Glide", 0, 100, 20},    {"Fifth", 0, 100, 35},
    {"Swing", 0, 100, 0},     {"Hats", 0, 100, 50},
    {"V2 patch", 0, EN_NSRC - 1, EN_FACT_BELL}, {"V3 patch", 0, EN_NSRC - 1, EN_FACT_BASS},
#define MS_PARAMS(w, n)                                                                                            \
    {w " mod" n " depth", 0, 100, 0}, {w " mod" n " rate", 0, 100, 31},                                            \
    {w " mod" n " shape", 0, EN_NMODSHAPE - 1, 0}, {w " mod" n " target", 0, EN_NMODDEST, 0},                      \
    {w " mod" n " spread", 0, 100, 100}, {w " mod" n " fade", 0, 100, 0},                                          \
    {w " mod" n " sync", 0, EN_NMODSYNC - 1, 0}, {w " mod" n " smooth", 0, 100, 0}
#define VOX_PARAMS(w)                                                                                              \
    {w " own", 0, 1, 0}, {w " index", -24, 24, 0}, MS_PARAMS(w, "1"), MS_PARAMS(w, "2"), MS_PARAMS(w, "3"),        \
    MS_PARAMS(w, "4"), MS_PARAMS(w, "5")
    VOX_PARAMS("V1"), VOX_PARAMS("V2"), VOX_PARAMS("V3"),
#define PL_PARAMS(w, role, dens, pace, gate)                                                                       \
    {w " env", 0, EN_NENV - 1, ENV_PATCH}, {w " gate", 4, 100, gate}, {w " role", 0, EN_NROLE - 1, role},          \
    {w " octave", -2, 2, 0}, {w " density", 0, 100, dens}, {w " pace", 0, 100, pace}, {w " glide", 0, 100, 20},    \
    {w " attack", 0, 100, 5}, {w " decay", 0, 100, 40}, {w " sustain", 0, 100, 70}, {w " release", 0, 100, 55}
    PL_PARAMS("V1", ROLE_CHORD, 100, 60, 70), PL_PARAMS("V2", ROLE_LOOPS, 40, 60, 25),
    PL_PARAMS("V3", ROLE_BASS, 35, 0, 80),
#undef PL_PARAMS
    {"Tape source", 0, 9, 0}, {"Dry/Wet", 0, 100, 50}, {"SOS", 0, 100, 70}, {"Speed", 0, 100, 75},
    {"Gene", 0, 100, 100}, {"Slide", 0, 100, 0}, {"Morph", 0, 100, 45}, {"Organize", 0, 7, 0},
    {"Tape FX", 0, 1, 0},     {"Wow", 0, 100, 25},      {"Drive", 0, 100, 20},    {"Tone", 0, 100, 85},
#define TM_PARAMS(n, target, depth, rate, shape)                                                                   \
    {"TM" n " target", 0, EN_NTMDEST, target},     {"TM" n " depth", 0, 100, depth}, {"TM" n " rate", 0, 100, rate}, \
    {"TM" n " shape", 0, EN_NMODSHAPE - 1, shape}, {"TM" n " sync", 0, 1, 0}, {"TM" n " smooth", 0, 100, 0}
    TM_PARAMS("1", 0, 0, 20, SHAPE_WALK), TM_PARAMS("2", 0, 0, 30, SHAPE_SINE), TM_PARAMS("3", 0, 0, 40, SHAPE_TRI),
    TM_PARAMS("4", 0, 0, 15, SHAPE_SINE), TM_PARAMS("5", 0, 0, 50, SHAPE_SH),
#undef TM_PARAMS
    {"Song", 0, EN_NSONG, 0}, {"Follow", 0, 100, 75}, {"Vary", 0, 100, 20}, {"Start", 0, 127, 0},
    {"V1 channel", 1, 16, 1}, {"V2 channel", 1, 16, 2}, {"V3 channel", 1, 16, 3}, {"Notes in", 0, 1, 0},
    {"Key sync", 0, 2, 0},     {"Visual", 0, 9, 0},      {"Colour", 0, 4, 0},      {"Trails", 0, 100, 60},
    {"React", 0, 100, 50},    {"Feedback", 0, 100, 25}, {"RGB", 0, 100, 20},     {"Hue", 0, 100, 10},
    {"Sync", 0, 100, 10},     {"Geometry", 0, 16, 0},    {"Sides", 3, 12, 5},      {"Mirror", 0, 4, 0},
    {"Speed", 0, 100, 50},    {"Layer", 0, 9, 0},       {"Blend", 0, 4, 0},       {"Mix", 0, 100, 60},
    {"Pulse", 0, 100, 50},    {"Morph", 0, 100, 0},     {"Mutate", 0, 100, 30},   {"Flow", 0, 100, 20},
    {"Chaos", 0, 100, 30},    {"Mosh", 0, 100, 35},     {"Sort", 0, 100, 15},     {"Corrupt", 0, 100, 20},
    {"Slice", 0, 100, 15},    {"Colorama", 0, 100, 0},  {"Poster", 0, 100, 0},    {"Pixel", 0, 100, 0},
    {"Key", 0, 100, 0},
    {"Freq", 0, 100, 40},     {"Wave", 0, 7, 0},        {"Angle", 0, 100, 0},     {"Scroll", -100, 100, 20},
    {"Freq 2", 0, 100, 0},    {"Combine", 0, 4, 1},     {"Warp", 0, 100, 30},     {"Osc sync", 0, 2, 0},
    {"Zoom", -100, 100, 0},   {"Rotate", -100, 100, 0}, {"Shift", -100, 100, 0},   {"Target", 0, 2, 0},
    {"Geometry 2", 0, 16, 0}, {"Sides 2", 3, 12, 6},    {"Size 2", 10, 200, 60},
    {"Bars", 0, 2, 0},        {"Delay", 1, 15, 4},      {"Bright", 0, 100, 50},
    {"Ear", 0, 100, 30},
    {"Source 1", 0, EN_AR_NSRC - 1, 2}, {"Target 1", 0, EN_AR_NTGT, 21}, {"Depth 1", -100, 100, 40},
    {"Source 2", 0, EN_AR_NSRC - 1, 4}, {"Target 2", 0, EN_AR_NTGT, 6}, {"Depth 2", -100, 100, 35},
    {"Source 3", 0, EN_AR_NSRC - 1, 5}, {"Target 3", 0, EN_AR_NTGT, 24}, {"Depth 3", -100, 100, 40},
    {"Source 4", 0, EN_AR_NSRC - 1, 0}, {"Target 4", 0, EN_AR_NTGT, 0}, {"Depth 4", -100, 100, 30},
#define VF_PARAMS(w) {w " filter", 0, 1, 0}, {w " cutoff", 0, 100, 60}, {w " resonance", 0, 100, 25}, {w " f.env", 0, 100, 30}
    VF_PARAMS("V1"), VF_PARAMS("V2"), VF_PARAMS("V3"),
#undef VF_PARAMS
#define XR_PARAMS(w, n) {w " r" n " source", 0, XS_N - 1, 0}, {w " r" n " target", 0, XT_N - 1, 0}, \
    {w " r" n " depth", -100, 100, 0}, {w " r" n " when", 0, XW_N - 1, XW_NOTE}, \
    {w " r" n " curve", 0, XC_N - 1, XC_LINEAR}, {w " r" n " smooth", 0, 100, 0}, \
    {w " r" n " min", 0, 100, 0}, {w " r" n " max", 0, 100, 100}
#define EX_PARAMS(w) {w " expr", 0, 100, 0}, {w " style", 0, XSTYLE_N - 1, XSTYLE_NATURAL}, \
    {w " phrase", 0, 100, 50}, {w " variation", 0, 100, 30}, {w " memory", 0, 100, 40}, {w " link", 0, 100, 50}, \
    {w " morph src", 0, EN_NSRC - 1, 0}, {w " morph", 0, 100, 0}, {w " morph kind", 0, XM_N - 1, XM_COLOR}, \
    XR_PARAMS(w, "1"), XR_PARAMS(w, "2"), XR_PARAMS(w, "3"), XR_PARAMS(w, "4"), \
    XR_PARAMS(w, "5"), XR_PARAMS(w, "6"), XR_PARAMS(w, "7"), XR_PARAMS(w, "8")
    EX_PARAMS("V1"), EX_PARAMS("V2"), EX_PARAMS("V3"),
    {"Motif", 0, 100, 35}, {"Rotation", 0, 100, 25}, {"Accent", 0, 100, 25}, {"Passing", 0, 100, 22},
    {"Humanize", 0, 100, 20}, {"Space", 0, 100, 0}, {"Range", 0, 100, 100},
    {"Dialogue", 0, 100, 0}, {"Length", 0, 100, 20}, {"Tension", 0, 100, 40}, {"Resolve", 0, 100, 70},
#undef EX_PARAMS
#undef XR_PARAMS
#undef VOX_PARAMS
#undef MS_PARAMS
};
_Static_assert(EN_NSLOT == 5, "VOX_PARAMS: five modulators");
_Static_assert(PL_N == 11, "PL_PARAMS: eleven a voice");
_Static_assert(sizeof PARAMS / sizeof *PARAMS == P_NPARAMS, "a name a parameter");
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
static const char *const TSRC_NAME[10] = {"Mix", "Voice 1", "Voice 2", "Voice 3", "File", "USB", "Play 1",
                                          "Play 2", "Play 3", "Play All"};
static const char *const TMDEST_NAME[EN_NTMSET] = {"Speed", "Gene", "Slide", "Morph", "Dry/Wet", "Tone", "Wow",
                                                   "Drive", "SOS", "Organize"};
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
const char *en_expr_target_scope(int target, int role)
{
    if (target == XT_OFF) return "ROUTE OFF";
    if (target == XT_DRIFT) return "GLOBAL HARMONY";
    if (target == XT_SWING) return "GLOBAL GROOVE";
    if (target == XT_PASSING) return role == ROLE_BASS ? "BASS ONLY" : "INACTIVE: BASS";
    if (target == XT_MOTIF || target == XT_ROTATION || target == XT_ACCENT ||
        (target >= XT_HUMAN && target <= XT_RESOLVE))
        return role == ROLE_LOOPS ? "LOOPS ONLY" : "INACTIVE: LOOPS";
    return "ALL ROLES";
}
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
static char *cat_word(char *b, const char *w)
{
    while (*w)
        *b++ = *w++;
    return b;
}
void en_moddest_text(int d, char *b)            /* (M1..M5 depth, rate: the voice's own modulators) */
{
    if (d >= MOD_EXPR && d < EN_NMODDEST) {
        copy9(d == MOD_EXPR ? "ExprAmt" : "ExprVar",b); return;
    }
    if (d >= MOD_SEQ && d < MOD_EXPR) {
        static const char *const names[15] = {"Density", "Evolve", "Drift", "Motif", "Rotation", "Accent", "Passing", "Swing", "Humanize", "Space", "Range", "Dialogue", "Length", "Tension", "Resolve"};
        copy9(names[d - MOD_SEQ], b); return;
    }
    if (d >= MOD_MSLOT && d < MOD_SEQ) {
        b[0] = 'M';
        b[1] = (char)('1' + (d - MOD_MSLOT) % 5);
        copy9(d - MOD_MSLOT < 5 ? " depth" : " rate", b + 2);
        return;
    }
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
        par[P_MS(v, 0, MS_DEST)] = (int16_t)(dest + 1);
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
    /* (1.5.0, CORTEX: the pieces after sleep and dreams) Hypnagogia, falling asleep (D Lydian, a halo, slow); Theta,
     * the dreaming brain's waves (A Dorian); REM, the eyes moving (E Aeolian, glass, quicker); Morpheus, who takes
     * human form (G Mixolydian, voices); Phobetor, the nightmare (C# Phrygian, low, restless); Phantasos, the
     * shapes of things (F# pentatonic, bells); Lethe, the river of forgetting (Eb Ionian, strings, long); Nyx, the
     * night itself (B minor pentatonic, deep, sparse) */
    {"Hypnagogia",   {60, 50, 35, 0, 30, 40, 50, 0, 50, 4, 80, 95, 35, 70, 30, 60, 0, 1, 58, 2, 3, 0, 5, 0, 40, 30, 0, 40, 30, 18, 30, 6}},
    {"Theta",        {55, 65, 40, 0, 40, 45, 30, 0, 45, 3, 70, 85, 45, 55, 45, 55, 0, 1, 66, 9, 1, 0, 4, 0, 25, 35, 0, 50, 25, 27, 25, 0}},
    {"REM",          {45, 75, 50, 0, 60, 60, 15, 0, 55, 2, 60, 75, 60, 45, 65, 70, 0, 1, 88, 4, 5, 1, 2, 0, 15, 40, 0, 60, 35, 45, 70, 2}},
    {"Morpheus",     {85, 30, 30, 0, 30, 35, 45, 0, 30, 5, 85, 90, 60, 70, 35, 55, 0, 1, 56, 7, 4, 0, 1, -1, 45, 30, 0, 40, 35, 23, 40, 3}},
    {"Phobetor",     {50, 45, 65, 0, 40, 55, 20, 0, 60, 1, 65, 80, 30, 40, 70, 75, 0, 1, 80, 1, 2, -1, 4, 0, 10, 50, 0, 70, 35, 47, 80, 3}},
    {"Phantasos",    {35, 85, 25, 0, 55, 50, 25, 0, 45, 2, 65, 80, 55, 45, 90, 50, 0, 1, 76, 6, 6, 1, 2, 1, 10, 30, 10, 85, 45, 31, 70, 1}},
    {"Lethe",        {65, 40, 30, 0, 25, 30, 70, 0, 70, 4, 80, 95, 30, 80, 20, 85, 0, 1, 52, 3, 0, 0, 3, 0, 50, 20, 0, 30, 40, 15, 20, 4}},
    {"Nyx",          {55, 55, 50, 0, 35, 50, 35, 0, 55, 3, 75, 90, 35, 60, 50, 65, 0, 1, 60, 11, 7, 0, 5, -1, 30, 45, 0, 45, 30, 20, 50, 6}},
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
    {                                            /* each piece's own: a second modulator a voice, the reel's first */
        static const int8_t M2[EN_NPIECES][EN_NVOX][4] = {   /* target, shape, depth, rate */
            {{MOD_PITCH, SHAPE_SINE, 12, 18}, {MOD_METAL, SHAPE_WALK, 20, 30}, {MOD_LEVEL, SHAPE_SINE, 15, 25}},
            {{MOD_SPACE, SHAPE_WALK, 35, 15}, {MOD_DECAY, SHAPE_SH, 25, 35}, {MOD_TIMBRE, SHAPE_SINE, 15, 12}},
            {{MOD_LFOPM, SHAPE_SINE, 30, 20}, {MOD_SPACE, SHAPE_TRI, 30, 22}, {MOD_PITCH, SHAPE_WALK, 10, 15}},
            {{MOD_TIMBRE, SHAPE_TRI, 25, 40}, {MOD_RATIO, SHAPE_SH, 20, 45}, {MOD_FEEDBACK, SHAPE_SINE, 15, 30}},
            {{MOD_FEEDBACK, SHAPE_CHAOS, 20, 45}, {MOD_METAL, SHAPE_CHAOS, 30, 50}, {MOD_LEVEL, SHAPE_SQUARE, 15, 55}},
            {{MOD_SPACE, SHAPE_SINE, 45, 10}, {MOD_TIMBRE, SHAPE_WALK, 30, 18}, {MOD_SPACE, SHAPE_WALK, 25, 14}},
            {{MOD_METAL, SHAPE_WALK, 25, 35}, {MOD_RATIO, SHAPE_WALK, 30, 40}, {MOD_DECAY, SHAPE_TRI, 20, 30}},
            {{MOD_DECAY, SHAPE_WALK, 30, 25}, {MOD_PITCH, SHAPE_SINE, 15, 28}, {MOD_TIMBRE, SHAPE_CHAOS, 25, 35}}};
        static const int8_t T1[EN_NPIECES][4] = {   /* the reel's: target, shape, depth, rate */
            {2, SHAPE_WALK, 30, 15}, {3, SHAPE_SINE, 25, 12}, {5, SHAPE_WALK, 30, 10}, {1, SHAPE_TRI, 25, 30},
            {0, SHAPE_SH, 10, 45}, {2, SHAPE_SINE, 40, 8}, {9, SHAPE_SH, 30, 35}, {3, SHAPE_WALK, 35, 20}};
        int v;
        for (v = 0; v < EN_NVOX; v++) {
            par[P_MS(v, 1, MS_DEST)] = (int16_t)(M2[i][v][0] + 1);
            par[P_MS(v, 1, MS_SHAPE)] = M2[i][v][1];
            par[P_MS(v, 1, MS_DEPTH)] = M2[i][v][2];
            par[P_MS(v, 1, MS_RATE)] = M2[i][v][3];
        }
        par[P_TM(0, TARGET)] = (int16_t)(T1[i][0] + 1);
        par[P_TM(0, SHAPE)] = T1[i][1];
        par[P_TM(0, DEPTH)] = T1[i][2];
        par[P_TM(0, RATE)] = T1[i][3];
    }
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

static float drift_secs(int v) { return v < 25 ? 2.0f * fm_exp2f((float)v * 0.17194612f) : 20.0f * fm_exp2f((float)v * 0.039069f); }   /* 2 s .. 5 min; preserve existing values from 25 upward */
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

/* the ADSR's knobs as times (1.5.20), ms: the attack's rise from silence (its rate 99 - A * 0.85), a decay's or a
 * release's fall of 60 dB (99 - x * 3 / 4); fm6_core's envelope at the knob's rate, timed by tools' model and the
 * simulator (a key at middle C: a patch's rate scaling makes high notes quicker) */
static const uint16_t ADSR_ATK_MS[101] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2,
    3, 3, 3, 3, 4, 5, 5, 6, 6, 7, 7, 8, 8, 10, 11, 11,
    11, 13, 17, 17, 19, 19, 22, 22, 27, 27, 34, 39, 39, 39, 45, 54,
    54, 68, 77, 77, 77, 91, 109, 109, 136, 136, 136, 155, 181, 181, 218, 272,
    272, 272, 311, 363, 363, 436, 545, 545, 545, 623, 623, 727, 872, 872, 872, 1090,
    1247, 1247, 1454, 1746, 1746, 1746, 2182, 2182, 2494, 2909, 2909, 2909, 3491, 4364, 4364, 4987,
    5818, 5818, 5818, 6982, 8728};
static const uint16_t ADSR_FALL_MS[101] = {
    3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 9, 11,
    13, 13, 13, 15, 18, 18, 18, 23, 26, 26, 26, 30, 37, 37, 37, 46,
    46, 46, 52, 61, 61, 61, 73, 91, 91, 91, 104, 122, 122, 122, 146, 146,
    183, 183, 209, 209, 244, 244, 292, 292, 366, 366, 418, 418, 487, 487, 585, 585,
    731, 731, 731, 836, 975, 975, 975, 1170, 1462, 1462, 1462, 1671, 1950, 1950, 1950, 2340,
    2925, 2925, 2925, 3343, 3343, 3343, 3900, 4680, 4680, 4680, 5850, 6686, 6686, 6686, 7800, 9360,
    9360, 9360, 11700, 11700, 13371};
static void ms_text(int t, char *b)              /* "4 ms", "120 ms", "1.2 s", "13 s" */
{
    if (t < 1000) {
        b = itoa_s(t, b);
        *b++ = ' ';
        *b++ = 'm';
    } else {
        b = tenths((t + 50) / 100, b);
        *b++ = ' ';
    }
    *b++ = 's';
    *b = 0;
}
static float vcf_hz(int cut) { return 30.0f * fm_exp2f((float)cut * 0.0924f); }   /* CUTOFF: 30 Hz .. 18 kHz */

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
    else if (mk == MS_DEST && v == EN_MOD_OFF)
        s = "Off";
    else if (mk == MS_DEST) {
        en_moddest_text(v - 1, b);
        return;
    } else if (mk == MS_SHAPE)
        s = EN_MODSHAPE_NAME[(unsigned)v < EN_NMODSHAPE ? v : 0];
    else if (mk == MS_SYNC)
        s = SYNC_NAME[(unsigned)v < EN_NMODSYNC ? v : 0];
    else if (mk == MS_RATE && par[P_MS(vv, ms, MS_SYNC)] == SYNC_TEMPO)
        s = DIV_NAME[div_of(v < 0 ? 0 : v > 100 ? 100 : v)];
    else if (mk == MS_FADE && v == 0)
        s = "Off";
    else if (i == P_RL(SRC))
        s = TSRC_NAME[(unsigned)v < 10u ? v : 0];
    else if (i == P_SG(SONG) && v == 0)
        s = "Off";
    else if (i == P_ST(NOTES))
        s = v ? "Play" : "Scale";
    else if (i == P_ST(KSYNC))
        s = v == 1 ? "Cortex" : v == 2 ? "Tape" : "Off";
    else if (i == P_ST(VMODE)) {
        static const char *const VM[10] = {"Bloom", "Ridge", "Prism", "Swarm", "Tunnel", "Plasma", "Spiral", "XOR",
                                           "Osc", "Score"};
        s = VM[(unsigned)v < 10u ? v : 0];
    } else if (i == P_ST(VGEO) || i == P_ST(VGEO2)) {
        static const char *const VG[17] = {"Off", "Polygon", "Lissajous", "Grid", "Mandala", "Scope", "Rutt", "Star",
                                           "Rings", "Spirograph", "Rose", "Cube", "Tunnel", "Starfield", "Circle",
                                           "Lattice", "Spiral"};
        s = VG[(unsigned)v < 17u ? v : 0];
    } else if (i == P_ST(VLAYER)) {
        static const char *const VL[10] = {"Off", "Bloom", "Ridge", "Prism", "Swarm", "Tunnel", "Plasma", "Spiral", "XOR",
                                           "Osc"};
        s = VL[(unsigned)v < 10u ? v : 0];
    } else if (i == P_ST(VOSHAPE)) {
        static const char *const VW[8] = {"Sine", "Tri", "Saw", "Square", "Noise", "Rings", "Polygon", "Spiral"};
        s = VW[(unsigned)v < 8u ? v : 0];
    } else if (i == P_ST(VOCOMB)) {
        static const char *const VC[5] = {"Add", "Multiply", "Diff", "Warp", "Field"};
        s = VC[(unsigned)v < 5u ? v : 0];
    } else if (i == P_ST(VXTGT)) {
        static const char *const VT[3] = {"Feedback", "Lines", "Both"};
        s = VT[(unsigned)v < 3u ? v : 0];
    } else if (i == P_ST(VOSYNC)) {
        static const char *const VS[3] = {"Free", "Notes", "Beat"};
        s = VS[(unsigned)v < 3u ? v : 0];
    } else if (i == P_ST(VBLEND)) {
        static const char *const VB[5] = {"Add", "Screen", "Max", "Diff", "Mask"};
        s = VB[(unsigned)v < 5u ? v : 0];
    } else if (i == P_ST(VBARS)) {
        static const char *const VB2[3] = {"Show", "Auto", "Hide"};
        s = VB2[(unsigned)v < 3u ? v : 0];
    } else if (i >= P_ST(AR1S) && i <= P_ST(AR4D) && (i - P_ST(AR1S)) % 3 == 0) {   /* a react slot's source */
        static const char *const AS[EN_AR_NSRC] = {"Off", "Level", "Bass", "Mid", "High", "Beat", "Voice 1", "Voice 2",
                                                   "Voice 3", "Notes", "Chord", "LFO", "Random"};
        s = AS[(unsigned)v < EN_AR_NSRC ? v : 0];
    } else if (i >= P_ST(AR1S) && i <= P_ST(AR4D) && (i - P_ST(AR1S)) % 3 == 1) {   /* its target: a setting's name */
        s = v == 0 ? "Off" : v >= EN_AR_NTGT ? "All" : PARAMS[P_ST(VMODE) + v - 1].name;
    } else if (i == P_ST(VMIRROR)) {
        static const char *const VR[5] = {"Off", "Mirror", "Flip", "Quad", "Kaleido"};
        s = VR[(unsigned)v < 5u ? v : 0];
    } else if (i == P_ST(VPAL)) {
        static const char *const VP[5] = {"Theme", "Ember", "Ice", "Acid", "Mono"};
        s = VP[(unsigned)v < 5u ? v : 0];
    }
    else if (i >= P_ST(CH1) && i <= P_ST(CH3)) {   /* "Ch 5" */
        b = cat_word(b, "Ch ");
        b = itoa_s(v, b);
        *b = 0;
        return;
    }
    else if (i == P_SG(SONG) || i == P_SG(START)) {   /* "Song 2", "Bar 5" */
        b = cat_word(b, i == P_SG(SONG) ? "Song " : "Bar ");
        b = itoa_s(i == P_SG(SONG) ? v : v + 1, b);
        *b = 0;
        return;
    } else if (i >= P_TMOD && i < P_SONGP && (i - P_TMOD) % TM_N == TM_TARGET && v == EN_MOD_OFF)
        s = "Off";
    else if (i >= P_TMOD && i < P_SONGP && (i - P_TMOD) % TM_N == TM_TARGET && v - 1 >= EN_NTMSET) {   /* another modulator's */
        b[0] = 'M';
        b[1] = (char)('1' + (v - 1 - EN_NTMSET) % EN_NTMOD);
        copy9(v - 1 - EN_NTMSET < EN_NTMOD ? " depth" : " rate", b + 2);
        return;
    } else if (i >= P_TMOD && i < P_SONGP && (i - P_TMOD) % TM_N == TM_TARGET)
        s = TMDEST_NAME[(unsigned)(v - 1) < EN_NTMSET ? v - 1 : 0];
    else if (i >= P_TMOD && i < P_SONGP && (i - P_TMOD) % TM_N == TM_SYNC)
        s = v ? "Tempo" : "Free";
    else if (i >= P_TMOD && i < P_SONGP && (i - P_TMOD) % TM_N == TM_RATE && par[i - TM_RATE + TM_SYNC])
        s = DIV_NAME[div_of(v < 0 ? 0 : v > 100 ? 100 : v)];
    else if (i >= P_TMOD && i < P_SONGP && (i - P_TMOD) % TM_N == TM_SHAPE)
        s = EN_MODSHAPE_NAME[(unsigned)v < EN_NMODSHAPE ? v : 0];
    else if (i == P_RL(SPEED) && v >= 48 && v <= 52)
        s = "Stop";
    else if (i == P_RL(FX))
        s = v ? "After" : "Before";
    else if (i == P_RL(MIX) && (v == 0 || v == 100))
        s = v ? "Wet" : "Dry";
    else if (i >= P_PLAY && i < P_TAPE && (i - P_PLAY) % PL_N == PL_ENV)
        s = ENV_NAME[(unsigned)v < EN_NENV ? v : 0];
    else if (i >= P_PLAY && i < P_TAPE && (i - P_PLAY) % PL_N == PL_ROLE)
        s = ROLE_NAME[(unsigned)v < EN_NROLE ? v : 0];
    else if (i >= P_PLAY && i < P_TAPE && (i - P_PLAY) % PL_N == PL_PACE && par[i - PL_PACE + PL_ROLE] != ROLE_CHORD) {
        static const char *const STEP[2][5] = {{"1/16", "1/8", "1/4", "1/2", "1/2"},
                                               {"Chord", "1 bar", "1/2", "1/4", "1/8"}};
        const int loops = par[i - PL_PACE + PL_ROLE] == ROLE_LOOPS;
        s = STEP[!loops][loops ? v / 25 : v < 20 ? 0 : v < 40 ? 1 : v < 60 ? 2 : v < 80 ? 3 : 4];
    } else if (i == P_MODE)
        s = EN_MODE_NAME[(unsigned)v < EN_NMODES ? v : 0];
    else if (i == P_ROOT)
        s = EN_NOTE_NAME[(unsigned)v < 12u ? v : 0];
    else if (i >= P_EXPR && i < P_SEQ) {
        const int off = (i - P_EXPR) % EN_XSTRIDE;
        if (off == EX_STYLE) {
            static const char *const XSTYLE_NAME[XSTYLE_N] = {"Natural", "Breath", "Pluck", "Legato", "Cloud"};
            s = XSTYLE_NAME[(unsigned)v < XSTYLE_N ? v : 0];
        } else if (off == EX_PHRASE) {
            b = itoa_s(4 + v / 12, b);
            *b++ = ' '; *b++ = 'n'; *b++ = 'o'; *b++ = 't'; *b++ = 'e'; *b++ = 's'; *b = 0;
            return;
        } else if (off == EX_MORPH_KIND) {
            static const char *const XM_NAME[XM_N] = {"Color", "Compat"};
            s = XM_NAME[(unsigned)v < XM_N ? v : 0];
        } else if (off == EX_MORPH_SRC) {
            if ((unsigned)v < EN_NFACT) {
                s = PATCH_NAME[v];
            } else {
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
        } else if (off >= EX_N) {
            const int rk = (off - EX_N) % XR_N;
            if (rk == XR_SOURCE) {
                static const char *const XS_NAME[XS_N] = {
                    "Off", "Energy", "Link", "Open", "Breath", "Density", "Duration",
                    "Accent", "Arch", "Silence", "Leap", "Register", "Other", "Memory", "Call", "Tension", "Answer", "Recall"
                };
                s = XS_NAME[(unsigned)v < XS_N ? v : 0];
            } else if (rk == XR_TARGET) {
                static const char *const XT_NAME[XT_N] = {
                    "Off", "Attack", "Decay", "Sustain", "Release", "Gate", "Level", "Cutoff", "Index", "Morph",
                    "Density", "Evolve", "Drift", "Motif", "Rotation", "Accent", "Passing", "Swing", "Humanize", "Space", "Range", "Dialogue", "Length", "Tension", "Resolve"
                };
                s = XT_NAME[(unsigned)v < XT_N ? v : 0];
            } else if (rk == XR_WHEN) {
                static const char *const XW_NAME[XW_N] = {"Note", "Phrase", "Live"};
                s = XW_NAME[(unsigned)v < XW_N ? v : 0];
            } else if (rk == XR_CURVE) {
                static const char *const XC_NAME[XC_N] = {"Linear", "Soft", "Step", "Center"};
                s = XC_NAME[(unsigned)v < XC_N ? v : 0];
            } else if (rk == XR_DEPTH) {
                if (v > 0)
                    *b++ = '+';
                b = itoa_s(v, b);
                *b++ = '%';
                *b = 0;
                return;
            } else if (rk == XR_SMOOTH) {
                b = itoa_s(v ? (int)(smooth_secs(v) * 1000.0f + 0.5f) : 0, b);
                *b++ = 'm';
                *b++ = 's';
                *b = 0;
                return;
            } else if (rk == XR_MIN || rk == XR_MAX) {
                b = itoa_s(v, b);
                *b++ = '%';
                *b = 0;
                return;
            }
        } else if (off == EX_MORPH || off == EX_AMOUNT || off == EX_VARIATION || off == EX_MEMORY || off == EX_LINK) {
            b = itoa_s(v, b);
            *b++ = '%';
            *b = 0;
            return;
        }
    }
    if (s) {
        copy9(s, b);
        return;
    }
    if (i >= P_PLAY && i < P_TAPE && ((i - P_PLAY) % PL_N == PL_A || (i - P_PLAY) % PL_N == PL_D ||
                                      (i - P_PLAY) % PL_N == PL_R)) {   /* the ADSR's times */
        const int x = v < 0 ? 0 : v > 100 ? 100 : v;
        ms_text((i - P_PLAY) % PL_N == PL_A ? ADSR_ATK_MS[x] : ADSR_FALL_MS[x], b);
        return;
    }
    if (i >= P_VCF && i < P_EXPR && (i - P_VCF) % VF_N == VF_ON) {
        copy9(ONOFF[v ? 1 : 0], b);
        return;
    }
    if (i >= P_VCF && i < P_EXPR && (i - P_VCF) % VF_N == VF_CUT) {   /* "440 Hz", "2.4 kHz" */
        const int hz = (int)(vcf_hz(v) + 0.5f);
        if (hz < 1000) {
            b = itoa_s(hz, b);
            *b++ = ' ';
        } else {
            b = tenths((hz + 50) / 100, b);
            *b++ = ' ';
            *b++ = 'k';
        }
        *b++ = 'H';
        *b++ = 'z';
        *b = 0;
        return;
    }
    if (i == P_SEQ_LENGTH || i == P_SEQ_RANGE) {
        const char *unit = i == P_SEQ_LENGTH ? " bars" : " oct";
        b = itoa_s(i == P_SEQ_LENGTH ? 2 + v*6/100 : 1 + v*3/100, b);
        while (*unit) { *b++ = *unit++; }
        *b = 0; return;
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
    if (i >= P_TMOD && i < P_SONGP && (i - P_TMOD) % TM_N == TM_SMOOTH) {   /* 2 ms .. 500 ms */
        b = itoa_s((int)(smooth_secs(v) * 1000.0f + 0.5f), b);
        *b++ = 'm';
        *b++ = 's';
        *b = 0;
        return;
    }
    if (i >= P_TMOD && i < P_SONGP && (i - P_TMOD) % TM_N == TM_RATE) {   /* a cycle: 60 s .. 0.12 s */
        b = tenths((int)(mod_secs(v) * 10.0f + 0.5f), b);
        *b++ = 's';
        *b = 0;
        return;
    }
    if (mk == MS_RATE || mk == MS_FADE) {        /* a cycle: 60 s .. 0.12 s; the fade: to 8 s */
        b = tenths((int)((mk == MS_RATE ? mod_secs(v) : fade_secs(v)) * 10.0f + 0.5f), b);
        *b++ = 's';
        *b = 0;
        return;
    }
    if (i == P_RL(SPEED)) {                      /* "+1.00x", "-0.50x" */
        const int c = v >= 73 && v <= 77 ? 100 : v >= 23 && v <= 27 ? -100 : (v - 50) * 4;
        int a = c < 0 ? -c : c;
        *b++ = c < 0 ? '-' : '+';
        b = itoa_s(a / 100, b);
        *b++ = '.';
        *b++ = (char)('0' + a % 100 / 10);
        *b++ = (char)('0' + a % 10);
        *b++ = 'x';
        *b = 0;
        return;
    }
    if (i == P_RL(ORG)) {                        /* the splice, 1..8 */
        b = itoa_s(v + 1, b);
        *b = 0;
        return;
    }
    if (i >= P_PLAY && i < P_TAPE && (i - P_PLAY) % PL_N == PL_PACE) {   /* a chord's breath, seconds a cycle */
        b = itoa_s(4 + (12 * v + 50) / 100, b);
        *b++ = 's';
        *b = 0;
        return;
    }
    if (i >= P_PLAY && i < P_TAPE && (i - P_PLAY) % PL_N == PL_GLIDE) {
        b = itoa_s(20 + 12 * v, b);
        *b++ = 'm';
        *b++ = 's';
        *b = 0;
        return;
    }
    if (i >= P_PLAY && i < P_TAPE && (i - P_PLAY) % PL_N == PL_GATE) {
        if (v < 5) {                             /* (below 5 %: Natural, a sixteenth) */
            const char *s = "Natural";
            while (*s)
                *b++ = *s++;
            *b = 0;
            return;
        }
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
    if ((i == P_TUNE || i == P_BOCT || i == P_BASSOCT || (i >= P_PLAY && i < P_TAPE && (i - P_PLAY) % PL_N == PL_OCT)) && v > 0)
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
enum { C_SET, C_POOL, C_PLAY, C_NEXT, C_FREEZE, C_SEED, C_PANIC, C_CLOCK, C_REGROW, C_PBYTE, C_PDONE, C_PLOAD,
       C_REEL, C_KEY, C_SONG, C_DETUNE, C_MLOAD, C_TAPE };
_Static_assert(P_NPARAMS <= 1024, "C_SET carries a parameter's index in 10 bits");
_Static_assert(FP_SIZE <= 256, "C_PBYTE carries a patch byte's offset in 8 bits");
#define QN 1024u                              /* a piece (project_apply) posts every parameter and patch byte at once */
static uint32_t q[QN];
static volatile uint32_t q_w, q_r;
static void post(uint32_t c, int a, int b)
{
    uint32_t w = q_w;
    if (w - q_r >= QN)
        return;                                   /* full: dropped (never, at the UI's rate) */
    q[w % QN] = c << 26 | ((uint32_t)a & 0x3FFu) << 16 | ((uint32_t)b & 0xFFFFu);   /* (6, 10, 16 bits) */
    BARRIER();
    q_w = w + 1;
}
void en_set(int p, int v) { post(C_SET, p, v); }
void en_pool(uint32_t pool) { post(C_POOL, 0, (int)(pool & 0xFFFu)); }
void en_play(int on) { post(C_PLAY, on, 0); }
void en_tape(int on) { post(C_TAPE, on, 0); }
void en_next(void) { post(C_NEXT, 0, 0); }
void en_freeze(int on) { post(C_FREEZE, on, 0); }
void en_seed(uint32_t s) { post(C_SEED, 0, (int)(s & 0xFFFFu)); }
void en_panic(void) { post(C_PANIC, 0, 0); }
void en_clock(int msg) { post(C_CLOCK, msg, 0); }
void en_regrow(void) { post(C_REGROW, 0, 0); }
void en_reel(int op) { post(C_REEL, op, 0); }
static const void *volatile reel_meta_req;      /* a file's splices (en_reel_take), taken at the next drain */
void en_reel_take(const void *meta)
{
    reel_meta_req = meta;
    post(C_REEL, EN_REEL_TAKE, 0);
}
void en_key(int v, int note, int on) { post(C_KEY, v, (note & 0x7F) | (on ? 0x100 : 0)); }
void en_keys_off(void) { post(C_KEY, 7, 0); }   /* (every key's note let go: a new piece) */
void en_detune(int dcents) { post(C_DETUNE, dcents, 0); }
static float kcents;                            /* Key sync Garden: the tape's cents, as a semitone's fraction */
static void kb_key(int v, int note, int on);    /* (the keyboard's notes: by fm_block) */
static const uint8_t *volatile song_req;        /* the song asked for (en_song), taken at the next drain */
void en_song(const uint8_t *rec)
{
    song_req = rec;
    post(C_SONG, 0, 0);
}
static void kb_reset(void);
static void reel_new(void);                      /* (reel.c) */
static void reel_take(void);
static void reel_dub(void);
static void reel_erase(void);
static void reel_chop_apply(void);
static void reel_init(void);
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
volatile uint8_t en_tape_playing;              /* the reel's own transport: it plays while the garden rests */
volatile uint8_t en_chord[EN_NPAD], en_chord_root, en_chord_n;
volatile float en_drift, en_pad_level, en_bass_level, en_tempo;
volatile uint8_t en_loop_pos[EN_NLOOP];
volatile int8_t en_loop_note[EN_NLOOP][EN_LOOP_MAX];
volatile float en_loop_glow[EN_NLOOP];
volatile uint32_t en_bars;
volatile float en_arc;
volatile uint8_t en_pad_note[EN_NPAD];
volatile float en_pad_amp[EN_NPAD];

static uint32_t pool = 0xAADu;                  /* Ionian until told */
static int nsteps, steps[12];                   /* the scale */
static int deg;                                 /* the chord's scale degree */
static int since_home;                          /* chord changes since the tonic (the pull home grows) */
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

en_note_t en_score[EN_SCORE_N];
volatile uint32_t en_score_w, en_now;    /* (en_now: the samples made) */
static uint32_t smp_clock;
static void midi(uint32_t st, uint32_t d1, uint32_t d2)
{
    const uint32_t ch = st & 15u;
    if ((st & 0xF0u) == 0x90u && d2) {          /* the score: a note begins */
        en_note_t *e = &en_score[en_score_w % EN_SCORE_N];
        e->note = (uint8_t)d1;
        e->voice = (uint8_t)ch;
        e->vel = (uint8_t)d2;
        e->on = smp_clock ? smp_clock : 1u;
        e->off = 0;
        en_score_w++;
    } else if ((st & 0xF0u) == 0x80u || (st & 0xF0u) == 0x90u) {   /* it ends: the latest of it still sounding */
        uint32_t k;
        for (k = 1; k <= EN_SCORE_N; k++) {
            en_note_t *e = &en_score[(en_score_w - k) % EN_SCORE_N];
            if (e->on && !e->off && e->note == d1 && e->voice == ch) {
                e->off = smp_clock ? smp_clock : 1u;
                break;
            }
        }
    }
    if (par[P_MIDI])
        en_midi_out(st, d1, d2);
}
static void score_close(int v)                  /* the score: voice v's notes (all, v < 0) ended now */
{
    uint32_t k;
    for (k = 0; k < EN_SCORE_N; k++)
        if (en_score[k].on && !en_score[k].off && (v < 0 || en_score[k].voice == (uint8_t)v))
            en_score[k].off = smp_clock ? smp_clock : 1u;
}


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
static float fm_vtap[EN_NVOX][FM6_N];           /* each voice's sound alone, mono (the reel's SOURCE) */
static uint32_t fm_pos = FM6_N;
static uint8_t vpat[EN_NVOX][FP_SIZE + 1];      /* each voice's patch, as the notes play it */
static uint8_t vstage[EN_NVOX][FP_SIZE + 1];    /* its bytes as they arrive, into vpat when applied */

static int32_t fm_logfreq(float note)          /* log2 Hz, Q24, of a MIDI pitch (a fraction too) and Tune */
{
    int32_t k = (int32_t)note;
    float fr = note - (float)k + (float)par[P_TUNE] * 0.01f + kcents;
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

/* the filter (1.5.20), after the Moog ladder: four one-pole low-passes in a row, the last fed back (RESONANCE: to
 * self-oscillation near 100) through a soft clip, as the transistors' tanh; each note its own (s: its four stages),
 * so each keeps its own colour as it fades. The cutoff: CUTOFF's, half following the note's pitch, opened by ENV with
 * the note's loudness now (its loudest carrier's envelope, linear). Off: nothing done, its stages cleared */
static float vcf_amp(const fm6_note_t *n, const uint8_t *p)   /* the note's carriers' loudness now, 0..1 */
{
    const uint32_t car = fm6_carriers(p[FP_ALG] & 31u);
    int32_t best = -(20 << 24);
    uint32_t k;
    for (k = 0; k < 6u; k++)
        if (car >> k & 1u) {
            const int32_t d = n->env[k].level - ((4032 + n->env[k].outlevel - 4256) << 16);
            best = d > best ? d : best;
        }
    return fm_exp2f(fm_clampf((float)best * (1.0f / 16777216.0f), -20.0f, 0.0f));
}
static void vcf(int v, float *s, float note, const fm6_note_t *n, const uint8_t *p, float cut, float *x)
{
    float g, k, oct;
    uint32_t i;
    if (!par[P_VF(v, VF_ON)]) {
        s[0] = s[1] = s[2] = s[3] = 0.0f;
        return;
    }
    oct = cut + (float)par[P_VF(v, VF_CUT)] * 0.0924f + (note - 60.0f) * (0.5f / 12.0f) +
          (float)par[P_VF(v, VF_ENV)] * 0.06f * vcf_amp(n, p);
    g = 1.0f - fm_expf(-2.0f * FM_PI * fm_minf(30.0f * fm_exp2f(fm_minf(oct, 9.3f)), 0.42f * EN_SR) * (1.0f / EN_SR));
    k = (float)par[P_VF(v, VF_RES)] * 0.041f;   /* (4: the ladder sings) */
    for (i = 0; i < FM6_N; i++) {
        /* (1.5.32) The drive leaves the clip room: 0.3 in, 3.333 back, unity in the passband until the voice itself
         * is at +11 dBFS, where 0.5 in lost 6 dB on a loud patch. The output carries half of what the feedback takes
         * from the bass (1 + k): at RESONANCE 100 that used to be 8 dB quieter than Off. */
        float u = x[i] * 0.3f - k * s[3];
        u = u > 1.5f ? 1.0f : u < -1.5f ? -1.0f : u - 0.148148f * u * u * u;   /* (a tanh's shape, no divide) */
        s[0] += g * (u - s[0]);
        s[1] += g * (s[0] - s[1]);
        s[2] += g * (s[1] - s[2]);
        s[3] += g * (s[2] - s[3]);
        x[i] = s[3] * (3.333f + 1.667f * k);   /* (half of the bass the feedback takes, back) */
    }
    for (i = 0; i < 4u; i++)                     /* (a stage gone quiet to nothing: 0, not a denormal crawling) */
        if (fm_fabsf(s[i]) < 1e-12f)
            s[i] = 0.0f;
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
    int16_t acc[XT_N];                          /* note/phrase routes and the strike's style/variation, before Amount */
    int8_t route[EN_XROUTES];                   /* captured continuous targets, removed if that route becomes Live */
    uint8_t target[EN_XROUTES];
    uint8_t env[6][4], amount;                  /* unexpressed articulation: restore it without restarting a note */
    uint32_t sounded;                          /* samples actually rendered since this attack */
    float gate;                                /* held-time multiplier, captured only at the strike */
} expr_note_t;
typedef struct {
    fm6_note_t fm;
    fm6_lfo_t lfo;
    expr_note_t ex;
    uint8_t vp[FP_SIZE + 1];                    /* the note's own copy of its voice's patch, made as it strikes */
    float note, target;                         /* MIDI: the note sounding, the next one (a chord, the bass) */
    float t, rate, peak;                        /* a chord: t 0..1 through the breath (per block), its pace, strength */
    float amp, tgt, bloom;                      /* its amplitude, ramped to tgt; its modulators' level (dB) */
    float pan, gl, gr, pl, pr;                  /* its place; the gains it had last block (ramped from) */
    float hf;                                   /* a chord: a handoff's fade (1 normal) */
    float vel, out, lv;                         /* the loops: the strike's velocity, the patch's level, MOD's LEVEL */
    int32_t wait, held;                         /* the loops: samples until it rings (a hand), until it is let go */
    int32_t slen;                               /* the loops: a song's note, held this long (samples; 0: the Gate's) */
    uint8_t moff;                               /* the loops: its MIDI note already let go (a newer one at its pitch) */
    uint8_t midi_note, hand, gen, up;           /* hand: 1 fading out of the old key, 2 into the new; gen: the patch it
                                                 * sings (its voice's gen as it started); up: its key let go */
    uint8_t on, rung, loop, fade;               /* the loops: sounding, started, its loop; shed: blocks to silence */
    float vf[4];                                /* its filter's four stages (vcf) */
} vnote_t;
typedef struct {
    vnote_t n[NPOLY];
    uint8_t role, gen;                          /* the role playing (PL_ROLE once faded); a new patch: gen one more */
    float vol, gain, tg, fo;                    /* Level; the patch's level; the transport's fade; the role's fade */
    float glide_k;                              /* 1 / GLIDE's time, in samples */
    uint8_t pc, rest, strike, xs, alt;          /* the bass: its pitch class; sitting this chord out; struck next
                                                 * block; fading out for a new patch; on a passing tone */
    uint8_t decays;                             /* its patch dies away (no carrier held): on AMB's envelope it plays
                                                 * as on the patch's, struck each time (patch_decays) */
    int rs;                                     /* the bass: blocks to a new patch's restrike (the knob at rest) */
    int32_t swait;                              /* the bass: samples to its strike (Swing) */
    int32_t held;                               /* the bass: samples until its key is let go; 0 let go */
    int32_t slen;                               /* the bass: a song's note, held this long at its strike (0: the Gate's) */
    float sglide;                               /* the bass: a song's note's glide, quick enough for its length (0: GLIDE's) */
    uint8_t mon;                                /* the bass: its MIDI note on (sent, not yet let go) */
    float xf;                                   /* the bass: the restrike's fade */
    int8_t loop_note[EN_NLOOP][EN_LOOP_MAX];    /* the loops: each loop's notes (scale steps from the chord), -1 rest */
    uint8_t loop_vel[EN_NLOOP][EN_LOOP_MAX], loop_pos[EN_NLOOP], loop_rest[EN_NLOOP];
    uint8_t loop_patch[EN_NLOOP][FP_SIZE + 1];  /* each loop's shade of the patch */
} voice_t;
static voice_t vc[EN_NVOX];
#define PL(v, k) (par[P_PL(v, PL_##k)])
#define GATE_F(v) ((float)PL(v, GATE) * 0.01f)
static float step_len;                          /* (a sixteenth, in samples: below) */
#define GATE_NAT(v) (PL(v, GATE) < 5)             /* Natural: each note held a sixteenth, as a key played */
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
static uint8_t adsr_level(int s)                /* the Sustain, s % of full, as a DX7 level (0.75 dB a step) */
{
    float l;
    if (s <= 0)
        return 0;
    l = 99.0f + 20.0f * fm_log2f((float)s * 0.01f) * 0.30103f / 0.75f;
    return (uint8_t)(l < 0.0f ? 0.0f : l + 0.5f);
}
static void adsr_patch(int v, uint8_t *p)
{
    const uint32_t car = fm6_carriers(p[FP_ALG] & 31u);
    const int a = PL(v, A), d = PL(v, D), su = PL(v, S), r = PL(v, R);
    uint32_t j;
    for (j = 0; j < 6u; j++) {
        uint8_t *o = p + j * FP_OP;
        if (!(car >> j & 1u))
            continue;
        o[FP_R1] = (uint8_t)(99 - (a * 85 + 50) / 100);   /* (1.5.20: to rate 14, ~8 s) */
        o[FP_R1 + 1] = (uint8_t)(99 - d * 3 / 4);
        o[FP_R1 + 2] = 99;
        o[FP_R1 + 3] = (uint8_t)(99 - r * 3 / 4);
        o[FP_L1] = 99;
        o[FP_L1 + 1] = o[FP_L1 + 2] = adsr_level(su);
        o[FP_L1 + 3] = 0;
    }
}
/* ---------------------------------------------------------- interpretation --- */
/* Live Level/Cutoff/Index run at FM6 block rate through gain/cutoff offsets, never re-keying an envelope.
 * The other targets are articulation, captured at the attack even when their route says Live. Phrase routes
 * capture together on the first note of each phrase; smoothing follows elapsed audio time, not note density. */
volatile uint8_t en_expression[EN_NVOX][4];
volatile int16_t en_expr_route[EN_NVOX][EN_XROUTES];
volatile uint8_t en_expr_phase[EN_NVOX];
static uint8_t mpatch[EN_NVOX][EN_PATCH], mstage[EN_NVOX][EN_PATCH], mvalid[EN_NVOX];
static volatile uint8_t mstaged[EN_NVOX];
static float xsm[EN_NVOX][EN_XROUTES], xtarget[EN_NVOX][EN_XROUTES];
static float xmem[EN_NVOX], xhold[EN_NVOX][EN_XROUTES], xshown[EN_NVOX][EN_XROUTES];
static uint8_t xvalid[EN_NVOX], xpos[EN_NVOX], xlast[EN_NVOX], xvel[EN_NVOX], xleap[EN_NVOX];
volatile uint8_t en_phrase_stage, en_phrase_leader, en_phrase_active;
static uint8_t music_stage, music_active, music_running, music_leader, music_recall[EN_NVOX];
static void music_tick(void);
static void music_reset(void);
static uint32_t expr_rng;
static float seq_offset(int v, int target);
static int16_t xseq[EN_NVOX][15];                /* (1.5.26) what Expression asks of the sequencer: Density, Evolve,
                                                 * Drift (the voices' mean), signed percent */
static uint32_t xduration_sum[EN_NVOX];
static uint8_t xrendered[EN_NVOX], xdensity[EN_NVOX], xduration[EN_NVOX];
/* Publish the previous audio block, so all voices see one coherent observation.
 * Four sounding notes fill Density; Duration reaches 100 at four seconds.
 * Release tails count until the FM core reports silence. */
static void expr_observe(int v, expr_note_t *n)
{
    const uint32_t limit = (uint32_t)(EN_SR * 4);
    if (n->sounded < limit) n->sounded += FM6_N;
    if (n->sounded > limit) n->sounded = limit;
    xduration_sum[v] += n->sounded;
    xrendered[v]++;
}
static void expr_observation_block(void)
{
    int v;
    for (v = 0; v < EN_NVOX; v++) {
        const unsigned count = xrendered[v];
        xdensity[v] = (uint8_t)(count >= NPOLY ? 100 : count * 100 / NPOLY);
        xduration[v] = count ? (uint8_t)(xduration_sum[v] / count * 100u / (uint32_t)(EN_SR * 4)) : 0;
        xrendered[v] = 0;
        xduration_sum[v] = 0;
    }
}

void en_morph_load(int voice, const uint8_t *patch)
{
    int i;
    if ((unsigned)voice >= EN_NVOX)
        return;
    if (!patch) {
        mstaged[voice] = 0;
        post(C_MLOAD, voice, 0);
        return;
    }
    for (i = 0; i < EN_PATCH; i++)
        mstage[voice][i] = patch[i] > en_patch_max(i) ? en_patch_max(i) : patch[i];
    mstaged[voice] = 1;
    post(C_MLOAD, voice, 1);
}
/* LFOs modulate effective interpretation only; persisted controls stay untouched. */
static int expr_control(int v, int k);
static int xclamp(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
static int xnorm(float f) { return xclamp((int)(f * 100.0f + 0.5f), 0, 100); }
static int xlive(int v, int r)
{
    const int t = par[P_XR(v, r, XR_TARGET)];
    return par[P_XR(v, r, XR_WHEN)] == XW_LIVE && (t == XT_LEVEL || t == XT_CUTOFF || t == XT_INDEX || (t >= XT_DENSITY && t <= XT_RESOLVE));
}
static float xrandom(void)
{
    uint32_t x = expr_rng;
    x ^= x << 13;
    x ^= x >> 17;
    expr_rng = x ^ (x << 5);
    return (float)(expr_rng >> 8) * (1.0f / 16777216.0f);
}
static int chord_n(int v);
static void expr_sources(int v, int k, int note, int vel, int *src)
{
    float others = 0.0f, peak = 0.0f, br = 0.0f;
    const int plen = 4 + par[P_EX(v, EX_PHRASE)] / 12;
    const int ph = xpos[v] % plen;
    int i, energy, leap = xleap[v];
    for (i = 0; i < EN_NVOX; i++) {
        peak = fm_maxf(peak, en_vox_level[i]);
        if (i != v) others += en_vox_level[i];
    }
    if (vc[v].role == ROLE_CHORD) {
        if (k >= 0) br = breath(vc[v].n[k].t);
        else for (i = 0; i < chord_n(v); i++) br = fm_maxf(br, breath(vc[v].n[i].t));
    } else br = (float)en_expr_phase[v] * 0.01f;
    energy = (xnorm(peak) + xclamp((int)((arcm + 1.0f) * 50.0f), 0, 100)) / 2;
    if (note >= 0 && xlast[v]) {
        int d = note - (int)xlast[v];
        leap = xclamp((d < 0 ? -d : d) * 100 / 12, 0, 100);
    }
    src[XS_OFF] = 0;
    src[XS_ENERGY] = energy;
    src[XS_LINK] = xnorm(others * 0.5f) * par[P_EX(v, EX_LINK)] / 100;
    src[XS_OPEN] = par[P_VF(v, VF_ON)] ? par[P_VF(v, VF_CUT)] : 100;
    src[XS_BREATH] = xnorm(br);
    src[XS_DENSITY] = xdensity[v];
    src[XS_DURATION] = xduration[v];
    src[XS_ACCENT] = xclamp((vel >= 0 ? vel : xvel[v]) * 100 / 127, 0, 100);
    src[XS_ARCH] = note >= 0 ? (ph * 2 > plen - 1 ? plen - 1 - ph : ph) * 200 / (plen - 1)
                                   : (en_expr_phase[v] > 50 ? 100 - en_expr_phase[v] : en_expr_phase[v]) * 2;
    src[XS_SILENCE] = 100 - xnorm(en_vox_level[v]); /* Actual layer activity, independent of tide and siblings. */
    src[XS_LEAP] = leap;
    src[XS_REGISTER] = xclamp(((note >= 0 ? note : xlast[v]) - 36) * 100 / 60, 0, 100);
    src[XS_OTHER] = xnorm(others * 0.5f);
    src[XS_MEMORY] = xclamp((int)xmem[v], 0, 100);
    src[XS_CALL] = music_active && music_stage < 2 && music_leader == v ? 100 : 0;
    src[XS_TENSION] = music_active && music_stage == 2 ? 100 : 0;
    src[XS_ANSWER] = music_active && music_stage == 3 ? 100 : 0;
    src[XS_RECALL] = music_active && music_recall[v] ? 100 : 0;
    for (i = 0; i < 4; i++) en_expression[v][i] = (uint8_t)src[XS_ENERGY + i];
}
static float expr_value(int v, int r, const int *src)
{
    const int curve = par[P_XR(v, r, XR_CURVE)];
    const int lo = par[P_XR(v, r, XR_MIN)], hi = par[P_XR(v, r, XR_MAX)];
    float x = (float)src[par[P_XR(v, r, XR_SOURCE)]];
    if (curve == XC_SOFT) x = x * x * (3.0f - 0.02f * x) * 0.01f;
    else if (curve == XC_STEP) x = x >= 50.0f ? 100.0f : 0.0f;
    else if (curve == XC_CENTER) x = fm_fabsf(x - 50.0f) * 2.0f;
    return (float)lo + (float)(hi - lo) * x * 0.01f;
}
static float expr_contribution(int v, int r)
{
    return xsm[v][r] * (float)par[P_XR(v, r, XR_DEPTH)] * 0.01f;
}
static void expr_block(int v)
{
    int src[XS_N], r;
    const int amount = expr_control(v, EX_AMOUNT);
    expr_sources(v, -1, -1, -1, src);
    /* Memory's 20 ms .. 2 s follows elapsed time too, and remains audible at Memory 100. */
    xmem[v] += ((float)src[XS_ENERGY] - xmem[v]) *
               fm_minf((float)FM6_N / (EN_SR * (0.02f + (float)par[P_EX(v, EX_MEMORY)] * 0.0198f)), 1.0f);
    for (r = 0; r < 15; r++) xseq[v][r] = 0;
    if (!amount) {
        for (r = 0; r < EN_XROUTES; r++) en_expr_route[v][r] = 0;
        return;
    }
    for (r = 0; r < EN_XROUTES; r++) {
        const int active = par[P_XR(v, r, XR_SOURCE)] && par[P_XR(v, r, XR_TARGET)];
        if (active) {
            const int smooth = par[P_XR(v, r, XR_SMOOTH)];
            /* Smooth the source continuously, then sample it at a note/phrase boundary when requested. */
            xtarget[v][r] = expr_value(v, r, src);
            xsm[v][r] += (xtarget[v][r] - xsm[v][r]) *
                (smooth ? fm_minf((float)FM6_N / (EN_SR * smooth_secs(smooth)), 1.0f) : 1.0f);
            if (xlive(v, r)) xshown[v][r] = expr_contribution(v, r);
        }
        en_expr_route[v][r] = (int16_t)(active ? xclamp((int)(xshown[v][r] * (float)amount * 0.01f), -100, 100) : 0);
        if (active) {                            /* (1.5.26) the sequencer's own targets, summed a voice */
            const int t = par[P_XR(v, r, XR_TARGET)];
            if (t >= XT_DENSITY && t <= XT_RESOLVE)
                /* Eight bounded routes sum to at most +/-800: int16_t is sufficient.
                 * Preserve cancellation across routes and LFOs; seq_offset clamps once. */
                xseq[v][t - XT_DENSITY] += en_expr_route[v][r];
        }
    }
}
static void expr_env(int v, expr_note_t *n, uint8_t *p, int capture)
{
    const uint32_t car = fm6_carriers(p[FP_ALG] & 31u);
    const int amount = expr_control(v, EX_AMOUNT);
    int i, j;
    static const uint8_t off[4] = {FP_R1, FP_R1 + 1, FP_L1 + 2, FP_R1 + 3};
    static const uint8_t tgt[4] = {XT_ATTACK, XT_DECAY, XT_SUSTAIN, XT_RELEASE};
    for (i = 0; i < 6; i++) {
        if (!(car >> i & 1u)) continue;
        for (j = 0; j < 4; j++) {
            uint8_t *b = p + i * FP_OP + off[j];
            int d = n->acc[tgt[j]] * amount / 100;
            if (capture) n->env[i][j] = *b;
            d = xclamp(d, -100, 100) * (j == 2 ? 3 : 4) / 10;
            *b = d ? (uint8_t)xclamp((int)n->env[i][j] + d, 0, 99) : n->env[i][j];
        }
    }
    n->amount = (uint8_t)amount;
}
static void expr_strike(int v, int k, expr_note_t *n, uint8_t *p, int32_t note, int32_t vel)
{
    const int amount = expr_control(v, EX_AMOUNT), plen = 4 + par[P_EX(v, EX_PHRASE)] / 12;
    const int ph = xpos[v] % plen;
    int src[XS_N], acc[XT_N] = {0}, r, i, morph;
    n->sounded = 0;
    expr_sources(v, k, note, vel, src);
    __builtin_memset(n->target, 0, sizeof n->target);
    for (r = 0; amount && r < EN_XROUTES; r++) {
        const int s = par[P_XR(v, r, XR_SOURCE)], t = par[P_XR(v, r, XR_TARGET)];
        const int phrase = par[P_XR(v, r, XR_WHEN)] == XW_PHRASE;
        if (!s || !t) { xshown[v][r] = 0.0f; continue; }
        if (!phrase || !ph || !(xvalid[v] >> r & 1u)) {
            xtarget[v][r] = expr_value(v, r, src);
            if (!par[P_XR(v, r, XR_SMOOTH)] || (!(xvalid[v] >> r & 1u) && !xlive(v, r)))
                xsm[v][r] = xtarget[v][r];
            xhold[v][r] = expr_contribution(v, r);
            xvalid[v] |= (uint8_t)(1u << r);
        }
        xshown[v][r] = phrase ? xhold[v][r] : expr_contribution(v, r);
        if (!xlive(v, r)) {
            acc[t] += (int)xshown[v][r];
            if (t == XT_LEVEL || t == XT_CUTOFF || t == XT_INDEX) {
                n->target[r] = (uint8_t)t;
                n->route[r] = (int8_t)xshown[v][r];
            }
        }
    }
    if (amount) {
        static const int8_t LEAN[XSTYLE_N][2] = {{0, 0}, {-30, 15}, {40, -25}, {-20, 25}, {-35, 10}};
        const int style = par[P_EX(v, EX_STYLE)], var = expr_control(v, EX_VARIATION);
        acc[XT_ATTACK] += LEAN[style][0];
        acc[XT_DECAY] += LEAN[style][1];
        for (i = 1; var && i <= XT_MORPH; i++) acc[i] += (int)((xrandom() * 2.0f - 1.0f) * (float)var * 0.3f);
    }
    for (i = 0; i < XT_N; i++) n->acc[i] = (int16_t)acc[i];
    n->gate = 1.0f + (float)xclamp(acc[XT_GATE] * amount / 100, -100, 100) * 0.0095f;
    morph = par[P_EX(v, EX_MORPH)] + xclamp(acc[XT_MORPH] * amount / 100, -100, 100);
    if (mvalid[v] && (morph = xclamp(morph, 0, 100)) > 0) {
        uint8_t o[EN_PATCH];
        en_morph_patch(p, mpatch[v], morph, par[P_EX(v, EX_MORPH_KIND)], o);
        __builtin_memcpy(p, o, EN_PATCH);
    }
    expr_env(v, n, p, 1);
    xlast[v] = (uint8_t)xclamp((int)note, 0, 127);
    xvel[v] = (uint8_t)xclamp((int)vel, 0, 127);
    xleap[v] = (uint8_t)src[XS_LEAP];
    en_expr_phase[v] = (uint8_t)(ph * 100 / (plen - 1));
    xpos[v] = (uint8_t)((ph + 1) % plen);
}
/* The FM core ramps these operator gain offsets over the block; no envelope or LFO is restarted. */
static float expr_mod(int v, expr_note_t *n, fm6_note_t *fm, uint8_t *p, int32_t *ol)
{
    const int amount = expr_control(v, EX_AMOUNT);
    int acc[XT_N], i, r;
    uint32_t car;
    if (n->amount != amount) { expr_env(v, n, p, 0); fm6_note_refresh(fm, p); }
    if (!amount) return 0.0f;
    for (i = 0; i < XT_N; i++) acc[i] = n->acc[i];
    for (r = 0; r < EN_XROUTES; r++)
        if (xlive(v, r) && par[P_XR(v, r, XR_SOURCE)]) {
            if (n->target[r]) acc[n->target[r]] -= n->route[r];
            acc[par[P_XR(v, r, XR_TARGET)]] += (int)expr_contribution(v, r);
        }
    car = fm6_carriers(p[FP_ALG] & 31u);
    for (i = 0; i < 6; i++) {
        const int base = p[i * FP_OP + FP_OL];
        const int d = xclamp(acc[car >> i & 1u ? XT_LEVEL : XT_INDEX] * amount / 100, -100, 100) * 3 / 10;
        if (d) ol[i] += (fm6_scaleout(xclamp(base + d, 0, 99)) - fm6_scaleout(base)) * 2097152;
    }
    return (float)xclamp(acc[XT_CUTOFF] * amount / 100, -100, 100) * 0.05f;
}
static int32_t expr_hold(const expr_note_t *n, int32_t samples)
{
    if (n->gate == 1.0f || samples <= 0) return samples;
    return (int32_t)fm_maxf((float)FM6_N, (float)samples * n->gate);
}

/* note k of voice v struck: its own copy of src (MOD's envelopes, the ADSR), its LFO again (Key sync: from the top) */
static void note_strike(int v, int k, const uint8_t *src, int32_t note, int32_t vel, int fresh)
{
    vnote_t *n = &vc[v].n[k];
    gen_note(GEN(v, k));
    mod_patch(GEN(v, k), src, n->vp);
    if (PL(v, ENV) == ENV_ADSR)
        adsr_patch(v, n->vp);
    expr_strike(v, k, &n->ex, n->vp, note, vel);
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
    expr_env(v, &n->ex, n->vp, 1);
    fm6_note_refresh(&n->fm, n->vp);
    fm6_lfo_reset(&n->lfo, n->vp);
}

/* ------------------------------------------------------------------ a chord --- */
static uint8_t load_cap;                        /* the load (en_load): 0 free, 1 over 72 %, 2 over 80 % (its average:
                                                 * a render's peaks run 20 % over it) */
void en_load(int pct)
{
    load_cap = (uint8_t)(pct >= 80 ? 2 : pct >= 72 ? (load_cap == 2 ? 2 : 1) : pct < 62 ? 0 : load_cap);
}
static int chord_n(int v)                       /* its voices sounding, 1..4 (under load one fewer, then two; two at
                                                 * least) */
{
    const int density = xclamp(PL(v, DENS) + (int)seq_offset(v, 0), 0, 100);
    const int n = 1 + (density * 3 + 50) / 100, m = n - load_cap;
    return m < 2 ? (n < 2 ? n : 2) : m;
}
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
static const uint8_t *song;                     /* (the song playing: below) */
static void chord_leave(int v)
{
    int i, k;
    for (i = 0; i < NPOLY; i++) {
        vnote_t *n = &vc[v].n[i];
        int pc = n->midi_note % 12, shared = 0;
        for (k = 0; k < chord_nt; k++)
            shared |= chord_pcs[k] == pc;
        if (n->amp > 0.002f && !shared && (!n->hand || (song && n->hand == 2)))
            n->hand = 1;                         /* (a song's chords: again, from where its fade in got to) */
        else if (n->amp <= 0.002f && !n->hand)
            n->note = n->target;
    }
}

static void chord_coefs(int v, int i)           /* every FM6 block (32 samples): the breath, the bloom */
{
    vnote_t *n = &vc[v].n[i];
    float br;
    if (n->hand == 1) {                         /* out of the old key (a song's chords: four times as quick) */
        n->hf -= song ? 4.0f * HAND_STEP : HAND_STEP;
        if (n->hf <= 0.0f) {
            n->hf = 0.0f;
            n->hand = 2;
            n->t = 0.15f;                       /* a breath already rising, on the new chord */
            chord_breathe(v, i);
        }
    } else if (n->hand == 2) {                  /* into the new */
        n->hf += song ? 2.5f * HAND_STEP : HAND_STEP * 0.6f;
        if (n->hf >= 1.0f) {
            n->hf = 1.0f;
            n->hand = 0;
        }
    }
    n->t += n->rate;
    if (n->t >= 1.0f) {
        n->t -= 1.0f;
        if (en_playing || (PL(v, ENV) == ENV_AMB && !vc[v].decays))   /* (stopped, on the patch's envelopes: no
                                                 * new breath under the fade) */
            chord_breathe(v, i);
    }
    br = breath(n->t);
    if (PL(v, ENV) != ENV_AMB || vc[v].decays || n->ex.gate != 1.0f) {
        const float gate = GATE_NAT(v) ? step_len * n->rate * (1.0f / 32.0f) : GATE_F(v) * 0.9f;
        if (!n->up && n->t >= (n->ex.gate == 1.0f ? gate : fm_minf(gate * n->ex.gate, 0.99f))) {
            fm6_note_key(&n->fm, n->vp, 0);
            if (en_playing)
                midi(0x80u | (uint32_t)v, n->midi_note, 0);   /* (MIDI out let go with it) */
            n->up = 1;
        }
    }
    if (PL(v, ENV) != ENV_AMB || vc[v].decays) {
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

static float mrate[EN_NVOX][EN_NSLOT];          /* each modulator's rate, times its modulators' (M depth, M rate) */
static void mod_step(void)                      /* once an FM6 block */
{
    const float dt = MOD_DT;
    int i, v, s;
    for (v = 0; v < EN_NVOX; v++) {
        float dd[EN_NSLOT] = {0}, rr[EN_NSLOT] = {0};
        for (s = 0; s < EN_NSLOT; s++) {        /* the modulators aimed at others: their value at the voice's first */
            const int d = vx[v].s[s].dest;
            if (d >= MOD_MSLOT && d < MOD_SEQ) {
                const float m = en_mod[s][VOX_GEN0[v]];
                if (d - MOD_MSLOT < EN_NSLOT)
                    dd[d - MOD_MSLOT] += 0.5f * m;   /* depth: +-50 */
                else
                    rr[d - MOD_MSLOT - EN_NSLOT] += 2.0f * m;   /* rate: +-2 octaves */
            }
        }
        for (s = 0; s < EN_NSLOT; s++) {
            vx[v].s[s].deff = fm_clampf((vx[v].s[s].depth + dd[s]) *   /* the arc: deeper when full, calmer when
                                                 * sparse; a DEPTH at 100 stays full (1.5.9) */
                                            (vx[v].s[s].depth >= 0.995f ? 1.0f : 1.0f + 0.35f * arcm), 0.0f, 1.0f);
            mrate[v][s] = vx[v].s[s].rate * fm_exp2f(rr[s]);
        }
    }
    for (s = 0; s < EN_NSLOT; s++) {
        float lead = 0.0f;
        for (i = 0; i < NGEN; i++) {
            const mslot_t *x = &vx[gen_vox[i]].s[s];
            gen_t *g = &gen[s][i];
            float f = mrate[gen_vox[i]][s] * (x->sync == SYNC_TEMPO ? 1.0f : g->pace), w, d, q, tau;
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
        const float d = en_mod[s][g], k = 1.0f, up = (d + x->deff) * 0.5f;   /* 0..deff (1.5.9: All as strong as each
                                                 * alone; it was 0.7) */
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
        if (x->dest >= MOD_OP && x->dest < MOD_MSLOT) {   /* an operator's own: operator n is the patch's (6 - n)th */
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
            dm += (int)(20.0f * d);                 /* (All as strong as Decay alone) */
            dc += (int)(6.0f * d);
            any = 1;
        }
        if (x->dest >= MOD_OP && x->dest < MOD_MSLOT) {
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
    case MS_DEST: x->dest = (uint8_t)(val > 0 ? val - 1 : 255); break;   /* (Off: 255, no target's) */
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

/* ------------------------------------------------------------------ grid --- */
static float step_len, pulse_left;               /* a sixteenth, in samples; to the next MIDI clock pulse */

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
static void chord_publish(void)                 /* chord_pcs to every Chord voice, and the garden's view */
{
    int v, i, f;
    for (v = 0; v < EN_NVOX; v++)
        if (vc[v].role == ROLE_CHORD)
            chord_retarget(v);
    f = first_role(ROLE_CHORD);
    for (i = 0; i < EN_NPAD; i++)               /* (the garden shows the first Chord voice's, or the chord's tones) */
        en_chord[i] = (uint8_t)(f >= 0 ? (int)vc[f].n[i].target : 60 + chord_pcs[i % chord_nt]);
    en_chord_root = (uint8_t)chord_pcs[0];
    en_chord_n = (uint8_t)chord_nt;
}
static void pad_retarget(void)                  /* the chord's tones (the scale's, on its degree), to every Chord voice */
{
    chord_nt = en_chord_tones(pool, par[P_ROOT], deg, chord_pcs);
    chord_publish();
}

static void bass_note(int v, int n)             /* the bass voice to note n (MIDI), struck */
{
    voice_t *c = &vc[v];
    c->swait = 0;
    c->n[0].target = (float)n;
    c->pc = (uint8_t)(n % 12);
    if (en_playing && (c->n[0].midi_note != n || !c->mon)) {   /* (the same pitch again once let go: on again) */
        if (c->mon)
            midi(0x80u | (uint32_t)v, c->n[0].midi_note, 0);
        midi(0x90u | (uint32_t)v, (uint32_t)n, 80);
        c->mon = 1;
    }
    if (c->n[0].midi_note != n)                  /* a new note for MOD (its fade, SYNC Note, the envelope) */
        gen_note(GEN(v, 0));
    c->strike = 1;                               /* (heard with the patch's envelopes: each chord struck) */
    c->n[0].midi_note = (uint8_t)n;
}
static void bass_to(int v, int pc)              /* C2..B2, octaves down or up (the garden's: GLIDE's glide) */
{
    vc[v].sglide = 0.0f;
    bass_note(v, 36 + pc % 12 + 12 * PL(v, OCT));
}
static void bass_song(int v, int n, int len16)   /* a song's note on a Bass voice, its length; its glide a quarter of it
                                                 * at most */
{
    vc[v].slen = (int32_t)((float)len16 * step_len);
    vc[v].sglide = len16 > 0 ? 4.0f / ((float)len16 * step_len) : 0.0f;
    bass_note(v, n);
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
static float swing_amt;                         /* Swing, 0..0.5: the off-beat step's delay, a step's fraction */
static uint8_t change_pending, fifth_done;

/* (1.5.26) what Expression asks of the sequencer, as the engine reads it: a voice's Evolve and Density, the chord
 * timer's Drift (the voices' mean). 0 everywhere: exactly the knobs, as before. */
/* Signed offsets never rewrite saved knobs. LFOs use each layer's lead generator;
 * global controls average layers to avoid tripling their range. */
static float seq_offset(int v, int target)
{
    float sum = (float)xseq[v][target];
    int slot;
    for (slot = 0; slot < EN_NSLOT; slot++)
        if (vx[v].s[slot].dest == MOD_SEQ + target)
            sum += en_mod[slot][VOX_GEN0[v]] * 100.0f;
    return fm_clampf(sum, -100.0f, 100.0f);
}
static int expr_control(int v, int k)
{
    if (k == EX_AMOUNT && par[P_EX(v,EX_AMOUNT)] == 0) return 0; /* Explicit bypass outranks LFO offsets. */
    float value=(float)par[P_EX(v,k)];
    int slot, dest=MOD_EXPR+(k==EX_VARIATION);
    for(slot=0;slot<EN_NSLOT;slot++)
        if(vx[v].s[slot].dest==dest) value+=en_mod[slot][VOX_GEN0[v]]*100.0f;
    return (int)fm_clampf(value,0.0f,100.0f);
}

static int seq_value(int v, int param, int target)
{
    return xclamp(par[param] + (int)seq_offset(v, target), 0, 100);
}
static float seq_global(int target)
{
    float sum = 0.0f; int v;
    for (v = 0; v < EN_NVOX; v++) sum += seq_offset(v,target);
    return sum / (float)EN_NVOX;
}
static float xevolve(int v) { return fm_clampf(evolve + seq_offset(v, XT_EVOLVE - XT_DENSITY) * 0.01f, 0.0f, 1.0f); }
static int xdens(int v) { return xclamp(PL(v, DENS) + (int)seq_offset(v, 0), 0, 100); }
static float xdrift_scale(void)
{
    int v, sum = 0;
    for (v = 0; v < EN_NVOX; v++)
        sum += (int)seq_offset(v, XT_DRIFT - XT_DENSITY);
    return fm_clampf(1.0f + (float)sum * (1.0f / (float)EN_NVOX) * 0.01f, 0.25f, 4.0f);
}

static void drift_reset(void)
{
    drift_total = drift_secs(par[P_DRIFT]) * (0.7f + 0.6f * frand()) * xdrift_scale() * EN_SR;
    drift_left = drift_total;
    fifth_done = 0;
}

static void chord_change(void)                  /* to another degree: the near, strong moves first */
{
    static const float W7[7] = {4.0f, 2.0f, 1.0f, 3.0f, 3.0f, 3.0f, 0.4f};
    /* where a degree tends to go: I->IV, ii->V, iii->ii, IV->I, V->I, vi->IV, vii->I (a soft cadence, not a rule) */
    static const uint8_t NEXT7[7] = {3, 4, 1, 0, 0, 3, 0};
    float w[12], sum = 0.0f, r, pull;
    int i, v;
    const int cv = first_role(ROLE_CHORD);
    const float evo = cv >= 0 ? xevolve(cv) : evolve;
    pull = evo * fm_minf((float)since_home * 0.25f, 1.0f);
    for (i = 0; i < nsteps; i++) {
        w[i] = i == deg ? 0.0f : nsteps == 7 ? W7[i] : i == 0 ? 3.0f : 1.5f;
        if (i != deg && evo > 0.0f) {            /* Evolve: a direction, never a rule (0: the old walk, untouched) */
            if (nsteps == 7 && i == NEXT7[deg])
                w[i] += 6.0f * evo;              /* the cadence's own move */
            if (i == 0)
                w[i] += 4.0f * pull;             /* longer away, more pull home */
            if (nsteps == 7 && (i == 3 || i == 4))
                w[i] += 2.0f * evo * fm_maxf(arcm, 0.0f);      /* full: the far side */
            if (w[i] < 0.05f)
                w[i] = 0.05f;                    /* (no move is forbidden: still generative) */
        }
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
    if (deg == 0)
        since_home = 0;
    else if (since_home < 8)
        since_home++;
    pad_retarget();
    for (v = 0; v < EN_NVOX; v++)
        if (vc[v].role == ROLE_CHORD) {
            chord_leave(v);
        } else if (vc[v].role == ROLE_BASS) {
            vc[v].alt = 0;
            bass_to(v, chord_pcs[0]);
            /* Evolve: the bass sometimes sits a chord out, more often when the arc is sparse */
            vc[v].rest = frand() < xevolve(v) * 0.3f + 0.25f * fm_maxf(-arcm, 0.0f);
        }
    drift_reset();
}

/* ------------------------------------------------------------- the loops --- */
static const uint8_t *song;
static int song_offset(void);
static int rand_offset(void)                    /* scale steps above the chord's degree */
{
    if (song && frand() < fm_minf((float)par[P_SG(FOLLOW)] * 0.02f, 1.0f)) {   /* a song: from its phrases */
        const int o = song_offset();
        if (o >= 0)
            return o;
    }
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
    float d = (float)xdens(v) * 0.05f * (1.0f + 0.8f * arcm);
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

/* a structured variation of a loop's motif: the same shape moved a scale step, turned round, or mirrored around its
 * middle. The count and the scale stay (the notes are still scale steps from the chord's degree); Evolve decides how
 * often loop_pass takes one, so at 0 a loop is only grown and pruned, as before. */
static void loop_motif(int v, int l)
{
    int i, len = EN_LOOP_LEN[l], n = 0, lo = 127, hi = -127, mid, kind = irand(3);
    const int choice = irand(100 + seq_value(v, P_SEQ_ROTATE, XT_ROTATION - XT_DENSITY) + seq_value(v, P_SEQ_ACCENT, XT_ACCENT - XT_DENSITY));
    if (choice >= 100) kind = choice < 100 + seq_value(v, P_SEQ_ROTATE, XT_ROTATION - XT_DENSITY) ? 3 : 4;
    int8_t o[EN_LOOP_MAX];
    uint8_t ov[EN_LOOP_MAX];
    for (i = 0; i < len; i++) {
        o[i] = vc[v].loop_note[l][i];
        ov[i] = vc[v].loop_vel[l][i];
        if (o[i] >= 0) {
            n++;
            if (o[i] < lo) lo = o[i];
            if (o[i] > hi) hi = o[i];
        }
    }
    if (n < 2 || nsteps <= 0)
        return;
    mid = (lo + hi) / 2;
    {
        const int shift = frand() < 0.5f ? 1 : -1;
        const int rotation = kind == 3 ? 1 + irand(len - 1) : 0;
        int xs[EN_LOOP_MAX], low = 127, lift = 0;
        for (i = 0; i < len; i++) {
            const int s = kind == 1 ? len - 1 - i : (i + rotation) % len, x0 = o[s];
            int x = x0;
            if (x0 < 0) {
                xs[i] = -128;                    /* (a rest, kept apart from a note that may land on -1) */
                vc[v].loop_vel[l][i] = 0;
                continue;
            }
            if (kind == 0)
                x += shift;                      /* the same shape, a scale step up or down */
            else if (kind == 2)
                x = 2 * mid - x;                 /* mirrored around its middle */
            xs[i] = x;
            vc[v].loop_vel[l][i] = kind == 4 ? (uint8_t)xclamp((int)ov[s] + ((i & 3) == 0 ? 12 : -4), 35, 127) : ov[s];
            if (x < low)
                low = x;
        }
        if (low < 0)                             /* (below the chord's degree: the whole shape an octave up) */
            lift = nsteps * ((-low + nsteps - 1) / nsteps);
        for (i = 0; i < len; i++)
            vc[v].loop_note[l][i] = xs[i] == -128 ? -1 : (int8_t)((xs[i] + lift) % (nsteps * 4));
    }
    loop_publish(v, l);
}

/* A phrase snapshot is audio-owned and restored only at a loop boundary. */
static int8_t phrase_notes[EN_NVOX][EN_NLOOP][EN_LOOP_MAX];
static uint8_t phrase_vel[EN_NVOX][EN_NLOOP][EN_LOOP_MAX];
static uint8_t phrase_pos[EN_NVOX][EN_NLOOP], phrase_len[EN_NVOX][EN_NLOOP], phrase_on[EN_NVOX][EN_NLOOP];
/* Two compact melodic memories per voice; reuse the existing phrase snapshot
 * for local rhythm. Memories contain scale offsets, so recall follows the current key. */
static int8_t motif_memory[EN_NVOX][2][EN_LOOP_MAX];
static uint8_t motif_velocity[EN_NVOX][2][EN_LOOP_MAX], motif_count[EN_NVOX], motif_head[EN_NVOX];
static uint16_t music_tick_pos, music_ticks;
static uint32_t music_cycle, music_stamp, loop_stamp[EN_NVOX][EN_NLOOP], loop_cycle[EN_NVOX][EN_NLOOP];
static void music_reset(void)
{
    en_phrase_stage=en_phrase_leader=en_phrase_active=0;
    music_stage=music_active=music_running=music_leader=0;
    music_tick_pos=music_ticks=0; music_cycle=music_stamp=0;
    __builtin_memset(music_recall,0,sizeof music_recall);
    __builtin_memset(motif_count,0,sizeof motif_count);
    __builtin_memset(motif_head,0,sizeof motif_head);
    __builtin_memset(loop_stamp,0,sizeof loop_stamp);
    __builtin_memset(loop_cycle,0,sizeof loop_cycle);
}
static void music_store(int v)
{
    int slot=motif_head[v];
    if(vc[v].role!=ROLE_LOOPS || !loop_count(v,0)) return;
    for(int i=0;i<EN_LOOP_MAX;i++) {
        motif_memory[v][slot][i]=vc[v].loop_note[0][i];
        motif_velocity[v][slot][i]=vc[v].loop_vel[0][i];
    }
    motif_head[v]^=1u;
    if(motif_count[v]<2) motif_count[v]++;
}
static int coordinated_loop(int v,int l);
static void music_tick(void)
{
    uint32_t previous_stamp=music_stamp;
    int dialogue=xclamp(par[P_SEQ_DIALOGUE]+(int)seq_global(XT_DIALOGUE-XT_DENSITY),0,100);
    if(song || !dialogue || fm_clampf(evolve+seq_global(XT_EVOLVE-XT_DENSITY)*0.01f,0,1)<=0) {
        music_running=music_active=en_phrase_active=0; music_tick_pos=music_ticks=0;
        __builtin_memset(music_recall,0,sizeof music_recall); return;
    }
    music_running=1;
    if(!music_tick_pos) {
        int length=xclamp(par[P_SEQ_LENGTH]+(int)seq_global(XT_LENGTH-XT_DENSITY),0,100);
        int candidate=-1;
        music_ticks=(uint16_t)((2+length*6/100)*16);
        for(int i=0;i<EN_NVOX;i++) {
            int v=(music_cycle ? music_leader+1+i : i)%EN_NVOX;
            if(vc[v].role==ROLE_LOOPS) { candidate=v; break; }
        }
        music_leader=(uint8_t)(candidate>=0 ? candidate : (int)(music_cycle%EN_NVOX));
        music_active=(uint8_t)(frand()<(float)dialogue*0.01f);
        music_cycle++; music_stamp++; music_stage=0;
        __builtin_memset(music_recall,0,sizeof music_recall);
        if(music_active) music_store(music_leader);
    }
    {
        uint8_t phase=(uint8_t)((uint32_t)music_tick_pos*4u/music_ticks);
        if(phase!=music_stage) { music_stage=phase; music_stamp++; }
    }
    if(music_active && music_stamp!=previous_stamp)
        for(int v=0;v<EN_NVOX;v++) if(vc[v].role==ROLE_LOOPS)
            for(int l=0;l<EN_NLOOP;l++) coordinated_loop(v,l);
    /* A Bass voice resolves on the same answer phase as its partners. */
    if(music_active && music_stage==3)
        for(int v=0;v<EN_NVOX;v++) if(vc[v].role==ROLE_BASS && vc[v].pc!=chord_pcs[0]) {
            vc[v].alt=0; bass_to(v,chord_pcs[0]);
        }
    en_phrase_stage=music_stage; en_phrase_leader=music_leader; en_phrase_active=music_active;
    music_tick_pos=(uint16_t)((music_tick_pos+1)%music_ticks);
}
static int coordinated_loop(int v,int l)
{
    int len=EN_LOOP_LEN[l];
    if(!music_active) return 0;
    if(loop_cycle[v][l]!=music_cycle) {
        for(int i=0;i<len;i++) { phrase_notes[v][l][i]=vc[v].loop_note[l][i]; phrase_vel[v][l][i]=vc[v].loop_vel[l][i]; }
        loop_cycle[v][l]=music_cycle;
    }
    if(loop_stamp[v][l]==music_stamp) return 1;
    loop_stamp[v][l]=music_stamp;
    if(music_stage==2 && frand()<(float)seq_value(v,P_SEQ_TENSION,XT_TENSION-XT_DENSITY)*0.01f)
        loop_motif(v,l);
    if(music_stage==3) {
        int owner=v, memory=par[P_EX(v,EX_MEMORY)]*expr_control(v,EX_AMOUNT)/100, bank;
        for(int i=0;i<len;i++) { vc[v].loop_note[l][i]=phrase_notes[v][l][i]; vc[v].loop_vel[l][i]=phrase_vel[v][l][i]; }
        if(memory>0 && v!=music_leader && motif_count[music_leader] && frand()<(float)par[P_EX(v,EX_LINK)]*0.01f)
            owner=music_leader;
        if(memory>0 && motif_count[owner] && frand()<(float)memory*0.01f) {
            int cursor=0;
            bank=motif_head[owner]^1u;
            if(motif_count[owner]>1 && frand()<(float)memory*0.01f) bank^=1;
            for(int i=0;i<len;i++) if(vc[v].loop_note[l][i]>=0) {
                int tries=0;
                while(tries++<EN_LOOP_LEN[0] && motif_memory[owner][bank][cursor%EN_LOOP_LEN[0]]<0) cursor++;
                int at=cursor++%EN_LOOP_LEN[0], offset=motif_memory[owner][bank][at];
                if(offset>=0) {
                    /* A reply keeps the listener's rhythm and register, borrowing contour. */
                    vc[v].loop_note[l][i]=(int8_t)(offset%(nsteps*4));
                    vc[v].loop_vel[l][i]=(uint8_t)xclamp(((int)phrase_vel[v][l][i]+motif_velocity[owner][bank][at])/2,1,127);
                    music_recall[v]=1;
                }
            }
        }
    }
    return 1;
}
static int phrase_pass(int v, int l)
{
    if(song) return 0;
    if(music_running) return coordinated_loop(v,l);
    int i, len = EN_LOOP_LEN[l], pos = phrase_pos[v][l];
    const int dialogue = seq_value(v,P_SEQ_DIALOGUE,XT_DIALOGUE-XT_DENSITY);
    if (!dialogue || xevolve(v) <= 0.0f) { phrase_pos[v][l] = phrase_on[v][l] = 0; return 0; }
    if (!pos) {
        phrase_len[v][l] = (uint8_t)(2 + seq_value(v,P_SEQ_LENGTH,XT_LENGTH-XT_DENSITY) * 6 / 100);
        phrase_on[v][l] = (uint8_t)(frand() < (float)dialogue * 0.01f);
        for (i=0;i<len;i++) { phrase_notes[v][l][i]=vc[v].loop_note[l][i]; phrase_vel[v][l][i]=vc[v].loop_vel[l][i]; }
    }
    if (phrase_on[v][l]) {
        /* Opening repetitions are stable; the penultimate pass asks, the last answers. */
        if (pos == phrase_len[v][l]-2 && frand() < (float)seq_value(v,P_SEQ_TENSION,XT_TENSION-XT_DENSITY)*0.01f)
            loop_motif(v,l);
        if (pos == phrase_len[v][l]-1)
            for (i=0;i<len;i++) { vc[v].loop_note[l][i]=phrase_notes[v][l][i]; vc[v].loop_vel[l][i]=phrase_vel[v][l][i]; }
    }
    phrase_pos[v][l]=(uint8_t)((pos+1)%phrase_len[v][l]);
    return phrase_on[v][l];
}
static void loop_pass(int v, int l)             /* the end of a pass: grow, prune, evolve */
{
    voice_t *c = &vc[v];
    int n = loop_count(v, l), t = loop_target(v), s;
    const float ev = xevolve(v);
    for (; n < t; n++)                          /* DENSITY: to its count by the end of a pass */
        loop_add(v, l);
    for (; n > t && (s = loop_pick(v, l)) >= 0; n--)
        c->loop_note[l][s] = -1;
    if (phrase_pass(v,l)) {
        c->loop_rest[l] = 0; /* Dialogue owns this pass; never latch an earlier stochastic rest. */
        loop_publish(v,l); return;
    }
    c->loop_rest[l] = frand() < ev * 0.15f + 0.15f * fm_maxf(-arcm, 0.0f);
    if (ev > 0.0f && frand() < ev * (float)seq_value(v, P_SEQ_MOTIF, XT_MOTIF - XT_DENSITY) * 0.01f) {
        loop_motif(v, l);                        /* a shape kept, moved or turned: the motif survives the pass */
    } else if (frand() < ev * 0.7f && (s = loop_pick(v, l)) >= 0) {
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

/* ------------------------------------------------------- the generator --- */
/* a voice's patch made anew (the Gen page, en_gen): a kind of carriers and modulators, their ratios, envelopes and LFO
 * drawn from a seed. BRIGHT (0..100): the modulators' index, how high their ratios may go, their sustain. MOTION
 * (0..100): the carriers' chorus (their detune), the LFO on the modulators' amplitude, their envelopes' slow sweep.
 * Every draw is made in the same order whatever the settings, so a seed and its settings make one patch, and turning
 * BRIGHT or MOTION moves that patch, not another */
const char *const EN_GEN_NAME[EN_NGEN] = {"Pad", "Glass", "Air", "Strings", "Bell", "Keys", "Pluck", "Bass", "Drone",
                                          "Texture"};
typedef struct {
    uint8_t alg[3];                             /* the algorithms it is made on (1..32) */
    uint8_t atk0, atk1, rel0, rel1;             /* the carriers' attack and release rates (the seed's, in these) */
    uint8_t dec, sus;                           /* their decay rate (R2, R3), their sustain level */
    uint8_t mol0, mol1;                         /* the modulators' level at BRIGHT 0 and 100 */
    int8_t matk;                                /* their attack against the carriers' (negative: slower, a bloom) */
    uint8_t msus;                               /* their sustain level at BRIGHT 0 (BRIGHT opens it) */
    uint8_t fb0, fb1;                           /* the feedback */
    uint8_t ratios;                             /* its ratios: GEN_RATIO's */
    uint8_t vib, kvs;                           /* a vibrato's depth (0 none), the carriers' velocity */
} gen_kind_t;
static const gen_kind_t GEN_KIND[EN_NGEN] = {
    /*  alg          atk     rel     dec sus  mol    matk msus fb   r  vib kvs */
    {{5, 1, 22}, 30, 38, 30, 38, 30, 94, 64, 80, -4, 82, 2, 5, 0, 0, 1},    /* Pad */
    {{5, 19, 22}, 68, 84, 32, 44, 42, 35, 68, 90, 8, 24, 0, 2, 1, 0, 2},    /* Glass */
    {{5, 22, 1}, 30, 38, 30, 38, 30, 94, 60, 76, -4, 80, 3, 6, 2, 4, 1},    /* Air */
    {{1, 2, 3}, 42, 54, 34, 44, 34, 92, 74, 88, 2, 86, 3, 5, 0, 4, 1},     /* Strings */
    {{5, 19, 17}, 90, 99, 30, 42, 38, 0, 76, 94, 4, 0, 0, 2, 3, 0, 2},     /* Bell */
    {{5, 4, 1}, 90, 99, 45, 55, 44, 24, 68, 90, 6, 12, 0, 3, 4, 0, 3},      /* Keys */
    {{5, 2, 1}, 95, 99, 48, 58, 48, 0, 66, 90, 8, 40, 2, 6, 0, 0, 3},       /* Pluck */
    {{16, 1, 18}, 90, 99, 58, 70, 52, 68, 76, 94, 4, 35, 4, 7, 5, 0, 2},     /* Bass */
    {{32, 31, 5}, 24, 32, 26, 32, 28, 96, 50, 74, -6, 80, 1, 4, 6, 0, 0},   /* Drone */
    {{22, 5, 31}, 26, 40, 28, 36, 30, 88, 48, 78, -8, 72, 3, 7, 7, 0, 1},   /* Texture */
};
/* the ratios: (coarse, fine) each, the carriers' and the modulators'; the modulators' list in order of brightness
 * (BRIGHT reaches further into it) */
typedef struct {
    uint8_t nc, nm;
    uint8_t car[6][2], mod[8][2];
} gen_ratio_t;
static const gen_ratio_t GEN_RATIO[8] = {
    {4, 6, {{1, 0}, {1, 0}, {2, 0}, {0, 0}}, {{1, 0}, {1, 0}, {2, 0}, {3, 0}, {4, 0}, {5, 0}}},              /* harmonic */
    {3, 7, {{1, 0}, {2, 0}, {4, 0}}, {{1, 0}, {1, 41}, {3, 50}, {2, 76}, {5, 0}, {7, 0}, {9, 0}}},           /* glass */
    {3, 4, {{1, 0}, {3, 0}, {5, 0}}, {{1, 0}, {1, 0}, {2, 0}, {3, 0}}},                                      /* formants */
    {3, 7, {{1, 0}, {2, 0}, {1, 0}}, {{1, 0}, {3, 50}, {2, 76}, {1, 41}, {5, 0}, {4, 20}, {7, 0}}},          /* bells */
    {3, 4, {{1, 0}, {1, 0}, {2, 0}}, {{1, 0}, {1, 0}, {14, 0}, {3, 0}}},                                     /* tines */
    {2, 5, {{0, 0}, {1, 0}}, {{1, 0}, {0, 0}, {1, 0}, {2, 0}, {3, 0}}},                                      /* bass */
    {6, 3, {{0, 0}, {1, 0}, {1, 50}, {2, 0}, {3, 0}, {4, 0}}, {{1, 0}, {2, 0}, {1, 0}}},                     /* drone */
    {4, 8, {{1, 0}, {1, 3}, {2, 0}, {3, 0}}, {{1, 0}, {2, 3}, {3, 17}, {5, 41}, {7, 0}, {9, 37}, {11, 0}, {13, 0}}},
};
static uint32_t gen_rng;
static uint32_t gen_draw(void)                  /* the generator's own (xorshift32), apart from the garden's */
{
    uint32_t x = gen_rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    gen_rng = x;
    return x >> 8;                               /* 24 bits */
}
static int gen_in(uint32_t d, int lo, int hi) { return lo + (int)(d % (uint32_t)(hi - lo + 1)); }
static int clamp_to(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

void en_gen(int kind, int bright, int motion, uint32_t seed, uint8_t *p)
{
    const gen_kind_t *g;
    kind = (unsigned)kind < EN_NGEN ? kind : 0;
    g = &GEN_KIND[kind];
    const gen_ratio_t *rt = &GEN_RATIO[g->ratios];
    uint32_t d[6][8], dv[8], car;
    int k, j, alg, catk, crel, seen[6][3], ns = 0;
    char name[12] = "GEN ";
    gen_rng = seed * 2654435761u ^ 0x9E3779B9u;
    if (!gen_rng)
        gen_rng = 1u;
    for (k = 0; k < 8; k++)                      /* every draw first, in one order */
        dv[k] = gen_draw();
    for (k = 0; k < 6; k++)
        for (j = 0; j < 8; j++)
            d[k][j] = gen_draw();
    bright = clamp_to(bright, 0, 100);
    motion = clamp_to(motion, 0, 100);
    alg = g->alg[dv[0] % 3u];
    car = fm6_carriers((uint32_t)(alg - 1));
    catk = gen_in(dv[1], g->atk0, g->atk1);
    crel = gen_in(dv[2], g->rel0, g->rel1);
    for (k = 1; k <= 6; k++) {                   /* operator k (1..6): its bit in car is 6 - k */
        const uint32_t *r = d[k - 1];
        const int is_car = (int)(car >> (6 - k) & 1u);
        int fc, ff, ol, det, r1, r2, r3, r4, l1, l2, l3, ams = 0, kvs, rd = 0;
        if (is_car) {
            const int x = (int)(r[0] % rt->nc);
            const int dt = (motion * 4 + 50) / 100 * (1 + (int)(r[1] % 2u)), sign = (k & 1) ? 1 : -1;
            fc = rt->car[x][0];
            ff = rt->car[x][1];
            ol = k == 1 ? 99 : gen_in(r[2], 84, 96);
            if (fc >= 2 && k != 1)               /* (BRIGHT: the upper carriers up or down, a carriers-only voice too) */
                ol = clamp_to(ol + (bright - 50) / 6, 0, 99);
            det = clamp_to(7 + sign * dt, 0, 14);
            r1 = clamp_to(catk + gen_in(r[3], -2, 2), 1, 99);
            r2 = clamp_to(g->dec + gen_in(r[4], -3, 3), 1, 99);
            r3 = clamp_to(r2 - 4, 1, 99);
            r4 = clamp_to(crel + gen_in(r[5], -2, 2), 1, 99);
            l1 = 99;
            l3 = g->sus ? clamp_to(g->sus + gen_in(r[6], -3, 2), 0, 99) : 0;
            l2 = g->sus ? 99 - (99 - l3) / 3 : 78;
            kvs = g->kvs;
            if (g->ratios == 7 && motion > 50)   /* (a texture's carriers breathe too) */
                ams = 1;
        } else {
            const int n = 2 + (rt->nm - 2) * clamp_to(bright - 30, 0, 70) / 70,   /* (the higher ratios past 30) */
                      x = (int)(r[0] % rt->nm) % (n < 1 ? 1 : n);
            const int swell = motion > 50 ? (motion - 50) / 5 : 0;   /* (0..10: a sweep up and down) */
            fc = rt->mod[x][0];
            ff = rt->mod[x][1];
            ol = clamp_to(g->mol0 + (g->mol1 - g->mol0) * bright / 100 + gen_in(r[2], -4, 4) - (fc >= 5 ? 6 : 0), 0, 99);
            det = clamp_to(7 + gen_in(r[1], -1, 1) * (motion > 30), 0, 14);
            r1 = clamp_to(catk + g->matk - swell + gen_in(r[3], -3, 3), 1, 99);
            r2 = clamp_to(g->dec - 2 - swell + gen_in(r[4], -3, 3), 1, 99);
            r3 = clamp_to(r2 - 4, 1, 99);
            r4 = clamp_to(crel + 6 + gen_in(r[5], -2, 2), 1, 99);
            l1 = 99;
            l3 = clamp_to(g->msus + bright / 6 - swell + gen_in(r[6], -4, 4), 0, 99);
            l2 = 99 - (99 - l3) / 3;
            kvs = clamp_to(g->kvs + 1, 0, 7);
            ams = motion > 65 ? 2 : motion > 25 ? 1 : 0;
            rd = 25;                             /* (softer up high) */
        }
        if (is_car) {                            /* carriers at one ratio kept apart (a chorus, not a beat to silence) */
            int s, same = 0;
            for (s = 0; s < ns; s++)
                if (seen[s][0] == fc && seen[s][1] == ff) {
                    same = ++seen[s][2];
                    ol = clamp_to(ol, 0, 99 - (same == 1 ? 10 : same == 2 ? 16 : 20));
                }
            if (!same && ns < 6) {
                seen[ns][0] = fc, seen[ns][1] = ff, seen[ns][2] = 0;
                ns++;
            }
        }
        fm_op(p, k, r1, r2, r3, r4, l1, l2, l3, 0, ol, fc, ff, det, kvs, 1);
        p[(6 - k) * FP_OP + FP_AMS] = (uint8_t)ams;
        p[(6 - k) * FP_OP + FP_RD] = (uint8_t)rd;
    }
    {
        const int fb = clamp_to(gen_in(dv[3], g->fb0, g->fb1) + (bright > 85), 0, 7);
        const int vib = g->vib, lfs = vib ? gen_in(dv[4], 30, 35) : 3 + motion * 15 / 100 + (int)(dv[4] % 5u);
        for (k = 0; EN_GEN_NAME[kind][k] && k < 6; k++)
            name[4 + k] = (char)(EN_GEN_NAME[kind][k] >= 'a' ? EN_GEN_NAME[kind][k] - 32 : EN_GEN_NAME[kind][k]);
        name[4 + k] = 0;
        fm_voice_par(p, alg, fb, lfs, vib ? 60 : 0, vib, g->ratios == 7 && motion > 60 ? 5 : 4, vib ? 2 : 0, name);
        p[FP_LAMD] = (uint8_t)(motion * 35 / 100);
    }
}

/* the patch varied by amount (0..100), from a seed: levels, rates, sustains, detune, a little the modulators' fine
 * ratio and the feedback, the LFO; past 70 a modulator now and then to another harmonic. Its algorithm and carriers'
 * ratios kept */
void en_gen_vary(uint8_t *p, int amount, uint32_t seed)
{
    const uint32_t car = fm6_carriers(p[FP_ALG] & 31u);
    const int a = clamp_to(amount, 0, 100);
    int k, s;
    gen_rng = seed * 2246822519u ^ 0x85EBCA6Bu;
    if (!gen_rng)
        gen_rng = 1u;
#define VARY(off, span, lo, hi) (p[off] = (uint8_t)clamp_to((int)p[off] + gen_in(gen_draw(), -(span), (span)), lo, hi))
    for (k = 0; k < 6; k++) {
        uint8_t *o = p + k * FP_OP;
        const int is_car = (int)(car >> k & 1u), off = k * FP_OP;
        (void)o;
        VARY(off + FP_OL, a * (is_car ? 4 : 8) / 100, is_car ? 60 : 0, 99);
        for (s = 0; s < 4; s++)
            VARY(off + FP_R1 + s, a * 10 / 100, 1, 99);
        VARY(off + FP_L1 + 2, a * 8 / 100, 0, 99);
        VARY(off + FP_DET, a * 3 / 100, 0, 14);
        if (!is_car && !o[FP_MODE]) {
            if ((int)(gen_draw() % 100u) < a / 2)
                VARY(off + FP_FF, a * 10 / 100, 0, 99);
            if (a > 70 && (int)(gen_draw() % 100u) < a - 70) {   /* to a neighbouring harmonic */
                static const uint8_t H[6] = {1, 2, 3, 4, 5, 7};
                o[FP_FC] = H[gen_draw() % 6u];
            }
        }
    }
    if ((int)(gen_draw() % 100u) < a)
        VARY(FP_FB, 1, 0, 7);
    VARY(FP_LFS, a * 15 / 100, 0, 99);
    VARY(FP_LAMD, a * 15 / 100, 0, 99);
#undef VARY
}

#ifdef EN_HOST
/* (the tests' probe) a patch's note alone, mono, its own envelopes: held on_s, let go for off_s; env[] the RMS of each
 * 50 ms; *harm its brightness, the RMS of its slope over its RMS as a frequency, in harmonics of the note (a sine 1),
 * from 50 ms to the release (a pluck's is in its first moments) */
int en_probe(const uint8_t *p, int note, float on_s, float off_s, float *env, int nenv, float *harm)
{
    static fm6_note_t n;
    static fm6_lfo_t lf;
    static uint8_t q[FP_SIZE + 1];
    static const int32_t zero[6] = {0, 0, 0, 0, 0, 0};
    const int seg = 2205, total = (int)((on_s + off_s) * EN_SR), off = (int)(on_s * EN_SR);
    int i, k = 0, acc = 0;
    float s2 = 0.0f, last = 0.0f, x[FM6_N], ss = 0.0f, sd = 0.0f;
    __builtin_memcpy(q, p, FP_SIZE);
    q[FP_SIZE] = 0;
    __builtin_memset(&n, 0, sizeof n);
    fm_note_on(&n, q, note, 100, 1);
    fm6_lfo_reset(&lf, q);
    fm6_lfo_key(&lf);
    for (i = 0; i < total; i += FM6_N) {
        const int32_t lv = fm6_lfo_sample(&lf), ld = fm6_lfo_delay(&lf);
        uint32_t j;
        if (i < off && i + FM6_N >= off)
            fm6_note_key(&n, q, 0);
        if (!fm_voice(&n, q, (float)note, lv, ld, 0.0f, 0, zero, zero, x))
            for (j = 0; j < FM6_N; j++)
                x[j] = 0.0f;
        for (j = 0; j < FM6_N; j++) {
            if (i > 2205 && i < off) {
                ss += x[j] * x[j];
                sd += (x[j] - last) * (x[j] - last);
            }
            last = x[j];
            s2 += x[j] * x[j];
            if (++acc == seg) {
                if (k < nenv)
                    env[k++] = fm_sqrtf(s2 / (float)seg);
                acc = 0;
                s2 = 0.0f;
            }
        }
    }
    *harm = ss > 0.0f ? fm_sqrtf(sd / ss) * EN_SR / (FM_TWO_PI * 440.0f * fm_exp2f((float)(note - 69) / 12.0f)) : 0.0f;
    return k;
}
#endif

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
static int patch_decays(const uint8_t *p)       /* no sounding carrier held past -16 dB (L3 78): a piano, a pluck */
{
    const uint32_t car = fm6_carriers(p[FP_ALG] & 31u);
    uint32_t j;
    for (j = 0; j < 6u; j++)
        if ((car >> j & 1u) && p[j * FP_OP + FP_OL] >= 40u && p[j * FP_OP + FP_L1 + 2] >= 78u)
            return 0;
    return 1;
}
static float patch_gain(int v, int src, const uint8_t *p)
{
    static const float BASE[EN_NROLE] = {1.97f, 1.0f, 1.0f};   /* (a chord's: 1.48 until 1.4.12, 3 .. 5 dB under
                                                                * AMB-1's own pads; +2.5 dB) */
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
    c->decays = (uint8_t)patch_decays(vpat[v]);
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
    motif_count[v]=motif_head[v]=0;
    __builtin_memset(loop_stamp[v],0,sizeof loop_stamp[v]);
    __builtin_memset(loop_cycle[v],0,sizeof loop_cycle[v]);
    __builtin_memset(phrase_pos[v], 0, sizeof phrase_pos[v]);
    __builtin_memset(phrase_on[v], 0, sizeof phrase_on[v]);
    voice_t *c = &vc[v];
    int i, l;
    midi(0xB0u | (uint32_t)v, 123, 0);
    for (i = 0; i < NPOLY; i++) {
        vnote_t *n = &c->n[i];
        n->amp = n->tgt = 0.0f;
        n->on = n->rung = n->hand = n->fade = 0;
        n->up = 1;
        n->hf = 1.0f;
        n->midi_note = 0;
        n->pan = PAD_PAN[i];
        n->gl = n->gr = n->pl = n->pr = 0.0f;
        n->t = START_T[i];
        n->note = n->target = (float)(50 + 7 * i + 12 * PL(v, OCT));
    }
    c->gain = patch_gain(v, par[EN_VOX_PATCH[v]], vpat[v]);
    c->decays = (uint8_t)patch_decays(vpat[v]);
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

static void bell_voice(int v, int l, int n, float vel, int32_t wait, int32_t slen)
{
    voice_t *c = &vc[v];
    int i, best = 0;
    float lo = 9.0f, p;
    vnote_t *b;
    if (load_cap) {                              /* under load: three notes a voice (two), the quietest over faded */
        int on = 0, q = -1;
        float ql = 9.0f;
        for (i = 0; i < NPOLY; i++) {
            vnote_t *o = &c->n[i];
            if (o->on && !o->fade) {
                const float a = o->rung ? fm_gain(&o->fm, o->vp) * o->vel : 2.0f;
                on++;
                if (a < ql) {
                    ql = a;
                    q = i;
                }
            }
        }
        if (q >= 0 && on >= (load_cap == 2 ? 2 : 3))
            c->n[q].fade = 6;
    }
    for (i = 0; i < NPOLY; i++) {               /* a free note, or the quietest (its carrier's gain) */
        const vnote_t *q = &c->n[i];
        float a = q->on ? (q->rung ? fm_gain(&q->fm, q->vp) * q->vel : 2.0f) : -1.0f;
        if (a < lo) {
            lo = a;
            best = i;
        }
    }
    for (i = 0; i < NPOLY; i++) {               /* (another note of this pitch still held: its MIDI off now, before
                                                 * the new one's on, not after it, cutting it) */
        vnote_t *o = &c->n[i];
        if (i != best && o->on && !o->moff && o->midi_note == (uint8_t)n) {
            midi(0x80u | (uint32_t)v, o->midi_note, 0);
            o->moff = 1;
        }
    }
    b = &c->n[best];
    if (b->on && !b->moff)
        midi(0x80u | (uint32_t)v, b->midi_note, 0);
    b->moff = 0;
    b->on = 1;
    b->rung = 0;
    b->fade = 0;
    b->midi_note = (uint8_t)n;
    b->loop = (uint8_t)l;
    b->vel = vel;
    p = (LOOP_PAN[l] + 1.0f) * (FM_PI / 4.0f);
    b->gl = fm_cosf(p);
    b->gr = fm_sinf(p);
    b->wait = wait;
    b->slen = slen;
    if (first_role(ROLE_LOOPS) == v)
        en_loop_glow[l] = 1.0f;
    midi(0x90u | (uint32_t)v, (uint32_t)n, (uint32_t)(vel * 110.0f));
}

static void bell_ring(int v, int l, int slot, int32_t sw)
{
    const int range = 1 + seq_value(v,P_SEQ_RANGE,XT_RANGE-XT_DENSITY) * 3 / 100;
    int offset = vc[v].loop_note[l][slot];
    int n;
    if (nsteps > 0) offset %= nsteps * range;
    n = bell_note(v, l, offset);
    if (((music_running && music_active && music_stage==3) || (!music_running && phrase_on[v][l] && phrase_pos[v][l] == 0)) &&
        frand() < (float)seq_value(v,P_SEQ_RESOLVE,XT_RESOLVE-XT_DENSITY)*0.01f) {
        int d, c, found=0;
        for (d=0;d<=6 && !found;d++) for(c=0;c<chord_nt;c++) {
            if ((n+d)%12==chord_pcs[c] && n+d<=127) { n+=d; found=1; break; }
            if ((n-d+120)%12==chord_pcs[c] && n-d>=0) { n-=d; found=1; break; }
        }
    }
    if (seq_value(v,P_SEQ_SPACE,XT_SPACE-XT_DENSITY) > 0 &&
        frand() < (float)seq_value(v,P_SEQ_SPACE,XT_SPACE-XT_DENSITY)*0.008f) return;
    float vel = (float)vc[v].loop_vel[l][slot] * 0.01f;
    int human = seq_value(v,P_SEQ_HUMAN,XT_HUMAN-XT_DENSITY);
    int cap = (int)fm_minf(1764.0f, step_len * 0.2f);
    int32_t wait = sw + (human ? irand(1 + cap * human / 100) : 0);            /* Swing, then a human hand: up to 60 ms late */
    bell_voice(v, l, n, vel, wait, 0);
    if (n + 12 <= 103 && n + 12 < bell_note(v,l,0) + range*12 && frand() < 0.08f + 0.12f * fm_maxf(arcm, 0.0f))
        bell_voice(v, l, n + 12, vel * 0.4f, wait + 300 + irand(2000), 0);   /* now and then, its octave */
}

/* ------------------------------------------------------------------ a song --- */
/* (song: declared before rand_offset) */
/* a song (tools/amb_mid.py, in the flash: the UI points the engine at it, en_song; amb.h has its record): the chords
 * the song's, a half bar each (the drift waits); FOLLOW from 50 its melody on the first Loops voice and its bass on the
 * first Bass voice, each note with a chance of (FOLLOW - 50) / 50 (the loops' own the rest); under 50 the loops' own
 * notes, new ones taken from the song's melody near where it is, FOLLOW / 50 of them. VARY: a melody note a scale step
 * off, a sixteenth late, left out. It goes round from START */
static int song_n, song_mask = -1, song_root = -1;
volatile uint32_t en_song_pos;
static uint32_t song_pos;
#define PSG(k) (par[P_SG(k)])
static int song_at(int part, int pos)            /* part 0 the melody's notes, 1 its lengths, 2 the bass's, 3 its */
{
    const int x = song[EN_SONG_HDR + 3 * ((song_n + 7) / 8) + part * song_n + pos];
    return x < 128 ? x : 0;                      /* (a byte not 7-bit: flash erased, a song half sent: none) */
}
static int song_tr(void)                        /* the song moved with the garden's root: its root less the song's,
                                                 * -6 .. +5 (0 as chosen: SONG sets the root) */
{
    const int d = ((par[P_ROOT] - song[9] % 12) % 12 + 12) % 12;
    return d > 5 ? d - 12 : d;
}
static uint32_t song_start(void)
{
    const uint32_t s = (uint32_t)PSG(START) * 16u;
    return s < (uint32_t)song_n ? s : 0u;
}
static void song_take(const uint8_t *rec)
{
    song = rec;
    song_n = rec ? rec[5] << 7 | rec[6] : 0;
    if (song_n <= 0 || song_n > EN_SONG_MAXN)
        song = 0;
    song_mask = song_root = -1;
    song_pos = song ? song_start() : 0u;
    en_song_pos = song_pos;
    if (!song) {                                 /* back to the garden's own chords, the basses to their root */
        int v;
        pad_retarget();
        for (v = 0; v < EN_NVOX; v++)
            if (vc[v].role == ROLE_CHORD)
                chord_leave(v);
            else if (vc[v].role == ROLE_BASS)
                bass_to(v, chord_pcs[0]);
        drift_reset();
    }
}
static int song_neighbour(int n, int dir)       /* the next note of the scale from n, up or down */
{
    int d;
    for (d = 1; d <= 3; d++)
        if (pool >> ((n + dir * d - par[P_ROOT] + 120) % 12) & 1u)
            return n + dir * d;
    return n;
}
static int song_mel_v = -1, song_bass_v = -1;  /* the voices playing a song's melody, its bass (song_parts) */
static void song_chord(int i)                   /* its chord at half bar i, if it changes */
{
    const uint8_t *c = song + EN_SONG_HDR + 3 * i;
    const int tr = (song_tr() + 12) % 12, m0 = (c[0] | c[1] << 6) & 0xFFF, fm = PSG(FOLLOW),
              mask = ((m0 << tr) | (m0 >> (12 - tr))) & 0xFFF, root = c[2] > 11 ? c[2] : (c[2] + tr) % 12;
    int k, v;
    if (root > 11 || !mask || (mask == song_mask && root == song_root))
        return;
    song_mask = mask;
    song_root = root;
    chord_nt = 0;
    for (k = 0; k < 12 && chord_nt < 4; k++)     /* its pitch classes, from its root up */
        if (k == 0 || mask >> ((root + k) % 12) & 1)
            chord_pcs[chord_nt++] = (root + k) % 12;
    {                                            /* its degree in the scale, else the nearest (the loops' notes from it) */
        int best = 12;
        for (k = 0; k < nsteps; k++) {
            const int d = (steps[k] + par[P_ROOT] - root + 120) % 12, dd = d > 6 ? 12 - d : d;
            if (dd < best) {
                best = dd;
                deg = k;
            }
        }
    }
    chord_publish();
    for (v = 0; v < EN_NVOX; v++)
        if (vc[v].role == ROLE_CHORD)
            chord_leave(v);
        else if (vc[v].role == ROLE_BASS && (fm < 75 || (v != song_mel_v && v != song_bass_v))) {
            vc[v].alt = 0;
            bass_to(v, root);                    /* (following the song: its notes, not the roots) */
        }
}
static int song_offset(void)                    /* a loops note from the song's melody near here: scale steps above
                                                 * the chord's degree; -1 none */
{
    int k, i = irand(64);
    for (k = 0; k < 64; k++, i = (i + 7) % 64) {
        const int pos = ((int)song_pos + i - 32 + song_n * 4) % song_n, n = song_at(0, pos);
        int x, rel;
        if (!n)
            continue;
        rel = (n + song_tr() - par[P_ROOT] + 120) % 12;
        for (x = 0; x < nsteps; x++)
            if (steps[x] == rel)
                return (x - deg + nsteps) % nsteps + (n >= 72 ? nsteps : 0);
    }
    return -1;
}
static float song_mel(void) { return song ? fm_clampf((float)(PSG(FOLLOW) - 50) * 0.02f, 0.0f, 1.0f) : 0.0f; }
/* who plays the song's melody and its bass (-1 none), whatever the roles: the melody on the first Loops voice, else
 * the highest Bass voice (one note at a time); the bass on the lowest Bass voice left, else the lowest Loops voice
 * left. The Chord voices play its chords; a voice with no part of it plays on, its own */
static void song_parts(void)
{
    int v, m = first_role(ROLE_LOOPS), b = -1;
    if (m < 0)
        for (v = 0; v < EN_NVOX; v++)
            if (vc[v].role == ROLE_BASS && (m < 0 || PL(v, OCT) > PL(m, OCT)))
                m = v;
    for (v = 0; v < EN_NVOX; v++)
        if (vc[v].role == ROLE_BASS && v != m && (b < 0 || PL(v, OCT) < PL(b, OCT)))
            b = v;
    if (b < 0)
        for (v = 0; v < EN_NVOX; v++)
            if (vc[v].role == ROLE_LOOPS && v != m && (b < 0 || PL(v, OCT) <= PL(b, OCT)))
                b = v;
    song_mel_v = m;
    song_bass_v = b;
}
static void song_step(void)                      /* a sixteenth of the song */
{
    const int pos = (int)song_pos, vary = PSG(VARY);
    const float f = song_mel();
    int v, n;
    if (song[0] != 'A') {                        /* (its flash rewritten under it: let go) */
        song_take(0);
        return;
    }
    song_parts();
    if (pos % 8 == 0)
        song_chord(pos / 8);
    if (f > 0.0f && (n = song_at(0, pos)) != 0 && (v = song_mel_v) >= 0 && frand() < f &&
        frand() >= (float)vary * 0.002f) {       /* the melody (VARY: now and then left out) */
        int32_t wait = 0;
        n += song_tr() + 12 * PL(v, OCT);
        if (frand() < (float)vary * 0.003f)      /* a scale step off */
            n = song_neighbour(n, frand() < 0.5f ? 1 : -1);
        if (frand() < (float)vary * 0.002f)      /* a sixteenth late */
            wait = (int32_t)step_len;
        while (n > 103)
            n -= 12;
        while (n < 36)
            n += 12;
        if (vc[v].role == ROLE_BASS) {
            bass_song(v, n > 96 ? n - 12 : n, song_at(1, pos));   /* (one at a time; its length, as the file has it) */
        } else {                                 /* (4. one place, a little right of the middle: loop 2's) */
            bell_voice(v, 1, n, 0.55f + 0.004f * (float)(song_at(1, pos) < 8 ? song_at(1, pos) : 8) * 10.0f, wait,
                       (int32_t)((float)song_at(1, pos) * step_len));
        }
    }
    if (f > 0.0f && (n = song_at(2, pos)) != 0 && (v = song_bass_v) >= 0 && frand() < f) {   /* the bass */
        n += song_tr();
        if (vc[v].role == ROLE_BASS) {           /* (2. its own pitch, with the voice's Octave; folded only past E1 ..
                                                 * C4) */
            n += 12 * PL(v, OCT);
            while (n > 60)
                n -= 12;
            while (n < 28)
                n += 12;
            bass_song(v, n, song_at(3, pos));
        } else {                                 /* on a Loops voice, low */
            n += 12 * PL(v, OCT);
            while (n > 60)
                n -= 12;
            while (n < 36)
                n += 12;
            bell_voice(v, 3, n, 0.8f, 0, (int32_t)((float)song_at(3, pos) * step_len));   /* (a little left) */
        }
    }
    if (++song_pos >= (uint32_t)song_n)
        song_pos = song_start();
    en_song_pos = song_pos;
}

/* the grid: one call a sixteenth (MIDI clock: 6 ticks) */
static uint32_t sixteenth;
static int32_t swing_delay(int st)              /* the off-beat step's late arrival (Swing), in samples: the second of
                                                 * each pair of steps, at a sixteenth or eighth grid (0: straight) */
{
    const float swing = fm_clampf(swing_amt + seq_global(XT_SWING - XT_DENSITY) * 0.005f, 0.0f, 0.5f);
    if (swing <= 0.0f || st <= 0 || st > 2)
        return 0;
    return (sixteenth / (uint32_t)st) & 1u ? (int32_t)(swing * (float)st * step_len) : 0;
}
static void tick(void)
{
    music_tick();
    int l, v;
    if (song)
        song_step();
    for (v = 0; v < EN_NVOX; v++) {
        voice_t *c = &vc[v];
        int st;
        if (c->role == ROLE_LOOPS && sixteenth % (uint32_t)loops_step(v) == 0u) {   /* the loops: PACE's step */
            const int32_t sw = swing_delay(loops_step(v));
            for (l = 0; l < EN_NLOOP; l++) {
                int s = c->loop_pos[l];
                if (c->loop_note[l][s] >= 0 && !c->loop_rest[l] &&
                    !(song && (v == song_mel_v || v == song_bass_v) && frand() < song_mel()))
                    bell_ring(v, l, s, sw);      /* (a song followed: the voices playing its melody and its bass give
                                                  * way to it, the loops' own the rest of the time; the others play on) */
                if (first_role(ROLE_LOOPS) == v)
                    en_loop_pos[l] = (uint8_t)s;
                if (++c->loop_pos[l] >= EN_LOOP_LEN[l]) {
                    c->loop_pos[l] = 0;
                    loop_pass(v, l);
                }
            }
        } else if (c->role == ROLE_BASS && (PL(v, ENV) != ENV_AMB || c->decays) && (st = bass_step(v)) > 0 &&
                   sixteenth % (uint32_t)st == 0u) {
            if (c->alt) {                        /* back from a passing tone to the chord's root */
                c->alt = 0;
                if (c->pc != chord_pcs[0])
                    bass_to(v, chord_pcs[0]);
                else
                    c->strike = 1;
            } else if (!(music_running && music_active && music_stage==3) && xevolve(v) > 0.0f && chord_nt > 1 && frand() < xevolve(v) * (float)seq_value(v, P_SEQ_PASS, XT_PASSING - XT_DENSITY) * 0.01f) {
                c->alt = 1;                      /* a chord tone on the way (the third or fifth) */
                bass_to(v, chord_pcs[1 + irand(chord_nt - 1)]);
            } else {
                c->strike = 1;                   /* the bass struck again, PACE's step */
            }
            c->swait = swing_delay(st);
        }
    }
    if ((sixteenth & 15u) == 0u && change_pending && !en_frozen && !song) {   /* chords move on the bar */
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
 * are for 29761 Hz; DS() scales them to 11.025 kHz, the plate's rate (1.4.14: a quarter of the work of 44.1 kHz; its
 * tank keeps nothing above ~3 kHz, its input band and damping take it, so what it loses above 5.5 kHz it never had):
 * reverb() is called every fourth sample, on the four's mean, and its output drawn as a line between its last two
 * (plate_step). Silent a while (rested, its tail gone), it rests too */
#define DS(n) (((n) * 3705 + 5000) / 10000)           /* 11025 / 29761 */
#define PRE_N 331                                    /* 30 ms */
#define EXC DS(16)
enum { L_PRE, L_IN1, L_IN2, L_IN3, L_IN4, L_APL, L_D1L, L_AP2L, L_D2L, L_APR, L_D1R, L_AP2R, L_D2R, L_N };
static const int DL_LEN[L_N] = {PRE_N, DS(142), DS(107), DS(379), DS(277), DS(672) + EXC + 2, DS(4453), DS(1800),
                                DS(3720), DS(908) + EXC + 2, DS(4217), DS(2656), DS(3163)};
#define DL_TOTAL (PRE_N + DS(142) + DS(107) + DS(379) + DS(277) + DS(672) + DS(4453) + DS(1800) + DS(3720) + DS(908) + \
                  DS(4217) + DS(2656) + DS(3163) + 2 * (EXC + 2))
/* the lines in 16 bits (half the RAM of floats: room for the tape), +-4 full scale: the tank's quantisation ~-78 dB */
#define PL_Q 8192.0f
static int16_t pl_buf[DL_TOTAL] EN_POOL;
typedef struct {
    int16_t *b;
    int n, i;
} dl_t;
static dl_t dl[L_N];
static float pl_bw, pl_damp_l, pl_damp_r, pl_decay = 0.5f, pl_decay_t = 0.5f, pl_dd2 = 0.5f, lfo_s, lfo_c = 1.0f;

static inline float tap(const dl_t *d, int k)
{
    int j = d->i - k;
    return (float)d->b[j < 0 ? j + d->n : j] * (1.0f / PL_Q);
}
static inline void push(dl_t *d, float x)
{
    const int q = (int)(x * PL_Q);               /* (toward zero: symmetric, no bias; a branch less a sample) */
    d->b[d->i] = (int16_t)(q > 32767 ? 32767 : q < -32767 ? -32767 : q);
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
    pl_bw += 0.84f * (x - pl_bw);                /* (0.6 at 22.05 kHz) */
    x = allpass(&dl[L_IN1], DS(142), pl_bw, 0.75f);
    x = allpass(&dl[L_IN2], DS(107), x, 0.75f);
    x = allpass(&dl[L_IN3], DS(379), x, 0.625f);
    x = allpass(&dl[L_IN4], DS(277), x, 0.625f);
    {
        float s = lfo_s + 0.0004f * lfo_c, c = lfo_c - 0.0004f * s;   /* (its rate as at 22.05 kHz before) */
        lfo_s = s;
        lfo_c = c;
    }
    a = allpass_mod(&dl[L_APL], (float)DS(672) + (float)EXC * lfo_s, x + pl_decay * fb_l, -0.7f);
    push(&dl[L_D1L], a);
    b = tap(&dl[L_D1L], DS(4453));
    pl_damp_l += 0.6975f * (b - pl_damp_l);     /* (0.45 at 22.05 kHz) */
    b = allpass(&dl[L_AP2L], DS(1800), pl_damp_l * pl_decay, pl_dd2);
    push(&dl[L_D2L], b);
    a = allpass_mod(&dl[L_APR], (float)DS(908) + (float)EXC * lfo_c, x + pl_decay * fb_r, -0.7f);
    push(&dl[L_D1R], a);
    b = tap(&dl[L_D1R], DS(4217));
    pl_damp_r += 0.6975f * (b - pl_damp_r);
    b = allpass(&dl[L_AP2R], DS(2656), pl_damp_r * pl_decay, pl_dd2);
    push(&dl[L_D2R], b);
    *ol = 0.6f * (tap(&dl[L_D1R], DS(266)) + tap(&dl[L_D1R], DS(2974)) - tap(&dl[L_AP2R], DS(1913)) +
                  tap(&dl[L_D2R], DS(1996)) - tap(&dl[L_D1L], DS(1990)) - tap(&dl[L_AP2L], DS(187)) -
                  tap(&dl[L_D2L], DS(1066)));
    *or_ = 0.6f * (tap(&dl[L_D1L], DS(353)) + tap(&dl[L_D1L], DS(3627)) - tap(&dl[L_AP2L], DS(1228)) +
                   tap(&dl[L_D2L], DS(2673)) - tap(&dl[L_D1R], DS(2111)) - tap(&dl[L_AP2R], DS(335)) -
                   tap(&dl[L_D2R], DS(121)));
}

static float pv_acc, pv_al, pv_ar, pv_bl, pv_br; /* the four's sum so far; the last two out */
static uint8_t pv_ph;
static uint32_t pv_quiet;                        /* its runs silent in and out (resting past PV_REST) */
#define PV_REST 33075u                           /* 3 s of its rate */
static inline void plate_step(float in, float *ol, float *or_)   /* a sample in, a sample out, at 44.1 kHz */
{
    const float f = (float)pv_ph * 0.25f;
    pv_acc += in;
    *ol = pv_al + (pv_bl - pv_al) * f;           /* (a line from the last to the next, a run late) */
    *or_ = pv_ar + (pv_br - pv_ar) * f;
    if (++pv_ph >= 4u) {
        const float x = pv_acc * 0.25f;
        float l = 0.0f, r = 0.0f;
        pv_ph = 0;
        pv_acc = 0.0f;
        if (fm_fabsf(x) > 1e-6f || pv_quiet < PV_REST) {
            reverb(x, &l, &r);
            pv_quiet = fm_fabsf(x) > 1e-6f || fm_fabsf(l) + fm_fabsf(r) > 2e-6f ? 0u : pv_quiet + 1u;
        }
        pv_al = pv_bl;
        pv_ar = pv_br;
        pv_bl = l;
        pv_br = r;
    }
}

static void plate_init(void)
{
    pv_acc = pv_al = pv_ar = pv_bl = pv_br = 0.0f;
    pv_ph = 0;
    pv_quiet = 0;
    int16_t *p = pl_buf;
    int k, i;
    for (k = 0; k < L_N; k++) {
        dl[k].b = p;
        dl[k].n = DL_LEN[k];
        dl[k].i = 0;
        p += DL_LEN[k];
    }
    for (i = 0; i < DL_TOTAL; i++)
        pl_buf[i] = 0;
    pl_bw = pl_damp_l = pl_damp_r = 0.0f;
    lfo_s = 0.0f;
    lfo_c = 1.0f;
}

/* ------------------------------------------------- echo: tape and Echoplex --- */
/* After Discreet Music's chain: an Echoplex (a short, smearing echo, ~160 ms, fed back) and a tape loop
 * (long: up to ~2.8 s, synced to the tempo), each repeat a little darker and thinner, with a little
 * wow. The tape runs at a quarter rate (11 kHz, 16-bit, mono): it loses its highs anyway. Two heads:
 * the first plays the loop's length, the second a part of it (3/4 at first). With Motion the echo
 * evolves: each head wanders across the field, the feedback leans now on one head, now on the other
 * (the repeats fall in changing patterns, left and right), and now and then the second head slides to
 * another part of the loop, as a head moved by hand (the repeats bend a little in pitch on the way).
 * The Echoplex has a second tap, for the right. */
#define TAPE_N 31000                             /* 2.8 s at 11.025 kHz (the rest of what it had: the looper's) */
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
static uint32_t ext_t0, ext_n;
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
    case P_SWING: swing_amt = (float)v * 0.005f; break;    /* 50% straight .. 75%: up to half a step late */
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
        else if (p >= P_PLAY && p < P_TAPE)
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
    since_home = 0;
    key_moved();
}

static void regrow(void)                        /* a new piece: fresh loops, home chord, patterns home */
{
    music_reset();
    int l, v;
    __builtin_memset(phrase_pos, 0, sizeof phrase_pos);
    __builtin_memset(phrase_on, 0, sizeof phrase_on);
    for (v = 0; v < EN_NVOX; v++) {
        vc[v].rest = 0;
        if (vc[v].role == ROLE_LOOPS)
            for (l = 0; l < EN_NLOOP; l++) {
                loop_regrow(v, l);
                vc[v].loop_rest[l] = 0;
            }
    }
    deg = 0;
    since_home = 0;
    key_moved();
    drift_reset();
    change_pending = 0;
}

static void notes_off(void)
{
    int i;
    score_close(-1);                             /* (All notes off goes out past midi(): the score told here) */
    if (!par[P_MIDI])                            /* (MIDI out off: nothing goes out, this neither) */
        return;
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
    for (i = 0; i < FB_N; i++)                  /* the FM block already made: quiet too */
        for (k = 0; k < FM6_N; k++)
            fm_buf[i][k] = 0.0f;
    notes_off();
}

static float arc, arc_tgt;
static uint32_t arc_left;

void en_init(void)
{
    music_reset();
    int i, v;
    for (i = 0; i <= SN; i++)
        sint[i] = fm_sinf((float)i * (FM_TWO_PI / (float)SN));
    __builtin_memset(en_score, 0, sizeof en_score);   /* (the score and the keys' notes: none of before) */
    en_score_w = 0;
    kb_reset();
    __builtin_memset(phrase_pos, 0, sizeof phrase_pos);
    __builtin_memset(phrase_on, 0, sizeof phrase_on);
    __builtin_memset(xseq, 0, sizeof xseq);
    __builtin_memset(xrendered, 0, sizeof xrendered);
    __builtin_memset(xduration_sum, 0, sizeof xduration_sum);
    __builtin_memset(xdensity, 0, sizeof xdensity);
    __builtin_memset(xduration, 0, sizeof xduration);
    __builtin_memset(xsm, 0, sizeof xsm);        /* (interpretation: no memory of before, no morph B) */
    __builtin_memset(xhold, 0, sizeof xhold);
    __builtin_memset(xtarget, 0, sizeof xtarget);
    __builtin_memset(xshown, 0, sizeof xshown);
    __builtin_memset(xvalid, 0, sizeof xvalid);
    __builtin_memset(xvel, 0, sizeof xvel);
    __builtin_memset(xleap, 0, sizeof xleap);
    __builtin_memset((void *)en_expr_route, 0, sizeof en_expr_route);
    __builtin_memset((void *)en_expr_phase, 0, sizeof en_expr_phase);
    expr_rng = 0xA511E9B3u;
    __builtin_memset(xmem, 0, sizeof xmem);
    __builtin_memset(xpos, 0, sizeof xpos);
    __builtin_memset(xlast, 0, sizeof xlast);
    __builtin_memset(mvalid, 0, sizeof mvalid);
    __builtin_memset((void *)mstaged, 0, sizeof mstaged);
    __builtin_memset((void *)en_expression, 0, sizeof en_expression);
    for (v = 0; v < EN_NVOX; v++) {             /* (an init again: no note of before rings on) */
        __builtin_memset(&vc[v], 0, sizeof vc[v]);
        vc[v].role = (uint8_t)v;
        vc[v].fo = 1.0f;
        vc[v].glide_k = 1.0f / (0.26f * EN_SR);
    }
    rng = 0x9E3779B9u;                           /* the same garden from every init (PLAY stirs it: en_seed), so a */
    mod_rng = 0x2545F491u;                       /* test can hear one setting against another on the same notes */
    en_playing = en_frozen = en_tape_playing = 0;   /* (resting: PLAY starts it from the top) */
    en_ext = 0;                                  /* (no external clock until its pulses come) */
    ext_bpm = 0.0f;
    ext_n = 0;
    pad_gate = 0.0f;
    arc = arc_tgt = 0.0f;
    arc_left = 0;
    sixteenth = 0;
    pulse = 0;
    change_pending = 0;
    fm_pos = FM6_N;
    reel_init();
    mod_init();
    plate_init();
    for (i = 0; i < TAPE_N; i++)
        tape[i] = 0;
    for (i = 0; i < PLEX_N; i++)
        plex[i] = 0;
    tape_w = plex_w = tape_ph = 0;
    head2 = head2_t = 0.75f;
    head2_v = 0.0f;
    head_left = 0;                               /* (its next draw: now, not after whatever was left of the last) */
    echo_len = 0.0f;
    q_r = q_w = 0;
    for (i = 0; i < NMOD; i++) {
        mod[i].v = mod[i].tgt = 0.0f;
        mod[i].left = 0;
    }
    arcm = 0.0f;
    for (i = 0; i < P_NPARAMS; i++)              /* (the patches built as their parameters arrive) */
        apply(i, PARAMS[i].def);
    deg = 0;                                     /* (a new init: the home chord, whatever was playing) */
    since_home = 0;
    pool_set(EN_MODE_POOL[par[P_MODE]]);
    for (v = 0; v < EN_NVOX; v++) {             /* each voice its role, from silence */
        vc[v].role = (uint8_t)PL(v, ROLE);
        role_enter(v);
    }
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
    if(from_top) { music_tick_pos=music_ticks=0; music_running=music_active=0; }
    int i, v;
    if (from_top) {
        pulse = 0;
        sixteenth = 0;
        song_pos = song ? song_start() : 0u;
        song_mask = song_root = -1;
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
            c->mon = 1;
        }
    }
    if (!en_ext)
        midi(from_top ? 0xFA : 0xFB, 0, 0);
}

static void play_stop(void)
{
    music_running=music_active=en_phrase_active=0;
    int v, i;
    en_playing = 0;
    kb_key(7, 0, 0);                             /* (every key's note let go) */
    for (v = 0; v < EN_NVOX; v++)
        vc[v].mon = 0;                           /* (all notes off goes out below) */
    pad_gate = 0.0f;
    for (v = 0; v < EN_NVOX; v++) {             /* on the patch's envelopes (or the ADSR): every note let go, its
                                                 * release ends it (AMB's: the slow fade alone) */
        voice_t *c = &vc[v];
        if (PL(v, ENV) == ENV_AMB && !c->decays)
            continue;
        for (i = 0; i < NPOLY; i++) {
            vnote_t *n = &c->n[i];
            if (c->role == ROLE_LOOPS) {
                if (n->held > 0)                 /* (let go at its next block) */
                    n->held = 1;
            } else if (!n->up || (c->role == ROLE_BASS && i == 0)) {
                fm6_note_key(&n->fm, n->vp, 0);
                n->up = 1;
            }
        }
        if (c->role == ROLE_BASS)
            c->held = 0;
    }
    if (!en_ext)
        midi(0xFC, 0, 0);
    notes_off();
}

static void drain(void)
{
    while (q_r != q_w) {
        uint32_t c = q[q_r % QN];
        int a = (int)(c >> 16 & 0x3FFu), b = (int)(int16_t)(c & 0xFFFFu);
        BARRIER();
        q_r++;
        switch (c >> 26) {
        case C_MLOAD:
            if (a < EN_NVOX) {
                mvalid[a] = (uint8_t)(b && mstaged[a]);
                if (mvalid[a])
                    __builtin_memcpy(mpatch[a], mstage[a], EN_PATCH);
            }
            break;
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
            expr_rng ^= (uint32_t)(b & 0xFFFF) * 2246822519u;
            if (!expr_rng) expr_rng = 1u;
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
        case C_REEL:
            if (a == EN_REEL_NEW)
                reel_new();
            else if (a == EN_REEL_DUB)
                reel_dub();
            else if (a == EN_REEL_ERASE)
                reel_erase();
            else if (a == EN_REEL_TAKE)
                reel_take();
            else if (a == EN_REEL_CHOP)
                reel_chop_apply();
            break;
        case C_REGROW: regrow(); break;
        case C_TAPE: en_tape_playing = (uint8_t)(a != 0); break;
        case C_KEY: kb_key(a, b & 0x7F, b >> 8 & 1); break;
        case C_SONG: song_take(song_req); break;
        case C_DETUNE: kcents = (float)a * 0.001f; break;
        }
    }
}

#include "reel.c"

/* --------------------------------------------------------------- render --- */
#define BLK 256
static float mixl[BLK], mixr[BLK], rsend[BLK], esend[BLK];
static float vsrc[EN_NVOX][BLK];                /* each voice alone (the reel's SOURCE) */
static float rl_in[BLK], rl_l[BLK], rl_r[BLK];  /* what the reel records, what it plays */
static float dryv[BLK], dry_g = 1.0f;           /* the garden's part (MIX), ramped */

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
    const int nn = chord_n(v), amb = PL(v, ENV) == ENV_AMB && !c->decays, pub = first_role(ROLE_CHORD) == v;
    float x[FM6_N], lvl = 0.0f;
    int32_t dtb[6], olb[6];
    mod_t m;
    int i;
    uint32_t j;
    c->tg += (pad_gate - c->tg) *               /* (the patch's envelopes: in at once, they shape it) */
             fm_minf((float)FM6_N / ((pad_gate <= 0.0f ? 5.0f : amb ? 2.0f : 0.01f) * EN_SR), 1.0f);
    for (i = 0; i < NPOLY; i++) {
        vnote_t *n = &c->n[i];
        float amp = n->amp, g, gl, gr, pn, cut;
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
        cut = expr_mod(v, &n->ex, &n->fm, n->vp, olb);
        g = c->vol * 0.5f * c->gain * c->fo * m.lvl;   /* (LEVEL ramped with the place, below) */
        pn = fm_clampf(n->pan + 0.35f * motion * mod[(i + 3) % NMOD_PAD].v + m.pan, -1.0f, 1.0f);
        gl = fm_cosf((pn + 1.0f) * (FM_PI / 4.0f)) * g;
        gr = fm_sinf((pn + 1.0f) * (FM_PI / 4.0f)) * g;
        if (fm_voice(&n->fm, n->vp, n->note + 0.08f * motion * mod[i].v + m.pitch + lfo_pm(lv, m.lfopm), lv, ld,
                     n->bloom + m.bloom, m.fb, dtb, olb, x)) {
            expr_observe(v, &n->ex);
            vcf(v, n->vf, n->note, &n->fm, n->vp, cut, x);
            const float dl = (gl - n->gl) * (1.0f / FM6_N), dr = (gr - n->gr) * (1.0f / FM6_N);
            float pl = n->gl, pr = n->gr;         /* the place ramped across the block (no zipper as it moves) */
            for (j = 0; j < FM6_N; j++) {        /* a slow drift of pitch above, as tape */
                amp += (n->tgt - amp) * 0.01f;
                pl += dl;
                pr += dr;
                fm_buf[FB_PADL][j] += x[j] * amp * pl;
                fm_buf[FB_PADR][j] += x[j] * amp * pr;
                fm_vtap[v][j] += x[j] * amp * (pl + pr) * 0.5f;
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
        float cut;
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
            if (PL(v, ENV) != ENV_AMB) {         /* the patch's envelopes (or the ADSR): held GATE of a bar (a
                                                 * song's note: its own length) */
                n->held = n->slen > 0 ? n->slen : (int32_t)(GATE_NAT(v) ? step_len : step_len * 16.0f * GATE_F(v));
            } else {                             /* AMB-1's: a bell held at a sustain let go after a beat */
                uint32_t car = fm6_carriers(p[FP_ALG] & 31u), j2, sus = 0;
                for (j2 = 0; j2 < 6u; j2++)
                    sus |= (car >> j2 & 1u) && p[j2 * FP_OP + FP_L1 + 2] && p[j2 * FP_OP + FP_OL];
                n->held = sus ? (int32_t)(step_len * 4.0f) : 0;
            }
            n->held = expr_hold(&n->ex, n->held);
            n->rung = 1;
        } else if (n->held > 0) {
            n->held -= FM6_N;
            if (n->held <= 0) {
                fm6_note_key(&n->fm, p, 0);
                if (!n->moff)
                    midi(0x80u | (uint32_t)v, n->midi_note, 0);   /* (MIDI out let go with it) */
                n->moff = 1;
            }
        }
        lv = fm6_lfo_sample(&n->lfo);
        ld = fm6_lfo_delay(&n->lfo);
        mod_of(GEN(v, i), p, &m, dtb, olb);
        cut = expr_mod(v, &n->ex, &n->fm, n->vp, olb);
        if (n->fade)                             /* shed: down to silence over its blocks (no click) */
            m.lvl = n->lv * (float)(n->fade - 1) / (float)n->fade;
        if (fm_voice(&n->fm, p, (float)n->midi_note + m.pitch + lfo_pm(lv, m.lfopm), lv, ld, m.bloom, m.fb, dtb, olb,
                     x)) {
            expr_observe(v, &n->ex);
            vcf(v, n->vf, (float)n->midi_note, &n->fm, p, cut, x);
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
                    fm_vtap[v][j] += sm * (pl + pr) * 0.5f;
                }
            }
            n->pl = gl;
            n->pr = gr;
            n->lv = m.lvl;
        }
        if ((n->fade && --n->fade == 0) || fm6_note_done(&n->fm, p, p[FP_ALG] & 31u)) {   /* rung out, or shed */
            n->on = 0;
            n->fade = 0;
            midi(0x80u | (uint32_t)v, n->midi_note, 0);
        }
    }
}

static void bass_block(int v)                   /* a Bass voice's block: its note, gliding, to the mix */
{
    voice_t *c = &vc[v];
    vnote_t *n = &c->n[0];
    const int env = PL(v, ENV) != ENV_AMB || c->decays || n->ex.gate != 1.0f;   /* (the patch's envelopes, or a patch that dies away: the
                                                 * level steady, the patch shapes it, struck each chord) */
    float x[FM6_N];
    int32_t dtb[6], olb[6];
    mod_t m;
    uint32_t j;
    int waiting = 0;
    if (c->rs > 0 && --c->rs == 0)               /* a new patch, the knob at rest: out, struck, in */
        c->xs = 1;
    if (c->xs && (c->xf <= 0.0f || n->amp < 1e-4f)) {
        note_strike(v, 0, vpat[v], (int32_t)n->note, 100, 1);
        c->xs = 0;
        c->held = expr_hold(&n->ex, (int32_t)(GATE_NAT(v) ? step_len : drift_left * GATE_F(v)));
    }
    if (c->swait > 0) {                          /* Swing: the odd step arrives a little late */
        c->swait -= FM6_N;
        if (c->swait > 0)
            waiting = 1;
        else
            c->swait = 0;
    }
    if (env) {                                   /* each chord (or PACE's step) struck, let go at GATE of it */
        if (c->strike && en_playing && !waiting) {
            if (c->rest) {                       /* (sitting this chord out: let go now) */
                fm6_note_key(&n->fm, n->vp, 0);
                c->held = 0;
            } else {
                const int st = bass_step(v);
                note_strike(v, 0, vpat[v], (int32_t)n->target, 100, n->amp < 1e-4f);
                c->held = c->slen > 0 ? c->slen         /* (a song's note: its own length) */
                          : (int32_t)(GATE_NAT(v) ? step_len : (st ? (float)st * step_len : drift_total) * GATE_F(v));
                c->held = expr_hold(&n->ex, c->held);
            }
        }
        if (c->held > 0 && (c->held -= FM6_N) <= 0) {
            fm6_note_key(&n->fm, n->vp, 0);
            if (c->mon)
                midi(0x80u | (uint32_t)v, n->midi_note, 0);   /* (MIDI out let go with it) */
            c->mon = 0;
        }
    }
    if (!waiting) {
        c->strike = 0;
        c->slen = 0;
    }
    if (n->amp < 1e-5f && !en_playing) {
        n->amp = 0.0f;
    } else {
        const float g = c->vol * 0.42f * 0.75f * c->gain * c->fo,
                    tgt = en_playing && (env || !c->rest) ? 1.0f : 0.0f,
                    rate = tgt > 0.0f ? 1.0f / ((env ? 0.003f : 2.5f) * EN_SR) : 1.0f / (4.0f * EN_SR),
                    xt = c->xs ? 0.0f : 1.0f;
        float amp = n->amp, gl, gr, xf = c->xf, cut;
        int32_t lv = fm6_lfo_sample(&n->lfo), ld = fm6_lfo_delay(&n->lfo);
        n->note += (n->target - n->note) * fm_minf((float)FM6_N * fm_maxf(c->glide_k, c->sglide), 1.0f);   /* GLIDE */
        mod_of(GEN(v, 0), n->vp, &m, dtb, olb);
        cut = expr_mod(v, &n->ex, &n->fm, n->vp, olb);
        gl = (1.0f - fm_maxf(m.pan, 0.0f) * 0.5f) * m.lvl;   /* SPACE: the bass leans a little, never far */
        gr = (1.0f + fm_minf(m.pan, 0.0f) * 0.5f) * m.lvl;
        if (fm_voice(&n->fm, n->vp, n->note + m.pitch + lfo_pm(lv, m.lfopm), lv, ld, m.bloom, m.fb, dtb, olb, x)) {
            expr_observe(v, &n->ex);
            vcf(v, n->vf, n->note, &n->fm, n->vp, cut, x);
            const float dl = (gl - n->gl) * (1.0f / FM6_N), dr = (gr - n->gr) * (1.0f / FM6_N);
            float pl = n->gl, pr = n->gr;
            for (j = 0; j < FM6_N; j++) {
                amp += (tgt - amp) * rate;
                xf = fm_clampf(xf + (xt > xf ? 1.0f : -1.0f) * (1.0f / 441.0f), 0.0f, 1.0f);   /* 10 ms */
                pl += dl;
                pr += dr;
                fm_buf[FB_BASSL][j] += x[j] * amp * xf * g * pl;
                fm_buf[FB_BASSR][j] += x[j] * amp * xf * g * pr;
                fm_vtap[v][j] += x[j] * amp * xf * g * (pl + pr) * 0.5f;
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
/* the keys on a voice's page: KB_N notes apart from the garden, each its voice's patch as it is now (ADSR'd when the
 * voice plays the ADSR), at its octave, its Level, its Index; held while the key is, then let go as the patch lets go;
 * to the bells' bus (its echo), the voice's tap (its scope, its meter) and the score */
#define KB_N 6                                  /* (Play All: two keys, three voices each) */
static struct {
    fm6_note_t fm;
    fm6_lfo_t lfo;
    expr_note_t ex;
    uint8_t vp[FP_SIZE + 1];
    uint8_t on, voice, note, key;               /* key: the key's note as pressed (released by it) */
    uint16_t rel;                               /* blocks since it was let go (1.5.17: at most 6 s, then gone) */
    float vf[4];                                /* its filter's (vcf) */
} kb[KB_N] EN_POOL;
#define KB_REL_FREE (3 * EN_SR / FM6_N)        /* its own release, 3 s; then a fade of 3 s more */
static void kb_reset(void) { __builtin_memset(kb, 0, sizeof kb); }
static void kb_key(int v, int note, int on)
{
    int i, k = -1;
    if (v == 7) {                                /* (en_keys_off: every key let go, its MIDI note too) */
        for (i = 0; i < KB_N; i++)
            if (kb[i].on == 1) {
                fm6_note_key(&kb[i].fm, kb[i].vp, 0);
                kb[i].on = 2;
                kb[i].rel = 0;
                midi(0x80u | (uint32_t)kb[i].voice, kb[i].note, 0);
            }
        return;
    }
    if ((unsigned)v >= EN_NVOX)
        return;
    if (!on) {                                   /* let go: the note of this key, this voice */
        for (i = 0; i < KB_N; i++)
            if (kb[i].on == 1 && kb[i].voice == v && kb[i].key == note) {
                fm6_note_key(&kb[i].fm, kb[i].vp, 0);
                kb[i].on = 2;
                kb[i].rel = 0;
                midi(0x80u | (uint32_t)v, kb[i].note, 0);
            }
        return;
    }
    for (i = 0; i < KB_N && k < 0; i++)          /* a free one, else one letting go, else the first */
        if (!kb[i].on)
            k = i;
    for (i = 0; i < KB_N && k < 0; i++)
        if (kb[i].on == 2)
            k = i;
    if (k < 0)
        k = 0;
    if (kb[k].on == 1)
        midi(0x80u | (uint32_t)kb[k].voice, kb[k].note, 0);
    kb[k].voice = (uint8_t)v;
    kb[k].key = (uint8_t)note;
    {
        const int n = note + 12 * PL(v, OCT);
        kb[k].note = (uint8_t)(n < 0 ? 0 : n > 127 ? 127 : n);
    }
    mod_patch(VOX_GEN0[v], vpat[v], kb[k].vp);  /* (1.5.11: MOD's DECAY and an operator's envelope, as the voice's) */
    if (PL(v, ENV) == ENV_ADSR)
        adsr_patch(v, kb[k].vp);
    expr_strike(v, -1, &kb[k].ex, kb[k].vp, kb[k].note, 100);
    fm_note_on(&kb[k].fm, kb[k].vp, kb[k].note, 100, 1);
    fm6_lfo_reset(&kb[k].lfo, kb[k].vp);
    fm6_lfo_key(&kb[k].lfo);
    kb[k].on = 1;
    midi(0x90u | (uint32_t)v, kb[k].note, 100);
}
static void kb_block(void)                       /* (1.5.11: each key moved by its voice's modulators, as the voice's
                                                 * first note is: Timbre, Pitch, Metal, Feedback, Space, Level, LFO PM,
                                                 * an operator's own) */
{
    float x[FM6_N];
    int i;
    uint32_t j;
    for (i = 0; i < KB_N; i++) {
        const uint8_t *p = kb[i].vp;
        const int v = kb[i].voice;
        int32_t lv, ld;
        float cut;
        if (!kb[i].on)
            continue;
        mod_t m;
        int32_t dtb[6], olb[6];
        lv = fm6_lfo_sample(&kb[i].lfo);
        ld = fm6_lfo_delay(&kb[i].lfo);
        mod_of(VOX_GEN0[v], p, &m, dtb, olb);
        cut = expr_mod(v, &kb[i].ex, &kb[i].fm, kb[i].vp, olb);
        if (fm_voice(&kb[i].fm, p, (float)kb[i].note + m.pitch + lfo_pm(lv, m.lfopm), lv, ld, m.bloom, m.fb, dtb, olb, x)) {
            expr_observe(v, &kb[i].ex);
            vcf(v, kb[i].vf, (float)kb[i].note, &kb[i].fm, p, cut, x);
            const float rf = kb[i].on == 2 && kb[i].rel > KB_REL_FREE   /* (let go 3 s ago: fading, 3 s) */
                                 ? 1.0f - (float)(kb[i].rel - KB_REL_FREE) / (float)KB_REL_FREE : 1.0f,
                        g = vc[v].vol * vc[v].gain * 0.45f * m.lvl * fm_maxf(rf, 0.0f),
                        a = (fm_clampf(m.pan, -1.0f, 1.0f) + 1.0f) * (FM_PI / 4.0f), gl = fm_cosf(a), gr = fm_sinf(a);
            for (j = 0; j < FM6_N; j++) {
                const float sm = x[j] * g;
                fm_buf[FB_BELLL][j] += sm * gl;
                fm_buf[FB_BELLR][j] += sm * gr;
                fm_buf[FB_BELLE][j] += sm * 0.5f;
                fm_vtap[v][j] += sm;
            }
        }
        if (kb[i].on == 2 && (fm6_note_done(&kb[i].fm, p, p[FP_ALG] & 31u) || ++kb[i].rel >= 2 * KB_REL_FREE))
            kb[i].on = 0;                        /* (done, or 6 s let go: gone, whatever its envelope holds) */
    }
}

static void fm_block(void)
{
    expr_observation_block();
    int v, i;
    uint32_t j;
    for (i = 0; i < FB_N; i++)
        for (j = 0; j < FM6_N; j++)
            fm_buf[i][j] = 0.0f;
    for (v = 0; v < EN_NVOX; v++)
        for (j = 0; j < FM6_N; j++)
            fm_vtap[v][j] = 0.0f;
    mod_step();
    if (first_role(ROLE_CHORD) < 0)
        en_pad_level = 0.0f;
    if (first_role(ROLE_BASS) < 0)
        en_bass_level = 0.0f;
    for (v = 0; v < EN_NVOX; v++) {
        voice_t *c = &vc[v];
        const int want = PL(v, ROLE);
        expr_block(v);
        if (want != c->role) {
            c->fo -= (float)FM6_N / (0.02f * EN_SR);
            if (c->fo <= 0.0f) {
                c->fo = 0.0f;
                c->role = (uint8_t)want;
                score_close(v);                  /* (its notes as they were: gone, unsaid) */
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
    kb_block();
}

/* the scope: armed, it waits for the voice to rise through zero (or 4096 samples: silence, a drone), takes every 2nd
 * sample, 128, scales them to their peak, and rests 1200 samples (the screen draws 20 a second) */
volatile int8_t en_scope_v = -1;
volatile uint32_t en_scope_seq;
int8_t en_scope[EN_SCOPE_N];
static float sc_buf[EN_SCOPE_N], sc_last;
static int sc_n = -1, sc_wait;
static void scope_take(float x)
{
    if (sc_n < 0) {
        if (sc_wait > 0) {
            sc_wait--;
        } else if ((sc_last <= 0.0f && x > 0.0f) || sc_wait < -4096) {
            sc_n = 0;
        } else {
            sc_wait--;
        }
        sc_last = x;
        if (sc_n < 0)
            return;
    }
    if (sc_wait++ & 1)
        return;
    sc_buf[sc_n++] = x;
    if (sc_n >= EN_SCOPE_N) {
        float pk = 1e-4f, k;
        int i;
        for (i = 0; i < EN_SCOPE_N; i++)
            pk = fm_maxf(pk, fm_fabsf(sc_buf[i]));
        k = pk < 0.002f ? 127.0f / 0.002f : 120.0f / pk;   /* (silence stays flat) */
        for (i = 0; i < EN_SCOPE_N; i++)
            en_scope[i] = (int8_t)fm_clampf(sc_buf[i] * k, -127.0f, 127.0f);
        en_scope_seq++;
        sc_n = -1;
        sc_wait = 1200;
    }
}

/* this chunk's n samples of the FM6 voices, a block made whenever the last is used up: the pad to its bus, the
 * bells and the bass to the mix and the sends */
volatile float en_vox_level[EN_NVOX], en_master_level;
static void fm_pull(uint32_t n)
{
    uint32_t j;
    float vpk[EN_NVOX] = {0.0f, 0.0f, 0.0f};
    int v;
    for (j = 0; j < n; j++) {
        float b, bl, br;
        if (fm_pos >= FM6_N) {
            fm_block();
            fm_pos = 0;
        }
        padl[j] = fm_buf[FB_PADL][fm_pos];
        padr[j] = fm_buf[FB_PADR][fm_pos];
        vsrc[0][j] = fm_vtap[0][fm_pos];
        vsrc[1][j] = fm_vtap[1][fm_pos];
        vsrc[2][j] = fm_vtap[2][fm_pos];
        vpk[0] = fm_maxf(vpk[0], fm_fabsf(vsrc[0][j]));
        vpk[1] = fm_maxf(vpk[1], fm_fabsf(vsrc[1][j]));
        vpk[2] = fm_maxf(vpk[2], fm_fabsf(vsrc[2][j]));
        if ((unsigned)en_scope_v < EN_NVOX)
            scope_take(vsrc[en_scope_v][j]);
        bl = fm_buf[FB_BASSL][fm_pos];
        br = fm_buf[FB_BASSR][fm_pos];
        b = 0.5f * (bl + br);
        mixl[j] += fm_buf[FB_BELLL][fm_pos] + bl;
        mixr[j] += fm_buf[FB_BELLR][fm_pos] + br;
        rsend[j] += fm_buf[FB_BELLE][fm_pos] * 0.7f + b * 0.08f;
        esend[j] += fm_buf[FB_BELLE][fm_pos] * 0.85f;
        fm_pos++;
    }
    {                                            /* (up at once, down in about a quarter of a second, whatever n) */
        const float fall = fm_expf(-(float)n * (1.0f / (0.25f * EN_SR)));
        for (v = 0; v < EN_NVOX; v++)
            en_vox_level[v] = fm_maxf(vpk[v], en_vox_level[v] * fall);
    }
}

/* the engine's ear (amb.h): the mix a sample in four (11 kHz), its bands and its last samples, for the screen */
volatile float en_bands[EN_NBAND];
volatile int8_t en_wave[EN_WAVE_N];
volatile uint32_t en_wave_w;
static float ear_lp[5], ear_env[EN_NBAND], ear_acc;
static uint8_t ear_ph;
static inline void ear(float m)
{
    static const float K[5] = {0.0819f, 0.2040f, 0.4337f, 0.7608f, 0.9423f};   /* 1 - e^(-2 pi fc / 11025) */
    float b[EN_NBAND];
    int k;
    ear_acc += m;
    if (++ear_ph < 4u)
        return;
    ear_ph = 0;
    m = ear_acc * 0.25f;
    ear_acc = 0.0f;
    for (k = 0; k < 5; k++)
        ear_lp[k] += K[k] * (m - ear_lp[k]);
    b[0] = ear_lp[0];
    for (k = 1; k < 5; k++)
        b[k] = ear_lp[k] - ear_lp[k - 1];
    b[5] = m - ear_lp[4];
    for (k = 0; k < EN_NBAND; k++) {
        const float a = fm_fabsf(b[k]);
        ear_env[k] = a > ear_env[k] ? ear_env[k] + (a - ear_env[k]) * 0.3f : ear_env[k] * 0.9995f;
        en_bands[k] = ear_env[k];
    }
    en_wave[en_wave_w % EN_WAVE_N] = (int8_t)fm_clampf(m * 160.0f, -127.0f, 127.0f);
    en_wave_w++;
}

static const int16_t *usb_p;                    /* what plays in over USB, this render's (en_usb) */
static uint32_t usb_n, usb_i;
void en_usb(const int16_t *mono, uint32_t n)
{
    usb_p = mono;
    usb_n = mono ? n : 0u;
    usb_i = 0;
}
int (*en_job_run)(void (*fn)(uint32_t), uint32_t arg);
int (*en_job_wait)(void);
/* the space (1.5.22): the echoes and the plate run a chunk behind, on the second core while this one makes the next
 * chunk's voices: the last chunk's sends in (sp_ein, sp_rin, its REVERB send sp_rs), their wet sound out into a ring
 * that keeps SP_LAT samples ahead of the mix (BLK: a chunk is never longer). The wet path is SP_LAT samples late (5.8 ms,
 * as a plate's pre-delay); the dry sound is not. One core: the same, in turn */
#define SP_LAT BLK
#define SP_RING 512u
static float sp_ein[BLK], sp_rin[BLK], sp_rs;
static uint32_t sp_n;
static float sp_wl[SP_RING], sp_wr[SP_RING];
static uint32_t sp_w = SP_LAT, sp_r;
static void space_run(void)
{
    uint32_t j;
    for (j = 0; j < sp_n; j++) {
        float wl, wr, el, er, xl, xr;
        echoes(sp_ein[j], &el, &er, &xl, &xr);
        plate_step((sp_rin[j] + (el + er + xl + xr) * 0.35f * sp_rs) * 0.8f, &wl, &wr);
        sp_wl[sp_w & (SP_RING - 1u)] = 0.75f * el + 0.3f * xl + 1.3f * wl;
        sp_wr[sp_w & (SP_RING - 1u)] = 0.75f * er + 0.3f * xr + 1.3f * wr;
        sp_w++;
    }
    sp_n = 0;
}
static void job_render(uint32_t n)               /* (on the second core: the reel's and the space's own state only) */
{
    reel_play(rl_l, rl_r, n);
    space_run();
}
static void render_chunk(int32_t *out, uint32_t n, float gain)
{
    uint32_t j;
    float mpk = 0.0f;
    const float rg = rev_send * rev_in_gain * (1.0f - 0.25f * arcm), eg = echo_send;
    const int sp = par[P_RL(SRC)], src = sp == 5 ? -1 : sp < 4 ? sp : sp >= 6 && sp <= 8 ? sp - 5 : sp == 9 ? 4 : 0,
              fx_after = par[P_RL(FX)] != 0;   /* (Play 1 .. 3: that voice, as the keys play it too; Play All: the three) */
    /* (File records the mix; USB, -1, what plays in over USB) */
    const float dry_t = fm_minf((float)(100 - par[P_RL(MIX)]) * 0.02f, 1.0f);   /* (the garden full to the middle) */
    for (j = 0; j < n; j++)
        mixl[j] = mixr[j] = rsend[j] = esend[j] = 0.0f;
    {   /* (1.5.22) the reel's genes on the second core while this one makes the voices; given up: here */
        const int two = en_job_run && en_job_run(job_render, n);
        fm_pull(n);
        pad_bus(n);
        if (!two || !en_job_wait || en_job_wait() < 0)
            job_render(n);
    }
    /* the reel and the garden: MIX (dry/wet), and FX: the space before the reel (the garden through it, the reel as it
     * was recorded, dry) or after it (the garden and the reel both into it). SOURCE: the whole mix (its space too
     * before, without after; the reel's own sound left out), one voice alone or the pulse, as they are made */
    for (j = 0; j < n; j++) {
        dry_g += (dry_t - dry_g) * 0.002f;
        if (fx_after) {
            const float m = (rl_l[j] + rl_r[j]) * (1.0f / 0.36f);
            if (src == 0)
                rl_in[j] = 0.5f * (mixl[j] + mixr[j]) * 0.36f;
            mixl[j] = mixl[j] * dry_g + rl_l[j] * (1.0f / 0.36f);
            mixr[j] = mixr[j] * dry_g + rl_r[j] * (1.0f / 0.36f);
            rsend[j] = rsend[j] * dry_g + m * 0.45f;
            esend[j] = esend[j] * dry_g + m * 0.3f;
        }
        dryv[j] = dry_g;
    }
    for (j = 0; j < n; j++) {
        const float wl = sp_wl[sp_r & (SP_RING - 1u)], wr = sp_wr[sp_r & (SP_RING - 1u)];
        float l, r;
        sp_r++;
        sp_ein[j] = esend[j] * eg;               /* (into the space, for the next chunk's job) */
        sp_rin[j] = rsend[j] * rg;
        l = (mixl[j] + wl) * 0.36f;
        r = (mixr[j] + wr) * 0.36f;
        if (src == 4)                            /* Play All: the three voices, layered */
            rl_in[j] = (vsrc[0][j] + vsrc[1][j] + vsrc[2][j]) * 0.24f;
        else if (src > 0)
            rl_in[j] = vsrc[src - 1][j] * 0.36f;
        else if (src < 0)                        /* USB: a phone's, a computer's, as it comes */
            rl_in[j] = usb_i < usb_n ? (float)usb_p[usb_i++] * (0.9f / 32768.0f) : 0.0f;
        if (!fx_after) {                         /* (before: the garden with its space, then the reel beside it) */
            if (src == 0)
                rl_in[j] = 0.5f * (l + r);
            l = l * dryv[j] + rl_l[j];
            r = r * dryv[j] + rl_r[j];
        }
        ear(0.5f * (l + r));                     /* (the screen's ear) */
        out[2 * j] = (int32_t)(fm_tanhf(l * gain) * 8300000.0f);
        out[2 * j + 1] = (int32_t)(fm_tanhf(r * gain) * 8300000.0f);
        mpk = fm_maxf(mpk, fm_maxf(fm_fabsf(l), fm_fabsf(r)));   /* (the mix, before the volume) */
    }
    sp_n = n;
    sp_rs = rev_send;
    en_master_level = fm_maxf(mpk, en_master_level * fm_expf(-(float)n * (1.0f / (0.25f * EN_SR))));
    reel_record(rl_in, n);
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
    if (en_playing && !en_frozen && !change_pending && !song) {
        drift_left -= (float)n;
        if (!fifth_done && drift_left < 0.5f * drift_total) {
            fifth_done = 1;                      /* halfway: a bass may lean on the fifth (its DENSITY) */
            for (i = 0; i < EN_NVOX; i++)
                if (vc[i].role == ROLE_BASS && frand() < (float)xdens(i) * 0.01f && chord_nt >= 3)
                    bass_to(i, chord_pcs[2]);
        }
        if (drift_left <= 0.0f)
            change_pending = 1;
    }
    en_drift = drift_total > 0.0f ? 1.0f - fm_clampf(drift_left / drift_total, 0.0f, 1.0f) : 0.0f;
    for (i = 0; i < EN_NLOOP; i++)
        en_loop_glow[i] *= 0.985f;
}

/* overloads: counted in the audio interrupt, acted on at the next render's start (the same context) */
static volatile uint8_t ov_now;
static uint8_t ov_run;
static uint32_t shed_t;                         /* (the last shed) */
volatile uint32_t en_sheds;
void en_overload(void) { ov_now = 1; }          /* (this render ran over: the next sees it) */
static int reel_shed(void);
static void shed(void)                          /* the cheapest to lose: a gene of the reel, else the softest bell as
                                                 * it sounds now; faded out over 6 blocks (4 ms), not cut */
{
    int v, i, bv = -1, bi = 0;
    float least = 1e9f;
    en_sheds++;
    if (reel_shed())
        return;
    for (v = 0; v < EN_NVOX; v++)
        for (i = 0; i < NPOLY; i++) {
            vnote_t *n = &vc[v].n[i];
            float a;
            if (vc[v].role != ROLE_LOOPS || !n->on || !n->rung || n->fade)
                continue;
            a = fm_gain(&n->fm, n->vp) * n->vel * n->out;
            if (a < least) {
                least = a;
                bv = v;
                bi = i;
            }
        }
    if (bv >= 0)
        vc[bv].n[bi].fade = 6;
}

void en_render(int32_t *out, uint32_t n, uint32_t gain_q12)
{
    const float gain = (float)gain_q12 * (1.0f / 4096.0f) * 2.0f;
    if (ov_now) {                                /* two renders in a row over 90 %: one shed, at most each 1/4 s */
        ov_now = 0;
        if (++ov_run >= 2u && smp_clock - shed_t >= (uint32_t)(EN_SR / 4.0f)) {
            ov_run = 0;
            shed_t = smp_clock;
            shed();
        }
    } else {
        ov_run = 0;
    }
    drain();
    if (en_ext && ++ext_age > 100u) {           /* the clock gone: the Tempo knob's again */
        en_ext = 0;
        if (ext_bpm > 0.0f) {
            ext_bpm = 0.0f;
            tempo_changed();
        }
    }
    smp_clock += n;
    en_now = smp_clock;
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
        if (timed)
            pulse_left -= (float)k;
        control(k);
        render_chunk(out, k, gain);
        out += 2 * k;
        n -= k;
    }
}

uint32_t en_alg_carriers(int alg) { return fm6_carriers((uint32_t)alg & 31u); }   /* (Tom: who is heard) */
