/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal */
/* The voice filter's own level: a sine at C4 through vcf(), so a patch that turns the filter On, or turns its
 * RESONANCE up, keeps the level it had. Wide open it must be transparent, loud or quiet (the soft clip used to take
 * 6 dB off a loud patch); RESONANCE 100 used to be 8 dB below resonance 0 (the feedback's bass); a low CUT is the
 * filter doing its job, and must still take the note down. */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "../../app/dsp/amb.c"

static int fails;
#define CHECK(c, ...)                                                                                              \
    do {                                                                                                           \
        if (!(c)) {                                                                                                \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);                                                            \
            printf(__VA_ARGS__);                                                                                   \
            printf("\n");                                                                                          \
            fails++;                                                                                               \
        }                                                                                                          \
    } while (0)

void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2)
{
    (void)st;
    (void)d1;
    (void)d2;
}

#define FREQ 261.6                                      /* C4 */

/* the passband gain: a C4 sine in, its peak out, once the ladder has settled */
static double gain(float amp, int cut, int res)
{
    float x[FM6_N], s[4] = {0};
    fm6_note_t n;
    uint8_t p[EN_PATCH];
    double pk = 0.0, ph = 0.0, w = 2.0 * 3.14159265358979323846 * FREQ / (double)EN_SR;
    int b, i;
    memset(&n, 0, sizeof n);
    memset(p, 0, sizeof p);
    p[FP_ALG] = 31;                                     /* six carriers: the operator's own shape does not matter */
    par[P_VF(0, VF_ON)] = 1;
    par[P_VF(0, VF_CUT)] = (int16_t)cut;
    par[P_VF(0, VF_RES)] = (int16_t)res;
    par[P_VF(0, VF_ENV)] = 0;
    for (b = 0; b < 400; b++) {
        for (i = 0; i < FM6_N; i++) {
            x[i] = amp * (float)sin(ph);
            ph += w;
            if (ph > 2.0 * 3.14159265358979323846)
                ph -= 2.0 * 3.14159265358979323846;
        }
        vcf(0, s, 60.0f, &n, p, 0.0f, x);
        if (b > 300)
            for (i = 0; i < FM6_N; i++) {
                double a = fabs((double)x[i]);
                pk = a > pk ? a : pk;
            }
    }
    return pk / (double)amp;
}

int main(void)
{
    float x[FM6_N], s[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    fm6_note_t n;
    uint8_t p[EN_PATCH];
    double g;
    int i;

    /* Off: the samples go through untouched, and the stages go quiet */
    memset(&n, 0, sizeof n);
    memset(p, 0, sizeof p);
    p[FP_ALG] = 31;
    par[P_VF(0, VF_ON)] = 0;
    for (i = 0; i < FM6_N; i++)
        x[i] = 0.25f;
    vcf(0, s, 60.0f, &n, p, 0.0f, x);
    CHECK(x[0] == 0.25f && x[FM6_N - 1] == 0.25f, "Off: the samples stay as they came (%f)", (double)x[0]);
    CHECK(s[0] == 0.0f && s[3] == 0.0f, "Off: the stages go quiet");

    /* wide open: the level a patch had, at every level the mixer sends */
    g = gain(0.05f, 100, 0);
    CHECK(g > 0.999 && g < 1.001, "quiet and wide open: unity (%f)", g);
    g = gain(1.0f, 100, 0);
    CHECK(g > 0.95, "C4 at 1.0 wide open: within 0.5 dB (%f)", g);
    g = gain(2.0f, 100, 0);
    CHECK(g > 0.90, "C4 at 2.0 wide open: within 1 dB (%f)", g);
    g = gain(4.0f, 100, 0);
    CHECK(g > 0.65, "C4 at 4.0 wide open: within 4 dB (%f)", g);

    /* RESONANCE: the bass it takes comes back, rather than the whole patch going quiet */
    {
        double r0 = gain(0.5f, 75, 0), r100 = gain(0.5f, 75, 100);
        CHECK(r100 > 0.55, "RESONANCE 100 within 5 dB of resonance 0 (%f against %f)", r100, r0);
        CHECK(r100 < r0, "and still takes some of the bass (%f against %f)", r100, r0);
    }

    /* CUT: a low cutoff still shuts the note up, as a lowpass should */
    {
        static const int dark[] = {40, 25};
        for (i = 0; i < 2; i++) {
            g = gain(0.5f, dark[i], 0);
            CHECK(g < 0.5, "CUT %d still darkens C4 (%f)", dark[i], g);
        }
        CHECK(gain(0.5f, 25, 0) < gain(0.5f, 40, 0), "and CUT 25 more than 40");
    }

    printf(fails ? "filter: %d FAILED\n" : "filter: ok\n", fails);
    return fails != 0;
}
