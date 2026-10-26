/* each voice's patches, each alone and dry for a while: rms and peak in dBFS, and the rms against the voice's first
 * patch (the level its *_GAIN in amb.c should take out). With the patch gains in amb.c as they are */
#include <math.h>
#include <stdio.h>
#include "amb.h"
void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2) { (void)st; (void)d1; (void)d2; }
static int32_t buf[2 * 256];
static double layer(int which, int patch, float *peak)
{
    static const int L[4] = {P_PAD, P_BELLS, P_BASS, P_PULSE};
    double s2 = 0;
    float pk = 0;
    long n = 0;
    int i, b;
    en_init();
    for (i = 0; i < 4; i++)
        en_set(L[i], i == which ? 70 : 0);
    en_set(P_ECHO, 0);
    en_set(P_REVERB, 0);
    en_set(P_DENSITY, 60);
    en_set(EN_VOX_PATCH[which], patch);
    en_play(1);
    for (b = 0; b < 40 * 44100 / 256; b++) {
        en_render(buf, 256, 2048);
        if (b < 5 * 44100 / 256)
            continue;
        for (i = 0; i < 512; i++) {
            float x = (float)buf[i] / 8388608.0f;
            s2 += (double)x * x;
            if (fabsf(x) > pk)
                pk = fabsf(x);
            n++;
        }
    }
    *peak = pk;
    return 10 * log10(s2 / (double)n + 1e-20);
}
int main(void)
{
    static const char *const V[EN_NVOX] = {"pad", "bells", "bass"};
    static const int N[EN_NVOX] = {EN_PADPATCHES, EN_BELLPATCHES, EN_BASSPATCHES};
    int v, p;
    for (v = 0; v < EN_NVOX; v++) {
        double r0 = 0;
        for (p = 0; p < N[v]; p++) {
            char t[16];
            float pk;
            double r = layer(v, p, &pk);
            if (!p)
                r0 = r;
            en_param_text(EN_VOX_PATCH[v], p, t);
            printf("%-6s %-9s rms %6.1f dB  peak %6.1f dB  vs first %+5.1f dB\n", V[v], t, r,
                   20 * log10((double)pk + 1e-12), r - r0);
        }
    }
    return 0;
}
