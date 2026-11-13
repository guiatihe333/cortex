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
/* a song's record for the tests: 4 bars at 100 BPM, C major; C and F a half bar each; a melody a quarter (C E G A
 * on C, F A C' E on F: the chord's own), the bass each half bar (C2, F2) */
static int test_song(uint8_t *r)
{
    static const uint8_t MEL_C[4] = {72, 76, 79, 76}, MEL_F[4] = {77, 81, 84, 81};
    const int n = 64, nch = 8, mel = EN_SONG_HDR + 3 * nch;
    int i;
    for (i = 0; i < EN_SONG_HDR + 3 * nch + 4 * n; i++)
        r[i] = 0;
    r[0] = 'A', r[1] = 'S', r[2] = 'N', r[3] = 'G', r[4] = 1;
    r[5] = (uint8_t)(n >> 7), r[6] = (uint8_t)(n & 127), r[7] = 0, r[8] = 100, r[9] = 0;
    r[10] = 0x35, r[11] = 0x2A;                   /* Ionian: 0xAB5 */
    for (i = 0; i < 12; i++)
        r[12 + i] = (uint8_t)"TEST SONG   "[i];
    for (i = 0; i < nch; i++) {                  /* C (0, 4, 7), F (5, 9, 0) */
        const int m = i & 1 ? (1 << 5 | 1 << 9 | 1) : (1 | 1 << 4 | 1 << 7);
        r[EN_SONG_HDR + 3 * i] = (uint8_t)(m & 63);
        r[EN_SONG_HDR + 3 * i + 1] = (uint8_t)(m >> 6 & 63);
        r[EN_SONG_HDR + 3 * i + 2] = (uint8_t)(i & 1 ? 5 : 0);
    }
    for (i = 0; i < n; i += 2) {
        r[mel + i] = (i / 8) & 1 ? MEL_F[(i / 2) % 4] : MEL_C[(i / 2) % 4];
        r[mel + n + i] = 2;
    }
    for (i = 0; i < n; i += 8) {
        r[mel + 2 * n + i] = (uint8_t)((i / 8) & 1 ? 41 : 36);
        r[mel + 3 * n + i] = 8;
    }
    return EN_SONG_HDR + 3 * nch + 4 * n;
}

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
        CHECK(!strcmp(t, "2s"), "drift 0: %s", t);
        en_param_text(P_DRIFT, 100, t);
        CHECK(!strcmp(t, "5:00"), "drift 100: %s", t);
        en_param_text(P_ETIME, 3, t);
        CHECK(!strcmp(t, "1 bar"), "etime: %s", t);
        en_param_text(P_BOCT, 1, t);
        CHECK(!strcmp(t, "+1"), "boct: %s", t);
        en_param_text(P_EX(VOX_PAD, EX_STYLE), 0, t);
        CHECK(!strcmp(t, "Natural"), "style 0: %s", t);
        en_param_text(P_EX(VOX_PAD, EX_STYLE), 1, t);
        CHECK(!strcmp(t, "Breath"), "style 1: %s", t);
        en_param_text(P_EX(VOX_PAD, EX_MORPH_KIND), 0, t);
        CHECK(!strcmp(t, "Color"), "morph kind 0: %s", t);
        en_param_text(P_EX(VOX_PAD, EX_MORPH_KIND), 1, t);
        CHECK(!strcmp(t, "Compat"), "morph kind 1: %s", t);
        en_param_text(P_XR(VOX_PAD, 0, XR_SOURCE), XS_ENERGY, t);
        CHECK(!strcmp(t, "Energy"), "source: %s", t);
        en_param_text(P_XR(VOX_PAD, 0, XR_TARGET), XT_ATTACK, t);
        CHECK(!strcmp(t, "Attack"), "target: %s", t);
        en_param_text(P_XR(VOX_PAD, 0, XR_WHEN), XW_NOTE, t);
        CHECK(!strcmp(t, "Note"), "when: %s", t);
        en_param_text(P_XR(VOX_PAD, 0, XR_CURVE), XC_LINEAR, t);
        CHECK(!strcmp(t, "Linear"), "curve: %s", t);
        en_param_text(P_XR(VOX_PAD, 0, XR_DEPTH), 25, t);
        CHECK(!strcmp(t, "+25%"), "depth: %s", t);
        en_param_text(P_EX(VOX_BASS, EX_MORPH), 50, t);
        CHECK(!strcmp(t, "50%"), "morph: %s", t);
        en_param_text(P_VF(VOX_PAD, VF_ON), 1, t);
        CHECK(!strcmp(t, "On"), "vcf on: %s", t);
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
        en_set(P_PL(VOX_BELLS, PL_DENS), 80);
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
            en_set(P_PL(VOX_BELLS, PL_DENS), 100);
            en_set(P_BTONE, TONE[k]);
            en_set(P_PL(VOX_BELLS, PL_OCT), OCT[k]);
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
        for (d = 0; d <= EN_NMODDEST; d++)           /* (0: Off) */
            for (k = 0; k < 2; k++) {
                vox_all(MS_DEST, d);
                vox_all(MS_SHAPE, k ? SHAPE_CHAOS : SHAPE_SINE);
                peak = 0.0f;
                run(3.0f);
                CHECK(!bad && peak < 1.0f, "MOD target %d shape %d: peak %f", d, k, (double)peak);
            }
        /* every shape: bounded, and (but the envelope, which waits for notes) moving */
        vox_all(MS_DEST, MOD_ALL + 1);
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
        static float reference[20 * 44100];
        double reference_energy = 0.0, feedback_error = 0.0;
        float zc[7], rms[2];
        int k;
        for (k = 0; k < 7; k++) {
            int b2, j;
            long z = 0;
            double s2 = 0.0;
            float prev = 0.0f;
            en_init();
            en_set(P_EVOLVE, 0); /* Compare DSP on a stable musical sequence. */
            en_set(P_SEQ_HUMAN, 0);
            en_set(P_DRIFT, 100);
            en_set(P_PAD, k >= 5 ? 70 : 0);
            en_set(P_BELLS, k >= 5 ? 0 : 65);
            en_set(P_BASS, 0);
            en_set(P_PULSE, 0);
            en_set(P_ECHO, 0);
            en_set(P_REVERB, 0);
            en_set(P_PL(VOX_BELLS, PL_DENS), 100);
            if (k < 3) {
                en_set(P_MS(VOX_BELLS, 0, MS_DEPTH), DEST[k] < 0 ? 0 : 100);
                en_set(P_MS(VOX_BELLS, 0, MS_DEST), DEST[k] < 0 ? 0 : DEST[k] + 1);
                en_set(P_MS(VOX_BELLS, 0, MS_RATE), 60);
            } else if (k == 3) {
                en_set(P_VX(VOX_BELLS, VX_INDEX), 24);
            } else if (k == 4) {
                patch_set(VOX_BELLS, EP_OPN(6) + EP_FC, 12);  /* the bell's modulator x12 (from x3) */
            } else if (k == 6) {
                en_set(P_MS(VOX_PAD, 0, MS_DEPTH), 100);
                en_set(P_MS(VOX_PAD, 0, MS_DEST), MOD_LEVEL + 1);
                en_set(P_MS(VOX_PAD, 0, MS_RATE), 80);   /* (fast: its mean, -3 dB, over the 20 s) */
            }
            en_play(1);
            for (b2 = 0; b2 < 20 * 44100 / 256; b2++) {
                en_render(buf, 256, 2048);
                for (j = 0; j < 256; j++) {
                    float x = (float)buf[2 * j];
                    if (k == 0) { reference[b2*256+j] = x; reference_energy += (double)x*(double)x; }
                    if (k == 2) { const double delta = (double)x-(double)reference[b2*256+j]; feedback_error += delta*delta; }
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
        CHECK(reference_energy > 0.0 && feedback_error > reference_energy * 0.001, "MOD Feedback not heard: %.0f vs %.0f", (double)zc[2],
              (double)zc[0]);                    /* (brighter or rounder by the notes it meets: heard either way) */
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
                en_set(P_PL(VOX_BELLS, PL_DENS), 100);
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
        en_factory(VOX_BELLS, EN_FACT_BELL + 3, 50, p);   /* (the tubular bell, as a bank's voice) */
        p[EP_ALG] = 0;
        en_set(P_BELLPATCH, EN_NFACT);
        for (k = 0; k < EN_PATCH; k++)
            en_patch_byte(VOX_BELLS, k, p[k]);
        en_patch_done(VOX_BELLS, 1);
        en_set(P_PAD, 0);
        en_set(P_BASS, 0);
        en_set(P_PL(VOX_BELLS, PL_DENS), 100);
        en_play(1);
        peak = 0.0f;
        run(12.0f);
        CHECK(!bad && peak > 0.004f && peak < 1.0f, "a bank's voice on the bells: peak %f", (double)peak);
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
                    en_set(P_MS(v, s, MS_DEST), (d + s * 7) % EN_NMODDEST + 1);
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
            en_set(P_PL(VOX_BELLS, PL_DENS), 100);
            if (DEST[k] >= 0) {
                en_set(P_MS(VOX_BELLS, 2, MS_DEPTH), 100);
                en_set(P_MS(VOX_BELLS, 2, MS_DEST), DEST[k] + 1);
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
            en_factory(VOX_BASS, EN_FACT_BASS, 50, p);
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
            en_factory(VOX_BASS, EN_FACT_BASS + 2, 50, q);
            if (t)
                for (k = 0; k < 900; k++) {   /* (in one drain, the queue short of full) */
                    en_factory(VOX_BASS, EN_FACT_BASS + k % 6, 50, p);
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

    /* 1.3.96. The voices alike: every voice in every role (all three Chords, all Loops, all Basses) sounds, bounded,
     * never NaN; a voice changed to another role while playing goes on sounding; the ADSR's attack is heard (the
     * chord's level 0.3 s in: at once at 0, slow at 100) */
    {
        int r, v, b2, j, e;
        float lv[2];
        for (r = 0; r < EN_NROLE; r++) {
            en_init();
            for (v = 0; v < EN_NVOX; v++) {
                en_set(P_PL(v, PL_ROLE), r);
                en_set(P_PL(v, PL_DENS), 80);
            }
            en_play(1);
            peak = 0.0f;
            bad = 0;
            run(12.0f);
            CHECK(!bad && peak > 0.01f && peak < 1.0f, "all three voices role %d: peak %f, NaN %d", r, (double)peak, bad);
        }
        en_init();
        en_play(1);
        run(3.0f);
        en_set(P_PL(VOX_PAD, PL_ROLE), ROLE_LOOPS);   /* the first voice to the loops, the third to chords */
        en_set(P_PL(VOX_BASS, PL_ROLE), ROLE_CHORD);
        en_set(P_PL(VOX_PAD, PL_DENS), 80);
        peak = 0.0f;
        bad = 0;
        run(8.0f);
        CHECK(!bad && peak > 0.01f && peak < 1.0f, "roles changed while playing: peak %f", (double)peak);
        CHECK(en_pad_level > 0.0f, "the third voice's chord not shown (%f)", (double)en_pad_level);
        for (e = 0; e < 2; e++) {               /* the first voice alone, a chord on the ADSR: attack 0, then 100 */
            en_init();
            en_set(P_BELLS, 0);
            en_set(P_BASS, 0);
            en_set(P_PULSE, 0);
            en_set(P_PL(VOX_PAD, PL_ENV), ENV_ADSR);
            en_set(P_PL(VOX_PAD, PL_A), e ? 100 : 0);
            en_play(1);
            lv[e] = 0.0f;
            for (b2 = 0; b2 < (int)(0.3f * 44100.0f) / 256; b2++) {
                en_render(buf, 256, 2048);
                for (j = 0; j < 512; j++)
                    if (fabsf((float)buf[j]) > lv[e])
                        lv[e] = fabsf((float)buf[j]);
            }
        }
        CHECK(lv[0] > 4.0f * lv[1], "ADSR attack 0 vs 100, 0.3 s in: %f vs %f", (double)lv[0], (double)lv[1]);
        en_init();
        en_play(1);
    }

    /* 1.3.97. The reel: 3 s of the garden recorded (a new splice), then the garden silenced: the reel plays it back, at
     * about its level; stopped (SPEED at its middle), quiet; backwards, and as overlapping genes spread and an octave
     * off, sounding, bounded; overdubbed (sound on sound) it goes on; erased, quiet */
    {
        float live, back;
        en_init();
        en_play(1); en_tape(1);
        run(4.0f);
        peak = 0.0f;
        run(1.0f);
        live = peak;
        en_reel(EN_REEL_NEW);
        run(3.0f);
        en_reel(EN_REEL_DUB);                    /* (recording a new one: stop) */
        run(0.1f);
        CHECK(en_reel_state == 0 && en_reel_nspl == 1 && en_reel_len > 60000u && en_reel_len < 70000u,
              "the reel recorded: state %d, splices %d, %u samples", en_reel_state, en_reel_nspl, en_reel_len);
        en_set(P_PAD, 0);
        en_set(P_BELLS, 0);
        en_set(P_BASS, 0);
        en_set(P_PULSE, 0);
        en_set(P_ECHO, 0);
        en_set(P_REVERB, 0);
        en_set(P_RL(MIX), 100);
        run(3.0f);
        peak = 0.0f;
        bad = 0;
        run(2.0f);
        back = peak;
        CHECK(!bad && back > 0.25f * live && back < 3.0f * live, "the reel played back: %f (the garden %f)",
              (double)back, (double)live);
        en_set(P_RL(SPEED), 50);
        run(0.5f);
        peak = 0.0f;
        run(0.5f);
        CHECK(peak < 0.15f * back, "the reel stopped: %f (playing %f; the space's tails remain)", (double)peak,
              (double)back);
        en_set(P_RL(SPEED), 25);                 /* backwards, overlapping genes, spread, an octave off */
        en_set(P_RL(GENE), 40);
        en_set(P_RL(MORPH), 100);
        peak = 0.0f;
        bad = 0;
        run(3.0f);
        CHECK(!bad && peak > 0.1f * live && peak < 1.0f, "backwards, as genes: %f", (double)peak);
        en_set(P_RL(SPEED), 75);
        en_set(P_PAD, 60);
        en_reel(EN_REEL_DUB);                    /* overdub the first splice */
        run(1.0f);
        CHECK(en_reel_state == 2, "overdubbing: state %d", en_reel_state);
        en_reel(EN_REEL_DUB);
        run(0.2f);
        CHECK(en_reel_state == 0 && en_reel_nspl == 1, "the overdub stopped: state %d", en_reel_state);
        en_reel(EN_REEL_ERASE);
        en_set(P_PAD, 0);
        run(1.0f);
        peak = 0.0f;
        run(0.5f);
        CHECK(en_reel_nspl == 0 && peak < 0.15f * back, "erased: %d splices, %f", en_reel_nspl, (double)peak);
        en_init();
        en_play(1); en_tape(1);
    }

    /* 1.5.24. The reel's auto-chop (EDIT held on its pages): the splice the UI picks cut at its attacks, the audio left
     * where it is; no attack, four alike; too short, nothing */
    {
        static const int16_t quiet[256] = {0};
        int16_t usb[256];
        int b, j, g, r = 0, guard, hit;
        uint32_t len, k;
        en_init();
        en_set(P_RL(SRC), 5);                    /* USB: the test's own signal */
        en_reel(EN_REEL_ERASE);
        run(0.1f);
        en_reel(EN_REEL_NEW);
        for (b = 0; b < 350; b++) {              /* 2 s: a burst every 0.25 s */
            for (j = 0; j < 256; j++) {
                g = b * 256 + j;
                usb[j] = (int16_t)(g % 11025 < 300 ? 12000.0f * sinf(6.2831853f * 660.0f * (float)g / 44100.0f) : 0.0f);
            }
            en_usb(usb, 256);
            en_render(buf, 256, 2048);
        }
        en_usb(quiet, 0);
        en_reel(EN_REEL_DUB);                    /* (recording a new one: stop) */
        run(0.2f);
        CHECK(en_reel_state == 0 && en_reel_nspl == 1, "the chop's tape: state %d, %d splices", en_reel_state,
              en_reel_nspl);
        len = en_reel_len;
        CHECK(len > 30000u && len < 50000u, "the chop's tape: %u samples", len);
        CHECK(en_reel_chop(0) == 0, "the chop started");
        for (guard = 0; guard < 1000 && (r = en_reel_chop_step()) == 1; guard++)
            ;
        CHECK(r == 0, "the chop's frames: %d", r);
        en_render(buf, 256, 2048);               /* (the engine cuts it) */
        CHECK(en_reel_nspl >= 3u && en_reel_nspl <= 8u, "the chop: %d splices", en_reel_nspl);
        CHECK(en_reel_s0[0] == 0u && en_reel_s1[en_reel_nspl - 1] == len, "the chop's ends: %u .. %u (of %u)",
              en_reel_s0[0], en_reel_s1[en_reel_nspl - 1], len);
        hit = 0;
        for (k = 0; k < en_reel_nspl; k++) {
            CHECK(en_reel_s0[k] % 256u == 0u, "the chop's splice %u starts at %u", k, en_reel_s0[k]);
            CHECK(en_reel_s1[k] - en_reel_s0[k] >= 2205u, "the chop's splice %u: %u samples", k,
                  en_reel_s1[k] - en_reel_s0[k]);
            if (k + 1u < en_reel_nspl)
                CHECK(en_reel_s1[k] == en_reel_s0[k + 1u], "the chop's splices %u and %u: %u, %u", k, k + 1u,
                      en_reel_s1[k], en_reel_s0[k + 1u]);
            if (k > 0u && en_reel_s0[k] > 5512u - 600u && en_reel_s0[k] < 5512u + 600u)
                hit = 1;
        }
        CHECK(hit, "the chop found an attack at 0.25 s: %d splices", en_reel_nspl);
        /* a steady tone: no attack, four alike */
        en_reel(EN_REEL_ERASE);
        run(0.1f);
        en_reel(EN_REEL_NEW);
        for (b = 0; b < 350; b++) {
            for (j = 0; j < 256; j++) {
                g = b * 256 + j;
                usb[j] = (int16_t)(12000.0f * (g < 2000 ? (float)g * (1.0f / 2000.0f) : 1.0f) *
                                   sinf(6.2831853f * 220.0f * (float)g / 44100.0f));
            }
            en_usb(usb, 256);
            en_render(buf, 256, 2048);
        }
        en_usb(quiet, 0);
        en_reel(EN_REEL_DUB);
        run(0.2f);
        len = en_reel_len;
        CHECK(en_reel_nspl == 1, "the steady tape: %d splices", en_reel_nspl);
        CHECK(en_reel_chop(0) == 0, "the steady chop started");
        for (guard = 0; guard < 1000 && (r = en_reel_chop_step()) == 1; guard++)
            ;
        en_render(buf, 256, 2048);
        CHECK(r == 0 && en_reel_nspl == 4u, "a steady splice: %d splices (frames %d)", en_reel_nspl, r);
        for (k = 0; k < en_reel_nspl; k++)
            CHECK(en_reel_s1[k] - en_reel_s0[k] >= 2205u, "the steady splice %u: %u samples", k,
                  en_reel_s1[k] - en_reel_s0[k]);
        /* too short for two pieces: nothing */
        en_reel(EN_REEL_ERASE);
        run(0.1f);
        en_reel(EN_REEL_NEW);
        for (b = 0; b < 24; b++) {               /* 0.14 s at 22.05 kHz */
            for (j = 0; j < 256; j++) {
                g = b * 256 + j;
                usb[j] = (int16_t)(12000.0f * sinf(6.2831853f * 220.0f * (float)g / 44100.0f));
            }
            en_usb(usb, 256);
            en_render(buf, 256, 2048);
        }
        en_usb(quiet, 0);
        en_reel(EN_REEL_DUB);
        run(0.2f);
        CHECK(en_reel_nspl == 1 && en_reel_chop(0) == -2, "a short splice: %d splices, chop %d", en_reel_nspl,
              en_reel_chop(0));
        en_reel(EN_REEL_ERASE);
        run(0.1f);
        CHECK(en_reel_chop(0) == -1, "an empty reel: chop %d", en_reel_chop(0));
        en_set(P_RL(SRC), 0);                    /* (the tests after: the mix again, not USB) */
        en_init();
        en_play(1); en_tape(1);
    }

    /* the reel at 1x is at the pitch it recorded: the bass alone (SOURCE Voice 3), its zero crossings live and back */
    {
        long zc[2];
        int t, b2, j;
        en_init();
        en_set(P_PAD, 0);
        en_set(P_BELLS, 0);
        en_set(P_PULSE, 0);
        en_set(P_ECHO, 0);
        en_set(P_REVERB, 0);
        en_set(P_RL(SRC), 3);
        en_set(P_RL(MIX), 0);                  /* (not heard while it records: the bass alone, live) */
        en_set(P_PL(VOX_BASS, PL_ENV), ENV_AMB);  /* (one note held: a steady pitch) */
        en_set(P_DRIFT, 100);
        en_play(1); en_tape(1);
        run(3.0f);
        en_reel(EN_REEL_NEW);
        for (t = 0; t < 2; t++) {
            float prev = 0.0f;
            if (t) {                             /* the reel back, the bass silenced */
                en_reel(EN_REEL_DUB);
                en_set(P_BASS, 0);
                en_set(P_RL(MIX), 100);
                run(1.0f);
            }
            zc[t] = 0;
            for (b2 = 0; b2 < 2 * 44100 / 256; b2++) {
                en_render(buf, 256, 2048);
                for (j = 0; j < 256; j++) {
                    float x = (float)buf[2 * j];
                    zc[t] += (x > 0.0f) != (prev > 0.0f);
                    prev = x;
                }
            }
        }
        CHECK(zc[0] > 100 && labs(zc[1] - zc[0]) < zc[0] / 20, "the reel's pitch: %ld crossings back, %ld live", zc[1],
              zc[0]);
        en_init();
        en_play(1); en_tape(1);
    }

    /* 1.3.98. MIX (dry/wet): at Wet the garden falls silent (the reel stopped: what is left is the space's tails), at
     * the middle both; FX after the reel: the reel goes through the reverb (its tail, the reel stopped, louder than with
     * the space before it, where the reel is dry) */
    {
        float both, wet, tail[2];
        int f;
        en_init();
        en_play(1); en_tape(1);
        run(3.0f);
        en_reel(EN_REEL_NEW);
        run(2.0f);
        en_reel(EN_REEL_DUB);
        run(1.0f);
        peak = 0.0f;
        run(1.0f);
        both = peak;
        en_set(P_RL(SPEED), 50);                 /* the reel stopped; Wet: the garden out too */
        en_set(P_RL(MIX), 100);
        run(4.0f);
        peak = 0.0f;
        run(1.0f);
        wet = peak;
        CHECK(wet < 0.2f * both, "Wet, the reel stopped: %f (both %f)", (double)wet, (double)both);
        en_set(P_PAD, 0);                        /* the garden silent, its tails gone */
        en_set(P_BELLS, 0);
        en_set(P_BASS, 0);
        en_set(P_PULSE, 0);
        en_set(P_ECHO, 0);
        en_set(P_SIZE, 30);
        run(12.0f);
        for (f = 0; f < 2; f++) {               /* the reel alone, the reverb full: its tail */
            en_set(P_RL(FX), f);
            en_set(P_RL(MIX), 50);
            en_set(P_REVERB, 100);
            en_set(P_RL(SPEED), 75);
            run(3.0f);
            en_set(P_RL(SPEED), 50);
            run(0.3f);
            peak = 0.0f;
            run(0.5f);
            tail[f] = peak;
            en_set(P_REVERB, 0);
            run(8.0f);
        }
        CHECK(tail[1] > 2.0f * tail[0], "FX after: the reel's reverb tail %f (before %f)", (double)tail[1],
              (double)tail[0]);
        en_init();
        en_play(1); en_tape(1);
    }

    /* the reel, read as it is recorded: silence overdubbed (SOS 0) never sounds louder than what it replaces (no burst
     * from the block being written, its bytes past the head of the coder's old state); after an erase, silence recorded plays silent (no gene hears what was there before) */
    {
        float before, during, ghost;
        en_init();
        en_set(P_ECHO, 0);
        en_set(P_REVERB, 0);
        en_play(1); en_tape(1);
        run(3.0f);
        en_reel(EN_REEL_NEW);
        run(2.0f);
        en_reel(EN_REEL_DUB);
        en_set(P_PAD, 0);
        en_set(P_BELLS, 0);
        en_set(P_BASS, 0);
        en_set(P_PULSE, 0);
        en_set(P_RL(MIX), 100);
        en_set(P_RL(GENE), 30);
        en_set(P_RL(MORPH), 70);
        en_set(P_RL(SOS), 0);
        run(2.0f);
        peak = 0.0f;
        run(3.0f);
        before = peak;
        en_reel(EN_REEL_DUB);                    /* silence over it (SOS 0: replacing it), the genes reading on */
        run(0.2f);
        peak = 0.0f;
        run(3.0f);
        during = peak;
        en_reel(EN_REEL_DUB);
        run(0.2f);
        CHECK(during < 1.55f * before, "overdubbing silence: %f (before %f)", (double)during, (double)before);
        en_reel(EN_REEL_ERASE);
        run(0.1f);
        en_reel(EN_REEL_NEW);                    /* silence onto the reel */
        run(2.0f);
        en_reel(EN_REEL_DUB);
        run(1.0f);
        peak = 0.0f;
        run(2.0f);
        ghost = peak;
        CHECK(ghost < 0.05f * before, "after an erase, silence recorded: %f (what was there %f)", (double)ghost,
              (double)before);
        en_init();
        en_play(1); en_tape(1);
    }

    /* 1.4.0: the reel's modulators: the first on SPEED, deep and quick, a square: the tape goes between -2x and +2x,
     * its output bounded, never NaN, the modulator's value swinging */
    {
        float lo = 1.0f, hi = -1.0f;
        int t;
        en_init();
        en_play(1); en_tape(1);
        run(2.0f);
        en_reel(EN_REEL_NEW);
        run(2.0f);
        en_reel(EN_REEL_DUB);
        en_set(P_TM(0, TARGET), 1);
        en_set(P_TM(0, DEPTH), 100);
        en_set(P_TM(0, RATE), 80);
        en_set(P_TM(0, SHAPE), SHAPE_SQUARE);
        peak = 0.0f;
        bad = 0;
        for (t = 0; t < 40; t++) {
            run(0.1f);
            lo = en_reel_mod[0] < lo ? en_reel_mod[0] : lo;
            hi = en_reel_mod[0] > hi ? en_reel_mod[0] : hi;
        }
        CHECK(!bad && peak < 1.0f && lo < -0.5f && hi > 0.5f, "the reel's modulator on Speed: %f .. %f, peak %f",
              (double)lo, (double)hi, (double)peak);
        en_init();
        en_play(1); en_tape(1);
    }

    /* 1.4.1: a modulator on another's depth: the voice's first modulator at depth 0, its second on M1's depth: the
     * first moves; the reel's the same (its second on M1's depth, M1 at 0) */
    {
        float m1 = 0.0f, t1 = 0.0f;
        int t;
        en_init();
        en_play(1); en_tape(1);
        en_set(P_MS(VOX_PAD, 0, MS_DEPTH), 0);
        en_set(P_MS(VOX_PAD, 0, MS_RATE), 80);
        en_set(P_MS(VOX_PAD, 1, MS_DEST), MOD_MSLOT + 0 + 1);
        en_set(P_MS(VOX_PAD, 1, MS_DEPTH), 100);
        en_set(P_MS(VOX_PAD, 1, MS_RATE), 60);
        en_set(P_MS(VOX_PAD, 1, MS_SHAPE), SHAPE_SQUARE);
        en_set(P_TM(0, DEPTH), 0);
        en_set(P_TM(0, RATE), 80);
        en_set(P_TM(1, TARGET), EN_NTMSET + 0 + 1);
        en_set(P_TM(1, DEPTH), 100);
        en_set(P_TM(1, RATE), 60);
        en_set(P_TM(1, SHAPE), SHAPE_SQUARE);
        en_reel(EN_REEL_NEW);
        run(1.5f);
        en_reel(EN_REEL_DUB);
        for (t = 0; t < 60; t++) {
            run(0.1f);
            m1 = fabsf(en_mod[0][0]) > m1 ? fabsf(en_mod[0][0]) : m1;
            t1 = fabsf(en_reel_mod[0]) > t1 ? fabsf(en_reel_mod[0]) : t1;
        }
        CHECK(m1 > 0.1f, "a voice's M1 moved by M2 on its depth: %f", (double)m1);
        CHECK(t1 > 0.1f, "the reel's M1 moved by M2 on its depth: %f", (double)t1);
        en_init();
        en_play(1); en_tape(1);
    }

    /* 1.4.4: a new splice while overdubbing: the block being overdubbed is finished as it was, so the splice decodes
     * as smoothly as before (no block of noise from the coder's state cut halfway) */
    {
        static const int16_t ST[89] = {
            7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97,
            107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
            876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871,
            5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623,
            27086, 29794, 32767};
        static const int8_t IX[16] = {-1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8};
        uint32_t n1, n3, bb;
        const uint8_t *hd = (const uint8_t *)en_reel_mem(1, &n1), *nb = (const uint8_t *)en_reel_mem(3, &n3);
        int jump = 0, clip = 0, k, prev = 0, bmax = 0;
        en_init();
        en_play(1); en_tape(1);
        en_reel(EN_REEL_ERASE);
        en_reel(EN_REEL_ERASE);
        en_set(P_RL(SOS), 100);
        en_reel(EN_REEL_NEW);
        run(2.0f);
        en_reel(EN_REEL_DUB);                    /* stop it */
        run(0.1f);
        en_reel(EN_REEL_DUB);                    /* overdub it, then a new splice halfway into a block */
        run(0.537f);
        en_reel(EN_REEL_NEW);
        run(0.3f);
        en_reel(EN_REEL_DUB);
        run(0.1f);
        for (bb = en_reel_s0[0] / 256u; bb < en_reel_s1[0] / 256u; bb++) {   /* each block decoded, as the reel does */
            int pred = (int16_t)(hd[bb * 4u] | hd[bb * 4u + 1u] << 8), idx = hd[bb * 4u + 2u], last = pred;
            if (bb > en_reel_s0[0] / 256u && abs(pred - prev) > bmax)
                bmax = abs(pred - prev);
            for (k = 0; k < 256; k++) {
                const uint32_t i = bb * 256u + (uint32_t)k;
                const int nn = i & 1u ? nb[i >> 1] >> 4 : nb[i >> 1] & 15, step = ST[idx];
                int d = step >> 3;
                if (nn & 4) d += step;
                if (nn & 2) d += step >> 1;
                if (nn & 1) d += step >> 2;
                pred = nn & 8 ? pred - d : pred + d;
                pred = pred > 32767 ? 32767 : pred < -32768 ? -32768 : pred;
                idx += IX[nn];
                idx = idx < 0 ? 0 : idx > 88 ? 88 : idx;
                jump += abs(pred - last) > 12000;
                clip += pred >= 32767 || pred <= -32768;
                last = pred;
            }
            prev = pred;
        }
        printf("  overdub then a new splice: the largest step between blocks %d\n", bmax);
        CHECK(en_reel_nspl == 2u, "a new splice after the overdub: %u", (unsigned)en_reel_nspl);
        CHECK(jump == 0 && clip == 0, "the overdubbed splice decodes smoothly: %d jumps, %d clipped", jump, clip);
        CHECK(bmax < 200, "no block of it cut halfway (a step at its end, back on course): %d", bmax);   /* (778 cut) */
        en_init();
        en_play(1); en_tape(1);
    }

    /* 1.4.7: an overload sheds: the reel's genes first (one at each), the render goes on */
    {
        int on0 = 0, on1 = 0, k;
        en_init();
        en_play(1); en_tape(1);
        en_reel(EN_REEL_ERASE);
        en_reel(EN_REEL_NEW);
        run(2.0f);
        en_reel(EN_REEL_DUB);
        en_set(P_RL(MORPH), 100);                /* (four genes at once) */
        run(1.0f);
        for (k = 0; k < 4; k++)
            on0 += en_reel_gene[k] >= 0;
        en_overload();                           /* one spike: nothing shed */
        run(0.006f);
        CHECK(en_sheds == 0u, "a single overload sheds nothing: %u", (unsigned)en_sheds);
        run(0.3f);                               /* (a quarter second since: one may shed) */
        en_overload();                           /* lasting (two renders): a gene shed */
        run(0.006f);
        en_overload();
        run(0.006f);
        for (k = 0; k < 4; k++)
            on1 += en_reel_gene[k] >= 0;
        CHECK(en_sheds == 1u && on1 >= 1, "a lasting overload sheds, the reel still heard: %d then %d genes, %u shed",
              on0, on1, (unsigned)en_sheds);
        for (k = 0; k < 40; k++) {               /* overloads on and on: never the last gene */
            en_overload();
            run(0.006f);
        }
        for (k = 0, on1 = 0; k < 4; k++)
            on1 += en_reel_gene[k] >= 0;
        CHECK(on1 >= 1 && en_reel_level > 0.0f, "overloads on and on: the reel still sounds (%d genes)", on1);
        en_init();
        en_play(1); en_tape(1);
    }

    /* 1.4.7: the score: notes come in as they are played, and stopping ends every one of them */
    {
        uint32_t k, open = 0, any = 0;
        en_init();
        en_play(1); en_tape(1);
        run(6.0f);
        for (k = 0; k < EN_SCORE_N; k++) {
            any += en_score[k].on != 0u;
            open += en_score[k].on && !en_score[k].off;
        }
        CHECK(any > 0u && open > 0u, "the score: %u notes, %u sounding", (unsigned)any, (unsigned)open);
        en_play(0); en_tape(0);
        run(0.1f);
        for (k = 0, open = 0; k < EN_SCORE_N; k++)
            open += en_score[k].on && !en_score[k].off;
        CHECK(open == 0u, "the score: stopped, none left sounding (%u)", (unsigned)open);
        en_init();
        en_play(1); en_tape(1);
    }

    /* 1.4.8: a splice too short to play is dropped as it stops; Organize on one plays the nearest that plays; a
     * modulated Morph stays out of the gaps */
    {
        int k, quiet = 0;
        en_init();
        en_play(1); en_tape(1);
        en_reel(EN_REEL_ERASE);
        en_reel(EN_REEL_NEW);
        run(2.0f);
        en_reel(EN_REEL_NEW);                    /* a second one, stopped at once */
        run(0.012f);
        en_reel(EN_REEL_DUB);
        run(0.05f);
        CHECK(en_reel_nspl == 1u, "a splice too short is dropped: %u splices", (unsigned)en_reel_nspl);
        en_set(P_RL(ORG), 1);                    /* (none there: the first plays) */
        en_set(P_RL(MORPH), 45);
        en_set(P_TM(0, TARGET), 1 + 3);          /* Morph, deep, quick */
        en_set(P_TM(0, DEPTH), 100);
        en_set(P_TM(0, RATE), 70);
        en_set(P_TM(0, SHAPE), SHAPE_SQUARE);
        run(1.0f);
        for (k = 0; k < 60; k++) {
            run(0.05f);
            quiet += en_reel_level < 1e-4f;
        }
        CHECK(quiet < 6, "a modulated Morph leaves no silence: %d of 60 quiet", quiet);
        en_init();
        en_play(1); en_tape(1);
    }

    /* 1.4.8: a key on a voice's page: the voice sounds (its tap), the note goes in the score, and lets go */
    {
        float lv = 0.0f;
        uint32_t k, open = 0;
        en_init();                               /* (resting: only the key sounds) */
        run(0.5f);
        en_key(VOX_BASS, 57, 1);                 /* voice 3: A3 */
        for (k = 0; k < 20; k++) {
            run(0.05f);
            lv = en_vox_level[VOX_BASS] > lv ? en_vox_level[VOX_BASS] : lv;
        }
        for (k = 0; k < EN_SCORE_N; k++)
            open += en_score[k].on && !en_score[k].off && en_score[k].voice == VOX_BASS;
        CHECK(lv > 0.01f, "a key sounds its voice: level %f", (double)lv);
        CHECK(open == 1u, "the key's note is in the score, sounding (%u)", (unsigned)open);
        en_key(VOX_BASS, 57, 0);
        run(0.1f);
        for (k = 0, open = 0; k < EN_SCORE_N; k++)
            open += en_score[k].on && !en_score[k].off && en_score[k].voice == VOX_BASS;
        CHECK(open == 0u, "let go: its note ends (%u)", (unsigned)open);
        en_init();
        en_play(1); en_tape(1);
    }

    /* 1.4.9: a song, FOLLOW 100: the melody (Loops) and the bass (Bass) only the song's notes, the chords its own;
     * FOLLOW 0: the chords still the song's */
    {
        static uint8_t rec[0x4000];
        uint32_t k, t0;
        int mel = 0, melbad = 0, bas = 0, basbad = 0, chordbad = 0, t2;
        test_song(rec);
        en_init();
        en_set(P_ROOT, 0);
        en_pool(0xAB5u);
        en_set(P_SG(FOLLOW), 100);
        en_set(P_SG(VARY), 0);
        en_song(rec);
        en_play(1); en_tape(1);
        run(0.3f);
        t0 = en_now;                             /* (the notes from here: the song's) */
        for (t2 = 0; t2 < 80; t2++) {
            run(0.05f);
            if (en_chord_root != 0 && en_chord_root != 5)
                chordbad++;
        }
        for (k = 0; k < EN_SCORE_N; k++) {
            const en_note_t *e = &en_score[k];
            if (!e->on || e->on < t0)
                continue;
            if (e->voice == VOX_BELLS) {
                mel++;
                melbad += !(e->note % 12 == 0 || e->note % 12 == 4 || e->note % 12 == 7 || e->note % 12 == 5 ||
                            e->note % 12 == 9);
            } else if (e->voice == VOX_BASS) {
                bas++;
                basbad += !(e->note % 12 == 0 || e->note % 12 == 5);
            }
        }
        CHECK(mel > 4 && melbad == 0, "a song followed: the melody's notes (%d, %d not its)", mel, melbad);
        CHECK(bas > 1 && basbad == 0, "a song followed: the bass's (%d, %d not its)", bas, basbad);
        CHECK(chordbad == 0, "a song: the chords its own (%d others)", chordbad);
        CHECK(en_song_pos < 64u, "a song goes round: at %u of 64", (unsigned)en_song_pos);
        en_song(0);
        en_init();
        en_play(1); en_tape(1);
    }

    /* 1.4.10: a song with all three voices on Loops: voice 1 its melody, voice 3 its bass (low), voice 2 its own loops
     * (it plays on: a voice not playing the song) */
    {
        static uint8_t rec[0x4000];
        uint32_t k, t0;
        int n2 = 0, n3 = 0, bad3 = 0, v, t1;
        test_song(rec);
        en_init();
        for (v = 0; v < EN_NVOX; v++)
            en_set(P_PL(v, PL_ROLE), ROLE_LOOPS);
        en_set(P_ROOT, 0);
        en_pool(0xAB5u);
        en_set(P_SG(FOLLOW), 100);
        en_set(P_SG(VARY), 0);
        en_song(rec);
        en_play(1); en_tape(1);
        run(1.0f);
        for (t1 = 0; t1 < 80; t1++) {           /* (the score holds 48: read it as it goes) */
            t0 = en_now;
            run(0.05f);
            for (k = 0; k < EN_SCORE_N; k++) {
                const en_note_t *e = &en_score[k];
                if (!e->on || e->on < t0)
                    continue;
                if (e->voice == 1)
                    n2++;
                if (e->voice == 2) {
                    n3++;
                    bad3 += !(e->note % 12 == 0 || e->note % 12 == 5) || e->note > 60;
                }
            }
        }
        CHECK(n2 > 0, "all on Loops: voice 2 plays on (%d notes)", n2);
        CHECK(n3 > 0 && bad3 == 0, "all on Loops: voice 3 the song's bass, low (%d notes, %d not)", n3, bad3);
        en_song(0);
        en_init();
        en_play(1); en_tape(1);
    }

    /* 1.4.10: no Loops voice (Bass, Bass, Chord): the higher Bass voice the song's melody, the other its bass */
    {
        static uint8_t rec[0x4000];
        uint32_t k, t0;
        int m = 0, mtone = 0, b = 0, bbad = 0, t1;
        test_song(rec);
        en_init();
        en_set(P_PL(0, PL_ROLE), ROLE_BASS);
        en_set(P_PL(1, PL_ROLE), ROLE_BASS);
        en_set(P_PL(2, PL_ROLE), ROLE_CHORD);
        en_set(P_PL(0, PL_OCT), 1);
        en_set(P_PL(1, PL_OCT), 0);
        en_set(P_ROOT, 0);
        en_pool(0xAB5u);
        en_set(P_SG(FOLLOW), 100);
        en_set(P_SG(VARY), 0);
        en_song(rec);
        en_play(1); en_tape(1);
        run(1.0f);
        for (t1 = 0; t1 < 80; t1++) {
            t0 = en_now;
            run(0.05f);
            for (k = 0; k < EN_SCORE_N; k++) {
                const en_note_t *e = &en_score[k];
                if (!e->on || e->on < t0)
                    continue;
                if (e->voice == 0) {
                    m++;
                    mtone += e->note % 12 == 4 || e->note % 12 == 7 || e->note % 12 == 9;
                } else if (e->voice == 1) {
                    b++;
                    bbad += !(e->note % 12 == 0 || e->note % 12 == 5);
                }
            }
        }
        CHECK(m > 8 && mtone > 4, "no Loops: the higher Bass voice the melody (%d notes, %d its E G A)", m, mtone);
        CHECK(b > 1 && bbad == 0, "no Loops: the other Bass voice the bass (%d notes, %d not)", b, bbad);
        en_song(0);
        en_init();
        en_play(1); en_tape(1);
    }

    /* 1.4.10: a song half sent (its notes erased flash, 0xFF): none of them played (the melody's voice, giving way to
     * the song at FOLLOW 100, silent), not a note each sixteenth */
    {
        static uint8_t rec[0x4000];
        uint32_t k, t0;
        int n1 = 0, t1, v, i;
        const int len = test_song(rec);
        for (i = EN_SONG_HDR + 3 * 8; i < len; i++)
            rec[i] = 0xFF;
        en_init();
        for (v = 0; v < EN_NVOX; v++)
            en_set(P_PL(v, PL_ROLE), ROLE_LOOPS);
        en_set(P_SG(FOLLOW), 100);
        en_set(P_SG(VARY), 0);
        en_song(rec);
        en_play(1); en_tape(1);
        run(0.5f);
        for (t1 = 0; t1 < 40; t1++) {
            t0 = en_now;
            run(0.05f);
            for (k = 0; k < EN_SCORE_N; k++)
                n1 += en_score[k].on && en_score[k].on >= t0 && en_score[k].voice == 0;
        }
        CHECK(n1 == 0, "a song half sent: its erased notes not played (%d)", n1);
        en_song(0);
        en_init();
        en_play(1); en_tape(1);
    }

    /* 1.4.9: SOURCE USB: the reel records what plays in over USB, not the garden: a tone in, the reel sounds; silence
     * in (the garden playing), the reel silent */
    {
        static int16_t tone[256], quiet[256];
        float lv[2] = {0.0f, 0.0f};
        int pass, b, i;
        for (i = 0; i < 256; i++)
            tone[i] = (int16_t)(12000.0f * sinf((float)i * (6.2831853f * 4.0f / 256.0f)));
        for (pass = 0; pass < 2; pass++) {
            en_init();
            en_play(1); en_tape(1);
            en_reel(EN_REEL_ERASE);
            en_set(P_RL(SRC), 5);
            en_set(P_RL(MIX), 100);              /* (the reel alone, full) */
            run(1.0f);
            en_reel(EN_REEL_NEW);
            for (b = 0; b < 300; b++) {          /* 1.7 s of it in, as app_render hands it on */
                en_usb(pass ? quiet : tone, 256);
                en_render(buf, 256, 2048);
            }
            en_usb(0, 0);
            en_reel(EN_REEL_DUB);                /* (stops the recording) */
            run(0.3f);
            for (b = 0; b < 20; b++) {
                run(0.05f);
                lv[pass] = en_reel_level > lv[pass] ? en_reel_level : lv[pass];
            }
        }
        CHECK(lv[0] > 0.02f && lv[1] < lv[0] * 0.1f, "SOURCE USB: a tone in, the reel %f; silence in, %f", (double)lv[0],
              (double)lv[1]);
        en_init();
        en_play(1); en_tape(1);
    }

    /* 1.4.11: AMS on a carrier (the LFO's amplitude modulation, at no depth) leaves its note as it is: the shift that
     * made 2^x was the wrong way, so a carrier with AMS sounded at full from the start, past its envelope */
    {
        uint8_t p[EN_PATCH + 1], q[EN_PATCH + 1];
        float e1[64], e2[64], h1, h2;
        int k;
        en_gen(0, 50, 0, 7u, p);
        memcpy(q, p, sizeof q);
        for (k = 0; k < 6; k++)
            q[k * EP_OP + EP_AMS] = 3;
        q[EP_LAMD] = 0;
        p[EP_LAMD] = 0;
        en_probe(p, 60, 2.0f, 0.5f, e1, 64, &h1);
        en_probe(q, 60, 2.0f, 0.5f, e2, 64, &h2);
        CHECK(e2[0] < 0.2f * e2[30] && fabsf(e2[30] - e1[30]) < 0.2f * e1[30],   /* (msfa's AMS: -1 dB at no depth) */
              "AMS on the carriers: the attack as without (%f then %f; without %f)", (double)e2[0], (double)e2[30],
              (double)e1[30]);
    }

    /* 1.4.11: the generator: every kind's voices valid and sounding; the same seed the same voice; BRIGHT brighter;
     * a voice varied stays valid and near; the pads' bank (B17) sounds, slow to rise, held */
    {
        uint8_t p[EN_PATCH + 1], q[EN_PATCH + 1];
        float env[80], hd, hb;
        int kind, s, k, bad = 0, silent = 0, brighter = 0, same = 1;
        for (kind = 0; kind < EN_NGEN; kind++) {
            float sd = 0.0f, sb = 0.0f;          /* (BRIGHT: its voices brighter on the whole, seed by seed they vary) */
            for (s = 1; s <= 4; s++) {
                en_gen(kind, 50, 50, (uint32_t)s * 77u, p);
                en_gen(kind, 50, 50, (uint32_t)s * 77u, q);
                same &= !memcmp(p, q, EN_PATCH);
                for (k = 0; k < EN_PATCH; k++)
                    bad += p[k] > en_patch_max(k);
                en_probe(p, 60, 2.0f, 0.2f, env, 80, &hd);
                {                                /* (its loudest in the first two seconds: a pluck's is at its start) */
                    float pk = 0.0f;
                    for (k = 0; k < 40; k++)
                        pk = env[k] > pk ? env[k] : pk;
                    silent += pk < 0.05f || pk > 4.0f;
                    if (pk < 0.05f || pk > 4.0f)
                        printf("  the generator: kind %d seed %d, its peak %f\n", kind, s, (double)pk);
                }
                en_gen(kind, 0, 50, (uint32_t)s * 77u, p);
                en_probe(p, 60, 2.0f, 0.2f, env, 80, &hd);
                en_gen(kind, 100, 50, (uint32_t)s * 77u, p);
                en_probe(p, 60, 2.0f, 0.2f, env, 80, &hb);
                sd += hd;
                sb += hb;
                memcpy(q, p, sizeof q);
                en_gen_vary(q, 60, 5u);
                for (k = 0; k < EN_PATCH; k++)
                    bad += q[k] > en_patch_max(k);
            }
            brighter += sb > sd * 1.1f;
        }
        CHECK(bad == 0 && same, "the generator: its voices in range (%d past), a seed the same voice (%d)", bad, same);
        CHECK(silent == 0, "the generator: every kind sounds, not too loud (%d not)", silent);
        CHECK(brighter == EN_NGEN, "the generator: BRIGHT brighter (%d kinds of %d)", brighter, EN_NGEN);
    }

    /* 1.4.12: a patch that dies away (the generator's Pluck) on the Bass voice and on a Chord voice, AMB's envelope: it
     * goes on sounding (struck each chord, each breath), not struck once and silent */
    {
        uint8_t p[EN_PATCH + 1];
        float first = 0.0f, late = 0.0f;
        int role, b, j;
        for (role = 0; role < 2; role++) {
            en_init();
            en_set(P_PL(0, PL_ROLE), role ? ROLE_BASS : ROLE_CHORD);
            en_set(P_PL(1, PL_ROLE), ROLE_LOOPS);
            en_set(P_PL(2, PL_ROLE), ROLE_LOOPS);
            en_set(P_PL(0, PL_ENV), ENV_AMB);
            en_set(P_BELLS, 0);
            en_set(P_BASS, 0);
            en_set(P_REVERB, 0);
            en_set(P_ECHO, 0);
            en_set(P_DRIFT, 0);                  /* (the chords move quickly: the bass struck often) */
            en_set(P_PADTONE, EN_PADS_SRC);
            en_gen(6, 50, 50, 3u, p);
            en_patch_load(0, p, 1);
            en_play(1); en_tape(1);
            first = late = 0.0f;
            for (b = 0; b < (int)(24.0f * 44100.0f / 256.0f); b++) {
                en_render(buf, 256, 2048);
                for (j = 0; j < 512; j++) {
                    const float x = (float)buf[j] * (float)buf[j];
                    if (b < (int)(8.0f * 44100.0f / 256.0f))
                        first += x;
                    else if (b >= (int)(16.0f * 44100.0f / 256.0f))
                        late += x;
                }
            }
            CHECK(late > 0.1f * first && late > 0.0f, "a dying patch on %s, AMB: it goes on (the last 8 s %g of the first %g)",
                  role ? "the bass" : "a chord", (double)late, (double)first);
        }
        en_init();
        en_play(1); en_tape(1);
    }

    /* 1.4.13: a splice's key studied: a D major chord (D F# A) played in over USB onto the reel, studied, its strongest
     * pitch classes D, F# and A; then Key sync Tape, the garden in C Ionian, SPEED at 1x: the reel snapped to a
     * semitone near 1x that sets D F# A on C's scale */
    {
        static int16_t tone[256];
        static const float HZ[3] = {293.665f, 369.994f, 440.0f};
        float kv[12], ph[3] = {0.0f, 0.0f, 0.0f}, rt, sh;
        int b, i, j, k, top[3], ok = 1, t, guard;
        en_init();
        en_set(P_ROOT, 0);
        en_pool(0xAB5u);
        en_play(1); en_tape(1);
        en_reel(EN_REEL_ERASE);
        en_set(P_RL(SRC), 5);
        en_set(P_RL(MIX), 100);
        run(0.3f);
        en_reel(EN_REEL_NEW);
        for (b = 0; b < 400; b++) {
            for (i = 0; i < 256; i++) {
                float x = 0.0f;
                for (j = 0; j < 3; j++) {
                    ph[j] += HZ[j] / 44100.0f;
                    ph[j] -= (float)(int)ph[j];
                    x += sinf(ph[j] * 6.2831853f) + 0.4f * sinf(ph[j] * 12.566371f);
                }
                tone[i] = (int16_t)(x * 5000.0f);
            }
            en_usb(tone, 256);
            en_render(buf, 256, 2048);
        }
        en_usb(0, 0);
        en_reel(EN_REEL_DUB);                    /* (stops the recording) */
        run(0.2f);
        for (guard = 0; guard < 200 && en_reel_study() >= 0; guard++)
            ;
        CHECK(en_reel_keyvec(0, kv), "a splice's key: studied (%d steps)", guard);
        for (k = 0; k < 3; k++) {                /* its three strongest */
            int bi = -1;
            for (i = 0; i < 12; i++)
                if ((k < 1 || i != top[0]) && (k < 2 || i != top[1]) && (bi < 0 || kv[i] > kv[bi]))
                    bi = i;
            top[k] = bi;
            ok &= bi == 2 || bi == 6 || bi == 9;
        }
        CHECK(ok, "a splice's key: D F# A its strongest (%d %d %d)", top[0], top[1], top[2]);
        en_set(P_ST(KSYNC), 2);
        en_set(P_RL(SPEED), 75);
        run(0.5f);
        rt = en_reel_rate;
        sh = 12.0f * log2f(fabsf(rt));
        t = (int)floorf(sh + 0.5f);
        CHECK(fabsf(sh - (float)t) < 0.01f && t >= -4 && t <= 4 && t != 0,
              "Key sync Tape: the reel a semitone shift near 1x (%f: %d)", (double)rt, t);
        ok = 1;
        for (k = 0; k < 3; k++)
            ok &= (0xAB5u >> (((top[k] + t) % 12 + 12) % 12)) & 1u;
        CHECK(ok, "Key sync Tape: D F# A moved %d, on C's scale", t);
        en_set(P_ST(KSYNC), 0);
        run(0.5f);
        CHECK(fabsf(en_reel_rate - 1.0f) < 0.001f, "Key sync Off: the reel at 1x again (%f)", (double)en_reel_rate);
        en_reel(EN_REEL_ERASE);
        en_init();
        en_play(1); en_tape(1);
    }

    /* 1.4.13: the song moves with the root: the test song (C and F) with the root at D, its melody's notes a tone up */
    {
        static uint8_t rec[0x4000];
        uint32_t k, t0;
        int mel = 0, bad = 0, t1;
        test_song(rec);
        en_init();
        en_set(P_ROOT, 0);
        en_pool(0xAB5u);
        en_set(P_SG(FOLLOW), 100);
        en_set(P_SG(VARY), 0);
        en_song(rec);
        en_set(P_ROOT, 2);                       /* (D: the song a tone up) */
        en_play(1); en_tape(1);
        run(0.5f);
        for (t1 = 0; t1 < 80; t1++) {
            t0 = en_now;
            run(0.05f);
            for (k = 0; k < EN_SCORE_N; k++) {
                const en_note_t *e = &en_score[k];
                if (!e->on || e->on < t0 || e->voice != VOX_BELLS)
                    continue;
                mel++;
                bad += !(e->note % 12 == 2 || e->note % 12 == 6 || e->note % 12 == 9 || e->note % 12 == 7 ||
                         e->note % 12 == 11);
            }
        }
        CHECK(mel > 4 && bad == 0, "the song a tone up with the root: its melody (%d, %d not)", mel, bad);
        CHECK(en_chord_root == 2 || en_chord_root == 7, "the song a tone up: its chords (%d)", en_chord_root);
        en_song(0);
        en_init();
        en_play(1); en_tape(1);
    }

    /* 1.4.14: under load (95 %) a Loops voice keeps two notes sounding, the chord one fewer: the score's notes open at
     * once on the bells' voice, two at most (a third fading is let go within 6 blocks) */
    {
        int t1, most = 0;
        uint32_t k;
        en_init();
        en_set(P_PL(1, PL_DENS), 100);
        en_set(P_PL(1, PL_PACE), 0);
        en_play(1); en_tape(1);
        en_load(95);
        run(2.0f);
        for (t1 = 0; t1 < 60; t1++) {
            int open = 0;
            run(0.05f);
            for (k = 0; k < EN_SCORE_N; k++)
                open += en_score[k].on && !en_score[k].off && en_score[k].voice == VOX_BELLS;
            most = open > most ? open : most;
        }
        CHECK(most <= 3, "under load: the loops two notes (and one fading) at most (%d)", most);
        en_load(0);
        en_init();
        en_play(1); en_tape(1);
    }

    /* stop: the tails die away */
    for (i = 0; i < P_NPARAMS; i++)
        en_set(i, en_param_info(i)->def);
    en_play(0); en_tape(0);
    run(40.0f);
    peak = 0.0f;
    run(1.0f);
    CHECK(peak < 0.01f, "quiet after stopping (%f)", (double)peak);
    CHECK(midi_count > 50u, "MIDI out (%u)", midi_count);

    printf(fails ? "amb: %d FAILED\n" : "amb: ok\n", fails);
    return fails != 0;
}
