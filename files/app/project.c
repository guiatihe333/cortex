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

static int mig7_on;                             /* format 7's project: its patches kept as they are */
static int patches_valid(void)                  /* (a byte past its range: held to it, the rest kept) */
{
    int i, v, k;
    if (pstore.magic != PROJ_MAGIC || (pstore.format != PROJ_FORMAT && !(mig7_on && pstore.format == 7u)))
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
 * ENV and GATE: 156, its patches (OBJ_PATCH) as they are). Carried over: each piece's
 * own, the new ones from its recipe; format 5's MOD as every voice's first modulator (en_mod_legacy); format 6's MOD
 * as the first modulator, its INDEX as it was, and its other FM edits (ratio, feedback, envelopes) baked into the
 * voice's patch (en_bake: the patch then edited, OWN) */
#define V3_NPARAMS (P_PADTONE + 1)
#define V4_NPARAMS (P_HATS + 1)
#define V5_NPARAMS (P_HATS + 5)
#define V6_VX 15
#define V6_NPARAMS (P_VOX + 3 * V6_VX)
#define V7_NPARAMS P_PLAY
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
            par[P_MS(v, 0, k)] = b[7 + k];
        mig6[n][v][0] = b[0], mig6[n][v][1] = b[2];
        for (k = 0; k < 4; k++)
            mig6[n][v][2 + k] = b[3 + k];
        par[P_VX(v, VX_OWN)] = (int16_t)(b[0] || b[2] || b[3] || b[4] || b[5] || b[6]);
    }
}
#define OLD_PROJECT(T, N, K, FIX)                                                                                  \
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
            FIX(proj.slot[i].par, o->slot[i].par);                                                                 \
            proj.slot[i].pool = o->slot[i].pool;                                                                   \
        }                                                                                                          \
        recipe(proj.piece, &r);                                                                                    \
        for (p = 0; p < P_NPARAMS; p++)                                                                            \
            proj.par[p] = p < (K) ? o->par[p] : r.par[p];                                                          \
        FIX(proj.par, o->par);                                                                                     \
    }
OLD_PROJECT(project_v3, V3_NPARAMS, V3_NPARAMS, fix_none)
OLD_PROJECT(project_v4, V4_NPARAMS, V4_NPARAMS, fix_none)
OLD_PROJECT(project_v5, V5_NPARAMS, P_BELLPATCH, fix_v5)
OLD_PROJECT(project_v6, V6_NPARAMS, P_BELLPATCH, fix_v6)
OLD_PROJECT(project_v7, V7_NPARAMS, V7_NPARAMS, fix_none)

int project_load(void)
{
    static union {
        project_t now;
        project_v3_t v3;
        project_v4_t v4;
        project_v5_t v5;
        project_v6_t v6;
        project_v7_t v7;
    } buf;
    int n, i, v, migrated = 1;
    memset(&buf, 0, sizeof buf);
    mig_n = mig6_on = mig7_on = 0;
    n = plat_store_load(OBJ_PROJ, &buf, sizeof buf);
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
        mig7_on = 1;
        from_project_v7(&buf.v7);
    } else {
        proj = buf.now;
        migrated = 0;
    }
    if (n < 8 || !project_valid()) {
        project_defaults();
        return -1;
    }
    memset(&pstore, 0, sizeof pstore);
    if ((migrated && !mig7_on) || plat_store_load(OBJ_PATCH, &pstore, sizeof pstore) < 8 || !patches_valid()) {
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

int project_save(void)
{
    int r;
    keep();
    r = plat_store_save(OBJ_PROJ, &proj, sizeof proj);
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
