/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal */
/* AMB-1 project: two flash objects, the settings (OBJ_PROJ) and the pieces' patches (OBJ_PATCH). A project of another
 * format, or none, gives the defaults. Part of the unity build, after app.h and bank.c.
 *
 * The pieces (their recipes are the engine's, en_piece): each keeps its own settings as you leave it, and each of its
 * three voices its own patch (as loaded from a source, or edited operator by operator); Tune and MIDI out are global. */
project_t proj;
uint8_t vpatch[EN_NVOX][EN_PATCH];
static patchstore_t pstore;

const char *piece_name(int i) { return en_piece_name(i); }

static int global_par(int p) { return p == P_TUNE || p == P_MIDI; }

static void recipe(int i, slot_t *s) { en_piece(i, s->par, &s->pool); }

/* the voices' patches as a piece's settings load them: each from its source (a bank's missing: the voice's first) */
static void patches_from(const int16_t *par, uint8_t pat[EN_NVOX][EN_PATCH])
{
    uint8_t p[EN_PATCH + 1];
    int v;
    for (v = 0; v < EN_NVOX; v++) {
        int src = par[EN_VOX_PATCH[v]];
        if ((unsigned)src < EN_NFACT)
            en_factory(v, src, par[P_BTONE], p);
        else if (!bank_voice(v, src, p))
            en_factory(v, 0, par[P_BTONE], p);
        memcpy(pat[v], p, EN_PATCH);
    }
}

static void take(const slot_t *s, uint8_t pat[EN_NVOX][EN_PATCH])   /* a piece into play, the globals kept */
{
    int p;
    for (p = 0; p < P_NPARAMS; p++)
        if (!global_par(p))
            proj.par[p] = s->par[p];
    proj.pool = s->pool;
    memcpy(vpatch, pat, sizeof vpatch);
}

static void keep(void)                           /* the piece playing, into its slot */
{
    int p;
    for (p = 0; p < P_NPARAMS; p++)
        proj.slot[proj.piece].par[p] = proj.par[p];
    proj.slot[proj.piece].pool = proj.pool;
    memcpy(pstore.pat[proj.piece], vpatch, sizeof vpatch);
}

void project_defaults(void)
{
    int i;
    memset(&proj, 0, sizeof proj);
    proj.magic = PROJ_MAGIC;
    proj.format = PROJ_FORMAT;
    pstore.magic = PROJ_MAGIC;
    pstore.format = PROJ_FORMAT;
    for (i = 0; i < NPIECES; i++) {
        recipe(i, &proj.slot[i]);
        patches_from(proj.slot[i].par, pstore.pat[i]);
    }
    for (i = 0; i < P_NPARAMS; i++)
        proj.par[i] = en_param_info(i)->def;
    take(&proj.slot[0], pstore.pat[0]);
    proj.leds = 1;
}

static int pool_ok(uint32_t p)                   /* the root and at least two more notes */
{
    int n = 0, i;
    for (i = 0; i < 12; i++)
        n += (int)(p >> i & 1u);
    return (p & 1u) && !(p >> 12) && n >= 3;
}

static int pars_ok(const int16_t *par)
{
    int i;
    for (i = 0; i < P_NPARAMS; i++) {
        const en_param_t *p = en_param_info(i);
        if (par[i] < p->lo || par[i] > p->hi)
            return 0;
    }
    return 1;
}

static int project_valid(void)                  /* an older format: the recipes again */
{
    int i;
    if (proj.magic != PROJ_MAGIC || !pool_ok(proj.pool) || !pars_ok(proj.par))
        return 0;
    if (proj.format != PROJ_FORMAT || proj.piece >= NPIECES)
        return 0;
    for (i = 0; i < NPIECES; i++)
        if (!pool_ok(proj.slot[i].pool) || !pars_ok(proj.slot[i].par))
            return 0;
    return 1;
}

static uint32_t keep_fmt;                       /* format 7's or 8's project: its patches kept as they are */
static int patches_valid(void)                  /* (a byte past its range: held to it, the rest kept) */
{
    int i, v, k;
    if (pstore.magic != PROJ_MAGIC || (pstore.format != PROJ_FORMAT && !(keep_fmt && pstore.format == keep_fmt)))
        return 0;
    for (i = 0; i < NPIECES; i++)
        for (v = 0; v < EN_NVOX; v++)
            for (k = 0; k < EN_PATCH; k++)
                if (pstore.pat[i][v][k] > en_patch_max(k))
                    pstore.pat[i][v][k] = en_patch_max(k);
    return 1;
}

/* older formats: fewer settings a piece (format 3, 0.9 before the layer pages: 23; format 4, before MOD: 28; format
 * 5, one MOD for all three voices: 32; format 6, each voice its own FM edits and MOD: 75; format 7, before each voice's
 * ENV and GATE: 156, its patches (OBJ_PATCH) as they are; format 8, before the voices were alike: 162, its patches as
 * they are, its GATEs kept, every voice on its patch's envelopes). Each: the layers' settings into the voices' own
 * (en_upgrade), the patches' sources into the eighteen (format 6 on: a bank's 12 on, the bells' own six 6 on, the
 * bass's 12 on). Carried over: each piece's
 * own, the new ones from its recipe; format 5's MOD as every voice's first modulator (en_mod_legacy); format 6's MOD
 * as the first modulator, its INDEX as it was, and its other FM edits (ratio, feedback, envelopes) baked into the
 * voice's patch (en_bake: the patch then edited, OWN) */
#define V3_NPARAMS (P_PADTONE + 1)
#define V4_NPARAMS (P_HATS + 1)
#define V5_NPARAMS (P_HATS + 5)
#define V6_VX 15
#define V6_NPARAMS (P_VOX + 3 * V6_VX)
#define V7_NPARAMS P_PLAY
#define V8_NPARAMS (P_PLAY + 3 * 2)
#define V9_NPARAMS P_TAPE
#define V10_NPARAMS (P_TAPE + 8)
#define V11_NPARAMS (P_TAPE + 12)
#define V12_NPARAMS (P_TMOD + 2 * 4)
static int off_up;                              /* a format before 14: the modulators' targets one on (Off first) */
static void target_up(int16_t *par, int t, int depth)
{
    par[t] = (int16_t)(par[depth] ? par[t] + 1 : 0);   /* (one at depth 0 did nothing: Off now) */
}
static void targets_up(int16_t *par, int lo, int k)   /* those of the settings lo .. k - 1, as they were stored */
{
    int v, s;
    if (!off_up)
        return;
    for (v = 0; v < EN_NVOX; v++)
        for (s = 0; s < EN_NSLOT; s++)
            if (P_MS(v, s, MS_DEST) >= lo && P_MS(v, s, MS_DEST) < k)
                target_up(par, P_MS(v, s, MS_DEST), P_MS(v, s, MS_DEPTH));
    for (s = 0; s < EN_NTMOD; s++)
        if (P_TM(s, TARGET) >= lo && P_TM(s, TARGET) < k)
            target_up(par, P_TM(s, TARGET), P_TM(s, DEPTH));
}
static int16_t mig6[NPIECES + 1][EN_NVOX][6];   /* format 6's ratio, feedback, attack, decay, sustain, release */
static int mig_n, mig6_on;
static void fix_none(int16_t *par, const int16_t *o) { (void)par, (void)o; }
static void fix_defaults(int16_t *par)
{
    int p;
    for (p = P_BELLPATCH; p < P_NPARAMS; p++)
        par[p] = en_param_info(p)->def;
}
static void fix_v5(int16_t *par, const int16_t *o)
{
    fix_defaults(par);
    en_mod_legacy(par, o[P_HATS + 1], o[P_HATS + 2], o[P_HATS + 3], o[P_HATS + 4]);
}
static void fix_v6(int16_t *par, const int16_t *o)
{
    int v, k, n = mig_n < NPIECES + 1 ? mig_n++ : NPIECES;
    fix_defaults(par);
    par[P_BELLPATCH] = o[P_BELLPATCH];
    par[P_BASSPATCH] = o[P_BASSPATCH];
    for (v = 0; v < EN_NVOX; v++) {
        const int16_t *b = o + P_VOX + v * V6_VX;
        par[P_VX(v, VX_INDEX)] = b[1];
        for (k = 0; k < MS_N; k++)               /* (format 6's MOD in MS_* order) */
            par[P_MS(v, 0, k)] = (int16_t)(b[7 + k] + (k == MS_DEST));   /* (Off first) */
        mig6[n][v][0] = b[0], mig6[n][v][1] = b[2];
        for (k = 0; k < 4; k++)
            mig6[n][v][2 + k] = b[3 + k];
        par[P_VX(v, VX_OWN)] = (int16_t)(b[0] || b[2] || b[3] || b[4] || b[5] || b[6]);
    }
}
static void fix_v11(int16_t *par, const int16_t *o)   /* the reel's source was the pulse: the mix now */
{
    (void)o;
    if (par[P_RL(SRC)] > 3)
        par[P_RL(SRC)] = 0;
}
static void fix_v12(int16_t *par, const int16_t *o)   /* format 12's two modulators of four into the five of six */
{
    int s, k;
    for (s = 0; s < 2; s++)
        for (k = 0; k < 4; k++)
            par[P_TMOD + s * TM_N + k] = (int16_t)(o[P_TMOD + s * 4 + k] + (k == TM_TARGET));   /* (Off first) */
}
static void fix_v8(int16_t *par, const int16_t *o)
{
    int v;
    for (v = 0; v < EN_NVOX; v++)
        par[P_PL(v, PL_GATE)] = o[P_PLAY + v * 2 + 1];
}
static void migrate_up(int16_t *par, uint32_t fmt)   /* a project of format fmt into this one's voices and sources */
{
    if (fmt >= 9u)                               /* (format 9's voices are as they are) */
        return;
    en_upgrade(par);
    if (par[P_PADTONE] >= 6)
        par[P_PADTONE] = (int16_t)(par[P_PADTONE] + 12);
    if (fmt >= 6u) {
        par[P_BELLPATCH] = (int16_t)(par[P_BELLPATCH] + (par[P_BELLPATCH] < 6 ? EN_FACT_BELL : 12));
        par[P_BASSPATCH] = (int16_t)(par[P_BASSPATCH] + 12);
    }
}
#define OLD_PROJECT(T, N, K, FIX, FMT)                                                                             \
    typedef struct {                                                                                               \
        int16_t par[N];                                                                                            \
        uint16_t pool;                                                                                             \
    } T##_slot_t;                                                                                                  \
    typedef struct {                                                                                               \
        uint32_t magic, format;                                                                                    \
        int16_t par[N];                                                                                            \
        uint16_t pool;                                                                                             \
        uint8_t leds, piece, rsv0[2];                                                                              \
        T##_slot_t slot[NPIECES];                                                                                  \
        uint8_t rsv[16];                                                                                           \
    } T##_t;                                                                                                       \
    static void from_##T(const T##_t *o)                                                                           \
    {                                                                                                              \
        slot_t r;                                                                                                  \
        int i, p;                                                                                                  \
        proj.magic = PROJ_MAGIC;                                                                                   \
        proj.format = PROJ_FORMAT;                                                                                 \
        proj.pool = o->pool;                                                                                       \
        proj.leds = o->leds;                                                                                       \
        proj.piece = o->piece < NPIECES ? o->piece : 0;                                                            \
        for (i = 0; i < NPIECES; i++) {                                                                            \
            recipe(i, &r);                                                                                         \
            for (p = 0; p < P_NPARAMS; p++)                                                                        \
                proj.slot[i].par[p] = p < (K) ? o->slot[i].par[p] : r.par[p];                                      \
            targets_up(proj.slot[i].par, 0, K);                                                                     \
            FIX(proj.slot[i].par, o->slot[i].par);                                                                 \
            migrate_up(proj.slot[i].par, FMT);                                                                     \
            proj.slot[i].pool = o->slot[i].pool;                                                                   \
        }                                                                                                          \
        recipe(proj.piece, &r);                                                                                    \
        for (p = 0; p < P_NPARAMS; p++)                                                                            \
            proj.par[p] = p < (K) ? o->par[p] : r.par[p];                                                          \
        targets_up(proj.par, 0, K);                                                                                 \
        FIX(proj.par, o->par);                                                                                     \
        migrate_up(proj.par, FMT);                                                                                 \
    }
OLD_PROJECT(project_v3, V3_NPARAMS, V3_NPARAMS, fix_none, 3u)
OLD_PROJECT(project_v4, V4_NPARAMS, V4_NPARAMS, fix_none, 4u)
OLD_PROJECT(project_v5, V5_NPARAMS, P_BELLPATCH, fix_v5, 5u)
OLD_PROJECT(project_v6, V6_NPARAMS, P_BELLPATCH, fix_v6, 6u)
OLD_PROJECT(project_v7, V7_NPARAMS, V7_NPARAMS, fix_none, 7u)
OLD_PROJECT(project_v8, V8_NPARAMS, P_PLAY, fix_v8, 8u)
OLD_PROJECT(project_v9, V9_NPARAMS, V9_NPARAMS, fix_none, 9u)
OLD_PROJECT(project_v10, V10_NPARAMS, V10_NPARAMS, fix_v11, 10u)
OLD_PROJECT(project_v11, V11_NPARAMS, V11_NPARAMS, fix_v11, 11u)
OLD_PROJECT(project_v12, V12_NPARAMS, P_TMOD, fix_v12, 12u)

/* formats 13 and 14 as stored: the settings to P_SPLIT in OBJ_PROJ (as the formats before, fewer settings), the rest in
 * OBJ_TAPE */
OLD_PROJECT(projstore, P_SPLIT, P_SPLIT, fix_none, 13u)
typedef struct {
    uint32_t magic, format;
    int16_t par[NPIECES + 1][P_NPARAMS - P_SPLIT];   /* each piece's, then the one playing's */
} tapestore_t;
_Static_assert(sizeof(projstore_t) <= PLAT_STORE_MAX && sizeof(tapestore_t) <= PLAT_STORE_MAX, "a store object");
static projstore_t pst PLAT_POOL;
static tapestore_t tst PLAT_POOL;
static void tape_from_store(void)               /* the reel's settings from OBJ_TAPE, or the pieces' recipes */
{
    int i, p;
    slot_t r;
    memset(&tst, 0, sizeof tst);
    if (plat_store_load(OBJ_TAPE, &tst, sizeof tst) < 8 || tst.magic != PROJ_MAGIC ||
        (tst.format != PROJ_FORMAT && tst.format != 13u)) {
        for (i = 0; i < NPIECES; i++) {
            recipe(i, &r);
            for (p = P_SPLIT; p < P_NPARAMS; p++)
                proj.slot[i].par[p] = r.par[p];
        }
        recipe(proj.piece, &r);
        for (p = P_SPLIT; p < P_NPARAMS; p++)
            proj.par[p] = r.par[p];
        return;
    }
    for (i = 0; i < NPIECES; i++)
        for (p = P_SPLIT; p < P_NPARAMS; p++)
            proj.slot[i].par[p] = tst.par[i][p - P_SPLIT];
    for (p = P_SPLIT; p < P_NPARAMS; p++)
        proj.par[p] = tst.par[NPIECES][p - P_SPLIT];
    if (tst.format == 13u) {                     /* (the reel's modulators' targets: Off first) */
        off_up = 1;
        for (i = 0; i < NPIECES; i++)
            targets_up(proj.slot[i].par, P_SPLIT, P_NPARAMS);
        targets_up(proj.par, P_SPLIT, P_NPARAMS);
    }
}

int project_load(void)
{
    static union {
        projstore_t now;
        project_v3_t v3;
        project_v4_t v4;
        project_v5_t v5;
        project_v6_t v6;
        project_v7_t v7;
        project_v8_t v8;
        project_v9_t v9;
        project_v10_t v10;
        project_v11_t v11;
        project_v12_t v12;
    } buf;
    int n, i, v, migrated = 1;
    memset(&buf, 0, sizeof buf);
    mig_n = mig6_on = 0;
    keep_fmt = 0;
    off_up = 0;
    n = plat_store_load(OBJ_PROJ, &buf, sizeof buf);
    off_up = n >= 8 && buf.now.format < 14u;
    if (n >= 8 && buf.now.magic == PROJ_MAGIC && buf.now.format == 3u)
        from_project_v3(&buf.v3);
    else if (n >= 8 && buf.now.magic == PROJ_MAGIC && buf.now.format == 4u)
        from_project_v4(&buf.v4);
    else if (n >= 8 && buf.now.magic == PROJ_MAGIC && buf.now.format == 5u)
        from_project_v5(&buf.v5);
    else if (n >= 8 && buf.now.magic == PROJ_MAGIC && buf.now.format == 6u) {
        mig6_on = 1;
        from_project_v6(&buf.v6);
    } else if (n >= 8 && buf.now.magic == PROJ_MAGIC && buf.now.format == 7u) {
        keep_fmt = 7u;
        from_project_v7(&buf.v7);
    } else if (n >= 8 && buf.now.magic == PROJ_MAGIC && buf.now.format == 8u) {
        keep_fmt = 8u;
        from_project_v8(&buf.v8);
    } else if (n >= 8 && buf.now.magic == PROJ_MAGIC && buf.now.format == 9u) {
        keep_fmt = 9u;
        from_project_v9(&buf.v9);
    } else if (n >= 8 && buf.now.magic == PROJ_MAGIC && buf.now.format == 10u) {
        keep_fmt = 10u;
        from_project_v10(&buf.v10);
    } else if (n >= 8 && buf.now.magic == PROJ_MAGIC && buf.now.format == 11u) {
        keep_fmt = 11u;
        from_project_v11(&buf.v11);
    } else if (n >= 8 && buf.now.magic == PROJ_MAGIC && buf.now.format == 12u) {
        keep_fmt = 12u;
        from_project_v12(&buf.v12);
    } else if (n >= 8 && buf.now.magic == PROJ_MAGIC && (buf.now.format == PROJ_FORMAT || buf.now.format == 13u)) {
        if (buf.now.format == 13u)
            keep_fmt = 13u;                      /* (its patches as they are) */
        from_projstore(&buf.now);                /* (its settings to P_SPLIT; then the reel's from OBJ_TAPE) */
        proj.leds = buf.now.leds;
        tape_from_store();
        migrated = buf.now.format == 13u;
    } else {
        proj.magic = 0;                          /* (none, or another: the defaults below) */
    }
    if (n < 8 || !project_valid()) {
        project_defaults();
        return -1;
    }
    memset(&pstore, 0, sizeof pstore);
    if ((migrated && !keep_fmt) || plat_store_load(OBJ_PATCH, &pstore, sizeof pstore) < 8 || !patches_valid()) {
        pstore.magic = PROJ_MAGIC;               /* none, or a format before them: from the pieces' sources */
        pstore.format = PROJ_FORMAT;
        for (i = 0; i < NPIECES; i++) {
            patches_from(proj.slot[i].par, pstore.pat[i]);
            if (mig6_on)
                for (v = 0; v < EN_NVOX; v++) {
                    const int16_t *m = mig6[i][v];
                    en_bake(pstore.pat[i][v], m[0], m[1], m[2], m[3], m[4], m[5]);
                }
        }
    }
    pstore.format = PROJ_FORMAT;
    memcpy(vpatch, pstore.pat[proj.piece], sizeof vpatch);
    if (mig6_on) {                               /* the playing piece as format 6 had it (its own edits) */
        patches_from(proj.par, vpatch);
        for (v = 0; v < EN_NVOX; v++) {
            const int16_t *m = mig6[NPIECES][v];
            en_bake(vpatch[v], m[0], m[1], m[2], m[3], m[4], m[5]);
        }
    }
    return 0;
}

int project_save(void)                          /* the settings in two objects (P_SPLIT), the patches in a third */
{
    int r, i, p;
    keep();
    memset(&pst, 0, sizeof pst);
    pst.magic = PROJ_MAGIC;
    pst.format = PROJ_FORMAT;
    pst.pool = proj.pool;
    pst.leds = proj.leds;
    pst.piece = proj.piece;
    tst.magic = PROJ_MAGIC;
    tst.format = PROJ_FORMAT;
    for (p = 0; p < P_NPARAMS; p++) {
        if (p < P_SPLIT)
            pst.par[p] = proj.par[p];
        else
            tst.par[NPIECES][p - P_SPLIT] = proj.par[p];
        for (i = 0; i < NPIECES; i++) {
            if (p < P_SPLIT)
                pst.slot[i].par[p] = proj.slot[i].par[p];
            else
                tst.par[i][p - P_SPLIT] = proj.slot[i].par[p];
        }
    }
    for (i = 0; i < NPIECES; i++)
        pst.slot[i].pool = proj.slot[i].pool;
    r = plat_store_save(OBJ_PROJ, &pst, sizeof pst);
    r = r ? r : plat_store_save(OBJ_TAPE, &tst, sizeof tst);
    return r ? r : plat_store_save(OBJ_PATCH, &pstore, sizeof pstore);
}

void project_piece(int i)
{
    keep();
    proj.piece = (uint8_t)((i % NPIECES + NPIECES) % NPIECES);
    take(&proj.slot[proj.piece], pstore.pat[proj.piece]);
}

void project_piece_reset(void)
{
    static uint8_t pat[EN_NVOX][EN_PATCH];
    slot_t s;
    recipe(proj.piece, &s);
    patches_from(s.par, pat);
    take(&s, pat);
}

/* ------------------------------------------------------- the voices' patches --- */
void project_patch_send(int voice, int fresh) { en_patch_load(voice, vpatch[voice], fresh); }

void project_source(int voice, int src)
{
    uint8_t p[EN_PATCH + 1];
    const int sp = EN_VOX_PATCH[voice];
    if (!bank_voice(voice, src, p))
        return;
    memcpy(vpatch[voice], p, EN_PATCH);
    proj.par[sp] = (int16_t)src;
    proj.par[P_VX(voice, VX_OWN)] = 0;
    en_set(sp, src);                             /* (the voice's own: the engine makes it from this) */
    en_set(P_VX(voice, VX_OWN), 0);
    if (src >= EN_NFACT)
        project_patch_send(voice, 1);
}

void project_patch_set(int voice, int off, int v)
{
    if ((unsigned)off >= EN_PATCH)
        return;
    v = v < 0 ? 0 : v > en_patch_max(off) ? en_patch_max(off) : v;
    if (!proj.par[P_VX(voice, VX_OWN)]) {        /* the first edit: from the source as it is now (Bell tone's) */
        uint8_t p[EN_PATCH + 1];
        if (bank_voice(voice, proj.par[EN_VOX_PATCH[voice]], p))
            memcpy(vpatch[voice], p, EN_PATCH);
        vpatch[voice][off] = (uint8_t)v;
        proj.par[P_VX(voice, VX_OWN)] = 1;
        en_set(P_VX(voice, VX_OWN), 1);
        project_patch_send(voice, 0);
        return;
    }
    vpatch[voice][off] = (uint8_t)v;
    project_patch_send(voice, 0);
}

void project_apply(void)
{
    int i;
    for (i = 0; i < P_NPARAMS; i++)
        en_set(i, proj.par[i]);
    en_pool(proj.pool);
    for (i = 0; i < EN_NVOX; i++)                /* (the voice's own six, as loaded: made by the engine) */
        if (proj.par[P_VX(i, VX_OWN)] || proj.par[EN_VOX_PATCH[i]] >= EN_NFACT)
            project_patch_send(i, 1);
}

/* ------------------------------------------------------------- the reel --- */
/* the reel is saved a sector a call (project_reel_step, a UI frame each): an erase stops everything for a moment, and
 * the main loop, between, keeps the watchdog fed and the screen showing how far it is. Its samples, its blocks' states,
 * then its splices (the seal: a reel half saved has none) */
static struct {
    uint8_t stage;                               /* 0 none, 1 the seal's sector, 2 the samples, 3 the states, 4 the seal */
    uint32_t k;
} rsv;
int project_reel_save(void)
{
    if (rsv.stage)
        return -1;
    rsv.stage = 1;
    rsv.k = 0;
    return 0;
}
static int sector_put(uint32_t off, const uint8_t *src, uint32_t n)   /* a sector erased, n (<= 4096) bytes into it */
{
    uint32_t k;
    if (plat_data_erase(off))
        return -1;
    for (k = 0; k < n; k += 256u)
        if (plat_data_write(off + k, src + k, n - k < 256u ? n - k : 256u))
            return -1;
    return 0;
}
int project_reel_step(void)
{
    uint32_t n;
    const uint8_t *src;
    int part = rsv.stage == 2 ? 2 : 1;
    if (!rsv.stage)
        return 0;
    if (rsv.stage == 1) {
        rsv.stage = plat_data_erase(REEL_BASE) ? 0 : 2;
        return rsv.stage ? 1 : -1;
    }
    if (rsv.stage == 4) {
        src = (const uint8_t *)en_reel_mem(0, &n);
        rsv.stage = 0;
        return plat_data_write(REEL_BASE, src, n) ? -1 : 2;
    }
    src = (const uint8_t *)en_reel_mem(part, &n);
    if (rsv.k < n) {
        const uint32_t base = REEL_BASE + (rsv.stage == 2 ? 0x3000u : 0x1000u);
        if (sector_put(base + rsv.k, src + rsv.k, n - rsv.k < 4096u ? n - rsv.k : 4096u)) {
            rsv.stage = 0;
            return -1;
        }
        rsv.k += 4096u;
        return 1;
    }
    rsv.stage++;
    rsv.k = 0;
    return 1;
}
int project_reel_progress(void)                 /* 0..100 while saving; -1 not saving */
{
    uint32_t n;
    if (!rsv.stage)
        return -1;
    if (rsv.stage != 2)
        return rsv.stage < 2 ? 0 : 95;
    en_reel_mem(2, &n);
    return n ? (int)(rsv.k * 95u / (n + 4095u)) : 95;
}
void project_reel_load(void)                    /* at the start: the reel kept, if one is */
{
    uint32_t n0, n1, n3;
    uint8_t *m = (uint8_t *)en_reel_mem(0, &n0), *h = (uint8_t *)en_reel_mem(1, &n1), *s;
    const uint8_t *f = plat_data(REEL_BASE);
    if (!f)
        return;
    memcpy(m, f, n0);
    if (!en_reel_valid())
        return;
    {
        uint32_t n2;
        en_reel_mem(2, &n2);
        s = (uint8_t *)en_reel_mem(3, &n3);
        memcpy(h, plat_data(REEL_BASE + 0x1000u), n1);
        memcpy(s, plat_data(REEL_BASE + 0x3000u), n2 < n3 ? n2 : n3);
    }
    en_reel_valid();
}
