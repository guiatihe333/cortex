/* what MOD does to a layer, dry, alone: per 46 ms window the spectral centroid, the pitch (autocorrelation) and
 * the level; then their spread over 30 s, and clicks (a sample far off the window's smooth course) */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "amb.h"
void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2) { (void)st; (void)d1; (void)d2; }
#define W 2048
static float mono[30 * 44100];
static void fft(float *re, float *im, int n)
{
    int i, j = 0, len;
    for (i = 1; i < n; i++) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) { float t = re[i]; re[i] = re[j]; re[j] = t; t = im[i]; im[i] = im[j]; im[j] = t; }
    }
    for (len = 2; len <= n; len <<= 1) {
        double a = -2 * M_PI / len;
        for (i = 0; i < n; i += len)
            for (j = 0; j < len / 2; j++) {
                float wr = (float)cos(a * j), wi = (float)sin(a * j);
                float ur = re[i + j], ui = im[i + j];
                float vr = re[i + j + len / 2] * wr - im[i + j + len / 2] * wi;
                float vi = re[i + j + len / 2] * wi + im[i + j + len / 2] * wr;
                re[i + j] = ur + vr; im[i + j] = ui + vi;
                re[i + j + len / 2] = ur - vr; im[i + j + len / 2] = ui - vi;
            }
    }
}
int main(int argc, char **argv)
{
    int layer = atoi(argv[1]), dest = atoi(argv[2]), depth = atoi(argv[3]), chaos = atoi(argv[4]), rate = atoi(argv[5]);
    static int32_t buf[512];
    static const int L[4] = {P_PAD, P_BELLS, P_BASS, P_PULSE};
    int i, b, n = 0, nw = 0, clicks = 0;
    double c_sum = 0, c_sq = 0, f_sum = 0, f_sq = 0, worst = 0, per = 0; int np = 0;
    en_init();
    for (i = 0; i < 4; i++) en_set(L[i], i == layer ? 70 : 0);
    en_set(P_ECHO, 0); en_set(P_REVERB, 0); en_set(P_MOTION, 0); en_set(P_DENSITY, 60); en_set(P_DRIFT, 100);
    en_set(P_MDEPTH, depth); en_set(P_MDEST, dest); en_set(P_MCHAOS, chaos); en_set(P_MRATE, rate);
    if (argc > 6) en_set(P_PADTONE, atoi(argv[6]));
    en_play(1);
    for (b = 0; b < 8 * 44100 / 256; b++) en_render(buf, 256, 2048);      /* past the first swell */
    for (b = 0; b < 30 * 44100 / 256; b++) {
        en_render(buf, 256, 2048);
        for (i = 0; i < 256; i++) mono[n++] = (float)(buf[2 * i] + buf[2 * i + 1]) / 16777216.0f;
    }
    for (i = 0; i + W <= n; i += W) {
        static float re[W], im[W];
        double e = 0, ce = 0, best = 0; int k, lag, bl = 0;
        for (k = 0; k < W; k++) { re[k] = mono[i + k] * (0.5f - 0.5f * cosf(2 * (float)M_PI * k / W)); im[k] = 0; e += (double)mono[i + k] * mono[i + k]; }
        if (e / W < 1e-7) continue;                                       /* (silent: a bell between strikes) */
        fft(re, im, W);
        { double s = 0, sw = 0; for (k = 1; k < W / 2; k++) { double m = sqrt((double)re[k] * re[k] + (double)im[k] * im[k]); s += m; sw += m * k; } ce = sw / s * 44100.0 / W; }
        {                                                                 /* the pitch: the normalised
                                                                           * autocorrelation's first strong peak,
                                                                           * interpolated (needs 2 windows) */
            static double ac[1500]; int lo = 100, hi = 1400; double y0, y1, y2, fl;
            if (i + W + hi > n) continue;
            for (lag = lo - 1; lag <= hi + 1; lag++) {
                double a = 0, e1 = 0, e2 = 0;
                for (k = 0; k < W; k++) { a += (double)mono[i + k] * mono[i + k + lag]; e1 += (double)mono[i + k] * mono[i + k]; e2 += (double)mono[i + k + lag] * mono[i + k + lag]; }
                ac[lag] = a / sqrt(e1 * e2 + 1e-30);
            }
            for (lag = lo; lag <= hi; lag++) if (ac[lag] > best) { best = ac[lag]; bl = lag; }
            per += best; np++;
            for (lag = lo; lag <= hi; lag++)                              /* the first peak within 3% of the best */
                if (ac[lag] >= ac[lag - 1] && ac[lag] >= ac[lag + 1] && ac[lag] > best * 0.97) { bl = lag; break; }
            y0 = ac[bl - 1]; y1 = ac[bl]; y2 = ac[bl + 1];
            fl = bl + 0.5 * (y0 - y2) / (y0 - 2 * y1 + y2 + 1e-30);
            { double f = 12 * log2(44100.0 / fl / 440.0) + 69; f_sum += f; f_sq += f * f; }
        }
        c_sum += ce; c_sq += ce * ce;
        nw++;
    }
    for (i = 2; i < n - 2; i++) {                                         /* a click: the second difference far
                                                                           * above its neighbourhood's */
        double d2 = fabs(mono[i] - 2 * mono[i - 1] + mono[i - 2]), loc = 0; int k;
        if (i < 66 || i > n - 66) continue;
        for (k = -32; k < 32; k++) loc += fabs(mono[i + k] - 2 * mono[i + k - 1] + mono[i + k - 2]);
        loc /= 64;
        if (loc > 1e-6 && d2 / loc > worst) worst = d2 / loc;
    }
    {
        double cm = c_sum / nw, cs = sqrt(fmax(c_sq / nw - cm * cm, 0)), fm = f_sum / nw, fs = sqrt(fmax(f_sq / nw - fm * fm, 0));
        (void)clicks;
        printf("centroid %5.0f Hz +-%5.0f  periodic %.3f  pitch spread %5.2f st  spikiest %5.1fx\n", cm, cs, per / np, fs, worst);
    }
    return 0;
}
