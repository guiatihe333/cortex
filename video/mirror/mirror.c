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
static int refine = 0, lcd_look = 0;               /* (refine 3 or 4: the FM-1's own frame enlarged S times, refined) */
static float rf_a[240 * 240 * 3], rf_h[240 * 960 * 3], rf_o[960 * 960 * 3];
static int rf_i[960][4];
static float rf_w[960][4];
static int rf_s = 0;
static void refine_frame(int S)                    /* screen (the FM-1's RGB565) -> rgba_hd, 240 S square */
{
    const int W = 240 * S;
    int x, y, k, c;
    if (rf_s != S) {                               /* (each output column's four sources, Catmull-Rom) */
        for (x = 0; x < W; x++) {
            const float f = ((float)x + 0.5f) / (float)S - 0.5f, fl = (float)(int)(f < 0 ? f - 1.0f : f), t = f - fl;
            const int i0 = (int)fl;
            const float w[4] = {((-0.5f * t + 1.0f) * t - 0.5f) * t, (1.5f * t - 2.5f) * t * t + 1.0f,
                                ((-1.5f * t + 2.0f) * t + 0.5f) * t, (0.5f * t - 0.5f) * t * t};
            for (k = 0; k < 4; k++) {
                const int j = i0 - 1 + k;
                rf_i[x][k] = j < 0 ? 0 : j > 239 ? 239 : j;
                rf_w[x][k] = w[k];
            }
        }
        rf_s = S;
    }
    for (y = 0; y < 240; y++)                      /* the FM-1's pixels; its 4 x 4 dither smoothed (a light 3 x 3) */
        for (x = 0; x < 240; x++) {
            float s[3] = {0, 0, 0};
            int dy, dx;
            for (dy = -1; dy <= 1; dy++)
                for (dx = -1; dx <= 1; dx++) {
                    const int yy = y + dy < 0 ? 0 : y + dy > 239 ? 239 : y + dy,
                              xx = x + dx < 0 ? 0 : x + dx > 239 ? 239 : x + dx;
                    const uint16_t v = (uint16_t)((screen[yy * 240 + xx] >> 8) | (screen[yy * 240 + xx] << 8));
                    const float wgt = dx || dy ? (dx && dy ? 1.0f : 2.0f) : 12.0f;   /* (the dither out, the edges kept) */
                    s[0] += wgt * (float)((v >> 11) << 3), s[1] += wgt * (float)((v >> 5 & 63u) << 2);
                    s[2] += wgt * (float)((v & 31u) << 3);
                }
            for (c = 0; c < 3; c++)
                rf_a[(y * 240 + x) * 3 + c] = s[c] * (1.0f / 24.0f);
        }
    for (y = 0; y < 240; y++)                      /* across */
        for (x = 0; x < W; x++)
            for (c = 0; c < 3; c++) {
                float s = 0.0f;
                for (k = 0; k < 4; k++)
                    s += rf_w[x][k] * rf_a[(y * 240 + rf_i[x][k]) * 3 + c];
                rf_h[(y * W + x) * 3 + c] = s;
            }
    for (y = 0; y < W; y++)                        /* down */
        for (x = 0; x < W; x++)
            for (c = 0; c < 3; c++) {
                float s = 0.0f;
                for (k = 0; k < 4; k++)
                    s += rf_w[y][k] * rf_h[(rf_i[y][k] * W + x) * 3 + c];
                rf_o[(y * W + x) * 3 + c] = s;
            }
    for (y = 0; y < W; y++)                        /* sharpened back (an unsharp mask over S pixels), the LCD's look */
        for (x = 0; x < W; x++) {
            const int xl = x - S < 0 ? 0 : x - S, xr = x + S >= W ? W - 1 : x + S,
                      yu = y - S < 0 ? 0 : y - S, yd = y + S >= W ? W - 1 : y + S;
            uint8_t *o = rgba_hd + ((size_t)y * W + x) * 4u;
            const int grid = lcd_look && (x % S == S - 1 || y % S == S - 1);
            for (c = 0; c < 3; c++) {
                const float v = rf_o[(y * W + x) * 3 + c],
                            blur = (rf_o[(y * W + xl) * 3 + c] + rf_o[(y * W + xr) * 3 + c] +
                                    rf_o[(yu * W + x) * 3 + c] + rf_o[(yd * W + x) * 3 + c]) * 0.25f;
                float r = v + 0.9f * (v - blur);
                if (lcd_look)                      /* (the panel's punch: a little more contrast, the grid between) */
                    r = 128.0f + (r - 128.0f) * 1.08f, r *= grid ? 0.86f : 1.0f;
                o[c] = (uint8_t)(r < 0.0f ? 0.0f : r > 255.0f ? 255.0f : r + 0.5f);
            }
            o[3] = 255;
        }
    vs_style_rgba(rgba_hd, W, S);                  /* STYLE, as the FM-1 shows it */
}
static vfollow_t st;

EXPORT uint8_t *mx_in(void) { return in_buf; }
EXPORT uint8_t *mx_rgba(void) { return hd || refine ? rgba_hd : rgba; }
EXPORT void mx_set_hd(int s) { hd = s < 0 ? 0 : s > 4 ? 4 : s == 1 ? 0 : s; refine = 0; }   /* 0 the FM-1's own 240;
                                                                                             * 2 .. 4: HD */
EXPORT void mx_set_refine(int s) { refine = s < 3 ? 3 : s > 4 ? 4 : s; hd = 0; }   /* the FM-1's own, refined S times */
EXPORT void mx_set_lcd(int on) { lcd_look = on != 0; }
EXPORT int mx_size(void) { return refine ? 240 * refine : hd ? 240 * hd : 240; }
EXPORT int mx_msg(uint32_t n)                    /* 0 nothing, 1 a frame (mx_rgba has it), 2 a SYNC, 3 a bad message */
{
    int r;
    uint32_t i;
    if (n > sizeof in_buf)
        return VF_BAD;
    r = vfollow_msg(&st, in_buf, n, hd ? 0 : screen);
    if (r == VF_FRAME && refine)
        refine_frame(refine);
    else if (r == VF_FRAME && hd)                       /* (painted at S times instead: the state moves as vs_paint moves it) */
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
