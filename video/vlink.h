/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 guiatihe333 */
/* CORTEX VIDEO's link to the computer's mirror: what the FM-1 tells it, frame by frame, so that the mirror's own copy of
 * the video synth (vsynth.c in WebAssembly) draws the same pictures, and records them with the sound.
 *
 * Every message is SysEx: F0 7D 43 56 TYPE BODY F7 (7D: non-commercial; "CV"), the body packed 7 bits in 8 (each group
 * of up to 7 bytes after a byte of their high bits, bit k for byte k).
 *   computer -> FM-1   TYPE 0x10  body: CMD   1 link on (and every second after, the keep-alive: 3 s without one and
 *                                                the link is off), 0 off, 2 sync again (the mirror lost a frame or
 *                                                disagrees); 3 a setting: k, value (i16): the FM-1 sets it as a
 *                                                knob would (the frames bring it back); 4 a scene: i (as its key);
 *                                                5 undo (as ENV); 6 a look: i
 *   FM-1 -> computer   TYPE 0x01  SYNC: VLINK_VERSION, VS_ENGINE (u16), the seed (u32), ST_N, the settings (i16 each):
 *                                 both engines
 *                                 reset (vs_reset(seed)) and set; the frames follow
 *                      TYPE 0x02  FRAME: seq (u16), flags (bit 0: REC), the field's sum after this frame (u32), the
 *                                 input (vs_in_t, below), the settings changed before it (a count, then setting u8 and
 *                                 value i16 each). The mirror: the changes, vs_frame(input), vs_paint, compare the sum
 * Numbers little-endian, floats as their IEEE bits. A frame is at most VLINK_MAX bytes on the wire. */
#pragma once
#include <stdint.h>
#ifndef VS_NO_STDINC
#include <string.h>
#endif
#include "vsynth.h"

#define VLINK_VERSION 2                          /* (2: the SYNC carries the engine's version) */
#define VLINK_MANU 0x7Du
#define VLINK_ID0 0x43u
#define VLINK_ID1 0x56u
enum { VLINK_SYNC = 0x01, VLINK_FRAME = 0x02, VLINK_CMD = 0x10 };
enum { VLINK_OFF = 0, VLINK_ON = 1, VLINK_RESYNC = 2, VLINK_SET = 3, VLINK_SCENE = 4, VLINK_UNDO = 5, VLINK_LOOK = 6 };
#define VLINK_NCHG 24                             /* settings changed a frame, at most (the rest wait a frame) */
#define VLINK_RAW_MAX (2 + 1 + 4 + 4 + 4 * 8 + 9 + 3 * VS_NNOTE + VS_WAVE_N + VS_NSPEC + 4 + 1 + 3 * VLINK_NCHG)
#define VLINK_MAX (5 + (VLINK_RAW_MAX * 8 + 6) / 7 + 1)
#define VLINK_SYNC_RAW (1 + 2 + 4 + 1 + 2 * ST_N)
#define VLINK_SYNC_MAX (5 + (VLINK_SYNC_RAW * 8 + 6) / 7 + 1)

static inline uint32_t vlink_pack(const uint8_t *in, uint32_t n, uint8_t *out)   /* 7 in 8: the bytes written */
{
    uint32_t i = 0, o = 0;
    while (i < n) {
        const uint32_t g = n - i < 7u ? n - i : 7u, h = o++;
        uint32_t k;
        out[h] = 0;
        for (k = 0; k < g; k++) {
            out[h] |= (uint8_t)((in[i + k] >> 7) << k);
            out[o++] = in[i + k] & 0x7Fu;
        }
        i += g;
    }
    return o;
}
static inline uint32_t vlink_unpack(const uint8_t *in, uint32_t n, uint8_t *out, uint32_t max)
{
    uint32_t i = 0, o = 0;
    while (i < n) {
        const uint8_t h = in[i++];
        uint32_t k;
        for (k = 0; k < 7u && i < n && o < max; k++)
            out[o++] = (uint8_t)(in[i++] | ((h >> k) & 1u) << 7);
    }
    return o;
}
static inline uint32_t vlink_frame(uint8_t type, const uint8_t *raw, uint32_t n, uint8_t *out)   /* F0 .. F7 */
{
    uint32_t o;
    out[0] = 0xF0, out[1] = VLINK_MANU, out[2] = VLINK_ID0, out[3] = VLINK_ID1, out[4] = type;
    o = 5 + vlink_pack(raw, n, out + 5);
    out[o++] = 0xF7;
    return o;
}

/* ---- the raw bodies */
typedef struct {
    uint8_t *p;
    uint32_t n;
} vl_w_t;
static inline void vl_u8(vl_w_t *w, uint32_t v) { w->p[w->n++] = (uint8_t)v; }
static inline void vl_u16(vl_w_t *w, uint32_t v) { vl_u8(w, v & 255u), vl_u8(w, v >> 8 & 255u); }
static inline void vl_u32(vl_w_t *w, uint32_t v) { vl_u16(w, v & 0xFFFFu), vl_u16(w, v >> 16); }
static inline void vl_f32(vl_w_t *w, float f)
{
    uint32_t u;
    memcpy(&u, &f, 4);
    vl_u32(w, u);
}
typedef struct {
    const uint8_t *p;
    uint32_t n, i;
    int bad;
} vl_r_t;
static inline uint32_t vl_g8(vl_r_t *r)
{
    if (r->i >= r->n) {
        r->bad = 1;
        return 0;
    }
    return r->p[r->i++];
}
static inline uint32_t vl_g16(vl_r_t *r) { const uint32_t a = vl_g8(r); return a | vl_g8(r) << 8; }
static inline uint32_t vl_g32(vl_r_t *r) { const uint32_t a = vl_g16(r); return a | vl_g16(r) << 16; }
static inline float vl_gf(vl_r_t *r)
{
    const uint32_t u = vl_g32(r);
    float f;
    memcpy(&f, &u, 4);
    return f;
}

static inline uint32_t vlink_sync_raw(uint8_t *raw, uint32_t seed, const int16_t *par)
{
    vl_w_t w = {raw, 0};
    int k;
    vl_u8(&w, VLINK_VERSION);
    vl_u16(&w, VS_ENGINE);
    vl_u32(&w, seed);
    vl_u8(&w, ST_N);
    for (k = 0; k < ST_N; k++)
        vl_u16(&w, (uint16_t)par[k]);
    return w.n;
}
/* a frame's body; chg: the settings changed (k, value), nchg of them */
static inline uint32_t vlink_frame_raw(uint8_t *raw, uint32_t seq, uint32_t flags, uint32_t sum, const vs_in_t *in,
                                       const uint8_t *chg_k, const int16_t *chg_v, uint32_t nchg)
{
    vl_w_t w = {raw, 0};
    uint32_t i, nn = in->nnote < VS_NNOTE ? in->nnote : VS_NNOTE;
    vl_u16(&w, seq & 0xFFFFu);
    vl_u8(&w, flags);
    vl_u32(&w, sum);
    vl_u32(&w, in->ms);
    vl_f32(&w, in->level);
    for (i = 0; i < VS_NBAND; i++)
        vl_f32(&w, in->bands[i]);
    vl_f32(&w, in->phase);
    vl_u8(&w, nn), vl_u8(&w, in->root), vl_u8(&w, in->cc), vl_u8(&w, in->vel);
    vl_u8(&w, in->clock_beat), vl_u8(&w, in->hold), vl_u8(&w, in->strobe), vl_u8(&w, in->mutate);
    for (i = 0; i < nn; i++)
        vl_u8(&w, in->note[i].voice), vl_u8(&w, in->note[i].note), vl_u8(&w, in->note[i].vel);
    for (i = 0; i < VS_WAVE_N; i++)
        vl_u8(&w, (uint8_t)in->wave[i]);
    for (i = 0; i < VS_NSPEC; i++)
        vl_u8(&w, in->spec[i]);
    vl_u8(&w, in->kick), vl_u8(&w, in->snare), vl_u8(&w, in->hat), vl_u8(&w, in->bright);
    if (nchg > VLINK_NCHG)
        nchg = VLINK_NCHG;
    vl_u8(&w, nchg);
    for (i = 0; i < nchg; i++)
        vl_u8(&w, chg_k[i]), vl_u16(&w, (uint16_t)chg_v[i]);
    return w.n;
}
typedef struct {
    uint32_t seq, flags, sum, nchg;
    vs_in_t in;
    uint8_t chg_k[VLINK_NCHG];
    int16_t chg_v[VLINK_NCHG];
} vlink_frame_t;
static inline int vlink_frame_read(const uint8_t *raw, uint32_t n, vlink_frame_t *f)   /* 0 = whole */
{
    vl_r_t r = {raw, n, 0, 0};
    uint32_t i;
    memset(f, 0, sizeof *f);
    f->seq = vl_g16(&r);
    f->flags = vl_g8(&r);
    f->sum = vl_g32(&r);
    f->in.ms = vl_g32(&r);
    f->in.level = vl_gf(&r);
    for (i = 0; i < VS_NBAND; i++)
        f->in.bands[i] = vl_gf(&r);
    f->in.phase = vl_gf(&r);
    f->in.nnote = (uint8_t)vl_g8(&r), f->in.root = (uint8_t)vl_g8(&r), f->in.cc = (uint8_t)vl_g8(&r);
    f->in.vel = (uint8_t)vl_g8(&r), f->in.clock_beat = (uint8_t)vl_g8(&r), f->in.hold = (uint8_t)vl_g8(&r);
    f->in.strobe = (uint8_t)vl_g8(&r), f->in.mutate = (uint8_t)vl_g8(&r);
    if (f->in.nnote > VS_NNOTE)
        return -1;
    for (i = 0; i < f->in.nnote; i++) {
        f->in.note[i].voice = (uint8_t)vl_g8(&r);
        f->in.note[i].note = (uint8_t)vl_g8(&r);
        f->in.note[i].vel = (uint8_t)vl_g8(&r);
    }
    for (i = 0; i < VS_WAVE_N; i++)
        f->in.wave[i] = (int8_t)vl_g8(&r);
    for (i = 0; i < VS_NSPEC; i++)
        f->in.spec[i] = (uint8_t)vl_g8(&r);
    f->in.kick = (uint8_t)vl_g8(&r), f->in.snare = (uint8_t)vl_g8(&r), f->in.hat = (uint8_t)vl_g8(&r);
    f->in.bright = (uint8_t)vl_g8(&r);
    f->nchg = vl_g8(&r);
    if (f->nchg > VLINK_NCHG)
        return -1;
    for (i = 0; i < f->nchg; i++) {
        f->chg_k[i] = (uint8_t)vl_g8(&r);
        f->chg_v[i] = (int16_t)vl_g16(&r);
    }
    return r.bad ? -1 : 0;
}
