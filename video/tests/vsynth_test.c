/* SPDX-License-Identifier: MPL-2.0 */
/* CORTEX VIDEO: the video synth alone, on the host (gcc, -ffp-contract=off as every build):
 *  - the same settings and inputs draw the same frames (two runs from a reset agree, frame by frame, painted too);
 *  - a reset brings back the first frames exactly, whatever came before;
 *  - every look and every picture paints without a fault, the field moves, the beat is heard;
 *  - with a path argument: a frame of each picture as PPM files there (a look at it) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "vsynth.h"

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static uint16_t px[240 * 240];
/* a made-up input: a kick each 500 ms, a hat each 125, a pad under it, the wave a sum of three sines */
static void input(vs_in_t *in, int f)
{
    const uint32_t ms = (uint32_t)f * 50u;
    const float kick = (ms % 500u) < 60u ? 1.0f - (float)(ms % 500u) / 60.0f : 0.0f,
                hat = (ms % 125u) < 25u ? 0.5f : 0.0f;
    int i;
    memset(in, 0, sizeof *in);
    in->ms = ms;
    in->bands[0] = 0.25f * kick + 0.02f;
    in->bands[1] = 0.18f * kick + 0.03f;
    in->bands[2] = 0.06f + 0.02f * sinf((float)f * 0.1f);
    in->bands[3] = 0.05f;
    in->bands[4] = 0.03f + 0.08f * hat;
    in->bands[5] = 0.02f + 0.06f * hat;
    in->level = 0.2f + 0.5f * kick;
    in->phase = (float)ms / 500.0f;
    for (i = 0; i < VS_WAVE_N; i++)
        in->wave[i] = (int8_t)(60.0f * sinf((float)(f * 128 + i) * 0.07f) + 20.0f * sinf((float)i * 0.9f) * kick);
    in->root = (uint8_t)(f / 40 % 12);
    if (f % 16 == 0) {
        in->nnote = 2;
        in->note[0].voice = 0, in->note[0].note = (uint8_t)(48 + f % 24), in->note[0].vel = 100;
        in->note[1].voice = 1, in->note[1].note = (uint8_t)(60 + f % 12), in->note[1].vel = 80;
    }
    in->clock_beat = (ms % 500u) == 0u;
    for (i = 0; i < VS_NSPEC; i++)
        in->spec[i] = (uint8_t)(i < 4 ? 250.0f * kick : 60.0f + 50.0f * sinf((float)(f + i) * 0.3f));
    in->kick = (uint8_t)(255.0f * kick), in->hat = (uint8_t)(hat * 400.0f), in->bright = (uint8_t)(f * 3);
}
static uint32_t paint_sum(void)
{
    uint32_t s = 2166136261u;
    int i;
    vs_paint(0, 28, px);                          /* (in bands, as the FM-1 paints: header, two halves, knobs) */
    vs_paint(28, 72, px + 28 * 240);
    vs_paint(100, 72, px + 100 * 240);
    vs_paint(172, 68, px + 172 * 240);
    for (i = 0; i < 240 * 240; i++)
        s = (s ^ px[i]) * 16777619u;
    return s;
}
static void defaults(void)
{
    int k;
    for (k = 0; k < ST_N; k++)
        vs_set(k, vs_par_info(k)->def);
}
static void run(int frames, uint32_t *sums, uint32_t *psums)
{
    vs_in_t in;
    int f;
    for (f = 0; f < frames; f++) {
        input(&in, f);
        if (f == 30)
            in.mutate = 2;
        vs_frame(&in);
        sums[f] = vs_sum();
        psums[f] = paint_sum();
    }
}
static void ppm(const char *dir, const char *name)
{
    char path[512];
    FILE *fp;
    int i;
    snprintf(path, sizeof path, "%s/%s.ppm", dir, name);
    fp = fopen(path, "wb");
    if (!fp)
        return;
    fprintf(fp, "P6 240 240 255\n");
    for (i = 0; i < 240 * 240; i++) {
        const uint16_t c = (uint16_t)((px[i] >> 8) | (px[i] << 8));   /* (the LCD's byte order back) */
        const unsigned char rgb[3] = {(unsigned char)((c >> 11) << 3), (unsigned char)(((c >> 5) & 63) << 2),
                                      (unsigned char)((c & 31) << 3)};
        fwrite(rgb, 1, 3, fp);
    }
    fclose(fp);
}

int main(int argc, char **argv)
{
    static uint32_t a[200], b[200], pa[200], pb[200];
    int f, m, k, moved = 0;
    defaults();
    vs_reset(1234);
    run(200, a, pa);
    vs_reset(999);                                /* something else in between */
    vs_set(ST_VMODE, 3);
    run(50, b, pb);
    defaults();
    vs_reset(1234);
    run(200, b, pb);
    for (f = 0; f < 200; f++) {
        CHECK(a[f] == b[f], "frame %d: the field differs after a reset", f);
        CHECK(pa[f] == pb[f], "frame %d: the painting differs after a reset", f);
        if (a[f] != b[f] || pa[f] != pb[f])
            break;
    }
    for (f = 1; f < 200; f++)
        moved += pa[f] != pa[f - 1];
    CHECK(moved > 150, "the picture hardly moves (%d of 199 frames changed)", moved);
    CHECK(vs_beat() >= 0.0f && vs_beat() <= 1.0f, "the beat out of its range");
    /* every picture, every geometry, every palette, with the breakers and the layer: no fault, some light */
    for (m = 0; m < VS_NMODE; m++) {
        vs_in_t in;
        uint32_t lit = 0;
        int i;
        defaults();
        vs_set(ST_VMODE, m);
        vs_set(ST_VGEO, 1 + m % 16);
        vs_set(ST_VGEO2, 16 - m % 16);
        vs_set(ST_VPAL, m % VS_NPAL);
        vs_set(ST_VLAYER, 1 + (m + 3) % VS_NMODE);
        vs_set(ST_VMOSH, 40), vs_set(ST_VSORT, 30), vs_set(ST_VCORRUPT, 30), vs_set(ST_VSLICE, 30);
        vs_set(ST_VPIXEL, m == 4 ? 40 : 0), vs_set(ST_VSTROBE, 50), vs_set(ST_VINVERT, m == 7 ? 100 : 0);
        vs_set(ST_VMIRROR, m % 5), vs_set(ST_VXTGT, 2), vs_set(ST_VXROT, 30), vs_set(ST_VXZOOM, 20);
        vs_set(ST_OSHAPE, 1 + m), vs_set(ST_ODRAW, m % 4), vs_set(ST_OCOUNT, 1 + m * 2), vs_set(ST_OARRAY, m % 6);
        vs_set(ST_OMORPH, 40), vs_set(ST_OBLEND, m % 3), vs_set(ST_OSIDES, 12), vs_set(ST_OZOOM, m * 12);
        for (k = 0; k < VS_NSLOT; k++) {          /* every slot busy, every source in turn */
            vs_set(ST_AR1S + VS_SLOT_W * k, 1 + (k + m) % (SRC_N - 1));
            vs_set(ST_AR1S + VS_SLOT_W * k + 1, k == 7 ? VS_NTGT : 1 + (k * 5 + m) % (VS_NTGT - 1));
            vs_set(ST_AR1S + VS_SLOT_W * k + 2, k & 1 ? -60 : 60);
            vs_set(ST_AR1S + VS_SLOT_W * k + 3, k % 4), vs_set(ST_AR1S + VS_SLOT_W * k + 4, k * 12);
        }
        vs_reset((uint32_t)m + 1u);
        for (f = 0; f < 60; f++) {
            input(&in, f);
            vs_frame(&in);
            paint_sum();
        }
        for (i = 0; i < 240 * 240; i++)
            lit += px[i] != 0;
        CHECK(lit > 240u * 24u, "picture %s: dark (%u lit pixels)", vs_par_text(ST_VMODE, m, (char[8]){0}), lit);
        if (argc > 1) {
            char nm[16];
            snprintf(nm, sizeof nm, "pic%d", m);
            ppm(argv[1], nm);
        }
    }
    {   /* the values' words */
        char t[16];
        CHECK(!strcmp(vs_par_text(ST_VMODE, 5, t), "Plasma"), "Visual 5");
        CHECK(!strcmp(vs_par_text(ST_AR1S + 1, VS_NTGT, t), "All"), "a target's All");
        CHECK(!strcmp(vs_par_text(ST_AR1S + 1, VS_TGT_LAST + 1, t), "Pulse"), "the last target");
        CHECK(!strcmp(vs_par_text(ST_AR1S + 3, 2, t), "Beat hold"), "a slot's mode");
        CHECK(!strcmp(vs_par_text(ST_AR1S + VS_SLOT_W, SRC_KICK, t), "Kick"), "slot 2's source");
        CHECK(!strcmp(vs_par_text(ST_VXZOOM, -40, t), "-40"), "a number");
        CHECK(!strcmp(vs_par_text(ST_AR1S, SRC_TEMPO, t), "Tempo"), "a source");
    }
    printf("vsynth_test: %s (%d failed)\n", fails ? "FAILED" : "ok", fails);
    return fails != 0;
}
