/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 guiatihe333 */
/* The mirror's side of the link (vlink.h): the FM-1's messages in, the same frames out. Used by the PC's mirror
 * (mirror/mirror.c, in WebAssembly) and by the host test that replays what the simulated FM-1 sent. One engine per
 * program: vsynth.c's state is the follower's. */
#pragma once
#include "vlink.h"

typedef struct {
    uint8_t synced;                               /* a SYNC came: the frames can be followed */
    uint8_t rec;                                  /* the FM-1 is recording (its REC) */
    uint8_t lost;                                 /* a frame went missing or disagreed since the SYNC: ask again */
    uint8_t other;                                /* the FM-1's engine is another version: this mirror cannot follow */
    uint32_t seq_next, frames, gaps, mismatches, syncs, bad;
    uint32_t last_ms;                             /* the FM-1's clock at the last frame */
} vfollow_t;
enum { VF_NONE, VF_FRAME, VF_SYNC, VF_BAD };

/* a message (F0 .. F7, whole); a FRAME is drawn into screen (240 x 240, the LCD's byte order) when screen is not 0 */
static inline int vfollow_msg(vfollow_t *s, const uint8_t *m, uint32_t n, uint16_t *screen)
{
    static uint8_t raw[VLINK_SYNC_RAW > VLINK_RAW_MAX ? VLINK_SYNC_RAW : VLINK_RAW_MAX];
    uint32_t k;
    if (n < 6 || m[0] != 0xF0u || m[1] != VLINK_MANU || m[2] != VLINK_ID0 || m[3] != VLINK_ID1 || m[n - 1] != 0xF7u)
        return VF_NONE;
    k = vlink_unpack(m + 5, n - 6, raw, sizeof raw);
    if (m[4] == VLINK_SYNC) {
        vl_r_t r = {raw, k, 0, 0};
        uint32_t seed, np, i;
        if (vl_g8(&r) != VLINK_VERSION || vl_g16(&r) != VS_ENGINE) {
            s->bad++;
            s->other = 1, s->synced = 0;
            return VF_BAD;
        }
        s->other = 0;
        seed = vl_g32(&r);
        np = vl_g8(&r);
        for (i = 0; i < np && i < (uint32_t)ST_N; i++)
            vs_set((int)i, (int16_t)vl_g16(&r));
        if (r.bad || np != (uint32_t)ST_N) {
            s->bad++;
            return VF_BAD;
        }
        vs_reset(seed);
        s->synced = 1, s->lost = 0, s->syncs++;
        s->seq_next = 0;                         /* (the first frame after a SYNC is 0) */
        return VF_SYNC;
    }
    if (m[4] == VLINK_FRAME) {
        vlink_frame_t f;
        uint32_t i;
        if (vlink_frame_read(raw, k, &f) != 0) {
            s->bad++;
            return VF_BAD;
        }
        s->rec = (uint8_t)(f.flags & 1u);
        if (!s->synced)
            return VF_NONE;
        if (f.seq != (s->seq_next & 0xFFFFu)) {  /* (one missed: the field has gone its own way) */
            s->gaps++;
            s->lost = 1;
        }
        s->seq_next = f.seq + 1u;
        for (i = 0; i < f.nchg; i++)
            vs_set(f.chg_k[i], f.chg_v[i]);
        vs_frame(&f.in);
        if (vs_sum() != f.sum) {
            s->mismatches++;
            s->lost = 1;
        }
        if (screen)
            vs_paint(0, 240, screen);
        s->frames++;
        s->last_ms = f.in.ms;
        return VF_FRAME;
    }
    return VF_NONE;
}
