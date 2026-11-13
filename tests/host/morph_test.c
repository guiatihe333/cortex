/* SPDX-License-Identifier: MPL-2.0 */
/* Public behavior checks: endpoint identity, pitch/topology invariants,
 * carrier/modulator-role mapping and legal bounded controls. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "amb.h"
static int fails;
#define CHECK(x) do { if (!(x)) { printf("FAIL line %d: %s\n", __LINE__, #x); fails++; } } while (0)
void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2) { (void)st; (void)d1; (void)d2; }
static int interpolated(int i)
{
    if (i < EP_PR1) { int f = i % EP_OP; return f < EP_BP || f == EP_LD || f == EP_RD || f == EP_OL; }
    return i == EP_FB || i == EP_LFS || i == EP_LFD || i == EP_LPMD || i == EP_LAMD;
}
static void check_controls(const uint8_t *p)
{
    int i;
    for (i = 0; i < EN_PATCH; i++) CHECK(p[i] <= en_patch_max(i));
}
int main(void)
{
    uint8_t a[EN_PATCH + 1], b[EN_PATCH + 1], p[EN_PATCH + 2], q[EN_PATCH + 1], prev[EN_PATCH];
    int i, t, aa, bb;
    en_factory(0, 0, 50, a);
    memcpy(b, a, EN_PATCH);
    for (i = 0; i < EP_NAME; i++) if (interpolated(i)) b[i] = en_patch_max(i) - a[i];
    memcpy(b + EP_NAME, "MORPH TEST", 10);
    CHECK(en_morph_compatible(a, b));
    for (t = 0; t <= 100; t++) {
        p[EN_PATCH] = 0x5a;
        en_morph_patch(a, b, t, XM_COMPATIBLE, p);
        CHECK(p[EN_PATCH] == 0x5a);
        check_controls(p);
        if (!t) CHECK(memcmp(p, a, EN_PATCH) == 0);
        if (t == 100) CHECK(memcmp(p, b, EN_PATCH) == 0);
        for (i = 0; i < EP_NAME; i++) {
            if (!interpolated(i)) CHECK(p[i] == a[i]);
            else { int lo = a[i] < b[i] ? a[i] : b[i], hi = a[i] > b[i] ? a[i] : b[i]; CHECK(p[i] >= lo && p[i] <= hi); }
        }
        if (t)
            for (i = 0; i < EP_NAME; i++) if (interpolated(i))
                CHECK(a[i] <= b[i] ? p[i] >= prev[i] : p[i] <= prev[i]);
        memcpy(prev, p, EN_PATCH);
        en_morph_patch(a, a, t, XM_COLOR, p);
        CHECK(memcmp(p, a, EN_PATCH) == 0);
    }
    /* Each structurally incompatible endpoint must be refused, including
     * pitch EG and transpose, not only the algorithm number. */
    for (i = 0; i < EP_NAME; i++) if (!interpolated(i) && en_patch_max(i)) {
        memcpy(q, b, EN_PATCH); q[i] = (uint8_t)((a[i] + 1) % (en_patch_max(i) + 1));
        CHECK(!en_morph_compatible(a, q));
        en_morph_patch(a, q, 100, XM_COMPATIBLE, p);
        CHECK(memcmp(p, a, EN_PATCH) == 0);
    }
    /* Distinct role markers independently expose incorrect OP-number
     * mapping when different algorithms have different carrier counts. */
    for (aa = 0; aa < 32; aa++) for (bb = 0; bb < 32; bb++) {
        uint32_t ac, bc;
        en_factory(0, 0, 50, a); memcpy(b, a, EN_PATCH);
        a[EP_ALG] = (uint8_t)aa; b[EP_ALG] = (uint8_t)bb;
        ac = en_alg_carriers(aa); bc = en_alg_carriers(bb);
        for (i = 0; i < 6; i++) {
            int k; uint8_t marker = bc >> i & 1u ? 80 : 20;
            for (k = 0; k < EP_BP; k++) b[i * EP_OP + k] = marker;
            b[i * EP_OP + EP_OL] = marker;
        }
        en_morph_patch(a, b, 100, XM_COLOR, p); check_controls(p);
        for (i = 0; i < EN_PATCH; i++) {
            int f = i % EP_OP;
            if (i >= EP_PR1 || (f >= EP_BP && f != EP_OL)) CHECK(p[i] == a[i]);
        }
        for (i = 0; i < 6; i++) {
            int k, role = (int)(ac >> i & 1u), has = role ? bc != 0 : bc != 63u;
            for (k = 0; k < EP_BP; k++) CHECK(p[i * EP_OP + k] == (has ? (role ? 80 : 20) : a[i * EP_OP + k]));
            CHECK(p[i * EP_OP + EP_OL] == (has ? (role ? 80 : 20) : a[i * EP_OP + EP_OL]));
        }
        memcpy(q, b, EN_PATCH); en_morph_patch(a, q, 50, XM_COLOR, q);
        en_morph_patch(a, b, 50, XM_COLOR, p); CHECK(memcmp(q, p, EN_PATCH) == 0);
        memcpy(q, a, EN_PATCH); en_morph_patch(q, b, 50, XM_COLOR, q);
        CHECK(memcmp(q, p, EN_PATCH) == 0);
    }
    /* Every factory pair, all intermediate positions: no out-of-range
     * controls and A's frequency/algorithm/pitch-envelope bytes unchanged. */
    for (aa = 0; aa < EN_NFACT; aa++) for (bb = 0; bb < EN_NFACT; bb++) {
        en_factory(0, aa, 50, a); en_factory(0, bb, 50, b);
        for (t = 0; t <= 100; t += 10) {
            en_morph_patch(a, b, t, XM_COLOR, p); check_controls(p);
            for (i = 0; i < EN_PATCH; i++) {
                int f = i % EP_OP;
                if (i >= EP_PR1 || (f >= EP_BP && f != EP_OL)) CHECK(p[i] == a[i]);
            }
        }
    }
    en_morph_patch(a, b, -40, XM_COLOR, p); CHECK(memcmp(p, a, EN_PATCH) == 0);
    en_morph_patch(a, b, 50, -1, p); CHECK(memcmp(p, a, EN_PATCH) == 0);
    en_morph_patch(a, 0, 50, XM_COLOR, p); CHECK(memcmp(p, a, EN_PATCH) == 0);
    memcpy(q, b, EN_PATCH); q[EP_ALG] = 255;
    en_morph_patch(a, q, 50, XM_COLOR, p); CHECK(memcmp(p, a, EN_PATCH) == 0);
    printf("FM morph: %s\n", fails ? "FAILED" : "all checks passed");
    return fails ? 1 : 0;
}
