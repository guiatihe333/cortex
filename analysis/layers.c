/* per-layer level of the AMB-1 engine: each voice layer alone, dry, for a while; rms and peak in dBFS */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "amb.h"
void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2) { (void)st; (void)d1; (void)d2; }
static int32_t buf[2 * 256];
static void layer(const char *name, int which, int extra_p, int extra_v)
{
    static const int L[4] = {P_PAD, P_BELLS, P_BASS, P_PULSE};
    double s2 = 0; float pk = 0; long n = 0; int i, b;
    en_init();
    for (i = 0; i < 4; i++) en_set(L[i], i == which ? 70 : 0);
    en_set(P_ECHO, 0); en_set(P_REVERB, 0); en_set(P_DENSITY, 60);
    if (extra_p >= 0) en_set(extra_p, extra_v);
    en_play(1);
    for (b = 0; b < 40 * 44100 / 256; b++) {
        en_render(buf, 256, 2048);
        if (b < 5 * 44100 / 256) continue;
        for (i = 0; i < 512; i++) { float x = (float)buf[i] / 8388608.0f; s2 += (double)x * x; if (fabsf(x) > pk) pk = fabsf(x); n++; }
    }
    printf("%-14s rms %6.1f dB  peak %6.1f dB\n", name, 10 * log10(s2 / (double)n + 1e-20), 20 * log10((double)pk + 1e-12));
}
int main(void)
{
    layer("pad Rhodes", 0, P_PADTONE, 0);
    layer("pad Voices", 0, P_PADTONE, 1);
    layer("bells tone 0", 1, P_BTONE, 0);
    layer("bells tone 50", 1, P_BTONE, 50);
    layer("bells tone 100", 1, P_BTONE, 100);
    layer("bass", 2, -1, 0);
    return 0;
}
