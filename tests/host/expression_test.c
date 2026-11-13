/* SPDX-License-Identifier: MPL-2.0 */
/* Focused engine regressions: inspect held times/envelope state as well as public snapshots. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../../app/dsp/amb.c"

static int fails;
#define CHECK(c, s) do { if (!(c)) { printf("FAIL: %s\n", s); fails++; } } while (0)
void en_midi_out(uint32_t s, uint32_t a, uint32_t b) { (void)s; (void)a; (void)b; }
static void set(int p, int value) { en_set(p, value); drain(); }
static void route(int v, int source, int target, int when, int depth, int smooth)
{
    set(P_EX(v, EX_AMOUNT), 100);
    set(P_EX(v, EX_VARIATION), 0);
    set(P_XR(v, 0, XR_SOURCE), source);
    set(P_XR(v, 0, XR_TARGET), target);
    set(P_XR(v, 0, XR_WHEN), when);
    set(P_XR(v, 0, XR_DEPTH), depth);
    set(P_XR(v, 0, XR_SMOOTH), smooth);
}
static int32_t loop_hold(int depth)
{
    vnote_t *n;
    en_init();
    route(1, XS_OPEN, XT_GATE, XW_NOTE, depth, 0);
    set(P_PL(1, PL_GATE), 50);
    set(P_PL(1, PL_ENV), ENV_PATCH);
    n = &vc[1].n[0];
    n->on = 1;
    n->vel = 0.8f;
    n->midi_note = 72;
    loops_block(1);
    return n->held;
}
static void basics(void)
{
    int32_t out[512];
    uint32_t garden;
    en_init();
    route(0, XS_OPEN, XT_LEVEL, XW_LIVE, -100, 0);
    set(P_VF(0, VF_ON), 1);
    set(P_VF(0, VF_CUT), 10);
    en_play(1);
    en_render(out, 256, 2048);
    set(P_VF(0, VF_CUT), 90);
    en_render(out, 256, 2048);
    CHECK(en_expression[0][2] == 90, "meters follow source changes during sustained notes");
    en_init();
    route(0, XS_OPEN, XT_LEVEL, XW_NOTE, -100, 100);
    note_strike(0, 0, vpat[0], 60, 100, 1);
    for (int i = 0; i < 100; i++) en_render(out, 256, 2048);
    CHECK(xsm[0][0] > 10.0f, "smooth 100 responds to elapsed time");
    en_init();
    route(0, XS_OPEN, XT_LEVEL, XW_NOTE, -100, 0);
    garden = rng;
    note_strike(0, 0, vpat[0], 60, 100, 1);
    CHECK(rng == garden, "expression at variation zero preserves garden random sequence");
    CHECK(loop_hold(-50) < loop_hold(0), "negative Gate route shortens actual key hold");
    CHECK(loop_hold(50) > loop_hold(0), "positive Gate route lengthens actual key hold");
}

static void live_targets(void)
{
    vnote_t *n;
    fm6_note_t before;
    uint8_t patch[EN_PATCH + 1], original[EN_PATCH];
    int32_t ol[6] = {0}, low_car = 0, low_mod = 0;
    int i, carrier = -1, modulator = -1;
    float cut;
    en_init();
    memcpy(patch, vpat[0], sizeof patch);
    patch[FP_ALG] = 4;                         /* has both carriers and modulators */
    for (i = 0; i < 6; i++) patch[i * FP_OP + FP_OL] = 80;
    route(0, XS_OPEN, XT_LEVEL, XW_LIVE, -100, 0);
    set(P_VF(0, VF_ON), 1);
    set(P_VF(0, VF_CUT), 20);
    for (i = 1; i < 3; i++) {
        set(P_XR(0, i, XR_SOURCE), XS_OPEN);
        set(P_XR(0, i, XR_TARGET), i == 1 ? XT_INDEX : XT_CUTOFF);
        set(P_XR(0, i, XR_WHEN), XW_LIVE);
        set(P_XR(0, i, XR_DEPTH), i == 1 ? 50 : 100);
    }
    note_strike(0, 0, patch, 60, 100, 1);
    n = &vc[0].n[0];
    before = n->fm;
    memcpy(original, n->vp, EN_PATCH);
    expr_block(0);
    cut = expr_mod(0, &n->ex, &n->fm, n->vp, ol);
    CHECK(cut > 0.99f && cut < 1.01f, "Live cutoff applies source at block rate");
    for (i = 0; i < 6; i++) {
        if (fm6_carriers(4) >> i & 1u) { carrier = i; low_car = ol[i]; }
        else { modulator = i; low_mod = ol[i]; }
    }
    CHECK(carrier >= 0 && modulator >= 0 && low_car < 0 && low_mod > 0,
          "Live level changes carriers and Live index changes modulators");
    set(P_VF(0, VF_CUT), 80);
    expr_block(0);
    memset(ol, 0, sizeof ol);
    cut = expr_mod(0, &n->ex, &n->fm, n->vp, ol);
    CHECK(cut > 3.99f && cut < 4.01f && ol[carrier] < low_car && ol[modulator] > low_mod,
          "all continuous targets respond without a new strike");
    CHECK(en_expr_route[0][0] == -80 && en_expr_route[0][1] == 40 && en_expr_route[0][2] == 80,
          "signed snapshots expose applied continuous routes");
    CHECK(memcmp(&before, &n->fm, sizeof before) == 0 && memcmp(original, n->vp, EN_PATCH) == 0,
          "Live movement preserves envelope, oscillator and patch state");
    set(P_EX(0, EX_AMOUNT), 0);
    expr_block(0);
    memset(ol, 0, sizeof ol);
    cut = expr_mod(0, &n->ex, &n->fm, n->vp, ol);
    CHECK(cut == 0.0f && ol[carrier] == 0 && ol[modulator] == 0,
          "Amount zero removes previously active live contributions");
    CHECK(!en_expr_route[0][0] && !en_expr_route[0][1] && !en_expr_route[0][2],
          "Amount zero clears route snapshots");
}
static void timing_edits(void)
{
    int32_t ol[6] = {0};
    float cut;
    en_init();
    route(0, XS_OPEN, XT_CUTOFF, XW_NOTE, 50, 0);
    note_strike(0, 0, vpat[0], 60, 100, 1);
    set(P_XR(0, 0, XR_WHEN), XW_LIVE);
    expr_block(0);
    cut = expr_mod(0, &vc[0].n[0].ex, &vc[0].n[0].fm, vc[0].n[0].vp, ol);
    CHECK(cut > 2.49f && cut < 2.51f, "changing Note to Live replaces its captured contribution");
    set(P_XR(0, 0, XR_TARGET), XT_LEVEL);
    expr_block(0);
    cut = expr_mod(0, &vc[0].n[0].ex, &vc[0].n[0].fm, vc[0].n[0].vp, ol);
    CHECK(cut == 0.0f, "retargeting a promoted Live route removes its former captured target");
}
static void phrase_and_smoothing(void)
{
    int first, i;
    float halfway, final;
    en_init();
    route(0, XS_DENSITY, XT_RELEASE, XW_PHRASE, 100, 0);
    set(P_EX(0, EX_PHRASE), 0);                 /* four notes per phrase */
    set(P_PL(0, PL_DENS), 20);
    note_strike(0, 0, vpat[0], 60, 100, 1);
    first = vc[0].n[0].ex.acc[XT_RELEASE];
    expr_observe(0, &vc[0].n[0].ex);
    expr_observe(0, &vc[0].n[1].ex);
    expr_observation_block();
    for (i = 1; i < 4; i++) {
        note_strike(0, i, vpat[0], 60 + i, 100, 1);
        CHECK(vc[0].n[i].ex.acc[XT_RELEASE] == first, "Phrase value stays coherent across notes");
        expr_block(0);
        CHECK(en_expr_route[0][0] == first, "Phrase meter reports the held contribution");
    }
    CHECK(en_expr_phase[0] == 100, "phrase monitor reaches its final position");
    note_strike(0, 0, vpat[0], 60, 100, 1);
    CHECK(vc[0].n[0].ex.acc[XT_RELEASE] > first && en_expr_phase[0] == 0,
          "next phrase captures fresh source and resets monitor phase");
    en_init();
    route(0, XS_OPEN, XT_LEVEL, XW_LIVE, 100, 100);
    for (i = 0; i < 689; i++) expr_block(0);    /* 0.5 seconds, independent of note count */
    halfway = xsm[0][0];
    for (i = 0; i < 689; i++) expr_block(0);
    final = xsm[0][0];
    CHECK(halfway > 60.0f && halfway < 66.0f && final > 84.0f && final < 89.0f,
          "smooth 100 follows its 500 ms time constant instead of freezing");
    set(P_XR(0, 0, XR_SMOOTH), 0);
    expr_block(0);
    CHECK(xsm[0][0] == 100.0f, "smooth zero applies source immediately");
}
static void articulation_and_keys(void)
{
    uint8_t plain[EN_PATCH];
    uint8_t stage;
    int32_t level, ol[6] = {0};
    uint32_t garden;
    int32_t hold;
    en_init();
    set(P_PL(0, PL_ENV), ENV_ADSR);
    note_strike(0, 0, vpat[0], 60, 100, 1);
    memcpy(plain, vc[0].n[0].vp, EN_PATCH);
    route(0, XS_OPEN, XT_ATTACK, XW_NOTE, -100, 0);
    note_strike(0, 0, vpat[0], 60, 100, 1);
    CHECK(memcmp(plain, vc[0].n[0].vp, EN_PATCH) != 0, "Note route changes articulation at strike");
    stage = vc[0].n[0].fm.env[0].ix;
    level = vc[0].n[0].fm.env[0].level;
    set(P_EX(0, EX_AMOUNT), 0);
    (void)expr_mod(0, &vc[0].n[0].ex, &vc[0].n[0].fm, vc[0].n[0].vp, ol);
    CHECK(memcmp(plain, vc[0].n[0].vp, EN_PATCH) == 0, "Amount zero restores the original ADSR bytes");
    CHECK(vc[0].n[0].fm.env[0].ix == stage && vc[0].n[0].fm.env[0].level == level,
          "restoring articulation does not restart envelopes");
    en_init();
    route(0, XS_OPEN, XT_LEVEL, XW_NOTE, -100, 0);
    set(P_EX(0, EX_VARIATION), 100);
    garden = rng;
    for (int i = 0; i < 40; i++) note_strike(0, 0, vpat[0], 60, 100, 1);
    CHECK(rng == garden, "nonzero expression variation also preserves garden RNG");
    en_init();
    route(0, XS_OPEN, XT_ATTACK, XW_NOTE, -100, 0);
    kb_key(0, 60, 1);
    CHECK(kb[0].ex.acc[XT_ATTACK] == -100, "keyboard notes receive note articulation routes");
    set(P_XR(0, 0, XR_TARGET), XT_CUTOFF);
    set(P_XR(0, 0, XR_WHEN), XW_LIVE);
    expr_block(0);
    CHECK(expr_mod(0, &kb[0].ex, &kb[0].fm, kb[0].vp, ol) < -4.99f,
          "keyboard notes receive continuous routes while held");
    kb_key(0, 60, 0);
    CHECK(!kb[0].fm.down && kb[0].on == 2, "physical key release still releases expressed notes");
    hold = loop_hold(100);
    set(P_XR(1, 0, XR_DEPTH), -100);
    expr_block(1);
    loops_block(1);
    CHECK(vc[1].n[0].held == hold - FM6_N, "editing Gate does not change an already latched hold");
    CHECK(loop_hold(-100) >= FM6_N, "Gate never creates a negative or zero key hold");
}

static int chord_release(int depth)
{
    int blocks = 0;
    vnote_t *n;
    en_init();
    route(0, XS_OPEN, XT_GATE, XW_NOTE, depth, 0);
    set(P_PL(0, PL_ENV), ENV_PATCH);
    set(P_PL(0, PL_GATE), 50);
    note_strike(0, 0, vpat[0], 60, 100, 1);
    n = &vc[0].n[0];
    n->t = 0.0f;
    n->rate = 0.001f;
    while (!n->up && blocks < 999) { chord_coefs(0, 0); blocks++; }
    CHECK(n->up, "chord Gate releases before the breath restarts");
    return blocks;
}
static int32_t bass_hold(int depth)
{
    en_init();
    route(2, XS_OPEN, XT_GATE, XW_NOTE, depth, 0);
    set(P_PL(2, PL_ENV), ENV_PATCH);
    set(P_PL(2, PL_GATE), 50);
    set(P_PL(2, PL_PACE), 90);
    vc[2].strike = 1;
    vc[2].rest = 0;
    en_playing = 1;
    bass_block(2);
    return vc[2].held;
}
static uint64_t render_hash(int configured)
{
    int32_t out[512];
    uint64_t h = 1469598103934665603ull;
    en_init();
    if (configured) for (int v = 0; v < EN_NVOX; v++) {
        set(P_EX(v, EX_STYLE), XSTYLE_CLOUD);
        set(P_EX(v, EX_VARIATION), 100);
        set(P_XR(v, 0, XR_SOURCE), XS_OPEN);
        set(P_XR(v, 0, XR_TARGET), XT_ATTACK);
        set(P_XR(v, 0, XR_DEPTH), -100);
        set(P_XR(v, 1, XR_SOURCE), XS_ENERGY);
        set(P_XR(v, 1, XR_TARGET), XT_LEVEL);
        set(P_XR(v, 1, XR_WHEN), XW_LIVE);
        set(P_XR(v, 1, XR_DEPTH), -100);
    }
    en_play(1);
    for (int b = 0; b < 200; b++) {
        en_render(out, 256, 2048);
        for (int j = 0; j < 512; j++) { h ^= (uint32_t)out[j]; h *= 1099511628211ull; }
    }
    return h;
}
static void compatibility(void)
{
    char text[16];
    CHECK(chord_release(-100) < chord_release(0) && chord_release(50) > chord_release(0),
          "both signs of Gate change chord key duration");
    CHECK(bass_hold(-100) < bass_hold(0) && bass_hold(50) > bass_hold(0),
          "both signs of Gate change bass key duration");
    en_param_text(P_EX(0, EX_PHRASE), 0, text);
    CHECK(strcmp(text, "4 notes") == 0, "phrase minimum displays its actual unit");
    en_param_text(P_EX(0, EX_PHRASE), 100, text);
    CHECK(strcmp(text, "12 notes") == 0, "phrase maximum displays its actual unit");
}
static void actual_observation(void)
{
    expr_note_t a = {0}, b = {0};
    int src[XS_N];
    en_init();
    set(P_PL(0, PL_DENS), 100);
    set(P_PL(0, PL_GATE), 100);
    expr_sources(0, -1, -1, -1, src);
    CHECK(src[XS_DENSITY] == 0 && src[XS_DURATION] == 0, "silent voice ignores density/gate knobs");
    a.sounded = (uint32_t)EN_SR * 2 - FM6_N;
    b.sounded = (uint32_t)EN_SR * 4 - FM6_N;
    expr_observe(0, &a);
    expr_observe(0, &b);
    expr_observation_block();
    expr_sources(0, -1, -1, -1, src);
    CHECK(src[XS_DENSITY] == 50, "two rendered notes occupy half the layer");
    CHECK(src[XS_DURATION] == 75, "mean sounded duration is three seconds");
    CHECK(xdensity[1] == 0, "observations remain per voice");
    expr_observation_block();
    CHECK(xdensity[0] == 0 && xduration[0] == 0, "silent block clears observations");
    note_strike(0, 0, vpat[0], 60, 100, 1);
    CHECK(vc[0].n[0].ex.sounded == 0, "new attack starts its own duration");
    en_init();
    en_key(0, 60, 1);
    { int32_t out[512]; en_render(out, 256, 2048); }
    CHECK(xdensity[0] >= 25, "keyboard audio contributes real density");
}
int main(int argc, char **argv)
{
    /* Fresh-process comparison avoids unrelated state retained across this engine's repeated en_init calls. */
    if (argc == 2) {
        printf("%llu\n", (unsigned long long)render_hash(strcmp(argv[1], "hash-configured") == 0));
        return 0;
    }
    actual_observation();
    basics();
    live_targets();
    timing_edits();
    phrase_and_smoothing();
    articulation_and_keys();
    compatibility();
    if (fails) { printf("expression: %d failed\n", fails); return 1; }
    puts("expression: all checks passed");
    return 0;
}
