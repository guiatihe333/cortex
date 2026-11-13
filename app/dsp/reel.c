/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal */
/* AMB-1 FM6's reel: one track of tape, 11.8 s, after a Tascam portastudio's tape and Make Noise's Morphagene's genes
 * (the idea, not their code). Part of the engine (amb.c includes it, after the echo).
 *
 * The tape is mono, 22.05 kHz, IMA ADPCM (4 bits a sample: 11.8 s in 130 KB of the pool), in blocks of 256 samples,
 * each with the coder's state at its start, so any block can be read on its own (a gene starts anywhere, goes either
 * way). What is recorded is SOURCE's: the whole mix (the tape's own sound left out) or one voice (before the space) or
 * the pulse, a little saturated, as onto tape. Recording makes SPLICES (up to 8, one after another on the reel): a
 * new one at the end (EDIT on the tape's pages), or the one ORGANIZE picks overdubbed, looping (REC), sound on sound
 * (SOS: how much of what was there stays, 0 replaced .. 100 all of it, the new on top). EDIT held on the tape's pages
 * cuts the splice ORGANIZE picks, in its place, at its attacks (the auto-chop: up to eight splices, the audio left as
 * it is, and four alike if it has no clear attack).
 *
 * The reel plays the splice ORGANIZE picks, as genes (grains): each a window of the splice (GENE: the whole splice ..
 * 1/256 of it, 20 ms at least), started where the tape's cursor is, past it by SLIDE, at SPEED (-2x .. stopped ..
 * +2x: pitch and time together, as tape; 1x and -1x stick), with the tape's wow and flutter. MORPH sets how they
 * follow: gaps between them (0 .. 40), one after another, joined (40 .. 55), overlapping two, three, four (55 .. 100),
 * spread across the field (past 75) and an octave up or down now and then (past 85). Recording and playing happen at
 * once. The reel's sound goes to the mix and a little to the space. */
#define RL_BLK 256
#define RL_NBLK 1016
#define RL_N (RL_BLK * RL_NBLK)                 /* 260096 samples: 11.8 s */
_Static_assert(RL_N == EN_REEL_N, "amb.h's reel length");
#define RL_SR 22050.0f
#define RL_NSPL 8
#define RL_NG 4                                  /* genes sounding at once */
#define RL_MAGIC 0x4C45455Eu                     /* "^EEL" */
#define RL_MIN 2205u                             /* a splice shorter (0.1 s) is too short to play: dropped */
static uint8_t rl_nib[RL_N / 2] EN_POOL;         /* two samples a byte, the first in the low nibble */
typedef struct {
    int16_t pred;
    uint8_t idx, pad;
} rl_hdr_t;
static rl_hdr_t rl_hdr[RL_NBLK] EN_POOL;
typedef struct {
    uint32_t magic, len, nspl, ver;              /* len: how far the reel is recorded (samples) */
    uint32_t s0[RL_NSPL], s1[RL_NSPL];           /* each splice: its first sample (a block's), one past its last */
    uint8_t kok[RL_NSPL], kman[RL_NSPL];         /* (1.4.13) each splice's key: studied (RL_KOK), set by hand (1..24) */
    uint8_t chroma[RL_NSPL][12];                 /* its 12 pitch classes, studied (0..255) */
} rl_meta_t;
static rl_meta_t rl_meta;

volatile uint8_t en_reel_state, en_reel_nspl;    /* 0 resting, 1 recording a new splice, 2 overdubbing */
volatile uint32_t en_reel_len, en_reel_w;        /* samples recorded; where it records */
volatile float en_reel_pos;                      /* the cursor in the splice playing, 0..1 */
volatile uint32_t en_reel_s0[8], en_reel_s1[8];
volatile int32_t en_reel_gene[4] = {-1, -1, -1, -1};
volatile float en_reel_level, en_reel_mod[EN_NTMOD];
volatile float en_reel_rate;
volatile int8_t en_reel_spl;

/* ---------------------------------------------------------------- the coder --- */
static const int16_t RL_STEP[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107,
    118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876,
    963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
    5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086,
    29794, 32767};
static const int8_t RL_IDX[16] = {-1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8};
static inline void rl_step(int *pred, int *idx, int n)   /* a nibble decoded: the state moved */
{
    int step = RL_STEP[*idx], d = step >> 3, p;
    if (n & 4)
        d += step;
    if (n & 2)
        d += step >> 1;
    if (n & 1)
        d += step >> 2;
    p = (n & 8) ? *pred - d : *pred + d;
    *pred = p > 32767 ? 32767 : p < -32768 ? -32768 : p;
    p = *idx + RL_IDX[n];
    *idx = p < 0 ? 0 : p > 88 ? 88 : p;
}
static inline int rl_code(int *pred, int *idx, int x)   /* a sample coded: its nibble, the state moved */
{
    int step = RL_STEP[*idx], d = x - *pred, n = 0;
    if (d < 0) {
        n = 8;
        d = -d;
    }
    if (d >= step) {
        n |= 4;
        d -= step;
    }
    step >>= 1;
    if (d >= step) {
        n |= 2;
        d -= step;
    }
    step >>= 1;
    if (d >= step)
        n |= 1;
    rl_step(pred, idx, n);
    return n;
}
static inline int rl_nibble(uint32_t i) { return i & 1u ? rl_nib[i >> 1] >> 4 : rl_nib[i >> 1] & 15; }
static void rl_decode(uint32_t b, int16_t *out)   /* block b whole */
{
    int pred = rl_hdr[b].pred, idx = rl_hdr[b].idx, k;
    uint32_t i = b * RL_BLK;
    for (k = 0; k < RL_BLK; k++) {
        rl_step(&pred, &idx, rl_nibble(i + (uint32_t)k));
        out[k] = (int16_t)pred;
    }
}

static float rl_pf(int k);
static int rl_pi(int k);

/* ------------------------------------------------------------- recording --- */
static uint8_t rl_rec, rl_stopping;              /* rl_rec as en_reel_state; stopping: the block finished first */
static uint32_t rl_w;                            /* where it records */
static int rl_spl;                               /* the splice recorded */
static int rl_ep, rl_ei;                         /* the coder's state */
static int16_t rl_old[RL_BLK];                   /* the block being written, as it was (sound on sound) */
static int16_t rl_new[RL_BLK];                   /* and as it is written so far (what the genes read of it) */
static float rl_d1, rl_d2;                       /* (format-free: the old decimator's state, kept for its reset) */
/* (1.5.22) the 2:1 decimator onto the tape: a 31-tap halfband (sinc under a Blackman window), not [1 2 1] / 4, which
 * let what is over 11 kHz fold back 13 dB down (bright FM: a hiss-like grit at 7 .. 11 kHz). A model on a recording
 * of the FM-1: its error against an ideal low-pass -29.5 dB -> -62.7 dB */
#define RL_HB 31
static const float RL_HBK[RL_HB] = {0.000000000f, 0.000000000f, 0.000410323f, -0.000000000f, -0.002230286f, 0.000000000f, 0.007100857f, -0.000000000f, -0.017917030f, 0.000000000f, 0.040107418f, -0.000000000f, -0.090106922f, 0.000000000f, 0.312633322f, 0.500004637f, 0.312633322f, 0.000000000f, -0.090106922f, -0.000000000f, 0.040107418f, 0.000000000f, -0.017917030f, -0.000000000f, 0.007100857f, 0.000000000f, -0.002230286f, -0.000000000f, 0.000410323f, 0.000000000f, 0.000000000f};
static float rl_hb[RL_HB];
static uint32_t rl_hbw;
/* (1.5.22) noise shaping: the coder's last error fed back (its noise 1 + 0.5 z^-1: out of the top, where it is heard
 * as hiss, into the lows, where the music covers it): 6 .. 11 kHz -48.1 -> -50.8 dB (the decoder is the same) */
static float rl_ns;
static uint8_t rl_half;

static void rl_publish(void)
{
    int k;
    for (k = 0; k < RL_NSPL; k++) {
        en_reel_s0[k] = k < (int)rl_meta.nspl ? rl_meta.s0[k] : 0u;
        en_reel_s1[k] = k < (int)rl_meta.nspl ? rl_meta.s1[k] : 0u;
    }
    en_reel_state = rl_rec;
    en_reel_nspl = (uint8_t)rl_meta.nspl;
    en_reel_len = rl_meta.len;
    en_reel_w = rl_w;
}
static void rl_block_start(void)                 /* rl_w at a block's start: what it held, the coder's state kept */
{
    uint32_t b = rl_w / RL_BLK;
    if (rl_w < rl_meta.len)
        rl_decode(b, rl_old);
    else
        __builtin_memset(rl_old, 0, sizeof rl_old);
    rl_hdr[b].pred = (int16_t)rl_ep;
    rl_hdr[b].idx = (uint8_t)rl_ei;
}
static void rl_stop_now(void)
{
    if (rl_rec == 1 && rl_meta.nspl && rl_meta.s1[rl_spl] - rl_meta.s0[rl_spl] < RL_MIN &&
        rl_spl == (int)rl_meta.nspl - 1) {      /* a new splice too short to play: gone, the reel as it was */
        rl_meta.len = rl_meta.s0[rl_spl];
        rl_meta.nspl--;
    }
    rl_rec = 0;
    rl_stopping = 0;
    rl_publish();
}
static void rl_put(float x);
static void reel_new(void)                       /* a new splice at the reel's end */
{
    uint32_t s;
    if (rl_rec == 2) {                           /* overdubbing: its block finished as it was (stopped halfway, the rest
                                                  * of it would decode from the coder's new state: noise) */
        rl_stopping = 1;
        while (rl_rec)
            rl_put(0.0f);
    } else if (rl_rec) {
        rl_stop_now();
    }
    s = (rl_meta.len + RL_BLK - 1u) / RL_BLK * RL_BLK;
    if (rl_meta.nspl >= RL_NSPL || s + RL_BLK * 8u > RL_N)
        return;                                  /* (full: the UI says so) */
    rl_spl = (int)rl_meta.nspl;
    rl_meta.s0[rl_spl] = rl_meta.s1[rl_spl] = s;
    rl_meta.kok[rl_spl] = rl_meta.kman[rl_spl] = 0;   /* (its key studied once it is recorded) */
    rl_meta.nspl++;
    rl_w = s;
    rl_ep = 0;
    rl_ei = 0;
    rl_ns = 0.0f;
    rl_rec = 1;
    rl_block_start();
    rl_publish();
}
static void reel_dub(void)                       /* the splice picked, overdubbed, looping; again: stop */
{
    int k = rl_pi(TP_ORG);
    if (rl_rec) {
        if (rl_rec == 2)
            rl_stopping = 1;                     /* (to the block's end, as it was) */
        else
            rl_stop_now();
        return;
    }
    if (!rl_meta.nspl) {
        reel_new();
        return;
    }
    rl_spl = k < (int)rl_meta.nspl ? k : (int)rl_meta.nspl - 1;
    rl_meta.kok[rl_spl] = 0;                     /* (studied again after) */
    rl_w = rl_meta.s0[rl_spl];
    rl_ep = rl_hdr[rl_w / RL_BLK].pred;
    rl_ei = rl_hdr[rl_w / RL_BLK].idx;
    rl_ns = 0.0f;
    rl_rec = 2;
    rl_block_start();
    rl_publish();
}
static void reel_erase(void)
{
    rl_rec = rl_stopping = 0;
    rl_meta.len = rl_meta.nspl = 0;
    rl_w = 0;
    rl_publish();
}

static float rl_drive(float x)                   /* DRIVE: onto tape, saturating sooner (its small gain kept, 1.1) */
{
    const float dr = rl_pf(TP_DRIVE), k = 1.1f + 0.04f * dr;
    return fm_tanhf(x * k) * (1.1f / k) * (1.0f + 0.01f * dr);
}

/* one sample at 22.05 kHz onto the reel */
static void rl_put(float x)
{
    const uint32_t k = rl_w % RL_BLK;
    float o = (float)rl_old[k] * (1.0f / 16384.0f), y;
    int q;
    if (rl_stopping)
        y = o;                                   /* (finishing the block as it was) */
    else if (rl_rec == 2)
        y = rl_drive(x) + o * rl_pf(TP_SOS) * 0.01f;
    else
        y = rl_drive(x);
    {
        const float t = fm_clampf(y, -1.99f, 1.99f) * 16384.0f + 0.5f * rl_ns;   /* (shaped) */
        q = rl_code(&rl_ep, &rl_ei, (int)fm_clampf(t, -32768.0f, 32767.0f));
        rl_ns = fm_clampf((float)rl_ep - t, -4000.0f, 4000.0f);
    }
    rl_new[k] = (int16_t)rl_ep;
    if (rl_w & 1u)
        rl_nib[rl_w >> 1] = (uint8_t)((rl_nib[rl_w >> 1] & 15) | q << 4);
    else
        rl_nib[rl_w >> 1] = (uint8_t)((rl_nib[rl_w >> 1] & 0xF0) | q);
    rl_w++;
    if (rl_rec == 1) {                           /* a new splice: as far as it has gone */
        rl_meta.s1[rl_spl] = rl_w;
        if (rl_w > rl_meta.len)
            rl_meta.len = rl_w;
        if (rl_w >= RL_N) {
            rl_stop_now();
            return;
        }
    } else if (rl_w >= rl_meta.s1[rl_spl]) {     /* overdubbing: round the splice again */
        rl_w = rl_meta.s0[rl_spl];
        if (rl_stopping) {
            rl_stop_now();
            return;
        }
    }
    if (rl_w % RL_BLK == 0u) {
        if (rl_stopping) {
            rl_stop_now();
            return;
        }
        rl_block_start();
    }
}

/* n samples at 44.1 kHz of SOURCE's, to the reel (2:1, through the halfband) */
static void reel_record(const float *in, uint32_t n)
{
    uint32_t j;
    if (!rl_rec)
        return;
    for (j = 0; j < n && rl_rec; j++) {
        const float x = in[j];
        rl_hb[rl_hbw] = x;
        rl_hbw = rl_hbw + 1u >= RL_HB ? 0u : rl_hbw + 1u;
        if (rl_half) {                           /* (the halfband: its every other tap is 0, skipped) */
            float acc = RL_HBK[(RL_HB - 1) / 2] * rl_hb[(rl_hbw + (RL_HB - 1) / 2) % RL_HB];
            uint32_t k;
            for (k = 0; k < RL_HB; k += 2)
                acc += RL_HBK[k] * rl_hb[(rl_hbw + k) % RL_HB];
            rl_put(acc);
        }
        rl_d2 = rl_d1;
        rl_d1 = x;
        rl_half ^= 1u;
    }
    rl_publish();
}

/* ----------------------------------------------------------------- genes --- */
typedef struct {
    uint8_t on, dir;                             /* dir: 1 forward, 0 back */
    float pos, inc;                              /* where it reads (samples, 22.05 kHz), its step a 44.1 kHz sample */
    float life, len;                             /* how far it has gone, how far it goes (samples of tape) */
    float edge;                                  /* its window's fade, a fraction of len each end */
    float gl, gr, pitch;                         /* its place; its pitch (a ratio) */
    uint32_t cb[2];                              /* its two decoded blocks (block + 1; 0 none) */
    float kill;                                  /* shed: its level falling to 0 (5 ms); 0 not shed */
    int16_t cache[2][RL_BLK];
} gene_t;
static gene_t gn[RL_NG];
static float rl_cursor, rl_spawn, rl_gain, rl_wow, rl_flut, rl_lpl, rl_lpr;
static uint32_t rl_rng = 0x1234567u;
static float rl_rand(void)
{
    rl_rng ^= rl_rng << 13;
    rl_rng ^= rl_rng >> 17;
    rl_rng ^= rl_rng << 5;
    return (float)(rl_rng >> 8) * (1.0f / 16777216.0f);
}
static int16_t gene_at(gene_t *g, uint32_t i)    /* the tape's sample i, through the gene's two blocks */
{
    const uint32_t b = i / RL_BLK;
    int s;
    if (rl_rec && b == rl_w / RL_BLK) {          /* the block being recorded: as written so far, the rest as it was (its
                                                  * bytes past the head are of the coder's old state: not decodable) */
        const uint32_t k = i % RL_BLK;
        return k < rl_w % RL_BLK ? rl_new[k] : rl_old[k];
    }
    s = g->cb[0] == b + 1u ? 0 : g->cb[1] == b + 1u ? 1 : -1;
    if (s < 0) {                                 /* an empty one, or the one further from b */
        const uint32_t d0 = g->cb[0] ? (g->cb[0] - 1u > b ? g->cb[0] - 1u - b : b - (g->cb[0] - 1u)) : 0xFFFFFFFFu,
                       d1 = g->cb[1] ? (g->cb[1] - 1u > b ? g->cb[1] - 1u - b : b - (g->cb[1] - 1u)) : 0xFFFFFFFFu;
        s = d0 >= d1 ? 0 : 1;
        rl_decode(b, g->cache[s]);
        g->cb[s] = b + 1u;
    }
    return g->cache[s][i % RL_BLK];
}
/* ------------------------------------------------------- the reel's modulators --- */
/* five, as a voice's: each a shape at its RATE (60 s .. 0.12 s a cycle, or with SYNC Tempo a note value), SMOOTH'ed,
 * DEPTH of its TARGET's range (half of it either way at 100), added to the knob's value while it plays: Speed, Gene,
 * Slide, Morph, Dry/Wet, Tone, Wow, Drive, SOS, Organize (in splices); or another modulator's depth (+-50) or rate
 * (+-2 octaves), by its value a chunk before */
static const uint8_t TM_PAR[EN_NTMSET] = {TP_SPEED, TP_GENE, TP_SLIDE, TP_MORPH, TP_MIX, TP_TONE, TP_WOW, TP_DRIVE,
                                          TP_SOS, TP_ORG};
static struct {
    float ph, hold, from, to, lx, vs;            /* vs: what it gives, smoothed, -1 .. 1 times its depth */
} tmg[EN_NTMOD];
static float tm_off[TP_N];                       /* each tape setting's offset now (its own units) */
static void tape_mods(uint32_t n)                /* once a chunk */
{
    float dd[EN_NTMOD] = {0}, rr[EN_NTMOD] = {0};
    int s, k;
    for (k = 0; k < TP_N; k++)
        tm_off[k] = 0.0f;
    for (s = 0; s < EN_NTMOD; s++) {             /* the modulators on others: their value a chunk ago */
        const int t = par[P_TM(s, TARGET)] - 1;   /* (Off: -1) */
        if (t >= EN_NTMSET && t < EN_NTMDEST) {
            if (t - EN_NTMSET < EN_NTMOD)
                dd[t - EN_NTMSET] += 50.0f * tmg[s].vs;
            else
                rr[t - EN_NTMSET - EN_NTMOD] += 2.0f * tmg[s].vs;
        }
    }
    for (s = 0; s < EN_NTMOD; s++) {
        const int t = par[P_TM(s, TARGET)] - 1, sh = par[P_TM(s, SHAPE)], rv = par[P_TM(s, RATE)];
        const float d = fm_clampf((float)par[P_TM(s, DEPTH)] + dd[s], 0.0f, 100.0f),
                    secs = par[P_TM(s, SYNC)] ? DIV_BEATS[div_of(rv)] * 60.0f / tempo_bpm() : mod_secs(rv),
                    sm = fm_minf((float)n / (smooth_secs(par[P_TM(s, SMOOTH)]) * EN_SR), 1.0f);
        float v, ph;
        tmg[s].ph += (float)n / (secs * EN_SR) * fm_exp2f(rr[s]);
        if (tmg[s].ph >= 1.0f) {                 /* a cycle's end: S&H's next, the walk's next end, the map's step */
            tmg[s].ph -= (float)(int)tmg[s].ph;
            tmg[s].hold = rl_rand() * 2.0f - 1.0f;
            tmg[s].from = tmg[s].to;
            tmg[s].to = rl_rand() * 2.0f - 1.0f;
            tmg[s].lx = 3.95f * tmg[s].lx * (1.0f - tmg[s].lx);
            if (tmg[s].lx < 0.001f || tmg[s].lx > 0.999f)
                tmg[s].lx = 0.2f + 0.6f * rl_rand();
        }
        ph = tmg[s].ph;
        switch (sh) {
        case SHAPE_TRI: v = ph < 0.5f ? 4.0f * ph - 1.0f : 3.0f - 4.0f * ph; break;
        case SHAPE_UP: v = 2.0f * ph - 1.0f; break;
        case SHAPE_DOWN: case SHAPE_ENV: v = 1.0f - 2.0f * ph; break;
        case SHAPE_SQUARE: v = ph < 0.5f ? 1.0f : -1.0f; break;
        case SHAPE_SH: v = tmg[s].hold; break;
        case SHAPE_WALK: v = tmg[s].from + (tmg[s].to - tmg[s].from) * (0.5f - 0.5f * fm_cosf(FM_PI * ph)); break;
        case SHAPE_CHAOS: v = tmg[s].lx * 2.0f - 1.0f; break;
        default: v = sine(ph); break;
        }
        tmg[s].vs += (v * d * 0.01f - tmg[s].vs) * sm;   /* SMOOTH */
        en_reel_mod[s] = tmg[s].vs;
        if (t >= 0 && t < EN_NTMSET)
            tm_off[TM_PAR[t]] += tmg[s].vs * 0.5f * (TM_PAR[t] == TP_ORG ? 7.0f : 100.0f);
    }
}
static float rl_pf(int k)                        /* tape setting k (TP_*) as it plays: the knob and its modulators */
{
    const en_param_t *pi = en_param_info(P_TAPE + k);
    return fm_clampf((float)par[P_TAPE + k] + tm_off[k], (float)pi->lo, (float)pi->hi);
}
static int rl_pi(int k) { return (int)(rl_pf(k) + 0.5f); }
static float rl_speed(void)                      /* SPEED: -2x .. 0 .. +2x, 1x and -1x sticking (+-2) */
{
    const int v = rl_pi(TP_SPEED);
    if (v >= 73 && v <= 77)
        return 1.0f;
    if (v >= 23 && v <= 27)
        return -1.0f;
    if (v >= 48 && v <= 52)
        return 0.0f;
    return (float)(v - 50) * 0.04f;
}

/* ------------------------------------------------------------- the splices' keys --- */
/* Each splice's key, so the garden and the reel can be played in one key (Key sync). The pitch a splice plays at is
 * the pitch it was recorded at times SPEED (12 log2 |speed| semitones: at 0.75x, -5); SPEED alone says nothing of what
 * was recorded. So each splice is studied once it is recorded (or comes in from a file): its sound's 12 pitch classes
 * (a chroma), from 36 Goertzel filters (C4 .. B6, where the notes and the bass's harmonics are), over up to 48 frames
 * of 4096 samples spread across it, each frame weighted alike. From the UI, a frame a call (en_reel_study). A splice's
 * key can be set by hand too (24 keys, a major or a minor profile). Then:
 *   Key sync Tape: SPEED snaps to the nearest semitone transposition that sets the splice's pitch classes on the
 *     garden's scale (the best fits, within 3 %, then the one nearest the knob): 1x may become 0.94x (-1);
 *   Key sync Garden: the garden's root moves to fit its scale to the splice as SPEED transposes it, and the garden is
 *     tuned by the cents SPEED leaves over (the UI, en_detune) */
#define RL_KOK 0x5A                              /* a splice's key known (its chroma studied) */
#define ST_FRAME 4096u
#define ST_NF 48u
static volatile uint32_t rl_gen;                 /* (a cut: the splices are not the ones being studied) */
static const float KK_MAJ[12] = {6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f, 2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f};
static const float KK_MIN[12] = {6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f, 2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f};
static struct {
    int8_t spl;                                  /* the splice being studied, -1 none */
    uint8_t nf, f;                               /* its frames, the next */
    uint32_t stride, gen;                        /* (gen: the reel's, so a cut meanwhile is seen) */
    float acc[12];
    float coef[36];
} st = {-1, 0, 0, 0, 0, {0}, {0}};

int en_reel_keyvec(int spl, float *v)            /* splice spl's 12 pitch classes' weights; 0: not known yet, or silent */
{
    int k, m;
    float sum = 0.0f;
    if ((unsigned)spl >= rl_meta.nspl)
        return 0;
    m = rl_meta.kman[spl];
    if (m >= 1 && m <= 24) {                     /* by hand: its key's profile */
        const float *pr = m <= 12 ? KK_MAJ : KK_MIN;
        const int t = (m - 1) % 12;
        for (k = 0; k < 12; k++)
            v[k] = pr[(k - t + 12) % 12];
        return 1;
    }
    if (rl_meta.kok[spl] != RL_KOK)
        return 0;
    for (k = 0; k < 12; k++)
        sum += v[k] = (float)rl_meta.chroma[spl][k];
    return sum > 0.0f;
}
int en_reel_keyman(int spl) { return (unsigned)spl < rl_meta.nspl ? rl_meta.kman[spl] : 0; }
void en_reel_keyset(int spl, int k)
{
    if ((unsigned)spl < RL_NSPL)
        rl_meta.kman[spl] = (uint8_t)((unsigned)k <= 24u ? k : 0);
}
int en_key_guess(const float *v, int *minor)     /* the key (its tonic) a weighting is most like: 24 profiles */
{
    int t, k, best = 0;
    float bs = -1e9f;
    *minor = 0;
    for (t = 0; t < 24; t++) {
        const float *pr = t < 12 ? KK_MAJ : KK_MIN;
        float c = 0.0f;
        for (k = 0; k < 12; k++)
            c += v[k] * pr[(k - t % 12 + 12) % 12];
        if (c > bs) {
            bs = c;
            best = t;
        }
    }
    *minor = best >= 12;
    return best % 12;
}

int en_reel_study(void)
{
    uint32_t s0, s1, len, a, i;
    int k, b;
    float g1[36], g2[36], e[36], tot = 0.0f;
    if (st.spl < 0) {                            /* the next splice to study: recorded, not being recorded */
        for (k = 0; k < (int)rl_meta.nspl; k++)
            if (rl_meta.kok[k] != RL_KOK && !(rl_rec && k == rl_spl) && rl_meta.s1[k] - rl_meta.s0[k] >= RL_MIN)
                break;
        if (k >= (int)rl_meta.nspl)
            return -1;
        st.spl = (int8_t)k;
        st.gen = rl_gen;
        len = rl_meta.s1[k] - rl_meta.s0[k];
        st.nf = (uint8_t)(len < ST_FRAME ? 1u : (len - ST_FRAME) / ST_FRAME + 1u > ST_NF ? ST_NF
                                                                                        : (len - ST_FRAME) / ST_FRAME + 1u);
        st.stride = st.nf > 1u ? (len - ST_FRAME) / (st.nf - 1u) / RL_BLK * RL_BLK : 0u;
        st.f = 0;
        for (k = 0; k < 12; k++)
            st.acc[k] = 0.0f;
        for (b = 0; b < 36; b++)                 /* C4 (MIDI 60) .. B6 */
            st.coef[b] = 2.0f * fm_cosf(FM_TWO_PI * 261.6256f * fm_exp2f((float)b / 12.0f) / RL_SR);
    }
    k = st.spl;
    if (k >= (int)rl_meta.nspl || (rl_rec && k == rl_spl)) {   /* (erased, or being recorded again: later) */
        st.spl = -1;
        return -1;
    }
    s0 = rl_meta.s0[k];
    s1 = rl_meta.s1[k];
    a = s0 + (uint32_t)st.f * st.stride;
    for (b = 0; b < 36; b++)
        g1[b] = g2[b] = 0.0f;
    {                                            /* a frame: decoded a sample at a time, through the 36 filters */
        uint32_t blk = a / RL_BLK;
        int pred = rl_hdr[blk].pred, idx = rl_hdr[blk].idx;
        for (i = 0; i < ST_FRAME && a + i < s1; i++) {
            float x;
            if (i && (a + i) % RL_BLK == 0u) {
                blk = (a + i) / RL_BLK;
                pred = rl_hdr[blk].pred;
                idx = rl_hdr[blk].idx;
            }
            rl_step(&pred, &idx, rl_nibble(a + i));
            x = (float)pred * (1.0f / 32768.0f);
            for (b = 0; b < 36; b++) {
                const float g0 = x + st.coef[b] * g1[b] - g2[b];
                g2[b] = g1[b];
                g1[b] = g0;
            }
        }
    }
    for (b = 0; b < 36; b++) {
        e[b] = fm_maxf(g1[b] * g1[b] + g2[b] * g2[b] - st.coef[b] * g1[b] * g2[b], 0.0f);
        tot += e[b];
    }
    if (tot > 1e-6f)                             /* (a frame of silence counts for nothing) */
        for (b = 0; b < 36; b++)
            st.acc[b % 12] += e[b] / tot;
    if (++st.f >= st.nf) {                       /* done: its chroma, 0..255 */
        float mx = 0.0f;
        if (st.gen == rl_gen) {                  /* (cut meanwhile: its chroma would be of another splice) */
            for (b = 0; b < 12; b++)
                mx = fm_maxf(mx, st.acc[b]);
            for (b = 0; b < 12; b++)
                rl_meta.chroma[k][b] = (uint8_t)(mx > 0.0f ? st.acc[b] / mx * 255.0f + 0.5f : 0.0f);
            rl_meta.kok[k] = RL_KOK;
        }
        st.spl = -1;
    }
    return k;
}

/* ------------------------------------------------------------ the auto-chop --- */
/* EDIT held on the tape's pages: the splice ORGANIZE picks is studied a frame at a time (a block of tape a step, as its
 * key is) and cut, where it lies, at its attacks: up to eight splices, as many as the reel has room for. The audio stays
 * where it is; only the splices' first and last samples change. A splice with no clear attack is cut into four alike
 * (or as many as fit). The new splices are studied again (en_reel_study). */
#define CH_MAX 24                                /* the attacks kept: the strongest, then in the order they are */
#define CH_BLOCKS 16u                            /* a step: 4096 samples */
typedef struct {
    uint32_t pos;
    float s;
} ch_on_t;
static struct {
    uint8_t on, started, nc;
    int8_t spl;
    uint32_t first, a, a1, last;
    float prev;
    ch_on_t cand[CH_MAX];
} ch;
static struct {
    uint8_t spl, n;
    uint32_t b[RL_NSPL + 1];                     /* each piece's first sample, and the last's end */
} ch_req;

int en_reel_chop(int spl)                        /* the splice the UI's ORGANIZE picks */
{
    uint32_t s0, s1;
    if (ch.on)
        return -4;
    if (en_reel_state)
        return -3;
    if (!rl_meta.nspl || (unsigned)spl >= rl_meta.nspl)
        return -1;
    s0 = rl_meta.s0[spl];
    s1 = rl_meta.s1[spl];
    if (s0 % RL_BLK || s1 <= s0 || s1 > rl_meta.len)
        return -1;
    if (s1 - s0 < 2u * RL_MIN + RL_BLK)          /* (two pieces at least, each from a block's start) */
        return -2;
    if (rl_meta.nspl >= RL_NSPL)
        return -5;
    ch.on = 1;
    ch.started = 0;
    ch.nc = 0;
    ch.spl = (int8_t)spl;
    ch.first = ch.a = ch.last = s0;
    ch.a1 = s1;
    ch.prev = 0.0f;
    return 0;
}
static void ch_put(uint32_t pos, float s)        /* an attack: kept with the strongest (CH_MAX of them) */
{
    int i, w = -1;
    float ws = 1e30f;
    if (ch.nc < CH_MAX)
        w = ch.nc++;
    else {
        for (i = 0; i < CH_MAX; i++)
            if (ch.cand[i].s < ws) {
                ws = ch.cand[i].s;
                w = i;
            }
        if (s <= ws)
            return;
    }
    ch.cand[w].pos = pos;
    ch.cand[w].s = s;
}
static int ch_make(void)                         /* the request: 0 made, -1 no piece */
{
    const uint32_t len = ch.a1 - ch.first;
    uint32_t b[RL_NSPL + 1];
    int avail = RL_NSPL - ((int)rl_meta.nspl - 1), i, j, nb = 1;
    ch_on_t t;
    if (avail > (int)(len / RL_MIN))
        avail = (int)(len / RL_MIN);
    if (avail < 2)
        return -1;
    for (i = 0; i < ch.nc; i++)                  /* the strongest first */
        for (j = i + 1; j < ch.nc; j++)
            if (ch.cand[j].s > ch.cand[i].s) {
                t = ch.cand[i];
                ch.cand[i] = ch.cand[j];
                ch.cand[j] = t;
            }
    if (ch.nc > avail - 1)
        ch.nc = (uint8_t)(avail - 1);
    for (i = 0; i < ch.nc; i++)                  /* then as they lie on the tape */
        for (j = i + 1; j < ch.nc; j++)
            if (ch.cand[j].pos < ch.cand[i].pos) {
                t = ch.cand[i];
                ch.cand[i] = ch.cand[j];
                ch.cand[j] = t;
            }
    b[0] = ch.first;
    for (i = 0; i < ch.nc; i++)
        if (ch.cand[i].pos - b[nb - 1] >= RL_MIN && ch.a1 - ch.cand[i].pos >= RL_MIN)
            b[nb++] = ch.cand[i].pos;
    if (nb < 2) {                                /* no attack: four alike, or as many as fit */
        int want = (int)(len / (2u * RL_MIN));
        if (want > avail)
            want = avail;
        if (want > 4)
            want = 4;
        if (want < 2)
            want = 2;
        for (i = 1; i < want; i++) {
            const uint32_t p =
                (ch.first + len * (uint32_t)i / (uint32_t)want) / RL_BLK * RL_BLK;
            if (p - b[nb - 1] >= RL_MIN && ch.a1 - p >= RL_MIN)
                b[nb++] = p;
        }
        if (nb < 2)
            return -1;
    }
    b[nb] = ch.a1;
    ch_req.spl = (uint8_t)ch.spl;
    ch_req.n = (uint8_t)nb;
    for (i = 0; i <= nb; i++)
        ch_req.b[i] = b[i];
    return 0;
}
int en_reel_chop_step(void)                      /* a step of it (the UI's tick): a frame of the splice */
{
    uint32_t end;
    int i;
    if (!ch.on)
        return -1;
    end = ch.a + CH_BLOCKS * RL_BLK;
    if (end > ch.a1)
        end = ch.a1;
    while (ch.a < end) {
        const uint32_t blk = ch.a / RL_BLK;
        int pred = rl_hdr[blk].pred, idx = rl_hdr[blk].idx;
        float acc = 0.0f;
        float e;
        for (i = 0; i < RL_BLK; i++) {
            rl_step(&pred, &idx, rl_nibble(ch.a + (uint32_t)i));
            acc += (float)(pred * pred);
        }
        e = acc * (1.0f / (RL_BLK * 32768.0f * 32768.0f));
        if (!ch.started) {
            ch.started = 1;
            ch.prev = e;
        } else {
            /* an attack: the block's energy nearly twice the one before's (a click, a note's start), well over the
             * tape's noise, and a splice's worth of tape after the last one */
            if (e > ch.prev * 1.8f && e > 4e-6f && ch.a - ch.last >= 2u * RL_MIN && ch.a1 - ch.a >= RL_MIN) {
                ch_put(ch.a, e / (ch.prev + 1e-9f));
                ch.last = ch.a;
            }
            ch.prev = e;
        }
        ch.a += RL_BLK;
    }
    if (ch.a < ch.a1)
        return 1;
    ch.on = 0;
    if (ch_make() != 0)
        return -2;
    st.spl = -1;                                 /* (a study of the old splice: it is not one any more) */
    BARRIER();
    post(C_REEL, EN_REEL_CHOP, 0);
    return 0;
}
static void reel_chop_apply(void)                /* (the engine's: the pieces in the splice's place) */
{
    const int k = ch_req.spl, m = ch_req.n, n = (int)rl_meta.nspl;
    int i, j;
    if (rl_rec || m < 2 || k < 0 || k >= n || n - 1 + m > RL_NSPL)
        return;
    if (rl_meta.s0[k] != ch_req.b[0] || rl_meta.s1[k] != ch_req.b[m])
        return;                                  /* (another splice, or another reel: not this one) */
    for (i = 1; i < m; i++)
        if (ch_req.b[i] % RL_BLK || ch_req.b[i] <= ch_req.b[i - 1] || ch_req.b[i] - ch_req.b[i - 1] < RL_MIN ||
            ch_req.b[m] - ch_req.b[i] < RL_MIN)
            return;
    for (i = n - 1; i > k; i--) {                /* the splices after it, moved along */
        rl_meta.s0[i + m - 1] = rl_meta.s0[i];
        rl_meta.s1[i + m - 1] = rl_meta.s1[i];
        rl_meta.kok[i + m - 1] = rl_meta.kok[i];
        rl_meta.kman[i + m - 1] = rl_meta.kman[i];
        for (j = 0; j < 12; j++)
            rl_meta.chroma[i + m - 1][j] = rl_meta.chroma[i][j];
    }
    for (i = 0; i < m; i++) {                    /* its pieces, in its place */
        rl_meta.s0[k + i] = ch_req.b[i];
        rl_meta.s1[k + i] = ch_req.b[i + 1];
        rl_meta.kok[k + i] = rl_meta.kman[k + i] = 0;
        for (j = 0; j < 12; j++)
            rl_meta.chroma[k + i][j] = 0;
    }
    rl_meta.nspl = (uint32_t)(n - 1 + m);
    if (rl_spl > k)
        rl_spl += m - 1;
    rl_gen++;
    rl_publish();
}

/* how well the splice's pitch classes (v), transposed by t semitones, sit on the garden's scale; the strongest on the
 * root counts a little more (a key, not only a scale). 0 .. ~1.2 */
static float rl_fit(const float *v, int t)
{
    const int root = par[P_ROOT];
    float in = 0.0f, sum = 0.0f, mx = 0.0f;
    int pc;
    for (pc = 0; pc < 12; pc++) {
        const int q = ((pc + t) % 12 + 12) % 12;
        sum += v[pc];
        mx = fm_maxf(mx, v[pc]);
        if (pool >> ((q - root + 12) % 12) & 1u)
            in += v[pc];
    }
    return sum > 0.0f ? in / sum + 0.2f * v[((root - t) % 12 + 12) % 12] / mx : 0.0f;
}
static float rl_snap(float s, int spl)           /* Key sync Tape: SPEED's nearest transposition into the garden's key */
{
    float v[12], f[12], best = 0.0f, a = fm_fabsf(s);
    int k, r, kn, pick = 0, dist = 99;
    if (par[P_ST(KSYNC)] != 2 || a < 0.05f || !en_reel_keyvec(spl, v))
        return s;
    for (r = 0; r < 12; r++)
        best = fm_maxf(best, f[r] = rl_fit(v, r));
    kn = (int)fm_floorf(12.0f * fm_log2f(a) + 0.5f);
    for (k = -36; k <= 12; k++) {                /* 2^(-36/12) = 0.125x .. 2x */
        const int r2 = (k % 12 + 12) % 12, d = k > kn ? k - kn : kn - k;
        if (f[r2] >= best - 0.03f && d < dist) {
            dist = d;
            pick = k;
        }
    }
    return (s < 0.0f ? -1.0f : 1.0f) * fm_exp2f((float)pick / 12.0f);
}

static void gene_start(int k0, uint32_t s0, float L, float glen, float rate, int morph)
{
    gene_t *g = &gn[k0];
    float st = rl_cursor + rl_pf(TP_SLIDE) * 0.01f * L, pm = 1.0f, pan = 0.0f;
    while (st >= L)
        st -= L;
    if (morph > 85 && rl_rand() < (float)(morph - 85) * (0.5f / 15.0f))
        pm = rl_rand() < 0.5f ? 2.0f : 0.5f;     /* an octave up or down */
    if (morph > 75)
        pan = (rl_rand() * 2.0f - 1.0f) * (float)(morph - 75) * 0.04f;
    g->on = 1;
    g->kill = 0.0f;
    g->cb[0] = g->cb[1] = 0u;                    /* (decoded afresh: what was recorded since is heard) */
    g->dir = rate >= 0.0f;
    g->pos = (float)s0 + st;
    g->pitch = pm;
    g->inc = rate * pm;
    g->life = 0.0f;
    g->len = glen;
    g->edge = morph <= 55 ? 0.03f : 0.03f + 0.47f * (float)(morph - 55) / 45.0f;
    g->gl = fm_cosf((pan + 1.0f) * (FM_PI / 4.0f)) * 1.4142f;
    g->gr = fm_sinf((pan + 1.0f) * (FM_PI / 4.0f)) * 1.4142f;
}

/* n samples of the reel's sound into l and r (the genes of the splice ORGANIZE picks, at SPEED). The genes are made at
 * the tape's own rate, 22.05 kHz (a sample of tape a step at 1x; half the work), each pair of the output's a line
 * from the last to the next (a sample late) */
static float rl_hl[4], rl_hr[4];                /* the last four made (1.5.22: a cubic between, not a line) */
static uint8_t rl_ph;
static void reel_play(float *l, float *r, uint32_t n)
{
    const int k = rl_pi(TP_ORG), mk = par[P_TAPE + TP_MORPH];
    /* (MORPH's gaps, 0 .. 40, only as the knob sets them: modulated from above 40, it stops at 41) */
    const int morph = mk > 40 && rl_pi(TP_MORPH) < 41 ? 41 : rl_pi(TP_MORPH);
    const float lvl = fm_minf(rl_pf(TP_MIX) * 0.02f, 1.0f),   /* (MIX: the reel full from the middle up) */
                wd = rl_pf(TP_WOW) * 0.0001f,
                tc = rl_pf(TP_TONE) >= 99.5f ? 1.0f
                                             : 1.0f - fm_expf(-FM_TWO_PI * 1500.0f *
                                                              fm_exp2f(rl_pf(TP_TONE) * 0.03f) / EN_SR),
                tgt = en_tape_playing && rl_meta.nspl && lvl > 0.0f ? 1.0f : 0.0f, rate0 = rl_speed();
    uint32_t j, s0, s1;
    float L, glen, iv, pk = 0.0f;
    int i, spp = 0;
    tape_mods(n);
    for (j = 0; j < n; j++)
        l[j] = r[j] = 0.0f;
    if (!rl_meta.nspl || (tgt <= 0.0f && rl_gain < 1e-4f)) {
        rl_gain = 0.0f;
        for (i = 0; i < RL_NG; i++) {
            gn[i].on = 0;
            en_reel_gene[i] = -1;
        }
        en_reel_pos = 0.0f;
        en_reel_level = 0.0f;
        return;
    }
    {                                            /* the splice picked; one too short to play: the nearest that plays */
        int sp = k < (int)rl_meta.nspl ? k : (int)rl_meta.nspl - 1, d;
        for (d = 0; d < (int)rl_meta.nspl * 2; d++) {
            const int c = sp + (d & 1 ? (d + 1) / 2 : -(d / 2));
            if (c >= 0 && c < (int)rl_meta.nspl && rl_meta.s1[c] - rl_meta.s0[c] >= RL_MIN) {
                sp = c;
                break;
            }
        }
        s0 = rl_meta.s0[sp];
        s1 = rl_meta.s1[sp];
        spp = sp;
    }
    if (s1 <= s0 + 64u)
        return;
    const float rate = rl_snap(rate0, spp);     /* (Key sync Tape: in the garden's key) */
    en_reel_rate = rate;
    en_reel_spl = (int8_t)spp;
    L = (float)(s1 - s0);
    glen = fm_maxf(L * fm_exp2f((rl_pf(TP_GENE) - 100.0f) * 0.08f), 441.0f);
    if (glen > L)
        glen = L;
    {                                            /* (1.5.13) no gene sounding (after STOP, the tape stopped, a new
                                                  * splice): the next at once, not after a whole interval (as long as
                                                  * the splice, or longer, at GENE 100: silent a turn, reversed or not) */
        int any = 0;
        for (i = 0; i < RL_NG; i++)
            any |= gn[i].on;
        if (!any && rl_spawn > 0.0f)
            rl_spawn = 0.0f;
    }
    iv = morph <= 40 ? glen * (1.0f + (float)(40 - morph) * 0.1f)            /* gaps */
         : morph <= 55 ? glen * 0.97f                                       /* one after another, joined */
                       : glen / (1.0f + (float)(morph - 55) / 15.0f);       /* overlapping, to four */
    for (j = 0; j < n; j++) {
        float sl, sr;
        rl_gain += (tgt - rl_gain) * 0.0005f;
        rl_ph ^= 1u;
        if (rl_ph) {                             /* a tape sample: the genes' next */
            float w, step;
            sl = sr = 0.0f;
            rl_wow += 1.0f / EN_SR;              /* the tape's wow (0.5 Hz) and flutter (6.3 Hz), two samples a step */
            rl_flut += 12.6f / EN_SR;
            if (rl_wow >= 1.0f)
                rl_wow -= 1.0f;
            if (rl_flut >= 1.0f)
                rl_flut -= 1.0f;
            w = 1.0f + wd * (sine(rl_wow) + 0.32f * sine(rl_flut));   /* WOW: 25 is 0.25 % and 0.08 % */
            step = rate * w;
            if (rate != 0.0f) {                  /* the cursor runs with the tape; a gene each interval */
                rl_cursor += step;
                while (rl_cursor >= L)
                    rl_cursor -= L;
                while (rl_cursor < 0.0f)
                    rl_cursor += L;
                rl_spawn -= fm_fabsf(step);
                if (rl_spawn <= 0.0f) {
                    int best = 0;
                    float most = -1.0f;
                    rl_spawn += iv;
                    for (i = 0; i < RL_NG; i++) {   /* a free one, or the one furthest through */
                        const float f = gn[i].on ? gn[i].life / gn[i].len : 2.0f;
                        if (f > most) {
                            most = f;
                            best = i;
                        }
                    }
                    gene_start(best, s0, L, glen, rate, morph);
                }
            }
            for (i = 0; i < RL_NG; i++) {
                gene_t *g = &gn[i];
                float f, e, a, x, p, x0;
                uint32_t i0, i1;
                if (!g->on)
                    continue;
                if (rate == 0.0f) {              /* the tape stopped: the genes die away (20 ms) */
                    g->life += g->len * 0.0022f;
                } else {
                    g->inc = rate * g->pitch * w;
                    g->life += fm_fabsf(g->inc);
                }
                f = g->life / g->len;
                if (f >= 1.0f) {
                    g->on = 0;
                    continue;
                }
                e = g->edge;
                a = f < e ? f / e : f > 1.0f - e ? (1.0f - f) / e : 1.0f;
                a = a * a * (3.0f - 2.0f * a);   /* (smoothstep: no corner) */
                if (g->kill > 0.0f) {            /* shed: faded out */
                    a *= g->kill;
                    g->kill -= 1.0f / 110.0f;
                    if (g->kill <= 0.0f)
                        g->on = 0;
                }
                if (rate != 0.0f) {
                    g->pos += g->inc;
                    while (g->pos >= (float)s1)
                        g->pos -= L;
                    while (g->pos < (float)s0)
                        g->pos += L;
                }
                i0 = (uint32_t)g->pos;
                i1 = i0 + 1u >= s1 ? s0 : i0 + 1u;
                p = g->pos - (float)i0;
                x0 = (float)gene_at(g, i0);
                {   /* (1.5.22) a 4-point Hermite through the samples round it, not a line: less dulling, less
                     * grit as SPEED moves the pitch (the second core has the time) */
                    const uint32_t im = i0 <= s0 ? s1 - 1u : i0 - 1u, i2 = i1 + 1u >= s1 ? s0 : i1 + 1u;
                    const float xm = (float)gene_at(g, im), x1 = (float)gene_at(g, i1), x2 = (float)gene_at(g, i2),
                                c1 = 0.5f * (x1 - xm), c2 = xm - 2.5f * x0 + 2.0f * x1 - 0.5f * x2,
                                c3 = 0.5f * (x2 - xm) + 1.5f * (x0 - x1);
                    x = (((c3 * p + c2) * p + c1) * p + x0) * (a * (1.0f / 16384.0f));
                }
                sl += x * g->gl;
                sr += x * g->gr;
            }
            rl_hl[0] = rl_hl[1], rl_hl[1] = rl_hl[2], rl_hl[2] = rl_hl[3], rl_hl[3] = sl;
            rl_hr[0] = rl_hr[1], rl_hr[1] = rl_hr[2], rl_hr[2] = rl_hr[3], rl_hr[3] = sr;
            sl = rl_hl[1];
            sr = rl_hr[1];
        } else {                                 /* between: the cubic's middle (-1 9 9 -1) / 16, not the average */
            sl = (9.0f * (rl_hl[1] + rl_hl[2]) - rl_hl[0] - rl_hl[3]) * (1.0f / 16.0f);
            sr = (9.0f * (rl_hr[1] + rl_hr[2]) - rl_hr[0] - rl_hr[3]) * (1.0f / 16.0f);
        }
        rl_lpl += (sl - rl_lpl) * tc;            /* TONE: 1.5 kHz .. open */
        rl_lpr += (sr - rl_lpr) * tc;
        l[j] = rl_lpl * lvl * rl_gain;
        r[j] = rl_lpr * lvl * rl_gain;
        pk = fm_maxf(pk, fm_fabsf(l[j]) + fm_fabsf(r[j]));
    }
    rl_lpl = fm_flush(rl_lpl);
    rl_lpr = fm_flush(rl_lpr);
    en_reel_pos = rl_cursor / L;
    en_reel_level += (pk * 2.0f - en_reel_level) * 0.2f;
    for (i = 0; i < RL_NG; i++)
        en_reel_gene[i] = gn[i].on ? (int32_t)gn[i].pos : -1;
}

static void reel_take(void)                      /* a file's reel: its blocks are in, its splices now */
{
    const void *m = reel_meta_req;
    int i;
    if (!m)
        return;
    rl_rec = rl_stopping = 0;
    __builtin_memcpy(&rl_meta, m, sizeof rl_meta);
    rl_cursor = 0.0f;
    for (i = 0; i < RL_NG; i++)
        gn[i].on = 0;
    en_reel_valid();                             /* (its splices checked; the genes' cached blocks dropped) */
}
static int reel_shed(void)                       /* an overload: the gene furthest through let go (never the last one
                                                  * sounding: the reel stays heard); 0 none to let go */
{
    int i, best = -1, on = 0;
    float most = -1.0f;
    for (i = 0; i < RL_NG; i++)
        if (gn[i].on && gn[i].kill <= 0.0f) {
            on++;
            if (gn[i].life / gn[i].len > most) {
                most = gn[i].life / gn[i].len;
                best = i;
            }
        }
    if (on < 2)
        return 0;
    gn[best].kill = 1.0f;                        /* (faded out, 5 ms) */
    return 1;
}

static void reel_init(void)                      /* (the reel itself stays: a reboot loads it, en_reel_mem) */
{
    int i;
    rl_rec = rl_stopping = 0;
    rl_cursor = rl_spawn = rl_gain = rl_lpl = rl_lpr = 0.0f;
    __builtin_memset(rl_hl, 0, sizeof rl_hl);
    __builtin_memset(rl_hr, 0, sizeof rl_hr);
    for (i = 0; i < EN_NTMOD; i++) {
        tmg[i].ph = tmg[i].hold = tmg[i].from = tmg[i].to = tmg[i].vs = 0.0f;
        tmg[i].lx = 0.3f + 0.1f * (float)i;
        en_reel_mod[i] = 0.0f;
    }
    for (i = 0; i < TP_N; i++)
        tm_off[i] = 0.0f;
    rl_d1 = rl_d2 = 0.0f;
    __builtin_memset(rl_hb, 0, sizeof rl_hb);
    rl_ns = 0.0f;
    rl_half = 0;
    for (i = 0; i < RL_NG; i++) {
        gn[i].on = 0;
        gn[i].cb[0] = gn[i].cb[1] = 0u;
    }
    if (rl_meta.magic != RL_MAGIC) {
        __builtin_memset(&rl_meta, 0, sizeof rl_meta);
        rl_meta.magic = RL_MAGIC;
    }
    rl_publish();
}

void *en_reel_mem(int part, uint32_t *n)
{
    if (part == 0) {
        *n = sizeof rl_meta;
        return &rl_meta;
    }
    if (part == 1) {
        *n = sizeof rl_hdr;
        return rl_hdr;
    }
    *n = (rl_meta.len + 1u) / 2u;
    if (part == 3)                               /* (the whole of it, for loading) */
        *n = sizeof rl_nib;
    return rl_nib;
}
int en_reel_valid(void)                          /* a reel loaded: its metadata sound; else none */
{
    uint32_t i;
    for (i = 0; i < RL_NG; i++)                  /* (the cached blocks are of another reel) */
        gn[i].cb[0] = gn[i].cb[1] = 0u;
    if (rl_meta.magic != RL_MAGIC || rl_meta.nspl > RL_NSPL || rl_meta.len > RL_N) {
        __builtin_memset(&rl_meta, 0, sizeof rl_meta);
        rl_meta.magic = RL_MAGIC;
        rl_publish();
        return 0;
    }
    for (i = 0; i < rl_meta.nspl; i++)
        if (rl_meta.s0[i] % RL_BLK || rl_meta.s1[i] > rl_meta.len || rl_meta.s0[i] > rl_meta.s1[i]) {
            rl_meta.nspl = i;
            break;
        }
    for (i = 0; i < RL_NSPL; i++) {              /* (a reel saved before 1.4.13, or a file's: its keys to study) */
        if (rl_meta.kok[i] != RL_KOK)
            rl_meta.kok[i] = 0;
        if (rl_meta.kman[i] > 24u)
            rl_meta.kman[i] = 0;
    }
    st.spl = -1;
    rl_publish();
    return 1;
}
