/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal */
/* The generative sequencer's own rules, on the host: a chord that keeps its direction without ever repeating a
 * degree, a motif that survives a loop's pass (moved, turned round, mirrored), Swing that arrives late, and the
 * bass's passing tone (and its return) only when Evolve asks for it. Evolve 0 must stay the old walk. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
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

/* a walk from one degree: its histogram, and how often it lands home */
static void walk(int ev, int from, int since, int n, int *hist, int *home, int *repeat)
{
    int i;
    en_init();
    apply(P_EVOLVE, ev);
    for (i = 0; i < nsteps; i++)
        hist[i] = 0;
    *home = *repeat = 0;
    for (i = 0; i < n; i++) {
        int last;
        if (from >= 0)
            deg = from;
        if (since >= 0)
            since_home = since;
        last = deg;
        chord_change();
        CHECK(deg >= 0 && deg < nsteps, "degree %d outside the scale (%d)", deg, nsteps);
        CHECK(deg != last, "the chord repeated degree %d", deg);
        hist[deg]++;
        if (deg == 0)
            (*home)++;
    }
}

static void chords(void)
{
    int hist[12], home, repeat, i, old_home, new_home, old_v, new_v;
    /* from every degree: never the same one twice, and every degree reachable */
    en_init();
    apply(P_EVOLVE, 100);
    for (i = 0; i < nsteps; i++)
        hist[i] = 0;
    for (i = 0; i < 4000; i++) {
        const int last = deg;
        chord_change();
        CHECK(deg >= 0 && deg < nsteps, "degree %d outside the scale", deg);
        CHECK(deg != last, "the chord repeated degree %d", deg);
        hist[deg]++;
    }
    for (i = 0; i < nsteps; i++)
        CHECK(hist[i] > 0, "degree %d never reached in 4000 changes", i);

    /* Evolve 0 is the old walk: from I, the weights are the old table (no direction, no pull) */
    walk(0, 0, 0, 20000, hist, &home, &repeat);
    CHECK(hist[1] > 2800 && hist[1] < 3600, "Evolve 0: ii %.3f (old 0.161)", (double)hist[1] / 20000.0);
    CHECK(hist[2] > 1300 && hist[2] < 1900, "Evolve 0: iii %.3f (old 0.081)", (double)hist[2] / 20000.0);
    CHECK(hist[3] > 4500 && hist[3] < 5300, "Evolve 0: IV %.3f (old 0.242)", (double)hist[3] / 20000.0);
    CHECK(hist[4] > 4500 && hist[4] < 5300, "Evolve 0: V %.3f (old 0.242)", (double)hist[4] / 20000.0);
    CHECK(hist[5] > 4500 && hist[5] < 5300, "Evolve 0: vi %.3f (old 0.242)", (double)hist[5] / 20000.0);
    CHECK(hist[6] > 300 && hist[6] < 1000, "Evolve 0: vii %.3f (old 0.032)", (double)hist[6] / 20000.0);

    /* Evolve: the cadence's move is stronger (V -> I), and longer away pulls home */
    walk(0, 4, 4, 4000, hist, &old_home, &repeat);
    old_v = hist[0];
    walk(100, 4, 4, 4000, hist, &new_home, &repeat);
    new_v = hist[0];
    CHECK(new_v > old_v + 500, "V -> I: Evolve 0 %d, Evolve 100 %d", old_v, new_v);

    walk(0, -1, -1, 6000, hist, &old_home, &repeat);
    walk(100, -1, -1, 6000, hist, &new_home, &repeat);
    CHECK(new_home > old_home, "home more often at Evolve 100: %d vs %d", new_home, old_home);
}

/* the loop's motif: the same count and shape, moved a step, turned round, or mirrored; never off the scale */
static int motif_kind(const int8_t *b, const int8_t *a, int len, int ns)
{
    int i, lo = 127, hi = -127, mid, sh, lift;
    for (i = 0; i < len; i++)
        if (b[i] >= 0) {
            if (b[i] < lo) lo = b[i];
            if (b[i] > hi) hi = b[i];
        }
    mid = (lo + hi) / 2;
    for (sh = -1; sh <= 1; sh += 2)
        for (lift = 0; lift <= 8 * ns; lift += ns) {
            int ok = 1;
            for (i = 0; i < len; i++)
                if (b[i] >= 0 ? a[i] != b[i] + sh + lift : a[i] >= 0)
                    ok = 0;
            if (ok)
                return 0;
        }
    {
        int ok = 1;
        for (i = 0; i < len; i++)
            if (a[i] != b[len - 1 - i])
                ok = 0;
        if (ok)
            return 1;
    }
    for (lift = 0; lift <= 8 * ns; lift += ns) {
        int ok = 1;
        for (i = 0; i < len; i++)
            if (b[i] >= 0 ? a[i] != 2 * mid - b[i] + lift : a[i] >= 0)
                ok = 0;
        if (ok)
            return 2;
    }
    if (!memcmp(a,b,(size_t)len)) return 4;
    for (sh = 1; sh < len; sh++) {
        int ok = 1;
        for (i = 0; i < len; i++) if (a[i] != b[(i+sh)%len]) ok = 0;
        if (ok) return 3;
    }
    return -1;
}

static void motifs(void)
{
    static const int slot[6] = {0, 2, 4, 6, 8, 10};
    static const int note[6] = {0, 2, 3, 5, 7, 4};
    const int v = first_role(ROLE_LOOPS), len = EN_LOOP_LEN[0];
    int8_t base[EN_LOOP_MAX];
    uint8_t basev[EN_LOOP_MAX];
    int seen[5] = {0}, i, seed;
    CHECK(v >= 0, "a Loops voice");
    en_init();
    for (i = 0; i < EN_LOOP_MAX; i++) {
        vc[v].loop_note[0][i] = -1;
        vc[v].loop_vel[0][i] = 0;
    }
    for (i = 0; i < 6; i++) {
        vc[v].loop_note[0][slot[i]] = (int8_t)note[i];
        vc[v].loop_vel[0][slot[i]] = (uint8_t)(40 + 8 * i);
    }
    memcpy(base, vc[v].loop_note[0], sizeof base);
    memcpy(basev, vc[v].loop_vel[0], sizeof basev);
    for (seed = 1; seed <= 400; seed++) {
        int8_t out[EN_LOOP_MAX];
        int before = 0, after = 0, vbefore = 0, vafter = 0, kind;
        memcpy(vc[v].loop_note[0], base, sizeof base);
        memcpy(vc[v].loop_vel[0], basev, sizeof basev);
        for (i = 0; i < len; i++) {
            before += vc[v].loop_note[0][i] >= 0;
            vbefore += vc[v].loop_vel[0][i];
        }
        rng = (uint32_t)seed * 2654435761u;
        loop_motif(v, 0);
        memcpy(out, vc[v].loop_note[0], sizeof out);
        for (i = 0; i < len; i++) {
            after += out[i] >= 0;
            vafter += vc[v].loop_vel[0][i];
            CHECK(out[i] < 64, "a motif note %d ran away", out[i]);
        }
        CHECK(after == before, "motif changed the count: %d -> %d", before, after);

        kind = motif_kind(base, out, len, nsteps);
        CHECK(kind >= 0, "seed %d: not a transposition, retrograde or mirror", seed);
        if (kind >= 0) {
            seen[kind] = 1;
            if (kind != 4) CHECK(vafter == vbefore, "pitch/rhythm transformation preserves velocity total");
            else for (i=0;i<len;i++) if (out[i]>=0)
                CHECK(vc[v].loop_vel[0][i] >= 35 && vc[v].loop_vel[0][i] <= 127, "accent velocity bounded");
        }
    }
    CHECK(seen[3] && seen[4], "rotation and accent variation observed");
    CHECK(seen[0] && seen[1] && seen[2], "all three motif moves seen (%d %d %d)", seen[0], seen[1], seen[2]);
}

static void swing(void)
{
    int v, i, found;
    en_init();
    step_len = 10000.0f;
    apply(P_SWING, 0);
    sixteenth = 1;
    CHECK(swing_delay(1) == 0, "Swing 0 is straight");
    apply(P_SWING, 100);
    sixteenth = 1;
    CHECK(swing_delay(1) == 5000, "75%%: the second sixteenth half a step late (%d)", (int)swing_delay(1));
    sixteenth = 2;
    CHECK(swing_delay(2) == 10000, "75%%: the second eighth a whole eighth-step late (%d)", (int)swing_delay(2));
    sixteenth = 4;
    CHECK(swing_delay(2) == 0, "the downbeat stays put");
    CHECK(swing_delay(4) == 0, "a quarter grid stays straight");

    /* the loops: Swing adds to the hand's humanisation */
    en_init();
    v = first_role(ROLE_LOOPS);
    vc[v].loop_note[0][0] = 3;
    vc[v].loop_vel[0][0] = 80;
    bell_ring(v, 0, 0, 0);
    for (i = 0, found = 0; i < NPOLY; i++)
        if (vc[v].n[i].on && vc[v].n[i].wait >= 0 && vc[v].n[i].wait < 2600)
            found = 1;
    CHECK(found, "no bell struck without swing");
    en_init();
    v = first_role(ROLE_LOOPS);
    vc[v].loop_note[0][0] = 3;
    vc[v].loop_vel[0][0] = 80;
    bell_ring(v, 0, 0, 9000);
    for (i = 0, found = 0; i < NPOLY; i++)
        if (vc[v].n[i].on && vc[v].n[i].wait >= 9000 && vc[v].n[i].wait < 9000 + 2600)
            found = 1;
    CHECK(found, "the bell did not wait for Swing");

    /* the bass: the odd eighth arrives late */
    en_init();
    apply(P_SWING, 100);
    v = first_role(ROLE_BASS);
    apply(P_PL(v, PL_PACE), 90);                /* the bass each eighth */
    step_len = 10000.0f;
    sixteenth = 2;
    tick();
    CHECK(vc[v].swait == 10000, "the bass's second eighth waits (%d)", (int)vc[v].swait);
    sixteenth = 4;
    tick();
    CHECK(vc[v].swait == 0, "the bass's downbeat does not wait");
}

/* (1.5.26) Expression can aim at the sequencer: Density, Evolve, Drift; 0 everywhere is the knobs exactly */
static void expr_sequencer(void)
{
    float base, scaled;
    int base_target, boosted;
    en_init();
    apply(P_EVOLVE, 0);
    apply(P_PL(0, PL_ROLE), ROLE_LOOPS);
    apply(P_PL(0, PL_DENS), 40);
    base_target = loop_target(0);
    CHECK(xseq[0][XT_DENSITY - XT_DENSITY] == 0, "no route: no sequencer contribution");
    CHECK(xevolve(0) == evolve && xdens(0) == PL(0, DENS), "no route: the knobs, exactly");

    par[P_EX(0, EX_AMOUNT)] = 100;
    par[P_XR(0, 0, XR_SOURCE)] = XS_OPEN;
    par[P_XR(0, 0, XR_TARGET)] = XT_DENSITY;
    par[P_XR(0, 0, XR_DEPTH)] = 100;
    par[P_XR(0, 0, XR_WHEN)] = XW_NOTE;
    par[P_XR(0, 0, XR_SMOOTH)] = 0;
    par[P_XR(0, 0, XR_MIN)] = 0;
    par[P_XR(0, 0, XR_MAX)] = 100;
    xshown[0][0] = 100.0f;                       /* (a strike captured it) */
    expr_block(0);
    boosted = loop_target(0);
    CHECK(xdens(0) > PL(0, DENS), "Density target raises the voice's density");
    CHECK(boosted >= base_target, "more notes asked for at the next pass");

    par[P_XR(0, 1, XR_SOURCE)] = XS_OPEN;
    par[P_XR(0, 1, XR_TARGET)] = XT_EVOLVE;
    par[P_XR(0, 1, XR_DEPTH)] = 100;
    par[P_XR(0, 1, XR_WHEN)] = XW_NOTE;
    par[P_XR(0, 1, XR_SMOOTH)] = 0;
    par[P_XR(0, 1, XR_MIN)] = 0;
    par[P_XR(0, 1, XR_MAX)] = 100;
    xshown[0][1] = 100.0f;
    expr_block(0);
    CHECK(xevolve(0) > 0.9f, "Evolve target from a closed knob (%d)", (int)(xevolve(0) * 100.0f));

    en_init();
    apply(P_EVOLVE, 0);
    rng = 0x9E3779B9u;
    drift_reset();
    base = drift_total;
    par[P_EX(0, EX_AMOUNT)] = 100;
    par[P_XR(0, 0, XR_SOURCE)] = XS_OPEN;
    par[P_XR(0, 0, XR_TARGET)] = XT_DRIFT;
    par[P_XR(0, 0, XR_DEPTH)] = -100;
    par[P_XR(0, 0, XR_WHEN)] = XW_NOTE;
    par[P_XR(0, 0, XR_SMOOTH)] = 0;
    par[P_XR(0, 0, XR_MIN)] = 0;
    par[P_XR(0, 0, XR_MAX)] = 100;
    xshown[0][0] = -100.0f;                      /* (the depth, as a strike had it) */
    expr_block(0);
    rng = 0x9E3779B9u;
    drift_reset();
    scaled = drift_total;
    CHECK(xdrift_scale() < 0.9f, "Drift target shortens the chord's timer");
    CHECK(scaled < base, "the timer itself is shorter (%d < %d)", (int)scaled, (int)base);
    CHECK(XT_N <= 32, "the targets fit the new five-bit format");
}

static void bass_passing(void)
{
    const int v = first_role(ROLE_BASS);
    int i, alt0 = 0, alt100 = 0, returns = 0, prev = 0;
    CHECK(v >= 0, "a Bass voice");
    en_init();
    apply(P_EVOLVE, 0);
    apply(P_PL(v, PL_PACE), 50);                /* the bass each half bar */
    sixteenth = 0;
    for (i = 0; i < 8 * 200; i++) {
        tick();
        if (vc[v].alt)
            alt0++;
    }
    CHECK(alt0 == 0, "Evolve 0 took a passing tone %d times", alt0);

    en_init();
    apply(P_EVOLVE, 100);
    apply(P_PL(v, PL_PACE), 50);
    sixteenth = 0;
    for (i = 0; i < 8 * 200; i++) {
        tick();
        if (prev && !vc[v].alt) {
            returns++;
            CHECK(vc[v].pc == chord_pcs[0], "the bass did not come back to the root (%d/%d)", vc[v].pc, chord_pcs[0]);
        }
        if (vc[v].alt) {
            alt100++;
            CHECK(vc[v].pc != chord_pcs[0], "a passing tone on the root");
        }
        prev = vc[v].alt;
    }
    CHECK(alt100 > 0, "Evolve 100 never took a passing tone");
    CHECK(returns > 0, "no passing tone came home");
}

static void new_sequence_routes(void)
{
    int t;
    char name[32];
    for (t = XT_MOTIF; t <= XT_RESOLVE; t++) {
        en_init();
        apply(P_EX(0, EX_AMOUNT), 100);
        apply(P_XR(0,0,XR_SOURCE), XS_OPEN);
        apply(P_XR(0,0,XR_TARGET), t);
        apply(P_XR(0,0,XR_WHEN), XW_LIVE);
        apply(P_XR(0,0,XR_DEPTH), 100);
        expr_block(0);
        CHECK(xseq[0][t-XT_DENSITY] == 100, "new live expression target %d", t);
        apply(P_XR(0,0,XR_DEPTH), -100);
        expr_block(0);
        CHECK(xseq[0][t-XT_DENSITY] == -100, "negative expression target %d", t);
        apply(P_EX(0, EX_AMOUNT), 0);
        expr_block(0);
        CHECK(xseq[0][t-XT_DENSITY] == 0, "bypass clears sequence target %d", t);
    }
    for (t = 0; t < 15; t++) {
        en_init();
        apply(P_MS(0,0,MS_DEST), MOD_SEQ+t+1);
        en_mod[0][VOX_GEN0[0]] = 1.0f;
        CHECK(seq_offset(0,t) == 100.0f, "LFO reaches sequence target %d",t);
        en_mod[0][VOX_GEN0[0]] = -1.0f;
        CHECK(seq_offset(0,t) == -100.0f, "LFO negative sequence target %d",t);
        xseq[0][t] = -100;
        CHECK(seq_offset(0,t) == -100.0f, "summed routes bounded %d",t);
        en_moddest_text(MOD_SEQ+t,name);
        CHECK(name[0] != 0, "target name %d",t);
    }
    CHECK(par[P_SEQ_MOTIF] == 35, "modulation never rewrites the saved knob");
}
static void dialogue_phrases(void)
{
    int v=1, l=0, i;
    int8_t notes[EN_LOOP_MAX];
    en_init();
    apply(P_EVOLVE,100); apply(P_SEQ_DIALOGUE,100);
    apply(P_SEQ_LENGTH,34); /* four passes */
    apply(P_SEQ_TENSION,100);
    for(i=0;i<EN_LOOP_LEN[l];i++) vc[v].loop_note[l][i]=(int8_t)(i%7);
    memcpy(notes,vc[v].loop_note[l],sizeof notes);
    CHECK(phrase_pass(v,l), "dialogue starts a coordinated phrase");
    CHECK(!memcmp(notes,vc[v].loop_note[l],sizeof notes), "opening preserves motif");
    CHECK(phrase_pass(v,l), "second pass belongs to the phrase");
    CHECK(!memcmp(notes,vc[v].loop_note[l],sizeof notes), "second repetition preserves motif");
    phrase_pass(v,l); /* tension */
    phrase_pass(v,l); /* answer */
    CHECK(!memcmp(notes,vc[v].loop_note[l],sizeof notes), "answer restores reference motif");
    CHECK(phrase_pos[v][l]==0, "answer closes cycle");
    apply(P_EVOLVE,0);
    CHECK(!phrase_pass(v,l), "Evolve zero disables dialogue");
    apply(P_EVOLVE,100); apply(P_SEQ_DIALOGUE,0);
    CHECK(!phrase_pass(v,l), "Dialogue zero preserves original evolution path");
    en_init();
    CHECK(phrase_on[v][l]==0, "restart clears phrase state");
}
static void rhythm_controls(void)
{
    int i, trial, skipped=0, v=1;
    en_init(); apply(P_SEQ_RANGE,0); apply(P_SEQ_HUMAN,0); apply(P_SEQ_SPACE,0);
    vc[v].loop_note[0][0]=50; vc[v].loop_vel[0][0]=70;
    for(i=0;i<NPOLY;i++) vc[v].n[i].on=0;
    bell_ring(v,0,0,123);
    for(i=0;i<NPOLY;i++) if(vc[v].n[i].on) {
        CHECK(vc[v].n[i].wait==123, "Humanize zero preserves exact step timing");
        CHECK(vc[v].n[i].midi_note>=36 && vc[v].n[i].midi_note<=103, "Range keeps MIDI register bounded");
    }
    apply(P_SEQ_HUMAN,100); apply(P_SEQ_SPACE,100);
    for(trial=0;trial<200;trial++) {
        int on=0;
        for(i=0;i<NPOLY;i++) vc[v].n[i].on=0;
        bell_ring(v,0,0,123);
        for(i=0;i<NPOLY;i++) if(vc[v].n[i].on) {
            on++;
            CHECK(vc[v].n[i].wait>=123 && vc[v].n[i].wait<=123+1764, "Humanize delay bounded");
        }
        skipped+=on==0;
    }
    CHECK(skipped>100 && skipped<200, "Space creates rests while retaining some notes");
    CHECK(vc[v].loop_note[0][0]==50, "Space and Range never erase or rewrite motif");
}
static void route_sum_order(void)
{
    const int permutations[3][3]={{100,100,-100},{100,-100,100},{-100,100,100}};
    int target, order, r, sign;
    for(target=XT_DENSITY;target<=XT_RESOLVE;target++)
        for(order=0;order<3;order++) for(sign=-1;sign<=1;sign+=2) {
            en_init(); apply(P_EX(0,EX_AMOUNT),100);
            for(r=0;r<3;r++) {
                apply(P_XR(0,r,XR_SOURCE),XS_OPEN);
                apply(P_XR(0,r,XR_TARGET),target);
                apply(P_XR(0,r,XR_WHEN),XW_LIVE);
                apply(P_XR(0,r,XR_DEPTH),sign*permutations[order][r]);
            }
            expr_block(0);
            CHECK(seq_offset(0,target-XT_DENSITY)==(float)(sign*100),
                  "route order %d, target %d, sign %d preserves algebraic sum",order,target,sign);
            apply(P_EX(0,EX_AMOUNT),50); expr_block(0);
            CHECK(seq_offset(0,target-XT_DENSITY)==(float)(sign*50),"Amount scales sum before final clamp");
        }
    en_init(); apply(P_EX(0,EX_AMOUNT),100);
    for(r=0;r<EN_XROUTES;r++) {
        apply(P_XR(0,r,XR_SOURCE),XS_OPEN); apply(P_XR(0,r,XR_TARGET),XT_MOTIF);
        apply(P_XR(0,r,XR_WHEN),XW_LIVE); apply(P_XR(0,r,XR_DEPTH),100);
    }
    expr_block(0);
    CHECK(xseq[0][XT_MOTIF-XT_DENSITY]==800,"full eight-route sum fits accumulator");
    apply(P_MS(0,0,MS_DEST),MOD_SEQ+XT_MOTIF-XT_DENSITY+1);
    en_mod[0][VOX_GEN0[0]]=-1.0f;
    CHECK(seq_offset(0,XT_MOTIF-XT_DENSITY)==100.0f,"LFO joins full expression sum before saturation");
    apply(P_EX(0,EX_AMOUNT),0); expr_block(0);
    CHECK(seq_offset(0,XT_MOTIF-XT_DENSITY)==-100.0f,"expression bypass leaves independent LFO active");
    CHECK(par[P_SEQ_MOTIF]==35,"summing leaves stored parameter unchanged");
}
int main(void)
{
    new_sequence_routes();
    CHECK(drift_secs(0) == 2.0f, "Drift nominal minimum is two seconds");
    CHECK(drift_secs(100)>299.0f && drift_secs(100)<301.0f, "Drift maximum remains five minutes");
    for(int d=1;d<=100;d++) CHECK(drift_secs(d)>drift_secs(d-1), "Drift increases monotonically");
    route_sum_order();
    rhythm_controls();
    dialogue_phrases();
    chords();
    motifs();
    swing();
    bass_passing();
    expr_sequencer();
    if (fails) {
        printf("seq: %d failure(s)\n", fails);
        return 1;
    }
    printf("seq: ok\n");
    return 0;
}
