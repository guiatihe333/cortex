/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal */
/* AMB-1 in fm1-base's host simulator (host/plat_host.c, which includes app.c and so this): AMB-1's checks for
 * "expect", its command, a reboot and the screen reader's dump.
 *
 * AMB-1's checks: battery (battery N: plat_battery as N, -1..4; "battery off" the simulator's own) playing frozen view piece pool dirty sixteenths ext pad_level bass_level chordroot chordN loopnotes
 * parN, patV_OFF (voice V's patch byte OFF) (fm1-base's own: store_writes clocks_out midi_out peak_db_max
 * peak_db_min). Its commands: storev3 N (the project as AMB-1 0.9 before the layer pages wrote it, format 3, the
 * playing piece's Pad at N); storev5 N (as the first FM6 build wrote it, format 5: one MOD for all three voices, its
 * depth N, chaos 90, target Feedback); storev6 N (as 1.3.91 wrote it, format 6: each voice's MOD at depth N, the
 * pad's FM edits Ratio x2 and Feedback +2); storev7 (as 1.3.93 wrote it, format 7: the pieces as they are, their
 * patches too); storev8 N (as 1.3.95 wrote it, format 8: every GATE at N, the bells on Tubular); storev13 (as 1.4.2 wrote it,
 * format 13: the modulators' targets with no Off before them); bankfake B (a bank of 32 voices, the own patches renamed TEST 01..,
 * sent to bank B as amb_syx.py sends one: 16 SysEx chunks); dx7voice (a DX7 single voice dump, the bass's Pick
 * named DX7 VOICE, as Dexed sends one); songfake N [BPM] (a test song, 4 bars of C and F, into song slot N as amb_mid.py
 * sends one); sysexfile PATH (a file of SysEx messages, each as from USB: tools/FITA.html's, written by
 * tests/host/fita_test.js). */
/* a song's record for the tests: 4 bars at 100 BPM, C major; C and F a half bar each; a melody a quarter (C E G A
 * on C, F A C' E on F: the chord's own), the bass each half bar (C2, F2) */
static int host_song(uint8_t *r)
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
static void host_pack(const uint8_t *p, uint8_t *m)   /* a patch as a DX7 bank packs it */
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

/* the knobs SEL's dice never move: every page rolled 8 times, each knob shown watched; the player's own (Tune, MIDI
 * out, the lights, the reel's source), the pickers (operator, stage, modulator), a transpose, an operator's ratio mode
 * (kept) and a splice to pick (none recorded) left out. Each named */
static int dice_unmoved(void)
{
    int v, i, r, bad = 0;
    const uint8_t view = ui.view;
    for (v = 0; v < NVIEWS; v++) {
        int k0[4], moved[4] = {0, 0, 0, 0};
        ui.view = (uint8_t)v;
        for (i = 0; i < 4; i++)
            k0[i] = knob_get(view_knob(v, i));
        for (r = 0; r < 8; r++) {
            dice_page(v);
            for (i = 0; i < 4; i++)
                moved[i] |= knob_get(view_knob(v, i)) != k0[i];
        }
        for (i = 0; i < 4; i++) {
            const int k = view_knob(v, i);
            char nm[24];
            if (moved[i] || k == K_NONE || k == K_OP || k == K_STAGE || k == K_PSTAGE || k == K_SLOT || k == K_TSLOT ||
                k == K_XSLOT || k == K_MCOMP ||
                k == K_LEDS || k == K_CPU || k == P_TUNE || k == P_MIDI || k == P_RL(SRC) || k == P_RL(ORG) ||
                k == P_SG(SONG) || k == P_SG(START) || ((k >= P_SETP && k < P_ST(VBRIGHT)) && !(k >= P_ST(VMODE) && k <= P_ST(VSIZE2))) ||
                k == K_RSLOT || k == K_TKEY ||
                k == K_TSHIFT ||
                k == KV(TRNSP) || k == KO(MODE) ||
                (k >= P_EXPR && ((k - P_EXPR) % EN_XSTRIDE == EX_MORPH_SRC || (k - P_EXPR) % EN_XSTRIDE == EX_MORPH_KIND)))
                continue;
            view_name(v, nm);
            printf("dice: %s, knob %d (%s) never moved\n", nm, i + 1, knob_label(v, i));
            bad++;
        }
    }
    ui.view = view;
    return bad;
}

/* Preset workflow invariant: interpretation must preserve synthesis, tuning and A/B endpoints. */
static int16_t host_xbase[P_NPARAMS];
static uint8_t host_xpatch[EN_NVOX][EN_PATCH];
static int host_xbase_kept(void)
{
    int p;
    for (p = 0; p < P_NPARAMS; p++) {
        const int off = p >= P_EXPR && p < P_SEQ ? (p - P_EXPR) % EN_XSTRIDE : -1;
        if ((p < P_EXPR || p >= P_SEQ || (off >= EX_MORPH_SRC && off <= EX_MORPH_KIND)) && proj.par[p] != host_xbase[p])
            return 0;
    }
    return !memcmp(host_xpatch, vpatch, sizeof host_xpatch);
}

int app_expect(const char *what, int *got)
{
    int g;
    if (!strcmp(what, "xbaseunchanged"))
        g = host_xbase_kept();
    else if (!strcmp(what, "playing"))
        g = en_playing;
    else if (!strcmp(what, "tape_playing"))
        g = en_tape_playing;
    else if (!strcmp(what, "frozen"))
        g = en_frozen;
    else if (!strcmp(what, "view"))
        g = ui.view;
    else if (!strcmp(what, "diceunmoved"))      /* the knobs the dice never move (dice_unmoved) */
        g = dice_unmoved();
    else if (!strcmp(what, "piece"))
        g = proj.piece;
    else if (!strcmp(what, "pool"))               /* the scale, relative to the root (hex ok) */
        g = proj.pool;
    else if (!strcmp(what, "battery"))
        g = batt();
    else if (!strcmp(what, "dirty"))
        g = ui.dirty;
    else if (!strcmp(what, "sixteenths"))
        g = (int)en_bars;
    else if (!strcmp(what, "ext"))
        g = en_ext;
    else if (!strcmp(what, "pad_level"))
        g = (int)(en_pad_level * 100.0f);
    else if (!strcmp(what, "bass_level"))
        g = (int)(en_bass_level * 100.0f);
    else if (!strcmp(what, "chordroot"))
        g = en_chord_root;
    else if (!strncmp(what, "chord", 5))          /* chordN: pad voice N's note */
        g = en_chord[atoi(what + 5) & 3];
    else if (!strcmp(what, "loopnotes")) {        /* the notes in all the loops */
        int l, i;
        g = 0;
        for (l = 0; l < EN_NLOOP; l++)
            for (i = 0; i < EN_LOOP_MAX; i++)
                g += en_loop_note[l][i] >= 0;
    } else if (!strcmp(what, "reelstate"))      /* the reel: 0 resting, 1 recording a splice, 2 overdubbing */
        g = en_reel_state;
    else if (!strcmp(what, "reelspl"))          /* its splices */
        g = en_reel_nspl;
    else if (!strcmp(what, "reelsec"))          /* its seconds recorded, tenths */
        g = (int)(en_reel_len * 10u / 22050u);
    else if (!strcmp(what, "reellevel"))        /* its sound now (1.5.25: the tape's own transport) */
        g = (int)(en_reel_level * 100.0f);
    else if (!strncmp(what, "xpar", 4) && strchr(what, '_')) {
        const int v = atoi(what + 4), k = atoi(strchr(what, '_') + 1);
        if ((unsigned)v >= EN_NVOX || (unsigned)k >= EN_XSTRIDE)
            return 0;
        g = proj.par[P_EX(v, k)];
    } else if (!strncmp(what, "xroute", 6) && strchr(what, '_')) {
        const int v = atoi(what + 6), k = atoi(strchr(what, '_') + 1);
        if ((unsigned)v >= EN_NVOX || (unsigned)k >= EN_XROUTES)
            return 0;
        g = en_expr_route[v][k];
    } else if (!strncmp(what, "xmeter", 6) && strchr(what, '_')) {
        const int v = atoi(what + 6), k = atoi(strchr(what, '_') + 1);
        if ((unsigned)v >= EN_NVOX || (unsigned)k >= 4u)
            return 0;
        g = en_expression[v][k];
    } else if (!strncmp(what, "xphase", 6)) {
        const int v = atoi(what + 6);
        if ((unsigned)v >= EN_NVOX)
            return 0;
        g = en_expr_phase[v];
    }
    else if (!strncmp(what, "par", 3))          /* parN: parameter N (amb.h P_*) */
        g = proj.par[atoi(what + 3)];
    else if (!strncmp(what, "pat", 3) && strchr(what, '_'))   /* patV_OFF: voice V's patch byte OFF */
        g = vpatch[atoi(what + 3) % EN_NVOX][atoi(strchr(what, '_') + 1) % EN_PATCH];
    else
        return 0;
    *got = g;
    return 1;
}

int app_host_cmd(const char *cmd, const char *a, const char *b, const char *c, const char *d)
{
    (void)b, (void)c, (void)d;
    if (!strcmp(cmd, "xsavebase")) {
        memcpy(host_xbase, proj.par, sizeof host_xbase);
        memcpy(host_xpatch, vpatch, sizeof host_xpatch);
        return 1;
    }

    if (!strcmp(cmd, "battery")) {               /* battery N: plat_battery as N (-1..4), "battery off" its own */
        batt_host = (int8_t)(strcmp(a, "off") ? atoi(a) : -2);
        return 1;
    }
    if (!strcmp(cmd, "storev3")) {
        static project_v3_t o;
        int i, k;
        memset(&o, 0, sizeof o);
        o.magic = PROJ_MAGIC;
        o.format = 3u;
        for (k = 0; k < V3_NPARAMS; k++)
            o.par[k] = proj.par[k];
        o.par[P_PAD] = (int16_t)atoi(a);
        o.pool = proj.pool;
        o.leds = proj.leds;
        o.piece = proj.piece;
        for (i = 0; i < NPIECES; i++) {
            for (k = 0; k < V3_NPARAMS; k++)
                o.slot[i].par[k] = proj.slot[i].par[k];
            o.slot[i].pool = proj.slot[i].pool;
        }
        o.slot[proj.piece].par[P_PAD] = o.par[P_PAD];
        plat_store_save(OBJ_PROJ, &o, sizeof o);
        return 1;
    }
    if (!strcmp(cmd, "storev5")) {               /* format 5, one MOD for all: depth N, rate 31, chaos 90, Feedback */
        static project_v5_t o;
        int i, k;
        memset(&o, 0, sizeof o);
        o.magic = PROJ_MAGIC;
        o.format = 5u;
        for (k = 0; k < P_BELLPATCH; k++)
            o.par[k] = proj.par[k];
        o.par[P_HATS + 1] = (int16_t)atoi(a);
        o.par[P_HATS + 2] = 31;
        o.par[P_HATS + 3] = 90;
        o.par[P_HATS + 4] = MOD_FEEDBACK;
        o.pool = proj.pool;
        o.leds = proj.leds;
        o.piece = proj.piece;
        for (i = 0; i < NPIECES; i++) {
            for (k = 0; k < V5_NPARAMS; k++)
                o.slot[i].par[k] = k < P_BELLPATCH ? proj.slot[i].par[k] : o.par[k];
            o.slot[i].pool = proj.slot[i].pool;
        }
        plat_store_save(OBJ_PROJ, &o, sizeof o);
        return 1;
    }
    if (!strcmp(cmd, "storev6")) {               /* format 6: each voice's MOD at depth N, the pad's x2 and FB +2 */
        static project_v6_t o;
        int i, k, v;
        memset(&o, 0, sizeof o);
        o.magic = PROJ_MAGIC;
        o.format = 6u;
        o.pool = proj.pool;
        o.leds = proj.leds;
        o.piece = proj.piece;
        for (i = 0; i <= NPIECES; i++) {
            int16_t *par = i < NPIECES ? o.slot[i].par : o.par;
            for (k = 0; k < P_BELLPATCH; k++)
                par[k] = i < NPIECES ? proj.slot[i].par[k] : proj.par[k];
            for (v = 0; v < EN_NVOX; v++) {
                int16_t *bk = par + P_VOX + v * V6_VX;
                bk[7] = (int16_t)atoi(a);         /* depth, rate, shape, target, spread */
                bk[8] = 31, bk[9] = SHAPE_SQUARE, bk[10] = MOD_PITCH, bk[11] = 100;
                if (v == VOX_PAD)
                    bk[0] = 1, bk[2] = 2;         /* Ratio x2, Feedback +2 */
            }
            if (i < NPIECES)
                o.slot[i].pool = proj.slot[i].pool;
        }
        plat_store_save(OBJ_PROJ, &o, sizeof o);
        return 1;
    }
    if (!strcmp(cmd, "storev7")) {               /* format 7 (as 1.3.93 wrote it): the pieces and their patches */
        static project_v7_t o;
        static patchstore_t ps;
        int i, k;
        memset(&o, 0, sizeof o);
        o.magic = PROJ_MAGIC;
        o.format = 7u;
        o.pool = proj.pool;
        o.leds = proj.leds;
        o.piece = proj.piece;
        keep();
        for (k = 0; k < V7_NPARAMS; k++)
            o.par[k] = proj.par[k];
        for (i = 0; i < NPIECES; i++) {
            for (k = 0; k < V7_NPARAMS; k++)
                o.slot[i].par[k] = proj.slot[i].par[k];
            o.slot[i].pool = proj.slot[i].pool;
        }
        ps = pstore;
        ps.format = 7u;
        plat_store_save(OBJ_PROJ, &o, sizeof o);
        plat_store_save(OBJ_PATCH, &ps, sizeof ps);
        return 1;
    }
    if (!strcmp(cmd, "sysexfile")) {             /* a file of SysEx messages (F0 .. F7), each to the app as from USB */
        static uint8_t d[1u << 20];
        FILE *f = fopen(a, "rb");
        size_t n, i, s = 0;
        if (!f) {
            printf("sysexfile: no %s\n", a);
            return 1;
        }
        n = fread(d, 1, sizeof d, f);
        fclose(f);
        for (i = 0; i < n; i++)
            if (d[i] == 0xF0)
                s = i + 1;
            else if (d[i] == 0xF7 && s)
                app_sysex(d + s, (uint32_t)(i - s)), s = 0;
        return 1;
    }
    if (!strcmp(cmd, "songfake")) {              /* a song into slot N, as amb_mid.py sends one: chunks, then done */
        static uint8_t r[0x4000], f[6 + 256];
        const int slot = atoi(a), n = host_song(r);
        int c;
        if (atoi(b) > 0)                         /* songfake N BPM: its tempo (100 else) */
            r[7] = (uint8_t)(atoi(b) >> 7 & 127), r[8] = (uint8_t)(atoi(b) & 127);
        for (c = 0; c * 256 < n; c++) {
            f[0] = 0x7D, f[1] = 0x41, f[2] = 0x4D, f[3] = 0x07, f[4] = (uint8_t)slot, f[5] = (uint8_t)c;
            memcpy(f + 6, r + c * 256, 256);
            app_sysex(f, 6 + 256);
        }
        f[3] = 0x0B;
        app_sysex(f, 5);
        return 1;
    }
    if (!strcmp(cmd, "storev30") || !strcmp(cmd, "storev31") || !strcmp(cmd, "storev32")) {
        const uint32_t old_format = !strcmp(cmd, "storev30") ? 30u : !strcmp(cmd, "storev31") ? 31u : 32u;
        const int old_end = old_format == 30u ? P_SEQ : P_SEQ + 4;
        uint32_t bit = 0;
        int i, p;
        project_save();
        pst.format = tst.format = pstore.format = old_format;
        memset(tst.expression, 0, sizeof tst.expression);
        for (i = 0; i <= NPIECES; i++) {
            const int16_t *values = i < NPIECES ? proj.slot[i].par : proj.par;
            for (p = P_EXPR; p < old_end; p++) {
                const en_param_t *q = en_param_info(p);
                const unsigned width = stored_expression_width(p, old_format);
                const unsigned value = (unsigned)(values[p] - q->lo);
                unsigned k;
                for (k = 0; k < width; k++, bit++)
                    tst.expression[bit >> 3] |= (uint8_t)(((value >> k) & 1u) << (bit & 7u));
            }
        }
        plat_store_save(OBJ_PROJ, &pst, sizeof pst);
        plat_store_save(OBJ_TAPE, &tst, sizeof tst);
        plat_store_save(OBJ_PATCH, &pstore, sizeof pstore);
        return 1;
    }
    if (!strcmp(cmd, "storev13")) {              /* format 13 (as 1.4.2 wrote it): the targets with no Off before them */
        int i, v, s;
        project_save();
        pst.format = tst.format = 13u;
        for (i = 0; i <= NPIECES; i++) {
            int16_t *par = i < NPIECES ? pst.slot[i].par : pst.par, *tp = tst.par[i];
            for (v = 0; v < EN_NVOX; v++)
                for (s = 0; s < EN_NSLOT; s++)
                    par[P_MS(v, s, MS_DEST)] = (int16_t)(par[P_MS(v, s, MS_DEST)] > 0 ? par[P_MS(v, s, MS_DEST)] - 1 : 0);
            for (s = 0; s < EN_NTMOD; s++)
                tp[P_TM(s, TARGET) - P_SPLIT] = (int16_t)(tp[P_TM(s, TARGET) - P_SPLIT] > 0 ? tp[P_TM(s, TARGET) - P_SPLIT] - 1 : 0);
        }
        plat_store_save(OBJ_PROJ, &pst, sizeof pst);
        {                                        /* (its OBJ_TAPE as 1.4.2 had it: to P_SONGP) */
            static tapestore14_t o;
            o.magic = tst.magic;
            o.format = 13u;
            for (i = 0; i <= NPIECES; i++)
                for (s = 0; s < P_SONGP - P_SPLIT; s++)
                    o.par[i][s] = tst.par[i][s];
            plat_store_save(OBJ_TAPE, &o, sizeof o);
        }
        return 1;
    }
    if (!strcmp(cmd, "storev8")) {               /* format 8 (as 1.3.95 wrote it): its GATEs at N */
        static project_v8_t o;
        static patchstore_t ps;
        int i, k, v;
        memset(&o, 0, sizeof o);
        o.magic = PROJ_MAGIC;
        o.format = 8u;
        o.pool = proj.pool;
        o.leds = proj.leds;
        o.piece = proj.piece;
        keep();
        for (i = 0; i <= NPIECES; i++) {
            int16_t *par = i < NPIECES ? o.slot[i].par : o.par;
            const int16_t *src = i < NPIECES ? proj.slot[i].par : proj.par;
            for (k = 0; k < P_PLAY; k++)
                par[k] = src[k];
            for (v = 0; v < EN_NVOX; v++) {
                par[P_PLAY + v * 2] = 0;
                par[P_PLAY + v * 2 + 1] = (int16_t)atoi(a);
            }
            par[P_BELLPATCH] = 3;                /* the bells' Tubular, as format 8 numbered it */
            if (i < NPIECES)
                o.slot[i].pool = proj.slot[i].pool;
        }
        ps = pstore;
        ps.format = 8u;
        plat_store_save(OBJ_PROJ, &o, sizeof o);
        plat_store_save(OBJ_PATCH, &ps, sizeof ps);
        return 1;
    }
    if (!strcmp(cmd, "bankfake")) {              /* bank B: the own patches, renamed, as amb_syx.py sends them */
        static uint8_t f[6 + 256], vm[4096];
        uint8_t p[EN_PATCH + 1];
        int b = atoi(a), v, k;
        for (v = 0; v < 32; v++) {
            en_factory(v % EN_NVOX, (v / EN_NVOX) % EN_NFACT, 50, p);
            memcpy(p + EP_NAME, "TEST      ", 10);
            p[EP_NAME + 5] = (char)('0' + (v + 1) / 10);
            p[EP_NAME + 6] = (char)('0' + (v + 1) % 10);
            host_pack(p, vm + v * 128);
        }
        for (k = 0; k < 16; k++) {
            f[0] = 0x7D, f[1] = 0x41, f[2] = 0x4D, f[3] = 0x01, f[4] = (uint8_t)b, f[5] = (uint8_t)k;
            memcpy(f + 6, vm + k * 256, 256);
            app_sysex(f, sizeof f);
        }
        return 1;
    }
    if (!strcmp(cmd, "dx7voice")) {              /* a DX7 single voice dump (VCED), as Dexed sends one */
        static uint8_t f[161];
        uint32_t k, sum = 0;
        f[0] = 0x43, f[1] = 0x00, f[2] = 0x00, f[3] = 0x01, f[4] = 0x1B;
        en_factory(VOX_BASS, 2, 50, f + 5);
        memcpy(f + 5 + EP_NAME, "DX7 VOICE ", 10);
        for (k = 0; k < 155u; k++)
            sum += f[5 + k];
        f[160] = (uint8_t)((128u - (sum & 127u)) & 127u);
        app_sysex(f, sizeof f);
        return 1;
    }
    return 0;
}

void app_host_reboot(void)
{
    memset(&proj, 0, sizeof proj);
    memset(vpatch, 0, sizeof vpatch);
}

void app_speak_dump(void) { speak_dump(); }

#ifdef PLAT_WEB
/* the browser emulator: a SysEx frame from the page (a song or a bank, as a computer sends it over USB; the bytes
 * between F0 and F7) to the app, which the base emulator only hands to its screen reader */
#pragma push_macro("__attribute__")
#undef __attribute__
static uint8_t web_sx[512];
__attribute__((used, visibility("default"))) uint8_t *web_app_sysex_buf(void) { return web_sx; }
__attribute__((used, visibility("default"))) int web_app_sysex(uint32_t n)
{
    return app_sysex(web_sx, n > sizeof web_sx ? (uint32_t)sizeof web_sx : n);
}
#pragma pop_macro("__attribute__")
#endif
