/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal */
/* The instrument on the host: the pure parts (Euclid, scales, chords, voice leading, names) and the
 * render (sound, no NaN, bounded, the loops evolve, the chord drifts, freeze holds). */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "amb.h"

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

static uint32_t midi_count;
void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2)
{
    (void)st;
    (void)d1;
    (void)d2;
    midi_count++;
}

static uint32_t pat(const char *s)
{
    uint32_t m = 0;
    int i;
    for (i = 0; s[i]; i++)
        if (s[i] == 'x')
            m |= 1u << i;
    return m;
}

static int popcount(uint32_t m)
{
    int c = 0;
    while (m) {
        c += (int)(m & 1u);
        m >>= 1;
    }
    return c;
}

static int32_t buf[2 * 256];
static float peak, sum2;
static int bad;
static void run(float secs)
{
    int blocks = (int)(secs * 44100.0f / 256.0f), b, j;
    for (b = 0; b < blocks; b++) {
        en_render(buf, 256, 2048);
        for (j = 0; j < 512; j++) {
            float x = (float)buf[j] / 8388608.0f;
            if (x != x)
                bad++;
            if (fabsf(x) > peak)
                peak = fabsf(x);
            sum2 += x * x;
        }
    }
}

static void vox_all(int k, int v)               /* the first modulator's setting, the same for all three voices */
{
    int x;
    for (x = 0; x < EN_NVOX; x++)
        en_set(P_MS(x, 0, k), v);
}

static void patch_set(int v, int off, int val) /* one byte of a voice's patch, an edit (as the FM pages make it) */
{
    en_set(P_VX(v, VX_OWN), 1);
    en_patch_byte(v, off, val);
    en_patch_done(v, 0);
}

static void pack(const uint8_t *p, uint8_t *m)  /* a patch into a DX7 bank's packed voice (the reverse of en_unpack) */
{
    int o, k;
    for (o = 0; o < 6; o++) {
        const uint8_t *s = p + o * EP_OP;
        uint8_t *d = m + o * 17;
        for (k = 0; k < 11; k++)
            d[k] = s[k];
        d[11] = (uint8_t)(s[EP_RC] << 2 | s[EP_LC]);
        d[12] = (uint8_t)(s[EP_DET] << 3 | s[EP_RS]);
        d[13] = (uint8_t)(s[EP_KVS] << 2 | s[EP_AMS]);
        d[14] = s[EP_OL];
        d[15] = (uint8_t)(s[EP_FC] << 1 | s[EP_MODE]);
        d[16] = s[EP_FF];
    }
    for (k = 0; k < 8; k++)
        m[102 + k] = p[EP_PR1 + k];
    m[110] = p[EP_ALG];
    m[111] = (uint8_t)(p[EP_OKS] << 3 | p[EP_FB]);
    m[112] = p[EP_LFS];
    m[113] = p[EP_LFD];
    m[114] = p[EP_LPMD];
    m[115] = p[EP_LAMD];
    m[116] = (uint8_t)(p[EP_LPMS] << 4 | p[EP_LFW] << 1 | p[EP_LKS]);
    m[117] = p[EP_TRNSP];
    for (k = 0; k < 10; k++)
        m[118 + k] = p[EP_NAME + k];
}

int main(void)
{
    int pcs[4], n, i;
    char name[16];

    /* Euclid: the classic patterns */
    CHECK(en_euclid(3, 8, 0) == pat("x..x..x."), "E(3,8) %x", en_euclid(3, 8, 0));
    CHECK(en_euclid(5, 8, 0) == pat("x.xx.xx."), "E(5,8) %x", en_euclid(5, 8, 0));
    CHECK(en_euclid(4, 16, 0) == pat("x...x...x...x..."), "E(4,16)");
    CHECK(en_euclid(0, 16, 0) == 0, "E(0,16)");
    CHECK(en_euclid(7, 7, 0) == 0x7Fu, "E(7,7)");
    for (i = 0; i <= 12; i++)
        CHECK(popcount(en_euclid(i, 12, 5)) == i, "E(%d,12) has %d hits", i, popcount(en_euclid(i, 12, 5)));
    CHECK(en_euclid(1, 7, 3) == 1u << 3, "rotation");

    /* scales and chords */
    {
        int st[12];
        CHECK(en_scale(EN_MODE_POOL[MODE_DORIAN], st) == 7 && st[2] == 3 && st[6] == 10, "Dorian steps");
        CHECK(en_scale(EN_MODE_POOL[MODE_PENT_MIN], st) == 5, "pentatonic minor has 5");
        CHECK(en_scale(0, st) == 1, "the root is always in");
    }
    n = en_chord_tones(EN_MODE_POOL[MODE_IONIAN], 0, 0, pcs);
    CHECK(n == 4 && pcs[0] == 0 && pcs[1] == 4 && pcs[2] == 7 && pcs[3] == 11, "C Ionian I = Cmaj7");
    CHECK(!strcmp(en_chord_name(pcs, n, name), "CM7"), "name %s", name);
    n = en_chord_tones(EN_MODE_POOL[MODE_DORIAN], 2, 0, pcs);
    CHECK(!strcmp(en_chord_name(pcs, n, name), "Dm7"), "D Dorian i: %s", name);
    n = en_chord_tones(EN_MODE_POOL[MODE_DORIAN], 2, 3, pcs);
    CHECK(!strcmp(en_chord_name(pcs, n, name), "G7"), "D Dorian IV: %s", name);
    n = en_chord_tones(EN_MODE_POOL[MODE_PENT_MAJ], 0, 0, pcs);
    CHECK(n == 3, "pentatonic chords have three tones (%d)", n);
    n = en_chord_tones(1u | 1u << 4 | 1u << 7 | 1u << 9, 0, 0, pcs);   /* four notes: no doubled tones */
    CHECK(n == 3 && pcs[0] != pcs[1] && pcs[1] != pcs[2] && pcs[0] != pcs[2], "4-note pool: distinct");
    n = en_chord_tones(1u | 1u << 4 | 1u << 7, 0, 1, pcs);
    CHECK(n == 3, "3-note pool: all three (%d)", n);

    /* voice leading: small moves, in range, no unisons */
    {
        int from[4] = {50, 57, 62, 66}, to[4], cm7[4] = {0, 4, 7, 11}, f7[4] = {5, 9, 0, 4}, tot = 0;
        en_voice_lead(from, cm7, 4, to, 48, 77);
        for (i = 0; i < 4; i++) {
            CHECK(to[i] >= 48 && to[i] <= 77, "range %d", to[i]);
            tot += abs(to[i] - from[i]);
        }
        CHECK(tot <= 8, "Cmaj7 from D-ish: moved %d", tot);
        memcpy(from, to, sizeof from);
        en_voice_lead(from, f7, 4, to, 48, 77);
        tot = 0;
        for (i = 0; i < 4; i++)
            tot += abs(to[i] - from[i]);
        CHECK(tot <= 6, "CM7 -> FM7 moved %d", tot);
        CHECK(to[0] != to[1] && to[1] != to[2] && to[2] != to[3] && to[0] != to[3], "no unisons");
    }

    /* texts */
    {
        char t[12];
        en_param_text(P_DRIFT, 0, t);
        CHECK(!strcmp(t, "20s"), "drift 0: %s", t);
        en_param_text(P_DRIFT, 100, t);
        CHECK(!strcmp(t, "5:00"), "drift 100: %s", t);
        en_param_text(P_ETIME, 3, t);
        CHECK(!strcmp(t, "1 bar"), "etime: %s", t);
        en_param_text(P_BOCT, 1, t);
        CHECK(!strcmp(t, "+1"), "boct: %s", t);
    }

    /* the render: silent at rest, sound when growing, never NaN, bounded */
    en_init();
    run(1.0f);
    CHECK(peak == 0.0f, "silent before play (%f)", (double)peak);
    en_play(1);
    peak = sum2 = 0.0f;
    run(20.0f);
    CHECK(!bad, "NaN in the output");
    CHECK(peak > 0.05f && peak < 0.99f, "peak %f", (double)peak);
    printf("  20 s playing: peak %.3f, rms %.4f, bars %u\n", (double)peak,
           (double)sqrtf(sum2 / (20.0f * 44100.0f * 2.0f)), en_bars / 16u);
    CHECK(en_bars / 16u >= 5u, "the grid ran (%u sixteenths)", en_bars);
    CHECK(en_pad_level > 0.5f, "the pad swelled (%f)", (double)en_pad_level);

    /* the chord drifts: with Drift at its shortest, a few changes in two minutes */
    {
        int changes = 0, last = en_chord_root * 16 + en_chord[0] % 16, k;
        en_set(P_DRIFT, 0);
        for (k = 0; k < 120; k++) {
            int now;
            run(1.0f);
            now = en_chord_root * 16 + en_chord[0] % 16;
            changes += now != last;
            last = now;
        }
        CHECK(changes >= 3, "chord changes in 2 min at Drift 0: %d", changes);
        printf("  chord changes in 2 min: %d\n", changes);
    }

    /* a chord change is heard: within 2 s no sounding voice sings a note outside the new chord */
    {
        int k, i, bad_s = 0, changes = 0, last = -1;
        en_set(P_DRIFT, 0);
        for (k = 0; k < 90 * 4; k++) {
            int sig = en_chord[0] | en_chord[1] << 7 | en_chord[2] << 14 | en_chord[3] << 21;
            static int since = -1;
            run(0.25f);
            if (sig != last) {
                if (last >= 0) {
                    changes++;
                    since = 0;
                }
                last = sig;
            } else if (since >= 0 && ++since == 8) {   /* 2 s after a change */
                uint32_t pcs = 0;
                for (i = 0; i < EN_NPAD; i++)
                    pcs |= 1u << (en_chord[i] % 12);
                for (i = 0; i < EN_NPAD; i++)
                    if (en_pad_amp[i] > 0.05f && !(pcs >> (en_pad_note[i] % 12) & 1u))
                        bad_s++;
                since = -1;
            }
        }
        CHECK(changes >= 3, "chord changes in 90 s at Drift 0: %d", changes);
        CHECK(!bad_s, "%d voices still off the new chord 2 s after a change", bad_s);
    }

    /* the loops evolve: with Evolve at 100, the phrases change */
    {
        int8_t before[EN_NLOOP][EN_LOOP_MAX];
        int diff = 0, l;
        en_set(P_EVOLVE, 100);
        en_set(P_DENSITY, 80);
        memcpy(before, (const void *)en_loop_note, sizeof before);
        run(90.0f);
        for (l = 0; l < EN_NLOOP; l++)
            for (i = 0; i < EN_LOOP_MAX; i++)
                diff += before[l][i] != en_loop_note[l][i];
        CHECK(diff > 0, "the loops evolved");
        for (l = 0; l < EN_NLOOP; l++)
            for (i = EN_LOOP_LEN[l]; i < EN_LOOP_MAX; i++)
                CHECK(en_loop_note[l][i] == -1, "loop %d slot %d past its end", l, i);
    }

    /* freeze: the chord holds */
    {
        int held = en_chord[0] + 128 * en_chord[1], k;
        en_freeze(1);
        for (k = 0; k < 60; k++) {
            run(1.0f);
            CHECK(en_chord[0] + 128 * en_chord[1] == held, "the chord moved while frozen");
        }
        en_freeze(0);
    }

    /* a new key while playing (a piece, the root, the scale): within 3 s no voice still sings the old
     * chord (a voice only takes its note at its next breath otherwise, up to a minute later) */
    {
        int k, stale = 0;
        en_freeze(0);
        en_set(P_ROOT, 6);                       /* a tritone away: nothing in common to hide behind */
        en_pool(EN_MODE_POOL[MODE_LYDIAN]);
        en_regrow();
        run(3.0f);
        for (k = 0; k < EN_NPAD; k++)
            if (en_pad_amp[k] > 0.02f && en_pad_note[k] != en_chord[k]) {
                printf("  voice %d sings %d, the chord has %d (amp %.2f)\n", k, en_pad_note[k], en_chord[k],
                       (double)en_pad_amp[k]);
                stale++;
            }
        CHECK(!stale, "%d voices still in the old key after 3 s", stale);
        en_set(P_ROOT, 2);
    }

    /* a pool change: the chord's tones are in it */
    {
        uint32_t pent = EN_MODE_POOL[MODE_PENT_MIN];
        en_pool(pent);
        run(0.1f);
        for (i = 0; i < 4; i++)
            CHECK(pent >> ((en_chord[i] - 2 + 120) % 12) & 1u, "chord note %d not in the pool", en_chord[i]);
    }

    /* every setting at its extremes: still bounded */
    {
        int p;
        for (p = 0; p < P_NPARAMS; p++)
            en_set(p, en_param_info(p)->hi);
        en_set(P_MIDI, 1);
        peak = 0.0f;
        run(30.0f);
        CHECK(!bad && peak < 1.0f, "all at max: peak %f", (double)peak);
        for (p = 0; p < P_NPARAMS; p++)
            en_set(p, en_param_info(p)->lo);
        en_set(P_TEMPO, 30);
        run(10.0f);
        CHECK(!bad, "all at min: NaN");
    }

    /* Bell tone and Bell oct are heard: brighter (zero crossings) and higher with them, bells alone */
    {
        float zc[3];
        int k;
        static const int TONE[3] = {0, 100, 0}, OCT[3] = {0, 0, 2};
        for (k = 0; k < 3; k++) {
            int b2, j;
            long z = 0;
            float prev = 0.0f;
            en_init();
            en_set(P_PAD, 0);
            en_set(P_BASS, 0);
            en_set(P_PULSE, 0);
            en_set(P_ECHO, 0);
            en_set(P_REVERB, 0);
            en_set(P_DENSITY, 100);
            en_set(P_BTONE, TONE[k]);
            en_set(P_BOCT, OCT[k]);
            en_play(1);
            for (b2 = 0; b2 < 20 * 44100 / 256; b2++) {
                en_render(buf, 256, 2048);
                for (j = 0; j < 256; j++) {
                    float x = (float)buf[2 * j];
                    z += (x > 0.0f) != (prev > 0.0f);
                    prev = x;
                }
            }
            zc[k] = (float)z;
        }
        CHECK(zc[1] > 2.5f * zc[0], "Bell tone 100 not brighter: %.0f vs %.0f", (double)zc[1], (double)zc[0]);
        CHECK(zc[2] > 1.8f * zc[0], "Bell oct +2 not higher: %.0f vs %.0f", (double)zc[2], (double)zc[0]);
        en_init();
        en_play(1);
    }

    /* ENV: AMB-1's own, the pad breathes in and the bass fades in over seconds; the patch's, both struck at once, the
     * patch's envelopes heard (its sound 0.3 s in, the pad's level then, the bass's) */
    {
        float lv[2][3];
        int e, b2, j;
        for (e = 0; e < 2; e++) {
            float pk = 0.0f;
            en_init();
            en_set(P_BELLS, 0);
            en_set(P_PULSE, 0);
            en_set(P_ECHO, 0);
            en_set(P_REVERB, 0);
            en_set(P_PL(VOX_PAD, PL_ENV), e);
            en_set(P_PL(VOX_BASS, PL_ENV), e);
            en_play(1);
            for (b2 = 0; b2 < (int)(0.3f * 44100.0f) / 256; b2++) {
                en_render(buf, 256, 2048);
                for (j = 0; j < 512; j++)
                    if (fabsf((float)buf[j]) > pk)
                        pk = fabsf((float)buf[j]);
            }
            lv[e][0] = pk, lv[e][1] = en_pad_level, lv[e][2] = en_bass_level;
        }
        CHECK(lv[1][0] > 3.0f * lv[0][0], "ENV Patch not struck at once: peak %f vs %f", (double)lv[1][0],
              (double)lv[0][0]);
        CHECK(lv[1][1] > 0.5f && lv[0][1] < 0.2f, "the pad's level 0.3 s in: %f (Patch), %f (Breath)",
              (double)lv[1][1], (double)lv[0][1]);
        CHECK(lv[1][2] > 0.9f && lv[0][2] < 0.2f, "the bass's level 0.3 s in: %f (Patch), %f (Held)",
              (double)lv[1][2], (double)lv[0][2]);
        en_init();
        en_play(1);
    }

    /* MOD: silent at depth 0; at depth 100 every generator moves, each its own way, inside -1..1, every target and
     * shape bounded and never NaN */
    {
        int k, g, d, moved = 0, apart = 0, out = 0;
        float lo[EN_NMOD], hi[EN_NMOD];
        vox_all(MS_DEPTH, 0);
        run(1.0f);
        for (g = 0; g < EN_NMOD; g++)
            CHECK(en_mod[0][g] == 0.0f, "MOD at depth 0: generator %d at %f", g, (double)en_mod[0][g]);
        vox_all(MS_DEPTH, 100);
        vox_all(MS_RATE, 80);
        vox_all(MS_SHAPE, SHAPE_WALK);
        for (g = 0; g < EN_NMOD; g++)
            lo[g] = hi[g] = en_mod[0][g];
        for (k = 0; k < 40; k++) {
            run(0.25f);
            for (g = 0; g < EN_NMOD; g++) {
                lo[g] = en_mod[0][g] < lo[g] ? en_mod[0][g] : lo[g];
                hi[g] = en_mod[0][g] > hi[g] ? en_mod[0][g] : hi[g];
                out += en_mod[0][g] < -1.0f || en_mod[0][g] > 1.0f || en_mod[0][g] != en_mod[0][g];
            }
            apart += en_mod[0][0] != en_mod[0][1];
        }
        for (g = 0; g < EN_NMOD; g++)
            moved += hi[g] - lo[g] > 0.2f;
        CHECK(!out, "MOD out of -1..1 (%d)", out);
        CHECK(moved == EN_NMOD, "MOD: %d of %d generators moved", moved, EN_NMOD);
        CHECK(apart > 30, "MOD: the pad's first two voices together (%d of 40 apart)", apart);
        for (d = 0; d < EN_NMODDEST; d++)
            for (k = 0; k < 2; k++) {
                vox_all(MS_DEST, d);
                vox_all(MS_SHAPE, k ? SHAPE_CHAOS : SHAPE_SINE);
                peak = 0.0f;
                run(3.0f);
                CHECK(!bad && peak < 1.0f, "MOD target %d shape %d: peak %f", d, k, (double)peak);
            }
        /* every shape: bounded, and (but the envelope, which waits for notes) moving */
        vox_all(MS_DEST, MOD_ALL);
        for (k = 0; k < EN_NMODSHAPE; k++) {
            float l0 = 9.0f, h0 = -9.0f;
            int j;
            vox_all(MS_SHAPE, k);
            out = 0;
            peak = 0.0f;
            for (j = 0; j < 30; j++) {
                run(0.1f);
                l0 = en_mod[0][0] < l0 ? en_mod[0][0] : l0;
                h0 = en_mod[0][0] > h0 ? en_mod[0][0] : h0;
                for (g = 0; g < EN_NMOD; g++)
                    out += en_mod[0][g] < -1.0f || en_mod[0][g] > 1.0f || en_mod[0][g] != en_mod[0][g];
            }
            CHECK(!out && !bad && peak < 1.0f, "MOD shape %d: out %d, peak %f", k, out, (double)peak);
            CHECK(k == SHAPE_ENV || h0 - l0 > 0.3f, "MOD shape %d: moved only %f", k, (double)(h0 - l0));
        }
        /* SPREAD 0: the pad's four as one; 100: apart */
        vox_all(MS_SHAPE, SHAPE_SINE);
        vox_all(MS_SPREAD, 0);
        run(1.0f);
        CHECK(fabsf(en_mod[0][0] - en_mod[0][1]) < 1e-3f && fabsf(en_mod[0][0] - en_mod[0][3]) < 1e-3f,
              "MOD spread 0: the pad's voices apart (%f %f %f)", (double)en_mod[0][0], (double)en_mod[0][1], (double)en_mod[0][3]);
        vox_all(MS_SPREAD, 100);
        /* SYNC Tempo: a square at 1/4, tempo 120: two cycles a second, four sign changes */
        {
            int flips = 0, j, s0;
            char t[16];
            en_set(P_TEMPO, 120);
            vox_all(MS_SYNC, SYNC_TEMPO);
            vox_all(MS_RATE, 60);                 /* 60 * 12 / 101: the eighth division, 1/4 */
            vox_all(MS_SHAPE, SHAPE_SQUARE);
            run(0.5f);
            en_param_text(P_MS(VOX_PAD, 0, MS_RATE), 60, t);
            CHECK(!strcmp(t, "1/4"), "MOD sync tempo: rate shows %s", t);
            s0 = en_mod[0][0] > 0.0f;
            for (j = 0; j < 861; j++) {          /* 5 s, a block (5.8 ms) at a time */
                run(0.006f);
                if ((en_mod[0][0] > 0.0f) != s0) {
                    s0 = !s0;
                    flips++;
                }
            }
            CHECK(flips >= 18 && flips <= 22, "MOD sync tempo 1/4 at 120: %d sign changes in 5 s (20)", flips);
            vox_all(MS_SYNC, SYNC_FREE);
            en_set(P_TEMPO, 72);
        }
        /* FADE and SMOOTH at their ends, NOTE sync, the envelope: bounded */
        vox_all(MS_FADE, 100);
        vox_all(MS_SMOOTH, 100);
        vox_all(MS_SYNC, SYNC_NOTE);
        vox_all(MS_SHAPE, SHAPE_ENV);
        peak = 0.0f;
        run(10.0f);
        CHECK(!bad && peak < 1.0f, "MOD fade, smooth, note sync, env: peak %f", (double)peak);
        vox_all(MS_FADE, 0);
        vox_all(MS_SMOOTH, 0);
        vox_all(MS_SYNC, SYNC_FREE);
        vox_all(MS_SHAPE, SHAPE_SINE);
        vox_all(MS_DEPTH, 0);
    }

    /* MOD's FM targets and the FM edits are heard on the bells alone (zero crossings: brighter): RATIO (harmonic
     * steps up) and FEEDBACK (more of it) one-sided at full depth; FM's Index +24 dB and Ratio x4. And LEVEL quieter,
     * on the pad alone (the bells' level wanders 2 dB from one garden to the next) */
    {
        static const int DEST[3] = {-1, MOD_RATIO, MOD_FEEDBACK};
        float zc[7], rms[2];
        int k;
        for (k = 0; k < 7; k++) {
            int b2, j;
            long z = 0;
            double s2 = 0.0;
            float prev = 0.0f;
            en_init();
            en_set(P_PAD, k >= 5 ? 70 : 0);
            en_set(P_BELLS, k >= 5 ? 0 : 65);
            en_set(P_BASS, 0);
            en_set(P_PULSE, 0);
            en_set(P_ECHO, 0);
            en_set(P_REVERB, 0);
            en_set(P_DENSITY, 100);
            if (k < 3) {
                en_set(P_MS(VOX_BELLS, 0, MS_DEPTH), DEST[k] < 0 ? 0 : 100);
                en_set(P_MS(VOX_BELLS, 0, MS_DEST), DEST[k] < 0 ? 0 : DEST[k]);
                en_set(P_MS(VOX_BELLS, 0, MS_RATE), 60);
            } else if (k == 3) {
                en_set(P_VX(VOX_BELLS, VX_INDEX), 24);
            } else if (k == 4) {
                patch_set(VOX_BELLS, EP_OPN(6) + EP_FC, 12);  /* the bell's modulator x12 (from x3) */
            } else if (k == 6) {
                en_set(P_MS(VOX_PAD, 0, MS_DEPTH), 100);
                en_set(P_MS(VOX_PAD, 0, MS_DEST), MOD_LEVEL);
                en_set(P_MS(VOX_PAD, 0, MS_RATE), 80);   /* (fast: its mean, -3 dB, over the 20 s) */
            }
            en_play(1);
            for (b2 = 0; b2 < 20 * 44100 / 256; b2++) {
                en_render(buf, 256, 2048);
                for (j = 0; j < 256; j++) {
                    float x = (float)buf[2 * j];
                    z += (x > 0.0f) != (prev > 0.0f);
                    prev = x;
                    s2 += (double)x * (double)x;
                }
            }
            zc[k] = (float)z;
            if (k >= 5)
                rms[k - 5] = (float)s2;
        }
        CHECK(zc[1] > 1.15f * zc[0], "MOD Ratio not brighter: %.0f vs %.0f", (double)zc[1], (double)zc[0]);
        CHECK(zc[2] > 1.05f * zc[0], "MOD Feedback not brighter: %.0f vs %.0f", (double)zc[2], (double)zc[0]);
        CHECK(zc[3] > 1.3f * zc[0], "FM Index +24 not brighter: %.0f vs %.0f", (double)zc[3], (double)zc[0]);
        CHECK(zc[4] > 1.15f * zc[0], "OP6 x12 not brighter: %.0f vs %.0f", (double)zc[4], (double)zc[0]);
        CHECK(rms[1] < 0.7f * rms[0], "MOD Level not quieter: %g vs %g", (double)rms[1], (double)rms[0]);
        printf("  the bells, zero crossings: off %.0f, MOD ratio %.0f, MOD feedback %.0f, FM index +24 %.0f, "
               "OP6 x12 %.0f\n", (double)zc[0], (double)zc[1], (double)zc[2], (double)zc[3], (double)zc[4]);
        en_init();
        en_play(1);
    }

    /* every voice's every patch, alone: heard, bounded; changed while it sounds (the pad crossing over, the bass
     * struck anew): bounded */
    {
        static const int LAYER[EN_NVOX] = {P_PAD, P_BELLS, P_BASS};
        static const int NP[EN_NVOX] = {EN_PADPATCHES, EN_BELLPATCHES, EN_BASSPATCHES};
        int v, p, k;
        for (v = 0; v < EN_NVOX; v++)
            for (p = 0; p < NP[v]; p++) {
                en_init();
                for (k = 0; k < EN_NVOX; k++)
                    en_set(LAYER[k], k == v ? 100 : 0);
                en_set(P_PULSE, 0);
                en_set(P_DENSITY, 100);
                en_set(EN_VOX_PATCH[v], p);
                en_play(1);
                peak = 0.0f;
                run(8.0f);
                CHECK(!bad && peak < 1.0f && peak > 0.02f, "voice %d patch %d: peak %f", v, p, (double)peak);
            }
        en_init();
        en_play(1);
        run(3.0f);
        peak = 0.0f;
        for (k = 0; k < 30; k++) {
            for (v = 0; v < EN_NVOX; v++) {
                en_set(EN_VOX_PATCH[v], k % NP[v]);
                patch_set(v, EP_OPN(1 + k % 6) + EP_R1, (k * 37) % 100);
                patch_set(v, EP_OPN(1 + (k + 3) % 6) + EP_L1 + 2, (k * 53) % 100);
                patch_set(v, EP_ALG, (k * 7) % 32);
            }
            run(0.2f);
        }
        run(3.0f);
        CHECK(!bad && peak < 1.0f, "patches and envelopes changed while playing: peak %f", (double)peak);
        en_init();
        en_play(1);
    }

    /* the patches: every voice's own six packed as a DX7 bank packs them come back the same (en_unpack); erased
     * flash and a byte out of range are not a voice; every byte within its range */
    {
        int v, s, k, same = 1, inrange = 1;
        uint8_t p[EN_PATCH + 1], q[EN_PATCH + 1], m[128];
        for (v = 0; v < EN_NVOX; v++)
            for (s = 0; s < EN_NFACT; s++) {
                en_factory(v, s, 50, p);
                for (k = 0; k < EN_PATCH; k++)
                    inrange &= p[k] <= en_patch_max(k);
                pack(p, m);
                same &= en_unpack(m, q) && !memcmp(p, q, EN_PATCH);
            }
        CHECK(inrange, "a factory patch's byte out of its range");
        CHECK(same, "pack, unpack: not the same patch");
        memset(m, 0xFF, sizeof m);
        CHECK(!en_unpack(m, q), "erased flash taken for a voice");
        en_factory(VOX_PAD, 0, 50, p);
        pack(p, m);
        m[14] = 120;                             /* OP6's level, past 99 */
        CHECK(en_unpack(m, q) && q[EP_OL] == 99, "a level of 120 not held to 99");
    }

    /* a bank's voice sent to a voice (bytes and done, as the FM page's Patch does): heard, bounded; then its operators
     * edited while it sounds (as the OP pages do), every algorithm: bounded */
    {
        uint8_t p[EN_PATCH + 1];
        int k, j;
        en_init();
        en_factory(VOX_BELLS, 3, 50, p);         /* (the tubular bell, as a bank's voice: source 6) */
        p[EP_ALG] = 0;
        en_set(P_BELLPATCH, EN_NFACT);
        for (k = 0; k < EN_PATCH; k++)
            en_patch_byte(VOX_BELLS, k, p[k]);
        en_patch_done(VOX_BELLS, 1);
        en_set(P_PAD, 0);
        en_set(P_BASS, 0);
        en_set(P_DENSITY, 100);
        en_play(1);
        peak = 0.0f;
        run(6.0f);
        CHECK(!bad && peak > 0.02f && peak < 1.0f, "a bank's voice on the bells: peak %f", (double)peak);
        en_set(P_PAD, 60);
        en_set(P_BASS, 45);
        peak = 0.0f;
        for (k = 0; k < 64; k++) {
            for (j = 0; j < EN_NVOX; j++) {
                patch_set(j, EP_OPN(1 + k % 6) + EP_OL, (k * 41) % 100);
                patch_set(j, EP_OPN(1 + (k + 1) % 6) + EP_FC, (k * 13) % 32);
                patch_set(j, EP_OPN(1 + (k + 2) % 6) + EP_FF, (k * 29) % 100);
                patch_set(j, EP_ALG, k % 32);
                patch_set(j, EP_FB, k % 8);
            }
            run(0.15f);
        }
        CHECK(!bad && peak < 1.0f, "operators edited while sounding: peak %f", (double)peak);
        en_init();
        en_play(1);
    }

    /* five modulators a voice, every target (the operators' too) and shape: bounded; LFO PM: bounded */
    {
        int v, s, d;
        en_init();
        en_play(1);
        for (d = 0; d < EN_NMODDEST; d += 3) {
            for (v = 0; v < EN_NVOX; v++)
                for (s = 0; s < EN_NSLOT; s++) {
                    en_set(P_MS(v, s, MS_DEPTH), 100);
                    en_set(P_MS(v, s, MS_DEST), (d + s * 7) % EN_NMODDEST);
                    en_set(P_MS(v, s, MS_SHAPE), (d + s) % EN_NMODSHAPE);
                    en_set(P_MS(v, s, MS_RATE), 40 + 10 * s);
                }
            peak = 0.0f;
            run(1.5f);
            CHECK(!bad && peak < 1.0f, "five modulators, targets from %d: peak %f", d, (double)peak);
        }
        for (v = 0; v < EN_NVOX; v++)
            for (s = 0; s < EN_NSLOT; s++)
                en_set(P_MS(v, s, MS_DEPTH), 0);
        run(2.0f);
        for (s = 0; s < EN_NSLOT; s++)
            for (d = 0; d < EN_NMOD; d++)
                CHECK(en_mod[s][d] == 0.0f, "modulator %d generator %d not back at rest: %f", s, d,
                      (double)en_mod[s][d]);
    }

    /* the operators' targets are heard: OP6 (the bells' modulator) Level at full depth makes them brighter or darker
     * than without, its Ratio brighter */
    {
        float zc[3];
        int k;
        static const int DEST[3] = {-1, MOD_OP + 5 * OPT_N + OPT_RATIO,
                                    MOD_OP + 5 * OPT_N + OPT_FINE};
        for (k = 0; k < 3; k++) {
            int b2, j;
            long z = 0;
            float prev = 0.0f;
            en_init();
            en_set(P_PAD, 0);
            en_set(P_BASS, 0);
            en_set(P_PULSE, 0);
            en_set(P_ECHO, 0);
            en_set(P_REVERB, 0);
            en_set(P_DENSITY, 100);
            if (DEST[k] >= 0) {
                en_set(P_MS(VOX_BELLS, 2, MS_DEPTH), 100);
                en_set(P_MS(VOX_BELLS, 2, MS_DEST), DEST[k]);
                en_set(P_MS(VOX_BELLS, 2, MS_RATE), 60);
            }
            en_play(1);
            for (b2 = 0; b2 < 20 * 44100 / 256; b2++) {
                en_render(buf, 256, 2048);
                for (j = 0; j < 256; j++) {
                    float x = (float)buf[2 * j];
                    z += (x > 0.0f) != (prev > 0.0f);
                    prev = x;
                }
            }
            zc[k] = (float)z;
        }
        CHECK(zc[1] > 1.15f * zc[0], "OP6 Ratio (the third modulator) not brighter: %.0f vs %.0f", (double)zc[1],
              (double)zc[0]);
        printf("  the bells, zero crossings: off %.0f, OP6 ratio %.0f, OP6 fine %.0f\n", (double)zc[0], (double)zc[1],
               (double)zc[2]);
        en_init();
        en_play(1);
    }

    /* 1.3.95. TRANSPOSE: the patch's (24 C3) moves its pitch, an octave up at 36 (the bass alone: twice the zero
     * crossings); a quick run of patches through the mailbox ends on its last, as one load of it does; a bank's value
     * past its range held to it (a voice, not refused); an external clock's tempo is the one that plays */
    {
        float lv2[2];
        uint8_t p[EN_PATCH + 1], q[EN_PATCH + 1], m[128];
        long zc[2];
        int t, k, b2, j;
        for (t = 0; t < 2; t++) {
            float prev = 0.0f;
            en_init();
            en_set(P_PAD, 0);
            en_set(P_BELLS, 0);
            en_set(P_PULSE, 0);
            en_set(P_ECHO, 0);
            en_set(P_REVERB, 0);
            en_factory(VOX_BASS, 0, 50, p);
            p[EP_TRNSP] = (uint8_t)(t ? 36 : 24);
            en_set(P_VX(VOX_BASS, VX_OWN), 1);
            en_patch_load(VOX_BASS, p, 1);
            en_play(1);
            run(4.0f);
            zc[t] = 0;
            for (b2 = 0; b2 < 4 * 44100 / 256; b2++) {
                en_render(buf, 256, 2048);
                for (j = 0; j < 256; j++) {
                    float x = (float)buf[2 * j];
                    zc[t] += (x > 0.0f) != (prev > 0.0f);
                    prev = x;
                }
            }
        }
        CHECK(zc[1] > 1.8f * zc[0] && zc[1] < 2.2f * zc[0], "TRANSPOSE +12: zero crossings %ld vs %ld", zc[1], zc[0]);

        for (t = 0; t < 2; t++) {                /* one load of Pick, or 900 of others ending on it: the bass alone,
                                                  * heard alike (its zero crossings, its level; the space's tails of
                                                  * before are not cleared by an init, so not sample for sample) */
            float prev = 0.0f;
            en_init();
            en_set(P_PAD, 0);
            en_set(P_BELLS, 0);
            en_set(P_PULSE, 0);
            en_set(P_ECHO, 0);
            en_set(P_REVERB, 0);
            en_set(P_VX(VOX_BASS, VX_OWN), 1);
            en_factory(VOX_BASS, 2, 50, q);
            if (t)
                for (k = 0; k < 900; k++) {   /* (in one drain, the queue short of full) */
                    en_factory(VOX_BASS, k % 6, 50, p);
                    en_patch_load(VOX_BASS, p, 1);
                }
            en_patch_load(VOX_BASS, q, 1);
            en_play(1);
            run(3.0f);
            zc[t] = 0;
            lv2[t] = 0.0f;
            for (b2 = 0; b2 < 2 * 44100 / 256; b2++) {
                en_render(buf, 256, 2048);
                for (j = 0; j < 256; j++) {
                    float x = (float)buf[2 * j];
                    zc[t] += (x > 0.0f) != (prev > 0.0f);
                    lv2[t] += x * x * 1e-12f;
                    prev = x;
                }
            }
        }
        CHECK(zc[0] > 100 && labs(zc[1] - zc[0]) <= zc[0] / 50 && fabsf(lv2[1] - lv2[0]) <= 0.03f * lv2[0],
              "900 patches through the mailbox: not as one load (crossings %ld vs %ld, level %f vs %f)", zc[1], zc[0],
              (double)lv2[1], (double)lv2[0]);

        en_factory(VOX_PAD, 0, 50, p);
        pack(p, m);
        m[14] = 127;                             /* OP6's output level, as an old bank has it */
        m[118] = 1;                              /* a control character in its name */
        CHECK(en_unpack(m, q) && q[EP_OL] == 99 && q[EP_NAME] == ' ', "a value past its range: %d", q[EP_OL]);
        for (k = 0; k < 128; k++)
            m[k] = 0;
        CHECK(!en_unpack(m, q), "a voice of zeros taken");

        en_init();
        en_set(P_TEMPO, 120);
        en_play(1);
        run(0.5f);
        en_clock(EN_CLK_START);
        for (b2 = 0; b2 < 6 * 24; b2++) {        /* six beats at 60, a pulse every 1837.5 samples */
            int n = 1837 + (b2 & 1), r;
            en_clock(EN_CLK_TICK);
            for (r = 0; r < n; r += 256)
                en_render(buf, (uint32_t)(n - r < 256 ? n - r : 256), 2048);
        }
        CHECK(en_tempo > 58.5f && en_tempo < 61.5f, "an external clock at 60: the tempo %f", (double)en_tempo);
        run(2.0f);                               /* the clock gone: the knob's */
        CHECK(en_tempo == 120.0f, "the clock gone: the tempo %f", (double)en_tempo);
        en_init();
        en_play(1);
    }

    /* stop: the tails die away */
    for (i = 0; i < P_NPARAMS; i++)
        en_set(i, en_param_info(i)->def);
    en_play(0);
    run(40.0f);
    peak = 0.0f;
    run(1.0f);
    CHECK(peak < 0.01f, "quiet after stopping (%f)", (double)peak);
    CHECK(midi_count > 50u, "MIDI out (%u)", midi_count);

    printf(fails ? "amb: %d FAILED\n" : "amb: ok\n", fails);
    return fails != 0;
}
