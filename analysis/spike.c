/* where the spikiest sample is (as modscope), and what MOD and the bass were doing around it */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "amb.h"
void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2) { (void)st; (void)d1; (void)d2; }
static float mono[30 * 44100], modv[30 * 44100 / 256 + 1], glow[30 * 44100 / 256 + 1];
int main(int argc, char **argv)
{
    int layer = atoi(argv[1]), dest = atoi(argv[2]), depth = atoi(argv[3]), chaos = atoi(argv[4]), rate = atoi(argv[5]);
    static int32_t buf[512];
    static const int L[4] = {P_PAD, P_BELLS, P_BASS, P_PULSE};
    int i, b, n = 0, at = 0; double worst = 0;
    en_init();
    for (i = 0; i < 4; i++) en_set(L[i], i == layer ? 70 : 0);
    en_set(P_ECHO, 0); en_set(P_REVERB, 0); en_set(P_MOTION, 0); en_set(P_DENSITY, 60); en_set(P_DRIFT, 100);
    en_set(P_MDEPTH, depth); en_set(P_MDEST, dest); en_set(P_MCHAOS, chaos); en_set(P_MRATE, rate);
    en_play(1);
    for (b = 0; b < 8 * 44100 / 256; b++) en_render(buf, 256, 2048);
    for (b = 0; b < 30 * 44100 / 256; b++) {
        en_render(buf, 256, 2048);
        modv[b] = en_mod[EN_NMOD - 1];
        { int l; glow[b] = 0; for (l = 0; l < EN_NLOOP; l++) if (en_loop_glow[l] > glow[b]) glow[b] = en_loop_glow[l]; }
        for (i = 0; i < 256; i++) mono[n++] = (float)(buf[2 * i] + buf[2 * i + 1]) / 16777216.0f;
    }
    for (i = 66; i < n - 66; i++) {
        double d2 = fabs(mono[i] - 2 * mono[i - 1] + mono[i - 2]), loc = 0; int k;
        for (k = -32; k < 32; k++) loc += fabs(mono[i + k] - 2 * mono[i + k - 1] + mono[i + k - 2]);
        loc /= 64;
        if (loc > 1e-6 && d2 / loc > worst) { worst = d2 / loc; at = i; }
    }
    printf("spikiest %.1fx at %.4f s (sample %d, in its block of 32: %d)\n", worst, at / 44100.0, at, (at + 8 * 44100 / 256 * 256) % 32);
    for (i = at - 6; i <= at + 4; i++) printf("  %d %+.7f d2 %+.8f\n", i, mono[i], mono[i] - 2 * mono[i - 1] + mono[i - 2]);
    printf("  bass gen around (per 256): %.4f %.4f %.4f %.4f\n", modv[at / 256 - 2], modv[at / 256 - 1], modv[at / 256], modv[at / 256 + 1]);
    printf("  bell glow before/at: %.3f %.3f (1.0 = a strike in that 256-sample block)\n", glow[at / 256 - 1], glow[at / 256]);
    return 0;
}
