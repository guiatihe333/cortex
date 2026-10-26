/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal */
/* AMB-1 project: one flash object (OBJ_PROJ). A project of another format, or none, gives the
 * defaults. Part of the unity build, after app.h.
 *
 * The pieces (their recipes are the engine's, en_piece): each keeps its own settings as you leave it;
 * Tune and MIDI out are global. */
project_t proj;

const char *piece_name(int i) { return en_piece_name(i); }

static int global_par(int p) { return p == P_TUNE || p == P_MIDI; }

static void recipe(int i, slot_t *s) { en_piece(i, s->par, &s->pool); }

static void take(const slot_t *s)                /* a piece's settings into play, the globals kept */
{
    int p;
    for (p = 0; p < P_NPARAMS; p++)
        if (!global_par(p))
            proj.par[p] = s->par[p];
    proj.pool = s->pool;
}

static void keep(void)                           /* the piece playing, into its slot */
{
    int p;
    for (p = 0; p < P_NPARAMS; p++)
        proj.slot[proj.piece].par[p] = proj.par[p];
    proj.slot[proj.piece].pool = proj.pool;
}

void project_defaults(void)
{
    int i;
    memset(&proj, 0, sizeof proj);
    proj.magic = PROJ_MAGIC;
    proj.format = PROJ_FORMAT;
    for (i = 0; i < NPIECES; i++)
        recipe(i, &proj.slot[i]);
    for (i = 0; i < P_NPARAMS; i++)
        proj.par[i] = en_param_info(i)->def;
    take(&proj.slot[0]);
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

/* older formats: fewer settings a piece (format 3, 0.9 before the layer pages: 23; format 4, before MOD: 28; format
 * 5, one MOD for all three voices: 32). Carried over: each piece's own, the new ones from its recipe; format 5's MOD
 * as every voice's own (en_mod_legacy), the patches and FM edits as they were (the defaults) */
#define V3_NPARAMS (P_PADTONE + 1)
#define V4_NPARAMS (P_HATS + 1)
#define V5_NPARAMS (P_HATS + 5)
static void fix_none(int16_t *par, const int16_t *o) { (void)par, (void)o; }
static void fix_v5(int16_t *par, const int16_t *o)
{
    int p;
    for (p = P_BELLPATCH; p < P_NPARAMS; p++)
        par[p] = en_param_info(p)->def;
    en_mod_legacy(par, o[P_HATS + 1], o[P_HATS + 2], o[P_HATS + 3], o[P_HATS + 4]);
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

int project_load(void)
{
    static union {
        project_t now;
        project_v3_t v3;
        project_v4_t v4;
        project_v5_t v5;
    } buf;
    int n;
    memset(&buf, 0, sizeof buf);
    n = plat_store_load(OBJ_PROJ, &buf, sizeof buf);
    if (n >= 8 && buf.now.magic == PROJ_MAGIC && buf.now.format == 3u)
        from_project_v3(&buf.v3);
    else if (n >= 8 && buf.now.magic == PROJ_MAGIC && buf.now.format == 4u)
        from_project_v4(&buf.v4);
    else if (n >= 8 && buf.now.magic == PROJ_MAGIC && buf.now.format == 5u)
        from_project_v5(&buf.v5);
    else
        proj = buf.now;
    if (n >= 8 && project_valid())
        return 0;
    project_defaults();
    return -1;
}

int project_save(void)
{
    keep();
    return plat_store_save(OBJ_PROJ, &proj, sizeof proj);
}

void project_piece(int i)
{
    keep();
    proj.piece = (uint8_t)((i % NPIECES + NPIECES) % NPIECES);
    take(&proj.slot[proj.piece]);
}

void project_piece_reset(void)
{
    slot_t s;
    recipe(proj.piece, &s);
    take(&s);
}

void project_apply(void)
{
    int i;
    for (i = 0; i < P_NPARAMS; i++)
        en_set(i, proj.par[i]);
    en_pool(proj.pool);
}
