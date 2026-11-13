/* SPDX-License-Identifier: MPL-2.0 */
/* FM-aware, note-on morphing. No oscillator is retuned, no algorithm is
 * interpolated, and no extra synthesis voice is required. COLOR keeps A's
 * complete frequency/routing structure and borrows B's envelopes and output
 * levels by carrier/modulator role. Its endpoint is intentionally a hybrid,
 * not a claim to reproduce B. COMPATIBLE reaches B exactly only when every
 * non-interpolated synthesis field matches (names may differ).
 *
 * DX7 envelope rates and levels are interpolated in their native bounded
 * control domain. This is monotone, not a claim that a rate code is a number
 * of milliseconds. Changing envelopes is captured at note-on, not reapplied
 * to the running envelope at audio rate. Even these rules cannot turn an
 * intentionally inharmonic input patch into a harmonic instrument. */
#include "amb.h"
#ifdef EN_HOST
#include <string.h>
#else
#define memcpy __builtin_memcpy
#endif

static int morph_continuous(int off)
{
    if (off < 6 * EP_OP) {
        const int f = off % EP_OP;
        return f < EP_BP || f == EP_LD || f == EP_RD || f == EP_OL;
    }
    return off == EP_FB || off == EP_LFS || off == EP_LFD ||
           off == EP_LPMD || off == EP_LAMD;
}

static int morph_valid(const uint8_t *p)
{
    int i;
    for (i = 0; i < EN_PATCH; i++)
        if (p[i] > en_patch_max(i))
            return 0;
    return 1;
}

int en_morph_compatible(const uint8_t *a, const uint8_t *b)
{
    int i;
    if (!a || !b || !morph_valid(a) || !morph_valid(b))
        return 0;
    for (i = 0; i < EP_NAME; i++)
        if (!morph_continuous(i) && a[i] != b[i])
            return 0;
    return 1;
}

static uint8_t morph_lerp(uint8_t a, uint8_t b, int amount)
{
    return (uint8_t)(((int)a * (100 - amount) + (int)b * amount + 50) / 100);
}

void en_morph_patch(const uint8_t *a, const uint8_t *b, int amount, int kind, uint8_t *out)
{
    uint8_t p[EN_PATCH];
    int i;
    if (!a || !out)
        return;
    amount = amount < 0 ? 0 : amount > 100 ? 100 : amount;
    if (!b || !amount || (kind != XM_COLOR && kind != XM_COMPATIBLE) || !morph_valid(b)) {
        if (out != a)
            memcpy(out, a, EN_PATCH);
        return;
    }
    if (kind == XM_COMPATIBLE) {
        if (!en_morph_compatible(a, b)) {
            if (out != a)
                memcpy(out, a, EN_PATCH);
            return;
        }
        if (amount == 100) {
            if (out != b)
                memcpy(out, b, EN_PATCH);
            return;
        }
        memcpy(p, a, EN_PATCH);
        for (i = 0; i < EP_NAME; i++)
            if (morph_continuous(i))
                p[i] = morph_lerp(a[i], b[i], amount);
    } else {
        const uint32_t ac = en_alg_carriers(a[EP_ALG]), bc = en_alg_carriers(b[EP_ALG]);
        uint8_t byrole[2][6], count[2] = {0, 0}, used[2] = {0, 0};
        memcpy(p, a, EN_PATCH);
        for (i = 0; i < 6; i++) {
            const int role = (int)((bc >> i) & 1u);
            byrole[role][count[role]++] = (uint8_t)i;
        }
        for (i = 0; i < 6; i++) {
            const int role = (int)((ac >> i) & 1u), ao = i * EP_OP;
            int bo, k;
            /* All-carrier B has no modulator envelope to lend. Keep A's
             * modulators instead of repurposing a B carrier as one. */
            if (!count[role])
                continue;
            bo = byrole[role][used[role]++ % count[role]] * EP_OP;
            for (k = EP_R1; k < EP_BP; k++)
                p[ao + k] = morph_lerp(a[ao + k], b[bo + k], amount);
            p[ao + EP_OL] = morph_lerp(a[ao + EP_OL], b[bo + EP_OL], amount);
        }
    }
    /* A local result also permits out == a or out == b without corrupting
     * an operator which a later role mapping still needs to read. */
    memcpy(out, p, EN_PATCH);
}
