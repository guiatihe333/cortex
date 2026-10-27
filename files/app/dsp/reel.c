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
 * (SOS: how much of what was there stays, 0 replaced .. 100 all of it, the new on top).
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
static uint8_t rl_nib[RL_N / 2] EN_POOL;         /* two samples a byte, the first in the low nibble */
typedef struct {
    int16_t pred;
    uint8_t idx, pad;
} rl_hdr_t;
static rl_hdr_t rl_hdr[RL_NBLK] EN_POOL;
typedef struct {
    uint32_t magic, len, nspl, ver;              /* len: how far the reel is recorded (samples) */
    uint32_t s0[RL_NSPL], s1[RL_NSPL];           /* each splice: its first sample (a block's), one past its last */
} rl_meta_t;
static rl_meta_t rl_meta;

volatile uint8_t en_reel_state, en_reel_nspl;    /* 0 resting, 1 recording a new splice, 2 overdubbing */
volatile uint32_t en_reel_len, en_reel_w;        /* samples recorded; where it records */
volatile float en_reel_pos;                      /* the cursor in the splice playing, 0..1 */
volatile uint32_t en_reel_s0[8], en_reel_s1[8];
volatile int32_t en_reel_gene[4] = {-1, -1, -1, -1};

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

/* ------------------------------------------------------------- recording --- */
static uint8_t rl_rec, rl_stopping;              /* rl_rec as en_reel_state; stopping: the block finished first */
static uint32_t rl_w;                            /* where it records */
static int rl_spl;                               /* the splice recorded */
static int rl_ep, rl_ei;                         /* the coder's state */
static int16_t rl_old[RL_BLK];                   /* the block being written, as it was (sound on sound) */
static float rl_d1, rl_d2;                       /* the 2:1 decimator's last two in */
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
    rl_rec = 0;
    rl_stopping = 0;
    rl_publish();
}
static void reel_new(void)                       /* a new splice at the reel's end */
{
    uint32_t s = (rl_meta.len + RL_BLK - 1u) / RL_BLK * RL_BLK;
    if (rl_rec)
        rl_stop_now();
    if (rl_meta.nspl >= RL_NSPL || s + RL_BLK * 8u > RL_N)
        return;                                  /* (full: the UI says so) */
    rl_spl = (int)rl_meta.nspl;
    rl_meta.s0[rl_spl] = rl_meta.s1[rl_spl] = s;
    rl_meta.nspl++;
    rl_w = s;
    rl_ep = 0;
    rl_ei = 0;
    rl_rec = 1;
    rl_block_start();
    rl_publish();
}
static void reel_dub(void)                       /* the splice picked, overdubbed, looping; again: stop */
{
    int k = par[P_RL(ORG)];
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
    rl_w = rl_meta.s0[rl_spl];
    rl_ep = rl_hdr[rl_w / RL_BLK].pred;
    rl_ei = rl_hdr[rl_w / RL_BLK].idx;
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
    const float k = 1.1f + 0.04f * (float)par[P_RL(DRIVE)];
    return fm_tanhf(x * k) * (1.1f / k) * (1.0f + 0.01f * (float)par[P_RL(DRIVE)]);
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
        y = rl_drive(x) + o * (float)par[P_RL(SOS)] * 0.01f;
    else
        y = rl_drive(x);
    q = (int)(fm_clampf(y, -1.99f, 1.99f) * 16384.0f);
    q = rl_code(&rl_ep, &rl_ei, q);
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

/* n samples at 44.1 kHz of SOURCE's, to the reel (2:1, a [1 2 1] / 4 low-pass) */
static void reel_record(const float *in, uint32_t n)
{
    uint32_t j;
    if (!rl_rec)
        return;
    for (j = 0; j < n && rl_rec; j++) {
        const float x = in[j];
        if (rl_half)
            rl_put(0.25f * rl_d2 + 0.5f * rl_d1 + 0.25f * x);
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
    int s = g->cb[0] == b + 1u ? 0 : g->cb[1] == b + 1u ? 1 : -1;
    if (s < 0) {                                 /* an empty one, or the one further from b */
        const uint32_t d0 = g->cb[0] ? (g->cb[0] - 1u > b ? g->cb[0] - 1u - b : b - (g->cb[0] - 1u)) : 0xFFFFFFFFu,
                       d1 = g->cb[1] ? (g->cb[1] - 1u > b ? g->cb[1] - 1u - b : b - (g->cb[1] - 1u)) : 0xFFFFFFFFu;
        s = d0 >= d1 ? 0 : 1;
        rl_decode(b, g->cache[s]);
        g->cb[s] = b + 1u;
    }
    return g->cache[s][i % RL_BLK];
}
static float rl_speed(void)                      /* SPEED: -2x .. 0 .. +2x, 1x and -1x sticking (+-2) */
{
    const int v = par[P_RL(SPEED)];
    if (v >= 73 && v <= 77)
        return 1.0f;
    if (v >= 23 && v <= 27)
        return -1.0f;
    if (v >= 48 && v <= 52)
        return 0.0f;
    return (float)(v - 50) * 0.04f;
}
static void gene_start(int k0, uint32_t s0, float L, float glen, float rate, int morph)
{
    gene_t *g = &gn[k0];
    float st = rl_cursor + (float)par[P_RL(SLIDE)] * 0.01f * L, pm = 1.0f, pan = 0.0f;
    while (st >= L)
        st -= L;
    if (morph > 85 && rl_rand() < (float)(morph - 85) * (0.5f / 15.0f))
        pm = rl_rand() < 0.5f ? 2.0f : 0.5f;     /* an octave up or down */
    if (morph > 75)
        pan = (rl_rand() * 2.0f - 1.0f) * (float)(morph - 75) * 0.04f;
    g->on = 1;
    g->dir = rate >= 0.0f;
    g->pos = (float)s0 + st;
    g->pitch = pm;
    g->inc = rate * 0.5f * pm;
    g->life = 0.0f;
    g->len = glen;
    g->edge = morph <= 55 ? 0.03f : 0.03f + 0.47f * (float)(morph - 55) / 45.0f;
    g->gl = fm_cosf((pan + 1.0f) * (FM_PI / 4.0f)) * 1.4142f;
    g->gr = fm_sinf((pan + 1.0f) * (FM_PI / 4.0f)) * 1.4142f;
}

/* n samples of the reel's sound into l and r (the genes of the splice ORGANIZE picks, at SPEED) */
static void reel_play(float *l, float *r, uint32_t n)
{
    const int k = par[P_RL(ORG)], morph = par[P_RL(MORPH)];
    const float lvl = fm_minf((float)par[P_RL(MIX)] * 0.02f, 1.0f),   /* (MIX: the reel full from the middle up) */
                wd = (float)par[P_RL(WOW)] * 0.0001f,
                tc = par[P_RL(TONE)] >= 100 ? 1.0f
                                            : 1.0f - fm_expf(-FM_TWO_PI * 1500.0f *
                                                             fm_exp2f((float)par[P_RL(TONE)] * 0.03f) / EN_SR),
                tgt = en_playing && rl_meta.nspl && lvl > 0.0f ? 1.0f : 0.0f, rate = rl_speed();
    uint32_t j, s0, s1;
    float L, glen, iv;
    int i;
    for (j = 0; j < n; j++)
        l[j] = r[j] = 0.0f;
    if (!rl_meta.nspl || (tgt <= 0.0f && rl_gain < 1e-4f)) {
        rl_gain = 0.0f;
        for (i = 0; i < RL_NG; i++) {
            gn[i].on = 0;
            en_reel_gene[i] = -1;
        }
        en_reel_pos = 0.0f;
        return;
    }
    {
        const int sp = k < (int)rl_meta.nspl ? k : (int)rl_meta.nspl - 1;
        s0 = rl_meta.s0[sp];
        s1 = rl_meta.s1[sp];
    }
    if (s1 <= s0 + 64u)
        return;
    L = (float)(s1 - s0);
    glen = fm_maxf(L * fm_exp2f((float)(par[P_RL(GENE)] - 100) * 0.08f), 441.0f);
    if (glen > L)
        glen = L;
    iv = morph <= 40 ? glen * (1.0f + (float)(40 - morph) * 0.1f)            /* gaps */
         : morph <= 55 ? glen * 0.97f                                       /* one after another, joined */
                       : glen / (1.0f + (float)(morph - 55) / 15.0f);       /* overlapping, to four */
    for (j = 0; j < n; j++) {
        float sl = 0.0f, sr = 0.0f, w, step;
        rl_gain += (tgt - rl_gain) * 0.0005f;
        rl_wow += 0.5f / EN_SR;                  /* the tape's wow (0.5 Hz) and flutter (6.3 Hz) */
        rl_flut += 6.3f / EN_SR;
        if (rl_wow >= 1.0f)
            rl_wow -= 1.0f;
        if (rl_flut >= 1.0f)
            rl_flut -= 1.0f;
        w = 1.0f + wd * (sine(rl_wow) + 0.32f * sine(rl_flut));   /* WOW: 25 is 0.25 % and 0.08 % */
        step = rate * 0.5f * w;
        if (rate != 0.0f) {                      /* the cursor runs with the tape; a gene each interval */
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
                for (i = 0; i < RL_NG; i++) {    /* a free one, or the one furthest through */
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
            float f, e, a, x, p;
            uint32_t i0, i1;
            if (!g->on)
                continue;
            if (rate == 0.0f) {                  /* the tape stopped: the genes die away (20 ms) */
                g->life += g->len * 0.0011f;
            } else {
                g->inc = rate * 0.5f * g->pitch * w;
                g->life += fm_fabsf(g->inc);
            }
            f = g->life / g->len;
            if (f >= 1.0f) {
                g->on = 0;
                continue;
            }
            e = g->edge;
            a = f < e ? f / e : f > 1.0f - e ? (1.0f - f) / e : 1.0f;
            a = a * a * (3.0f - 2.0f * a);       /* (smoothstep: no corner) */
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
            x = ((float)gene_at(g, i0) + ((float)gene_at(g, i1) - (float)gene_at(g, i0)) * p) * (1.0f / 16384.0f);
            x *= a;
            sl += x * g->gl;
            sr += x * g->gr;
        }
        rl_lpl += (sl - rl_lpl) * tc;            /* TONE: 1.5 kHz .. open */
        rl_lpr += (sr - rl_lpr) * tc;
        l[j] = rl_lpl * lvl * rl_gain;
        r[j] = rl_lpr * lvl * rl_gain;
    }
    rl_lpl = fm_flush(rl_lpl);
    rl_lpr = fm_flush(rl_lpr);
    en_reel_pos = rl_cursor / L;
    for (i = 0; i < RL_NG; i++)
        en_reel_gene[i] = gn[i].on ? (int32_t)gn[i].pos : -1;
}

static void reel_init(void)                      /* (the reel itself stays: a reboot loads it, en_reel_mem) */
{
    int i;
    rl_rec = rl_stopping = 0;
    rl_cursor = rl_spawn = rl_gain = rl_lpl = rl_lpr = 0.0f;
    rl_d1 = rl_d2 = 0.0f;
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
    rl_publish();
    return 1;
}
