/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 guiatihe333 */
/* CORTEX VIDEO's mirror, the WebAssembly half: the video synth (vsynth.c, the FM-1's own) following the FM-1's link
 * (vfollow.h). The page writes a SysEx message (F0 .. F7) to mx_in(), calls mx_msg(n), and reads the frame as RGBA
 * from mx_rgba(). Built by mirror/build.sh (emcc, -ffp-contract=off as the FM-1's build) */
#include "vfollow.h"
#include "vpages.h"

#define EXPORT __attribute__((used, visibility("default")))
static uint8_t in_buf[4096];
static uint16_t screen[240 * 240];
static uint8_t rgba[240 * 240 * 4];
static uint8_t rgba_hd[960 * 960 * 4];             /* (HD: the frame painted at 240 S, S up to 4) */
static int hd = 3;
static vfollow_t st;

EXPORT uint8_t *mx_in(void) { return in_buf; }
EXPORT uint8_t *mx_rgba(void) { return hd ? rgba_hd : rgba; }
EXPORT void mx_set_hd(int s) { hd = s < 0 ? 0 : s > 4 ? 4 : s == 1 ? 0 : s; }   /* 0 the FM-1's own 240; 2 .. 4: HD */
EXPORT int mx_size(void) { return hd ? 240 * hd : 240; }
EXPORT int mx_msg(uint32_t n)                    /* 0 nothing, 1 a frame (mx_rgba has it), 2 a SYNC, 3 a bad message */
{
    int r;
    uint32_t i;
    if (n > sizeof in_buf)
        return VF_BAD;
    r = vfollow_msg(&st, in_buf, n, hd ? 0 : screen);
    if (r == VF_FRAME && hd)                       /* (painted at S times instead: the state moves as vs_paint moves it) */
        vs_paint_hd(hd, rgba_hd);
    else if (r == VF_FRAME)
        for (i = 0; i < 240u * 240u; i++) {      /* (RGB565 in the LCD's byte order to RGBA, the low bits filled) */
            const uint16_t c = (uint16_t)((screen[i] >> 8) | (screen[i] << 8));
            const uint32_t r5 = c >> 11, g6 = c >> 5 & 63u, b5 = c & 31u;
            rgba[4 * i] = (uint8_t)(r5 << 3 | r5 >> 2);
            rgba[4 * i + 1] = (uint8_t)(g6 << 2 | g6 >> 4);
            rgba[4 * i + 2] = (uint8_t)(b5 << 3 | b5 >> 2);
            rgba[4 * i + 3] = 255;
        }
    return r;
}
/* the counters: 0 synced, 1 rec, 2 lost, 3 frames, 4 gaps, 5 disagreeing, 6 syncs, 7 bad, 8 the FM-1's ms, 9 the FM-1's engine another
 * version */
EXPORT uint32_t mx_stat(int i)
{
    switch (i) {
    case 0: return st.synced;
    case 1: return st.rec;
    case 2: return st.lost;
    case 3: return st.frames;
    case 4: return st.gaps;
    case 5: return st.mismatches;
    case 6: return st.syncs;
    case 7: return st.bad;
    case 8: return st.last_ms;
    case 9: return st.other;
    default: return 0;
    }
}
EXPORT void mx_lost_clear(void) { st.lost = 0; }
/* the controls: the FM-1's pages and groups (vpages.h), the settings' names, ranges, values and words */
static char txt[24];
EXPORT int mx_npage(void) { return NPAGE; }
EXPORT const char *mx_page_name(int p) { return PAGES[p].name; }
EXPORT int mx_page_group(int p) { return PAGES[p].group; }
EXPORT int mx_page_knob(int p, int i) { return PAGES[p].k[i]; }
EXPORT int mx_ngroup(void) { return GR_N; }
EXPORT const char *mx_group_name(int g) { return GROUP_NAME[g]; }
EXPORT int mx_video_source(void)
{
    const int visual = vs_get(ST_VMODE);
    return visual >= VS_NMODE ? visual - VS_NMODE + 1 : 0;
}
EXPORT int mx_npar(void) { return ST_N; }
EXPORT int mx_slot_w(void) { return VS_SLOT_W; }
EXPORT int mx_nslot(void) { return VS_NSLOT; }
EXPORT int mx_tr1s(void) { return ST_TR1S; }
EXPORT const char *mx_par_name(int k) { return vs_par_info(k)->name; }
EXPORT int mx_par_lo(int k) { return vs_par_info(k)->lo; }
EXPORT int mx_par_hi(int k) { return vs_par_info(k)->hi; }
EXPORT int mx_par_get(int k) { return vs_get(k); }
EXPORT int mx_par_eff(int k) { return vs_eff(k); }
EXPORT const char *mx_par_text(int k, int v)
{
    const char *s = vs_par_text(k, v, txt);
    return s;
}
EXPORT float mx_beat(void) { return vs_beat(); }
