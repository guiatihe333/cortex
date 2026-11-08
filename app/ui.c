/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal */
/* AMB-1 UI: the panel and the screen. Part of the unity build (after gfx.c and project.c); the host
 * simulator includes it too.
 *
 *   keys           the scale: each key adds or removes its note from the pool (the root stays)
 *   PLAY           the garden grows / rests        ARP   the next chord, now
 *   REC            FREEZE: the chord holds, the reverb rings on    OCT- / OCT+   the bells' octave
 *   SELECT tempo   ALGORITHM mode (loads a pool)   PRESETS the piece (Root is on the Setup page)
 *   HOME SEQ FX SEL GLO   the pages: LAYERS, MOTION, SPACE, TONE, SETUP (EDIT steps through them)
 *   ENV ARP LFO again     that voice's own pages, three groups: VOICE (its patch, the LFO, the pitch envelope),
 *                  OPERATOR (each of its six operators, as a DX7 edits them) and MOD (its five modulators); the
 *                  button goes on to the next group, SELECT steps through the group's pages (and the voice's)
 *   KNOB 1-4       the page's four values (on the operator pages KNOB 1 picks the operator, on MOD the modulator)
 *   SAVE           saves (it also saves by itself a few seconds after a change, while resting);
 *                  held 1.5 s: the piece back to its recipe
 * MIDI in: channel 1 notes, the scale; clock; CC and NRPN set what the knobs set (midi_cc); MIDI out: what the knobs
 * set, as CC and NRPN (with MIDI out on) */
#include "fastmath.h"

/* CORTEX's letters (1.5.1): the panel's face melted, still read as it was: each column of a letter on a slow wave (a
 * pixel, two for the big faces), drops running down from some strokes' feet, and a ghost of the letter a pixel down
 * and right in its colour turned (red to green, green to blue, blue to red) and halved: the planes of a video synth
 * slipped apart. Fixed by where the letter is, so it does not crawl */
static const uint8_t MELT_W[16] = {0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0};
static inline void melt_px(int32_t px, int32_t py, uint32_t a, uint32_t r, uint32_t g, uint32_t b)
{
    uint16_t *q;
    if ((uint32_t)px >= cv_w || py < cv_y0 || py >= cv_y1)
        return;
    q = &cv_px[(uint32_t)py * cv_w + (uint32_t)px];
    if (a >= 15u) {
        *q = swap16((uint16_t)(r << 11 | g << 5 | b));
    } else {
        const uint32_t bg = swap16(*q), br = bg >> 11, bgg = (bg >> 5) & 63u, bb = bg & 31u;
        *q = swap16((uint16_t)((r * a + br * (15u - a)) / 15u << 11 | (g * a + bgg * (15u - a)) / 15u << 5 |
                               (b * a + bb * (15u - a)) / 15u));
    }
}
static int32_t cv_text_melt(int32_t x, int32_t y, const felucca_font_t *f, const char *s, uint16_t c)
{
    const uint32_t r = c >> 11, g = (c >> 5) & 63u, b = c & 31u;
    const uint32_t hr = b >> 1, hg = r, hb = g >> 2;   /* the ghost: turned, halved */
    const int32_t amp = f->h >= 24u ? 2 : 1, room = (int32_t)f->h + amp;
    for (; *s; s++) {
        const uint32_t gi = glyph(f, (uint8_t)*s), w = f->bw[gi], bpr = (w + 1u) / 2u;
        const uint8_t *gd = f->data + f->off[gi];
        uint32_t gx, gy;
        for (gx = 0; gx < w; gx++) {
            const int32_t px = x - f->pad + (int32_t)gx, dy = (int32_t)MELT_W[(uint32_t)px >> 1 & 15u] * amp;
            int32_t low = -1;
            uint32_t la = 0;
            for (gy = 0; gy < f->h; gy++) {
                uint32_t a = gd[gy * bpr + gx / 2u];
                const int32_t py = y + (int32_t)gy + dy + cv_oy;
                a = (gx & 1u) ? (a & 15u) : (a >> 4);
                if (!a)
                    continue;
                if (a >= 8u)
                    melt_px(px + 1, py + 1, a >> 1, hr, hg, hb);
                melt_px(px, py, a, r, g, b);
                if (a >= 10u)
                    low = (int32_t)gy, la = a;
            }
            if (low >= 0) {                      /* a drop from this foot, now and then */
                const uint32_t hs = ((uint32_t)px * 2654435761u ^ (uint32_t)(uint8_t)*s * 40503u) >> 24;
                if (hs % 6u == 0u) {
                    const int32_t len = (1 + (int32_t)(hs >> 3 & 1u) + (int32_t)(hs >> 5 & 1u)) * amp;
                    int32_t k;
                    for (k = 1; k <= len && low + k < room; k++)
                        melt_px(px, y + low + k + dy + cv_oy, la * (uint32_t)(len + 1 - k) / (uint32_t)(len + 1), r,
                                g, b);
                }
            }
        }
        x += f->adv[gi];
    }
    return x;
}
#define cv_text cv_text_melt
static void ui_blit(uint32_t x, uint32_t y);
#define cv_blit(x, y) ui_blit(x, y)

/* the pages, one a layer (a button each); SELECT steps through them in this order. Each voice (the pad, the bells,
 * the bass) has NSUB more, in three groups (SUB_*), with no button of their own: its page's button pressed again goes
 * to the next group (the page it was left on) and from the last back to the layer's page; SELECT steps through the
 * voice's own pages; the voice's button stays lit on them */
enum { V_GARDEN, V_PAD, V_BELLS, V_BASS, V_SPACE, V_SETUP, V_TAPE, V_GENE, V_DECK, V_TMOD1, V_TMOD2, V_TKEY, V_SONG,
       V_MIDI, V_VIS, V_VID, V_SHAPE, V_VMIX, V_LIFE, V_MOSH, V_CHROMA, V_REACT, V_SCREEN, V_OSC, V_OSC2, V_XFORM, V_MAIN };
#define VIS_OSC 8                               /* (VISUAL's Osc: drawn, not one MORPH turns into) */
enum { S_GEN, S_VOICE, S_PLAY, S_ADSR, S_LFO, S_LFO2, S_PEG, S_OP, S_OPENV, S_OP2, S_OP3, S_OP4, S_OP5, S_MOD, S_MOD2, S_MOD3, NSUB };
#define NVIEWS (V_MAIN + EN_NVOX * NSUB)
#define NGROUP 4
static const uint8_t GROUP0[NGROUP + 1] = {S_GEN, S_VOICE, S_OP, S_MOD, NSUB};   /* each group's first page */
/* (1.5.10) the layers' pages in the order SELECT, HOME and the page dots take them: the video synth from its source
 * out (what makes the picture, its shape, a second over it, its motion, its breaking, its colour, what moves it,
 * the screen); Osc's two only while Osc is in use (page_on) */
static const uint8_t PAGE_ORDER[] = {V_GARDEN, V_PAD, V_BELLS, V_BASS, V_SPACE, V_SETUP, V_MIDI, V_SONG, V_TAPE,
                                     V_GENE, V_DECK, V_TMOD1, V_TMOD2, V_TKEY, V_VIS, V_OSC, V_OSC2, V_SHAPE, V_VMIX,
                                     V_VID, V_XFORM, V_LIFE, V_MOSH, V_CHROMA, V_REACT, V_SCREEN};
_Static_assert(sizeof PAGE_ORDER == V_MAIN, "every layer page once");
static const char *const MAIN_NAME[V_MAIN] = {"Cortex", "Voice 1", "Voice 2", "Voice 3", "Space", "Setup", "Tape",
                                              "Gene", "Deck", "Tape Mod", "Tape Mod 2", "Tape Key", "Song", "MIDI", "Visual", "Video", "Shape", "Mix", "Life", "Mosh",
                                              "Chroma", "React", "Screen", "Osc", "Osc 2",
                                              "Transform"};
static const char *const SUB_NAME[NSUB] = {"Gen", "Patch", "Play", "ADSR", "LFO", "LFO 2", "Pitch EG", "OP", "OP Env", "OP 2", "OP 3",
                                           "OP 4", "OP 5", "Mod", "Mod 2", "Mod 3"};
static const uint8_t VIEW_BTN[V_MAIN] = {B_HOME, B_ENV, B_ARP, B_LFO, B_FX, B_GLO, B_SEQ, B_SEQ, B_SEQ, B_SEQ, B_SEQ,
                                         B_SEQ, B_GLO, B_GLO, B_HOME, B_HOME, B_HOME, B_HOME, B_HOME, B_HOME, B_HOME,
                                         B_HOME, B_HOME, B_HOME, B_HOME, B_HOME};
static int tape_view(int v) { return v >= V_TAPE && v <= V_TKEY; }   /* the reel's six pages (SEQ, again ..) */
static const uint8_t VOX_VIEW[EN_NVOX] = {V_PAD, V_BELLS, V_BASS};
#define KEY_NOTE0 53                            /* key 0: F3 (MIDI) */
static int view_vox(int v)                     /* the voice a page is of (its layer's or its own); -1 none */
{
    return v >= V_MAIN ? (v - V_MAIN) / NSUB : v >= V_PAD && v <= V_BASS ? v - V_PAD : -1;
}
static int view_sub(int v) { return v >= V_MAIN ? (v - V_MAIN) % NSUB : -1; }   /* its page of the voice's own */
static int view_main(int v) { return v >= V_MAIN ? VOX_VIEW[view_vox(v)] : v; }   /* its dot */
static int view_group(int v)                   /* the group a voice's page is in; -1 the layer's page */
{
    int s = view_sub(v), g;
    for (g = 0; g < NGROUP; g++)
        if (s >= GROUP0[g] && s < GROUP0[g + 1])
            return g;
    return -1;
}
static void view_name(int v, char *b)          /* "Garden", "Bells OP Env" */
{
    const char *s = MAIN_NAME[view_main(v)];
    int k = 0;
    while (*s)
        b[k++] = *s++;
    if (v >= V_MAIN) {
        b[k++] = ' ';
        for (s = SUB_NAME[view_sub(v)]; *s;)
            b[k++] = *s++;
    }
    b[k] = 0;
}

/* the four knobs of each page: a parameter (< P_NPARAMS), or one of these: the panel's own, the voice's patch source,
 * the operator, envelope stage and modulator picked, a byte of the voice's patch (KP_V + its offset) or of the
 * operator picked (KP_O + its field), the stage picked's rate or level, the modulator picked's setting (KM + MS_*) */
enum { K_LEDS = 1000, K_CPU, K_NONE, K_SRC, K_OP, K_STAGE, K_PSTAGE, K_SLOT, K_TSLOT, K_GKIND, K_GBRIGHT, K_GMOTION,
       K_GVARY, K_TKEY, K_TSHIFT, K_RSLOT, KP_OR, KP_OL, KP_PR, KP_PL,
       KP_V = 1100, KP_O = 1300, KM = 1400, KPL = 1500, KTM = 1600, KAR = 1700 };   /* KPL + PL_*: how the page's voice plays
                                                                          * (P_PL); KTM + TM_*: the reel's modulator
                                                                          * picked (P_TM) */
#define KPL_(k) (KPL + PL_##k)
#define KO(f) (KP_O + EP_##f)
#define KV(f) (KP_V + EP_##f)
#define KMS(k) (KM + MS_##k)
static const uint16_t MAIN_KNOB[V_MAIN][4] = {
    {P_TEMPO, P_EVOLVE, P_MOTION, P_DRIFT},
    {P_PAD, KPL_(ROLE), KPL_(OCT), KPL_(DENS)},   /* the three voices alike */
    {P_BELLS, KPL_(ROLE), KPL_(OCT), KPL_(DENS)},
    {P_BASS, KPL_(ROLE), KPL_(OCT), KPL_(DENS)},
    {P_ECHO, P_ETIME, P_REVERB, P_SIZE},
    {P_ROOT, P_TUNE, P_MIDI, K_LEDS},
    {P_RL(SRC), P_RL(MIX), P_RL(SOS), P_RL(SPEED)},
    {P_RL(GENE), P_RL(SLIDE), P_RL(MORPH), P_RL(ORG)},
    {P_RL(FX), P_RL(WOW), P_RL(DRIVE), P_RL(TONE)},
    {K_TSLOT, KTM + TM_TARGET, KTM + TM_DEPTH, KTM + TM_RATE},   /* the reel's five, as a voice's MOD */
    {K_TSLOT, KTM + TM_SHAPE, KTM + TM_SYNC, KTM + TM_SMOOTH},
    {P_ST(KSYNC), K_TKEY, K_TSHIFT, K_NONE},    /* Key sync, the splice picked's key, the reel's transposition */
    {P_SG(SONG), P_SG(FOLLOW), P_SG(VARY), P_SG(START)},   /* the song (a MIDI file's) */
    {P_ST(CH1), P_ST(CH2), P_ST(CH3), P_ST(NOTES)},   /* MIDI: the voices' channels, notes in */
    {P_ST(VMODE), P_ST(VPAL), P_ST(VTRAIL), P_ST(VREACT)},   /* the first page's visual */
    {P_ST(VFEED), P_ST(VRGB), P_ST(VHUE), P_ST(VSYNC)},   /* ... as a video synth */
    {P_ST(VGEO), P_ST(VSIDES), P_ST(VMIRROR), P_ST(VSPEED)},   /* ... its shape */
    {P_ST(VLAYER), P_ST(VBLEND), P_ST(VMIX), P_ST(VPULSE)},   /* ... a second picture over it, the beats */
    {P_ST(VMORPH), P_ST(VMUTATE), P_ST(VFLOW), P_ST(VCHAOS)},   /* ... its life */
    {P_ST(VMOSH), P_ST(VSORT), P_ST(VCORRUPT), P_ST(VSLICE)},   /* ... broken as data */
    {P_ST(VFOLD), P_ST(VPOSTER), P_ST(VPIXEL), P_ST(VKEY)},   /* ... its chroma */
    {K_RSLOT, KAR, KAR + 1, KAR + 2},            /* ... what it hears: a react slot's source, target, depth */
    {P_ST(VBARS), P_ST(VDELAY), P_ST(VBRIGHT), P_ST(VEAR)},   /* ... the screen */
    {P_ST(VOFREQ), P_ST(VOSHAPE), P_ST(VOANGLE), P_ST(VOSPEED)},   /* ... Osc: its oscillator */
    {P_ST(VOFREQ2), P_ST(VOCOMB), P_ST(VOWARP), P_ST(VOSYNC)},     /* ... a second, how they meet */
    {P_ST(VXTGT), P_ST(VXZOOM), P_ST(VXROT), P_ST(VXSHX)},       /* ... a transform: what of it, how */
};
static const uint16_t SUB_KNOB[NSUB][4] = {
    {K_GKIND, K_GBRIGHT, K_GMOTION, K_GVARY},  /* the generator (EDIT: a new voice, REC: varied) */
    {K_SRC, KV(ALG), KV(FB), K_NONE},          /* (INDEX: P_VX, put in below) */
    {KPL_(ENV), KPL_(GATE), KPL_(PACE), KPL_(GLIDE)},   /* (the voice's P_PL, put in below) */
    {KPL_(A), KPL_(D), KPL_(S), KPL_(R)},
    {KV(LFS), KV(LFD), KV(LPMD), KV(LAMD)},
    {KV(LFW), KV(LPMS), KV(LKS), KV(TRNSP)},
    {K_PSTAGE, KP_PR, KP_PL, KV(OKS)},
    {K_OP, KO(FC), KO(FF), KO(OL)},
    {K_OP, K_STAGE, KP_OR, KP_OL},
    {K_OP, KO(MODE), KO(DET), KO(KVS)},
    {K_OP, KO(BP), KO(LD), KO(RD)},
    {K_OP, KO(LC), KO(RC), KO(RS)},
    {K_OP, KO(AMS), K_NONE, K_NONE},
    {K_SLOT, KMS(DEST), KMS(DEPTH), KMS(RATE)},
    {K_SLOT, KMS(SHAPE), KMS(SYNC), KMS(SMOOTH)},
    {K_SLOT, KMS(SPREAD), KMS(FADE), K_NONE},
};
static const char *const MAIN_LABEL[V_MAIN][4] = {
    {"Tempo", "Evolve", "Motion", "Drift"},
    {"Level", "Role", "Octave", "Density"},
    {"Level", "Role", "Octave", "Density"},
    {"Level", "Role", "Octave", "Density"},
    {"Echo", "Echo time", "Reverb", "Size"},
    {"Root", "Tune", "MIDI out", "Lights"},
    {"Source", "Dry/Wet", "SOS", "Speed"},
    {"Gene", "Slide", "Morph", "Organize"},
    {"FX", "Wow", "Drive", "Tone"},
    {"Mod", "Target", "Depth", "Rate"},
    {"Mod", "Shape", "Sync", "Smooth"},
    {"Key sync", "Key", "Shift", ""},
    {"Song", "Follow", "Vary", "Start"},
    {"V1 chan", "V2 chan", "V3 chan", "Notes in"},
    {"Visual", "Colour", "Trails", "React"},
    {"Feedback", "RGB", "Hue", "Sync"},
    {"Geometry", "Sides", "Mirror", "Speed"},
    {"Layer", "Blend", "Mix", "Pulse"},
    {"Morph", "Mutate", "Flow", "Chaos"},
    {"Mosh", "Sort", "Corrupt", "Slice"},
    {"Colorama", "Poster", "Pixel", "Key"},
    {"Slot", "Source", "Target", "Depth"},
    {"Bars", "Delay", "Bright", "Ear"},
    {"Freq", "Wave", "Angle", "Scroll"},
    {"Freq 2", "Combine", "Warp", "Sync"},
    {"Target", "Zoom", "Rotate", "Shift"},
};
static const char *const SUB_LABEL[NSUB][4] = {
    {"Kind", "Bright", "Motion", "Vary"},
    {"Patch", "Algorithm", "Feedback", "Index"},
    {"Envelope", "Gate", "Pace", "Glide"},
    {"Attack", "Decay", "Sustain", "Release"},
    {"Speed", "Delay", "Pitch mod", "Amp mod"},
    {"Wave", "Pitch sens", "Key sync", "Transpose"},
    {"Stage", "Rate", "Level", "Osc sync"},
    {"Operator", "Ratio", "Fine", "Level"},
    {"Operator", "Stage", "Rate", "Level"},
    {"Operator", "Mode", "Detune", "Velocity"},
    {"Operator", "Break", "L depth", "R depth"},
    {"Operator", "L curve", "R curve", "Rate scale"},
    {"Operator", "Amp sens", "", ""},
    {"Mod", "Target", "Depth", "Rate"},
    {"Mod", "Shape", "Sync", "Smooth"},
    {"Mod", "Spread", "Fade", ""},
};
static int view_knob(int v, int i)             /* page v's knob i */
{
    int k = v < V_MAIN ? MAIN_KNOB[v][i] : SUB_KNOB[view_sub(v)][i];
    if (k >= KPL && k < KPL + PL_N)
        return P_PL(view_vox(v), k - KPL);
    if (v < V_MAIN)
        return k;
    if (view_sub(v) == S_VOICE && i == 3)
        return P_VX(view_vox(v), VX_INDEX);
    if (k == K_SRC)
        return EN_VOX_PATCH[view_vox(v)];       /* (the source is a parameter, turned its own way: knob_turn) */
    return k;
}
static const char *knob_label(int v, int i) { return v < V_MAIN ? MAIN_LABEL[v][i] : SUB_LABEL[view_sub(v)][i]; }

/* the screen's colours: a palette a page, after the records of Aphex Twin (their colours only: no names, no marks).
 * Each: the ground, a panel, rules, dim and plain ink, the accent (the page, the knob being turned, a message's
 * word); then the layers (the chord, the loops, the bass, the reel), each with a pale tint for shapes at rest, and
 * the frozen chord's. */
typedef struct {
    uint16_t bg, panel, line, dim, text, acc, pad, pad_t, bell, bell_t, bass, bass_t, pulse, pulse_t, frost, frost_t;
} theme_t;
static const theme_t THEMES[] = {
    /* the garden: Selected Ambient Works 85-92, black and white; a few details (the accent, the chord) in Volume
     * II's gold */
    {RGB(0, 0, 0), RGB(28, 28, 28), RGB(64, 64, 64), RGB(140, 140, 140), RGB(244, 244, 240), RGB(218, 188, 128),
     RGB(218, 188, 128), RGB(70, 60, 40), RGB(180, 180, 176), RGB(50, 50, 50), RGB(132, 132, 128), RGB(44, 44, 44),
     RGB(100, 100, 98), RGB(40, 40, 40), RGB(210, 210, 210), RGB(80, 80, 80)},
    /* the reel: the back of Selected Ambient Works Volume II, its paper and its circles */
    {RGB(232, 228, 214), RGB(246, 244, 236), RGB(196, 190, 172), RGB(120, 112, 96), RGB(30, 28, 24), RGB(184, 100, 44),
     RGB(150, 172, 50), RGB(214, 220, 170), RGB(98, 104, 44), RGB(200, 200, 168), RGB(184, 100, 44), RGB(226, 196, 166),
     RGB(150, 36, 36), RGB(222, 180, 172), RGB(92, 64, 40), RGB(200, 184, 164)},
    /* voice 1: its front, rust and gold */
    {RGB(24, 16, 8), RGB(56, 38, 20), RGB(100, 72, 40), RGB(176, 146, 100), RGB(240, 226, 196), RGB(222, 184, 112),
     RGB(222, 184, 112), RGB(86, 64, 32), RGB(244, 214, 156), RGB(92, 72, 44), RGB(176, 110, 52), RGB(72, 46, 22),
     RGB(150, 110, 70), RGB(60, 40, 20), RGB(250, 214, 150), RGB(110, 80, 44)},
    /* voice 2: drukqs, the prepared piano's warm light on black */
    {RGB(8, 6, 4), RGB(40, 24, 12), RGB(84, 46, 18), RGB(176, 120, 70), RGB(252, 232, 204), RGB(255, 136, 32),
     RGB(255, 136, 32), RGB(90, 42, 12), RGB(255, 196, 110), RGB(96, 64, 30), RGB(200, 72, 20), RGB(76, 28, 8),
     RGB(150, 100, 60), RGB(60, 40, 22), RGB(255, 220, 160), RGB(110, 80, 50)},
    /* voice 3: Syro, green ink on a dark green */
    {RGB(6, 22, 14), RGB(18, 44, 28), RGB(36, 80, 52), RGB(110, 170, 126), RGB(214, 244, 214), RGB(130, 255, 140),
     RGB(130, 255, 140), RGB(30, 80, 44), RGB(190, 240, 160), RGB(40, 76, 44), RGB(70, 190, 110), RGB(24, 64, 36),
     RGB(150, 200, 160), RGB(40, 70, 50), RGB(220, 255, 220), RGB(60, 100, 70)},
    /* the space: Richard D. James Album, its sky */
    {RGB(4, 14, 34), RGB(14, 34, 70), RGB(34, 64, 112), RGB(124, 160, 210), RGB(226, 238, 255), RGB(96, 176, 255),
     RGB(96, 176, 255), RGB(24, 56, 104), RGB(200, 226, 255), RGB(40, 66, 106), RGB(60, 120, 200), RGB(20, 44, 84),
     RGB(150, 170, 200), RGB(40, 56, 84), RGB(220, 236, 255), RGB(70, 96, 140)},
    /* setup: a deep violet, the video synth's night, and the warning yellow */
    {RGB(30, 10, 44), RGB(58, 24, 80), RGB(96, 48, 128), RGB(176, 150, 200), RGB(246, 238, 252), RGB(255, 214, 0),
     RGB(255, 214, 0), RGB(84, 64, 24), RGB(226, 206, 246), RGB(74, 40, 96), RGB(200, 120, 255), RGB(64, 30, 86),
     RGB(160, 130, 190), RGB(56, 26, 74), RGB(250, 244, 255), RGB(110, 70, 140)},
};
static const theme_t *TH = &THEMES[0];
static theme_t th_live;                         /* the page's colours, with the live ones in its details */
static uint16_t ui_lc[3];                       /* the live colours: the chord's tones, round the wheel */
static uint32_t ui_lsig;                        /* (they, coarsely: the bands drawn again when they move) */
static uint8_t ui_wash_on;                      /* the bands' ground washed with them (not the visual's) */
static int theme_of(int v)                     /* a page's palette: the voices', their own pages too */
{
    static const uint8_t T[V_MAIN] = {0, 2, 3, 4, 5, 6, 1, 1, 1, 1, 1, 1, 0, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    return T[view_main(v)];
}
#define K_BG (TH->bg)
#define K_PANEL (TH->panel)
#define K_LINE (TH->line)
#define K_DIM (TH->dim)
#define K_TEXT (TH->text)
#define K_ACC (TH->acc)
#define K_BELL (TH->bell)
#define K_BELL_T (TH->bell_t)
#define K_PAD (TH->pad)
#define K_PAD_T (TH->pad_t)
#define K_PULSE (TH->pulse)
#define K_PULSE_T (TH->pulse_t)
#define K_BASS (TH->bass)
#define K_BASS_T (TH->bass_t)
#define K_FROST (TH->frost)
#define K_FROST_T (TH->frost_t)

static struct {
    uint8_t view;
    uint32_t btn, keys, btn_used;
    uint32_t enc_t[NE];
    uint8_t enc_run[NE];               /* detents in the current quick run, per knob (accel) */
    uint32_t down_t[NB];               /* when each button went down */
    uint32_t turn_t;                   /* the last knob turn */
    uint8_t help;                      /* a button's card is up: the button + 1 */
    uint8_t reset_done;                /* SAVE held: the piece went back to its recipe this hold */
    uint8_t erase_done;                /* REC held on the reel's pages: the reel erased this hold */
    uint8_t dirty;                     /* the project changed since the last save */
    uint32_t act_t, saved_t;
    char msg[2][24];
    uint32_t msg_until;
    int8_t touched;                    /* the knob turned last (0..3), shown bright for a moment */
    uint32_t touch_until;
    uint32_t frame;
    uint32_t sig[3];                   /* what each band shows: redrawn when it changes */
    uint8_t tslot;                     /* the reel's modulator picked (0..) */
    uint8_t rslot;                     /* the visual's react slot picked (0..3) */
    int8_t key_vox[32];                /* each key held on a voice's page: the voice it plays (-1 none) */
    uint8_t in_ch, in_port;            /* MIDI in: the last note's channel (1..16, 0 none yet) and port (PORT_*) */
    uint8_t op[EN_NVOX], stage[EN_NVOX], pstage[EN_NVOX], slot[EN_NVOX];   /* each voice's operator (1..6),
                                        * envelope stage, pitch envelope stage and modulator (0..) picked */
    uint8_t gsub[EN_NVOX][NGROUP];     /* each voice's group: the page it was left on */
    uint8_t gen[EN_NVOX][4];           /* each voice's generator: its kind, bright, motion, vary */
    uint32_t gen_seed[EN_NVOX];        /* the voice it made last (its seed) */
    uint8_t gen_dirty;                 /* a knob of it turned: made again this frame (2: a new kind, fresh) */
    uint8_t cc_in;                     /* a knob's value came in as MIDI: not sent out again */
    uint8_t nrpn[16][2];               /* each channel's NRPN, as CC 99 / 98 set it */
} ui;

static uint8_t vis_over;                        /* the header and the knobs drawn over the visual (no ground, no blit) */
static uint8_t vhold;                           /* the visual's genome held (REC on its pages) */
static void vis_mutate(float amount);
/* the screen reader (announce.c; speak() below): a message went to it this frame, so speak() leaves what it
 * changed be; and the transport as last said (PLAY says its own: the engine takes it a block later) */
static uint8_t sp_said;
static struct {
    uint8_t playing;
    uint32_t play_t;                   /* when PLAY (or the start) set it: the engine catches up within ms */
} sp_tp;
static void sp_piece(void);
static void msg_text(const char *a, const char *b, char *t)   /* the message as a reader gets it: "a b" */
{
    uint32_t i, k = 0;
    for (i = 0; i < 23u && a && a[i]; i++)
        t[k++] = a[i];
    if (b && b[0])
        t[k++] = ' ';
    for (i = 0; i < 23u && b && b[i]; i++)
        t[k++] = b[i];
    while (k && t[k - 1] == '.')               /* "UPDATE MODE IN 3..": the dots are for the eye */
        k--;
    t[k] = 0;
}
/* a message for a moment in the header; flags: how a screen reader gets it (0: the caller tells it) */
static void say_as(const char *a, const char *b, uint32_t flags)
{
    uint32_t i;
    char t[48];
    for (i = 0; i < 23u && a && a[i]; i++)
        ui.msg[0][i] = a[i];
    ui.msg[0][i] = 0;
    for (i = 0; i < 23u && b && b[i]; i++)
        ui.msg[1][i] = b[i];
    ui.msg[1][i] = 0;
    ui.msg_until = plat_ms() + 1300u;
    if (flags && ann_on()) {
        msg_text(a, b, t);
        ann_say(t, 0, flags);
        sp_said = 1;
    }
}
static void say(const char *a, const char *b) { say_as(a, b, ANN_NOW); }
/* something did not happen: a screen reader says it whole (what follows does not cut it off) */
static void warn(const char *a, const char *b) { say_as(a, b, ANN_NOW | ANN_KEEP); }
void ui_say(const char *a, const char *b) { say(a, b); }

static void itoa_u(uint32_t v, char *b)
{
    char t[12];
    int n = 0;
    do {
        t[n++] = (char)('0' + v % 10u);
        v /= 10u;
    } while (v);
    while (n)
        *b++ = t[--n];
    *b = 0;
}

static void mark_dirty(void) { ui.dirty = 1; }
int ui_dirty(void) { return ui.dirty != 0; }

static int pool_count(uint32_t p)
{
    int n = 0, i;
    for (i = 0; i < 12; i++)
        n += (int)(p >> i & 1u);
    return n;
}
static uint32_t pool_abs(void)                 /* the pool as absolute pitch classes */
{
    uint32_t p = proj.pool, r = (uint32_t)proj.par[P_ROOT];
    return ((p << r) | (p >> (12u - r))) & 0xFFFu;
}
static const char *mode_name(void)
{
    return proj.pool == EN_MODE_POOL[proj.par[P_MODE]] ? EN_MODE_NAME[proj.par[P_MODE]] : "Custom";
}

/* ------------------------------------------------------------ values --- */
static int kvox(void)                          /* the voice the page shown is of (the pad on the others) */
{
    int v = view_vox(ui.view);
    return v < 0 ? VOX_PAD : v;
}
int ui_voice(void) { return kvox(); }
static int is_src(int k) { return k == P_PADTONE || k == P_BELLPATCH || k == P_BASSPATCH; }
static int src_vox(int k) { return k == P_PADTONE ? VOX_PAD : k == P_BELLPATCH ? VOX_BELLS : VOX_BASS; }
static int knob_off(int k)                     /* a patch knob's byte (of the voice shown); -1 not one */
{
    const int v = kvox(), o = EP_OPN(ui.op[v] ? ui.op[v] : 1);
    if (k >= KP_V && k < KP_V + EN_PATCH)
        return k - KP_V;
    if (k >= KP_O && k < KP_O + EP_OP)
        return o + k - KP_O;
    if (k == KP_OR || k == KP_OL)
        return o + (k == KP_OR ? EP_R1 : EP_L1) + ui.stage[v];
    if (k == KP_PR || k == KP_PL)
        return (k == KP_PR ? EP_PR1 : EP_PL1) + ui.pstage[v];
    return -1;
}
static int knob_par(int k)                     /* a knob's parameter (a modulator's: the one picked); -1 none */
{
    if (k < P_NPARAMS)
        return k;
    if (k >= KM && k < KM + MS_N)
        return P_MS(kvox(), ui.slot[kvox()], k - KM);
    if (k >= KTM && k < KTM + TM_N)
        return P_TMOD + ui.tslot * TM_N + (k - KTM);
    if (k >= KAR && k < KAR + 3)
        return P_ST(AR1S) + ui.rslot * 3 + (k - KAR);
    return -1;
}
static int tkey_spl(void)                       /* the splice Tape Key shows: the one ORGANIZE picks */
{
    const int n = en_reel_nspl, k = proj.par[P_RL(ORG)];
    return n ? (k < n ? k : n - 1) : 0;
}
static int knob_get(int k)
{
    const int v = kvox(), p = knob_par(k), off = knob_off(k);
    if (p >= 0)
        return proj.par[p];
    if (off >= 0)
        return vpatch[v][off];
    if (k == K_LEDS)
        return proj.leds;
    if (k == K_CPU)
        return (int)plat_cpu_pct();
    if (k == K_OP)
        return ui.op[v] ? ui.op[v] : 1;
    if (k == K_STAGE || k == K_PSTAGE || k == K_SLOT)
        return 1 + (k == K_STAGE ? ui.stage[v] : k == K_PSTAGE ? ui.pstage[v] : ui.slot[v]);
    if (k == K_TSLOT)
        return 1 + ui.tslot;
    if (k == K_RSLOT)
        return 1 + ui.rslot;
    if (k >= K_GKIND && k <= K_GVARY)
        return ui.gen[v][k - K_GKIND];
    if (k == K_TKEY)
        return en_reel_keyman(tkey_spl());
    return 0;
}
static void knob_range(int k, int *lo, int *hi)
{
    const int p = knob_par(k), off = knob_off(k);
    *lo = 0;
    if (p >= 0) {
        *lo = en_param_info(p)->lo;
        *hi = en_param_info(p)->hi;
    } else if (off >= 0) {
        *hi = en_patch_max(off);
    } else if (k == K_OP || k == K_STAGE || k == K_PSTAGE || k == K_SLOT || k == K_TSLOT || k == K_RSLOT) {
        *lo = 1;
        *hi = k == K_OP ? 6 : k == K_SLOT ? EN_NSLOT : k == K_TSLOT ? EN_NTMOD : 4;
    } else if (k >= K_GKIND && k <= K_GVARY) {
        *hi = k == K_GKIND ? EN_NGEN - 1 : 100;
    } else if (k == K_TKEY || k == K_TSHIFT) {
        *hi = k == K_TKEY ? 24 : 0;
    } else {
        *hi = k == K_LEDS ? 3 : k == K_CPU ? 100 : 0;
    }
}

static char *cat_s(char *b, const char *s)
{
    while (*s)
        *b++ = *s++;
    *b = 0;
    return b;
}
static char *cat_i(char *b, int v, int sign)    /* a number (sign: "+" before a positive one) */
{
    if (v < 0) {
        *b++ = '-';
        v = -v;
    } else if (sign && v > 0) {
        *b++ = '+';
    }
    itoa_u((uint32_t)v, b);
    while (*b)
        b++;
    return b;
}
/* a patch byte's value as the screen shows it: the operator's frequency (x1.50, or in fixed mode 440Hz), detune
 * -7..+7, the curves, the break point as a note, the algorithm 1..32, the LFO's waves, transpose and the pitch
 * envelope's levels from their middle */
static void patch_text(int off, int val, char *b)
{
    static const char *const CURVE[4] = {"-Lin", "-Exp", "+Exp", "+Lin"};
    static const char *const WAVE[6] = {"Triangle", "Saw down", "Saw up", "Square", "Sine", "S&H"};
    static const char *const ONOFF[2] = {"Off", "On"};
    const uint8_t *pv = vpatch[kvox()];
    if (off < 6 * EP_OP) {
        const int f = off % EP_OP;
        const uint8_t *o = pv + off - f;
        if (f == EP_FC || f == EP_FF) {          /* the frequency, its coarse and fine together */
            int fc = f == EP_FC ? val : o[EP_FC], ff = f == EP_FF ? val : o[EP_FF];
            if (!o[EP_MODE]) {
                int r = (fc ? fc * 100 : 50) * (100 + ff) / 100;   /* the ratio, in hundredths */
                b = cat_s(b, "x");
                b = cat_i(b, r / 100, 0);
                *b++ = '.';
                *b++ = (char)('0' + r % 100 / 10);
                *b++ = (char)('0' + r % 10);
                *b = 0;
            } else {                             /* fixed: 10^(coarse mod 4 + fine / 100) Hz */
                int hz = (int)(fm_exp2f(3.3219281f * ((float)(fc & 3) + (float)ff * 0.01f)) + 0.5f);
                b = cat_i(b, hz, 0);
                cat_s(b, "Hz");
            }
            return;
        }
        if (f == EP_MODE) {
            cat_s(b, val ? "Fixed" : "Ratio");
            return;
        }
        if (f == EP_DET) {
            cat_i(b, val - 7, 1);
            return;
        }
        if (f == EP_LC || f == EP_RC) {
            cat_s(b, CURVE[val & 3]);
            return;
        }
        if (f == EP_BP) {                        /* 0 is A-1, 39 C3 */
            int n = val + 9;
            b = cat_s(b, EN_NOTE_NAME[n % 12]);
            cat_i(b, n / 12 - 1, 0);
            return;
        }
    } else if (off == EP_ALG) {
        cat_i(b, val + 1, 0);
        return;
    } else if (off == EP_LFW) {
        cat_s(b, WAVE[val < 6 ? val : 0]);
        return;
    } else if (off == EP_LKS || off == EP_OKS) {
        cat_s(b, ONOFF[val ? 1 : 0]);
        return;
    } else if (off == EP_TRNSP) {
        cat_i(b, val - 24, 1);
        return;
    } else if (off >= EP_PL1 && off < EP_PL1 + 4) {
        cat_i(b, val - 50, 1);
        return;
    }
    cat_i(b, val, 0);
}
static void src_text(int k, int v, char *b)     /* a source: its name ("Rhodes", a bank voice's), "*" edited. The
                                                 * voice's own, of a bank: its name as the piece keeps it (the bank
                                                 * since sent again may hold another there) */
{
    int n = 0;
    b[0] = 0;
    if (v >= EN_NFACT && v == proj.par[k]) {
        const uint8_t *nm = vpatch[src_vox(k)] + EP_NAME;
        int i;
        for (i = 0; i < 10; i++)
            if ((b[i] = (char)(nm[i] < 32u || nm[i] > 126u ? ' ' : nm[i])) != ' ')
                n = i + 1;
        b[n] = 0;
        n = 0;
    }
    if (!b[0] && (v < EN_NFACT || !bank_name(v, b)))
        en_param_text(k, v, b);
    while (b[n])
        n++;
    if (proj.par[P_VX(src_vox(k), VX_OWN)] && v == proj.par[k]) {
        if (n > 8)
            n = 8;
        b[n++] = '*';
        b[n] = 0;
    }
}
static void song_name(int slot, char *b)        /* "Off", a song's name, or "Empty" */
{
    const uint8_t *r = slot > 0 ? song_rec(slot - 1) : 0;
    int k, n = 0;
    if (slot <= 0) {
        cat_s(b, "Off");
        return;
    }
    if (!r) {
        cat_s(b, "Empty");
        return;
    }
    for (k = 0; k < 12; k++)
        if ((b[k] = (char)r[12 + k]) != ' ')
            n = k + 1;
    b[n] = 0;
}
static void song_attach(int user)               /* the piece's SONG to the engine; chosen by hand: its key and tempo */
{
    const int slot = proj.par[P_SG(SONG)];
    const uint8_t *r = slot > 0 ? song_rec(slot - 1) : 0;
    en_song(r);
    if (r && user) {
        int t = r[7] << 7 | r[8], t2;
        char nm[16];
        while (t > 120)
            t /= 2;                              /* (a fast song: half, as the garden's tempo goes to 120) */
        t = t < 30 ? 30 : t;
        proj.par[P_ROOT] = r[9] % 12;
        en_set(P_ROOT, proj.par[P_ROOT]);
        proj.pool = (uint16_t)(((r[10] | r[11] << 6) | 1) & 0xFFF);
        for (t2 = 0; t2 < EN_NMODES; t2++)       /* (a mode's scale: its mode, by name) */
            if (EN_MODE_POOL[t2] == proj.pool) {
                proj.par[P_MODE] = (int16_t)t2;
                en_set(P_MODE, t2);
            }
        en_pool(proj.pool);
        proj.par[P_TEMPO] = (int16_t)t;
        en_set(P_TEMPO, t);
        proj.par[P_SG(FOLLOW)] = 100;            /* (every note of it: lower, the garden mixes in) */
        en_set(P_SG(FOLLOW), 100);
        song_name(slot, nm);
        say("SONG", nm);
    } else if (user && slot > 0) {
        warn("SONG", "EMPTY (AMB_MID.PY)");
    }
}
void ui_song_writing(int slot)                  /* a song's flash about to be rewritten: let go if it plays */
{
    if (proj.par[P_SG(SONG)] == slot + 1)
        en_song(0);
}
void ui_reel_in(int ok)                         /* a file's reel came in: SOURCE File, saved (a sector a frame) */
{
    (void)ok;
    proj.par[P_RL(SRC)] = 4;
    en_set(P_RL(SRC), 4);
    proj.par[P_RL(ORG)] = 0;
    en_set(P_RL(ORG), 0);
    mark_dirty();
    if (project_reel_save() == 0)
        say("TAPE", "FROM A FILE");
    else
        warn("TAPE IN", "NOT SAVED YET");
}
void ui_song_in(int slot)                       /* a song came in */
{
    char nm[16];
    song_name(slot + 1, nm);
    if (proj.par[P_SG(SONG)] == slot + 1)
        song_attach(0);
    say("SONG IN", nm);
}

static void knob_text(int k, char *b)
{
    const int v = knob_get(k), p = knob_par(k), off = knob_off(k);
    if (p == P_SG(SONG)) {
        song_name(v, b);
    } else if (p >= 0 && is_src(p)) {
        src_text(p, v, b);
    } else if (p >= 0) {
        en_param_text(p, v, b);
    } else if (off >= 0) {
        patch_text(off, v, b);
    } else if (k == K_LEDS) {                    /* On: the keys, and the unlit buttons glow; Keys: no glow */
        cat_s(b, v == 3 ? "Visual" : v == 2 ? "Keys" : v ? "On" : "Off");
    } else if (k == K_CPU) {
        b = cat_i(b, v, 0);
        cat_s(b, "%");
    } else if (k == K_OP) {
        b = cat_s(b, "OP");
        cat_i(b, v, 0);
    } else if (k == K_STAGE || k == K_PSTAGE || k == K_SLOT || k == K_TSLOT || k == K_RSLOT) {
        cat_i(b, v, 0);
    } else if (k == K_GKIND) {
        cat_s(b, EN_GEN_NAME[(unsigned)v < EN_NGEN ? v : 0]);
    } else if (k == K_TKEY) {                    /* "A min" studied, "A min*" set by hand, "..." being studied */
        float kv[12];
        int mi, t;
        if (!en_reel_nspl) {
            cat_s(b, "-");
        } else if (v) {
            b = cat_s(b, EN_NOTE_NAME[(v - 1) % 12]);
            cat_s(b, v > 12 ? " min*" : " maj*");
        } else if (en_reel_keyvec(tkey_spl(), kv)) {
            t = en_key_guess(kv, &mi);
            b = cat_s(b, EN_NOTE_NAME[t]);
            cat_s(b, mi ? " min" : " maj");
        } else {
            cat_s(b, "...");
        }
    } else if (k == K_TSHIFT) {                  /* the reel's transposition, semitones ("+3.0", "-4.9"), as it plays */
        const float r = en_reel_rate, a = fm_fabsf(r);
        if (!en_reel_nspl || a < 0.01f) {
            cat_s(b, a < 0.01f && en_reel_nspl ? "Stop" : "-");
        } else {
            const int t = (int)fm_floorf(12.0f * fm_log2f(a) * 10.0f + 0.5f);
            *b++ = t < 0 ? '-' : '+';
            b = cat_i(b, (t < 0 ? -t : t) / 10, 0);
            *b++ = '.';
            b = cat_i(b, (t < 0 ? -t : t) % 10, 0);
        }
    } else if (k >= K_GBRIGHT && k <= K_GVARY) {
        cat_i(b, v, 0);
    } else {
        b[0] = 0;
    }
}

/* MIDI CC (in and out): a voice's own on its channel (the pad 1, the bells 2, the bass 3, as its notes go out), the
 * rest on channel 16. On a voice's channel: CC 20 its INDEX, CC 21..60 its five modulators (MS_* each), and its patch
 * as NRPN (CC 99 0 or 1, CC 98 the byte's offset 0..127 (+128), CC 6 the value). On channel 16: CC 20..49 parameters
 * 0..29 (the layers, the space, the tone, setup; a patch source as its number). A value is scaled to its range,
 * 0..127 */
#define CC0 20
#define CC_PL 102                              /* a voice's channel: 102.. how it plays (amb.h PL_*: ENV, GATE,
                                                * ROLE, OCTAVE, DENSITY, PACE, GLIDE, A, D, S, R; past 99/98, 6, 64) */
static void cc_out(int ch, int cc, int v) { en_midi_out(0xB0u | (uint32_t)ch, (uint32_t)cc, (uint32_t)v); }
static int cc_scale(int v, int lo, int hi) { return hi > lo ? ((v - lo) * 127 + (hi - lo) / 2) / (hi - lo) : 0; }
static void par_out(int p)                      /* a parameter the knobs set, out as its CC */
{
    const en_param_t *pi = en_param_info(p);
    int v = proj.par[p];
    if (ui.cc_in || !proj.par[P_MIDI])
        return;
    if (p < P_VOX) {
        if (p != P_MIDI && p - P_PAD < 30)
            cc_out(15, CC0 + p, is_src(p) ? (v > 127 ? 127 : v) : cc_scale(v, pi->lo, pi->hi));
    } else if (p >= P_TAPE) {
        if (50 + p - P_TAPE < 128)               /* (the visual's last settings have no CC) */
            cc_out(15, 50 + p - P_TAPE, cc_scale(v, pi->lo, pi->hi));
    } else if (p >= P_PLAY) {
        cc_out((p - P_PLAY) / PL_N, CC_PL + (p - P_PLAY) % PL_N, cc_scale(v, pi->lo, pi->hi));
    } else if ((p - P_VOX) % VX_N != VX_OWN) {
        cc_out((p - P_VOX) / VX_N, CC0 + (p - P_VOX) % VX_N - VX_INDEX, cc_scale(v, pi->lo, pi->hi));
    }
}
static void patch_out(int voice, int off, int v)   /* a patch byte, out as NRPN */
{
    if (ui.cc_in || !proj.par[P_MIDI])
        return;
    cc_out(voice, 99, off >> 7);
    cc_out(voice, 98, off & 127);
    cc_out(voice, 6, v);
}

static void knob_set(int k, int v)
{
    int lo, hi;
    const int p = knob_par(k), off = knob_off(k);
    if (k == K_CPU || k == K_NONE || k == K_TSHIFT)
        return;
    knob_range(k, &lo, &hi);
    v = v < lo ? lo : v > hi ? hi : v;
    if (v == knob_get(k))
        return;
    if (p >= 0 && is_src(p)) {                   /* a source: its patch, as loaded */
        project_source(src_vox(p), v);
        par_out(p);
    } else if (p >= 0) {
        proj.par[p] = (int16_t)v;
        en_set(p, v);
        par_out(p);
        if (p == P_SG(SONG) || p == P_SG(START))
            song_attach(p == P_SG(SONG));        /* (a song chosen: its key and tempo; a start: from there) */
    } else if (off >= 0) {
        project_patch_set(kvox(), off, v);
        patch_out(kvox(), off, v);
    } else if (k == K_LEDS) {
        proj.leds = (uint8_t)v;
    } else if (k == K_OP || k == K_STAGE || k == K_PSTAGE || k == K_SLOT) {   /* the page's pick: not the sound */
        uint8_t *s = k == K_OP ? ui.op : k == K_STAGE ? ui.stage : k == K_PSTAGE ? ui.pstage : ui.slot;
        s[kvox()] = (uint8_t)(k == K_OP ? v : v - 1);
        return;
    } else if (k == K_TSLOT) {
        ui.tslot = (uint8_t)(v - 1);
        return;
    } else if (k == K_RSLOT) {
        ui.rslot = (uint8_t)(v - 1);
        return;
    } else if (k == K_TKEY) {                    /* the splice's key by hand (with the reel: SAVE keeps it) */
        en_reel_keyset(tkey_spl(), v);
        return;
    } else if (k >= K_GKIND && k <= K_GVARY) {   /* the generator: its voice made again (at the frame's end) */
        ui.gen[kvox()][k - K_GKIND] = (uint8_t)v;
        if (k != K_GVARY && ui.gen_dirty < 2u)
            ui.gen_dirty = k == K_GKIND ? 2u : 1u;
        return;
    }
    mark_dirty();
}

/* knob acceleration by speed (X0X's): a few clicks are one step each however quick, so small
 * corrections stay exact; a longer quick turn speeds up to 8 a detent, so half a turn sweeps 0-100.
 * Small ranges never accelerate. */
static uint8_t accel_off;              /* the host simulator's "spin": exact steps */
static int32_t accel(int role, int32_t s, int range)
{
    uint32_t now = plat_ms(), dt = now - ui.enc_t[role], a = (uint32_t)(s < 0 ? -s : s), m;
    ui.enc_t[role] = now;
    if (!a)
        return s;
    if (a > 1)
        dt /= a;                                      /* per detent */
    ui.enc_run[role] = (uint8_t)(dt < 60u ? (ui.enc_run[role] + a > 255u ? 255u : ui.enc_run[role] + a) : a);
    if (range <= 24 || accel_off || ui.enc_run[role] < 4u)
        return s;
    m = dt < 12u ? 8u : dt < 25u ? 5u : dt < 45u ? 3u : 1u;
    if (range < 100 && m > 3u)
        m = 3u;
    return s * (int32_t)m;
}

static void turn(int role, int k, int32_t e)
{
    int lo, hi;
    if (!e || k == K_NONE || k == K_CPU || k == K_TSHIFT)
        return;
    if (knob_par(k) >= 0 && is_src(knob_par(k))) {   /* a source: a detent a voice, the empty slots passed by */
        int s = knob_get(k), n = e < 0 ? -e : e;
        while (n--)
            s = bank_next(s, e > 0 ? 1 : -1);
        knob_set(k, s);
        return;
    }
    knob_range(k, &lo, &hi);
    knob_set(k, knob_get(k) + accel(role, e, hi - lo));
}

/* ------------------------------------------------------------- the pool --- */
static void vis_key(int k);
static void toggle_note(int pc)                 /* a key: its note in or out of the scale */
{
    int rel = (pc - proj.par[P_ROOT] + 12) % 12;
    uint32_t p = proj.pool ^ (1u << rel);
    if (rel == 0) {
        warn("THE ROOT STAYS", "(SETUP)");
        return;
    }
    if (pool_count(p) < 3) {
        warn("3 NOTES", "AT LEAST");
        return;
    }
    proj.pool = (uint16_t)p;
    en_pool(p);
    say(p >> rel & 1u ? "ADDED" : "REMOVED", EN_NOTE_NAME[pc]);
    mark_dirty();
}

static void set_mode(int m)
{
    m = (m % EN_NMODES + EN_NMODES) % EN_NMODES;
    proj.par[P_MODE] = (int16_t)m;
    proj.pool = EN_MODE_POOL[m];
    en_set(P_MODE, m);
    en_pool(proj.pool);
    say("MODE", EN_MODE_NAME[m]);
    mark_dirty();
}

static void set_piece(int i)                    /* the piece playing kept as it is; piece i as it was left */
{
    project_piece(i);
    project_apply();
    en_seed(plat_ms());
    en_regrow();
    say_as("PIECE", piece_name(proj.piece), 0);
    sp_piece();                                  /* a screen reader: the piece with its key and tempo */
    mark_dirty();
}

/* ------------------------------------------------------------ the dice --- */
/* SEL rolls the dice: tapped alone, on the page shown (its values, in ranges that keep it music: no voice silenced, no
 * value at an extreme the ear hears as broken); held with a page's button, on that whole layer (a voice: its patch, how
 * it plays, its modulators; the garden: the garden and new loops; the pulse: new patterns; SETUP: a new key); with
 * EDIT, the loops and patterns grown anew (the settings kept). Held alone a second: back to before the last roll (one
 * step of undo: the piece's settings and patches as they were). */
static void caps(char *d, const char *s, int n);
static uint32_t dice_rng = 0x6A09E667u;
static struct {
    uint8_t ok;
    int16_t par[P_NPARAMS];
    uint16_t pool;
    uint8_t pat[EN_NVOX][EN_PATCH];
} undo;
static int dice(int lo, int hi)                 /* lo..hi, both in */
{
    uint32_t x = dice_rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    dice_rng = x;
    return lo + (int)((x >> 8) % (uint32_t)(hi - lo + 1));
}
static void dice_keep(void)                     /* the undo: the piece as it is before a roll */
{
    memcpy(undo.par, proj.par, sizeof undo.par);
    undo.pool = proj.pool;
    memcpy(undo.pat, vpatch, sizeof undo.pat);
    undo.ok = 1;
    dice_rng ^= plat_ms() * 2654435761u;
    if (!dice_rng)
        dice_rng = 1u;
}
static void dice_par(int p, int lo, int hi)     /* a parameter rolled in lo..hi (within its range) */
{
    const en_param_t *pi = en_param_info(p);
    int v = dice(lo < pi->lo ? pi->lo : lo, hi > pi->hi ? pi->hi : hi);
    proj.par[p] = (int16_t)v;
    en_set(p, v);
    par_out(p);
}
static void dice_byte(int v, int off, int lo, int hi)
{
    int x = dice(lo, hi);
    project_patch_set(v, off, x);
    patch_out(v, off, x);
}
static void dice_source(int v)                  /* a patch: the voice's own six or a bank's voice, as loaded */
{
    const int sp = EN_VOX_PATCH[v];
    int s = dice(0, EN_NSRC - 1);
    uint8_t q[EN_PATCH + 1];
    if (s >= EN_NFACT && !bank_voice(v, s, q)) {
        s = bank_next(s, 1);
        if (s >= EN_NFACT && !bank_voice(v, s, q))
            s = dice(0, EN_NFACT - 1);
    }
    project_source(v, s);
    par_out(sp);
}
static void dice_op(int v, int n)               /* operator n (1..6): a harmonic ratio, a level, an envelope */
{
    static const uint8_t RATIO[8] = {0, 1, 1, 2, 2, 3, 4, 7};   /* (0 is x0.5) */
    const int o = EP_OPN(n), fc = RATIO[dice(0, 7)], ff = dice(0, 1) ? 0 : dice(1, 50);   /* (half of them in tune) */
    dice_byte(v, o + EP_MODE, 0, 0);
    dice_byte(v, o + EP_FC, fc, fc);
    dice_byte(v, o + EP_FF, ff, ff);
    dice_byte(v, o + EP_OL, 55, 95);
    dice_byte(v, o + EP_DET, 5, 9);
}
static void dice_env(int v, int n)              /* operator n's envelope: an attack, a decay, a sustain, a release */
{
    const int o = EP_OPN(n);
    dice_byte(v, o + EP_R1, 35, 99);
    dice_byte(v, o + EP_R1 + 1, 20, 80);
    dice_byte(v, o + EP_R1 + 2, 15, 70);
    dice_byte(v, o + EP_R1 + 3, 20, 75);
    dice_byte(v, o + EP_L1, 80, 99);
    dice_byte(v, o + EP_L1 + 1, 60, 99);
    dice_byte(v, o + EP_L1 + 2, 0, 90);
    dice_byte(v, o + EP_L1 + 3, 0, 12);         /* (the end: silence, or near it) */
}
static void dice_adsr(int v)                    /* an attack, a decay, a sustain, a release */
{
    dice_par(P_PL(v, PL_A), 0, 60);
    dice_par(P_PL(v, PL_D), 10, 80);
    dice_par(P_PL(v, PL_S), 20, 100);
    dice_par(P_PL(v, PL_R), 10, 80);
}
static void dice_mod(int v, int s, int on)      /* modulator s: a target, a shape, a pace; on 0: off */
{
    static const uint8_t DEST[] = {MOD_TIMBRE, MOD_TIMBRE, MOD_METAL, MOD_FEEDBACK, MOD_DECAY, MOD_PITCH, MOD_SPACE,
                                   MOD_LEVEL, MOD_LFOPM, MOD_ALL};
    int d = dice(0, (int)sizeof DEST);           /* (one more: an operator's own, its level or fine pitch) */
    if (!on) {
        dice_par(P_MS(v, s, MS_DEPTH), 0, 0);
        dice_par(P_MS(v, s, MS_DEST), EN_MOD_OFF, EN_MOD_OFF);
        return;
    }
    if (d == (int)sizeof DEST)
        d = MOD_OP + (dice(1, 6) - 1) * OPT_N + (dice(0, 1) ? OPT_LEVEL : OPT_FINE);
    else
        d = DEST[d];
    proj.par[P_MS(v, s, MS_DEST)] = (int16_t)(d + 1);
    en_set(P_MS(v, s, MS_DEST), d + 1);
    par_out(P_MS(v, s, MS_DEST));
    dice_par(P_MS(v, s, MS_DEPTH), 10, d == MOD_PITCH || d == MOD_LEVEL ? 35 : 60);
    dice_par(P_MS(v, s, MS_RATE), 10, 70);
    dice_par(P_MS(v, s, MS_SHAPE), 0, EN_NMODSHAPE - 1);
    dice_par(P_MS(v, s, MS_SPREAD), 30, 100);
    dice_par(P_MS(v, s, MS_FADE), 0, 50);
    dice_par(P_MS(v, s, MS_SYNC), 0, EN_NMODSYNC - 1);
    dice_par(P_MS(v, s, MS_SMOOTH), 0, 60);
}
/* the Gen page: voice v made from its kind, bright and motion, and its seed (the knobs move the voice made); EDIT a
 * new seed, REC the voice as it is varied by VARY (each kept for SEL's undo) */
static void gen_apply(int v, int fresh)
{
    uint8_t p[EN_PATCH + 1];
    en_gen(ui.gen[v][0], ui.gen[v][1], ui.gen[v][2], ui.gen_seed[v], p);
    project_patch_all(v, p, fresh);
    mark_dirty();
}
static void gen_new(int v)
{
    char t[12];
    dice_keep();
    ui.gen_seed[v] = (uint32_t)dice(1, 0x7FFFFF);
    gen_apply(v, 1);
    caps(t, EN_GEN_NAME[ui.gen[v][0] % EN_NGEN], sizeof t);
    say("NEW VOICE", t);
}
static void gen_vary(int v)
{
    uint8_t p[EN_PATCH + 1];
    char t[12];
    dice_keep();
    if (proj.par[P_VX(v, VX_OWN)])
        memcpy(p, vpatch[v], EN_PATCH);
    else if (!bank_voice(v, proj.par[EN_VOX_PATCH[v]], p))
        return;
    en_gen_vary(p, ui.gen[v][3], (uint32_t)dice(1, 0x7FFFFF));
    project_patch_all(v, p, 0);
    mark_dirty();
    itoa_u(ui.gen[v][3], t);
    say("VARIED", t);
}
static void dice_layer(int vw)                  /* a layer's page: its four, in their ranges */
{
    switch (vw) {
    case V_GARDEN:
        dice_par(P_TEMPO, 50, 100);
        dice_par(P_EVOLVE, 10, 80);
        dice_par(P_MOTION, 20, 90);
        dice_par(P_DRIFT, 10, 70);
        break;
    case V_PAD: case V_BELLS: case V_BASS: {    /* a voice: its level, a role, an octave for it, a density */
        const int v = view_vox(vw), r = dice(0, EN_NROLE - 1);
        dice_par(vw == V_PAD ? P_PAD : vw == V_BELLS ? P_BELLS : P_BASS, 45, 80);
        dice_par(P_PL(v, PL_ROLE), r, r);
        dice_par(P_PL(v, PL_OCT), r == ROLE_BASS ? -1 : r == ROLE_LOOPS ? 0 : -1, r == ROLE_BASS ? 0 : 1);
        dice_par(P_PL(v, PL_DENS), r == ROLE_CHORD ? 40 : 15, r == ROLE_CHORD ? 100 : 80);
        break;
    }
    case V_TMOD1: {                             /* the reel's five: two or three on (a target, a depth, a pace), the
                                                 * rest Off */
        const int on = dice(2, 3);
        int k;
        for (k = 0; k < EN_NTMOD; k++) {
            const int b = P_TMOD + k * TM_N;
            dice_par(b + TM_TARGET, k < on ? 1 : EN_MOD_OFF, k < on ? EN_NTMSET : EN_MOD_OFF);
            dice_par(b + TM_DEPTH, k < on ? 10 : 0, k < on ? 55 : 0);
            dice_par(b + TM_RATE, 5, 70);
        }
        break;
    }
    case V_TMOD2: {                             /* the five's shapes, syncs, smoothing */
        int k;
        for (k = 0; k < EN_NTMOD; k++) {
            dice_par(P_TMOD + k * TM_N + TM_SHAPE, 0, SHAPE_CHAOS);
            dice_par(P_TMOD + k * TM_N + TM_SYNC, 0, 1);
            dice_par(P_TMOD + k * TM_N + TM_SMOOTH, 0, 60);
        }
        break;
    }
    case V_SONG:
        dice_par(P_SG(FOLLOW), 0, 100);
        dice_par(P_SG(VARY), 0, 100);
        break;
    case V_MIDI: case V_TKEY:                   /* (channels and keys: not the dice's) */
        break;
    case V_VIS:                                 /* a picture, its colours, its trails, how it hears */
        dice_par(P_ST(VMODE), 0, 7);
        dice_par(P_ST(VPAL), 0, 4);
        dice_par(P_ST(VTRAIL), 20, 95);
        dice_par(P_ST(VREACT), 20, 90);
        break;
    case V_VID:                                 /* the video synth: its feedback, split, colours turning, sync */
        dice_par(P_ST(VFEED), 0, 80);
        dice_par(P_ST(VRGB), 0, 80);
        dice_par(P_ST(VHUE), 0, 60);
        dice_par(P_ST(VSYNC), 0, 60);
        break;
    case V_SHAPE:                               /* a geometry, its sides, a fold, a pace */
        dice_par(P_ST(VGEO), 0, 6);
        dice_par(P_ST(VSIDES), 3, 9);
        dice_par(P_ST(VMIRROR), 0, 4);
        dice_par(P_ST(VSPEED), 20, 90);
        break;
    case V_VMIX:                                /* a second picture, how it blends, how much, the beats' hit */
        dice_par(P_ST(VLAYER), 0, 8);
        dice_par(P_ST(VBLEND), 0, 4);
        dice_par(P_ST(VMIX), 20, 90);
        dice_par(P_ST(VPULSE), 10, 90);
        break;
    case V_LIFE:                                /* how it wanders, mutates, flows, is thrown */
        dice_par(P_ST(VMORPH), 0, 80);
        dice_par(P_ST(VMUTATE), 10, 90);
        dice_par(P_ST(VFLOW), 0, 80);
        dice_par(P_ST(VCHAOS), 0, 80);
        break;
    case V_MOSH:                                /* how it smears, sorts, breaks, slices */
        dice_par(P_ST(VMOSH), 0, 90);
        dice_par(P_ST(VSORT), 0, 80);
        dice_par(P_ST(VCORRUPT), 0, 80);
        dice_par(P_ST(VSLICE), 0, 80);
        break;
    case V_REACT:                               /* the slot picked: a source, a target, a depth */
        dice_par(P_ST(AR1S) + ui.rslot * 3, 1, EN_AR_NSRC - 1);
        dice_par(P_ST(AR1T) + ui.rslot * 3, 1, EN_AR_NTGT);
        dice_par(P_ST(AR1D) + ui.rslot * 3, -80, 80);
        break;
    case V_OSC:                                 /* an oscillator: its frequency, wave, angle, scroll */
        dice_par(P_ST(VOFREQ), 5, 80);
        dice_par(P_ST(VOSHAPE), 0, 7);
        dice_par(P_ST(VOANGLE), 0, 100);
        dice_par(P_ST(VOSPEED), -60, 60);
        break;
    case V_XFORM:                               /* the feedback zoomed, turned, carried */
        dice_par(P_ST(VXZOOM), -40, 60);
        dice_par(P_ST(VXROT), -50, 50);
        dice_par(P_ST(VXSHX), -30, 30);
        dice_par(P_ST(VXTGT), 0, 2);
        break;
    case V_OSC2:                                /* a second, how they meet, how far it bends, its sync */
        dice_par(P_ST(VOFREQ2), 0, 70);
        dice_par(P_ST(VOCOMB), 0, 4);
        dice_par(P_ST(VOWARP), 0, 80);
        dice_par(P_ST(VOSYNC), 0, 2);
        break;
    case V_SCREEN:                              /* its light, how it hears (the bars left as they are) */
        dice_par(P_ST(VBRIGHT), 30, 80);
        dice_par(P_ST(VEAR), 0, 80);
        break;
    case V_CHROMA:                              /* its colours folded, stepped, in blocks, cut */
        dice_par(P_ST(VFOLD), 0, 80);
        dice_par(P_ST(VPOSTER), 0, 70);
        dice_par(P_ST(VPIXEL), 0, 60);
        dice_par(P_ST(VKEY), 0, 60);
        break;
    case V_SPACE:
        dice_par(P_ECHO, 15, 70);
        dice_par(P_ETIME, 0, 5);
        dice_par(P_REVERB, 30, 90);
        dice_par(P_SIZE, 40, 100);
        break;
    case V_TAPE: {                              /* the tape: a level, a sound on sound, a speed (1x, -1x, or about) */
        static const int8_t SPEED[6] = {75, 75, 25, 62, 88, 38};
        const int sp = SPEED[dice(0, 5)];
        dice_par(P_RL(MIX), 30, 80);
        dice_par(P_RL(SOS), 30, 90);
        dice_par(P_RL(SPEED), sp, sp);
        break;
    }
    case V_GENE:                                /* the genes: their size, where, how they follow, a splice */
        dice_par(P_RL(GENE), 20, 100);
        dice_par(P_RL(SLIDE), 0, 100);
        dice_par(P_RL(MORPH), 20, 95);
        dice_par(P_RL(ORG), 0, en_reel_nspl ? (int)en_reel_nspl - 1 : 0);
        break;
    case V_DECK:                                /* the deck: the space before or after, its wow, drive and tone */
        dice_par(P_RL(FX), 0, 1);
        dice_par(P_RL(WOW), 0, 60);
        dice_par(P_RL(DRIVE), 0, 70);
        dice_par(P_RL(TONE), 40, 100);
        break;
    default: {                                  /* SETUP: a new key (the root, a mode) */
        int m = dice(0, EN_NMODES - 1);
        dice_par(P_ROOT, 0, 11);
        proj.par[P_MODE] = (int16_t)m;
        proj.pool = EN_MODE_POOL[m];
        en_set(P_MODE, m);
        en_pool(proj.pool);
        break;
    }
    }
}
static void dice_page(int vw)                   /* the page shown */
{
    const int v = view_vox(vw), s = view_sub(vw);
    int k;
    if (vw < V_MAIN) {
        dice_layer(vw);
        return;
    }
    switch (s) {
    case S_GEN:                                 /* a kind, how bright, how it moves, a new seed: a voice made */
        ui.gen[v][0] = (uint8_t)dice(0, EN_NGEN - 1);
        ui.gen[v][1] = (uint8_t)dice(15, 85);
        ui.gen[v][2] = (uint8_t)dice(10, 90);
        ui.gen[v][3] = (uint8_t)dice(10, 60);
        ui.gen_seed[v] = (uint32_t)dice(1, 0x7FFFFF);
        gen_apply(v, 1);
        break;
    case S_VOICE:
        dice_source(v);
        dice_par(P_VX(v, VX_INDEX), -6, 6);
        break;
    case S_PLAY:
        dice_par(P_PL(v, PL_ENV), 0, EN_NENV - 1);
        dice_par(P_PL(v, PL_GATE), 20, 90);
        dice_par(P_PL(v, PL_PACE), 0, 100);
        dice_par(P_PL(v, PL_GLIDE), 0, 60);
        break;
    case S_ADSR:
        dice_adsr(v);
        break;
    case S_LFO: case S_LFO2:
        dice_byte(v, EP_LFS, 10, 70);
        dice_byte(v, EP_LFD, 0, 60);
        dice_byte(v, EP_LPMD, 0, 20);
        dice_byte(v, EP_LAMD, 0, 30);
        dice_byte(v, EP_LFW, 0, 5);
        dice_byte(v, EP_LPMS, 0, 4);
        dice_byte(v, EP_LKS, 0, 1);              /* (LFO 2's key sync; its transpose stays) */
        break;
    case S_PEG:                                 /* the pitch envelope: small moves around the note */
        for (k = 0; k < 4; k++) {
            dice_byte(v, EP_PR1 + k, 40, 99);
            dice_byte(v, EP_PL1 + k, k == 3 ? 50 : 44, k == 3 ? 50 : 56);
        }
        dice_byte(v, EP_OKS, 0, 1);
        break;
    case S_OPENV:                               /* the six envelopes */
        for (k = 1; k <= 6; k++)
            dice_env(v, k);
        break;
    case S_MOD: case S_MOD2: case S_MOD3: {     /* the five: two to four on, the rest Off */
        const int on = dice(2, 4);
        for (k = 0; k < EN_NSLOT; k++)
            dice_mod(v, k, k < on);
        break;
    }
    case S_OP2:                                 /* the six: ratio kept (not fixed), a detune, a velocity */
        for (k = 1; k <= 6; k++) {
            dice_byte(v, EP_OPN(k) + EP_MODE, 0, 0);
            dice_byte(v, EP_OPN(k) + EP_DET, 3, 11);
            dice_byte(v, EP_OPN(k) + EP_KVS, 0, 7);
        }
        break;
    case S_OP3:                                 /* the six's keyboard scaling: a break point, its depths */
        for (k = 1; k <= 6; k++) {
            dice_byte(v, EP_OPN(k) + EP_BP, 15, 75);
            dice_byte(v, EP_OPN(k) + EP_LD, 0, 50);
            dice_byte(v, EP_OPN(k) + EP_RD, 0, 50);
        }
        break;
    case S_OP4:                                 /* its curves, its rate scaling */
        for (k = 1; k <= 6; k++) {
            dice_byte(v, EP_OPN(k) + EP_LC, 0, 3);
            dice_byte(v, EP_OPN(k) + EP_RC, 0, 3);
            dice_byte(v, EP_OPN(k) + EP_RS, 0, 3);
        }
        break;
    case S_OP5:                                 /* the six's amp modulation sensitivity */
        for (k = 1; k <= 6; k++)
            dice_byte(v, EP_OPN(k) + EP_AMS, 0, 3);
        break;
    default:                                    /* the operator page: all six */
        for (k = 1; k <= 6; k++)
            dice_op(v, k);
        break;
    }
}
static void dice_voice(int v)                   /* a voice whole: its patch, how it plays, two or three modulators */
{
    int s, on = dice(2, 3);
    dice_layer(VOX_VIEW[v]);
    dice_source(v);
    dice_par(P_VX(v, VX_INDEX), -6, 6);
    dice_par(P_PL(v, PL_ENV), 0, EN_NENV - 1);
    dice_par(P_PL(v, PL_GATE), 20, 90);
    dice_par(P_PL(v, PL_PACE), 0, 100);
    dice_par(P_PL(v, PL_GLIDE), 0, 60);
    dice_adsr(v);
    for (s = 0; s < EN_NSLOT; s++)
        dice_mod(v, s, s < on);
}
static void dice_roll(int b)                    /* SEL alone (b B_SEL), or held with button b */
{
    int v;
    if (b != B_SEL && b != B_EDIT) {             /* (a button with no layer: nothing rolled, the undo kept) */
        for (v = 0; v < V_MAIN && VIEW_BTN[v] != b; v++)
            ;
        if (v >= V_MAIN)
            return;
    }
    dice_keep();
    if (b == B_SEL) {
        dice_page(ui.view);
        say("DICE", "THIS PAGE");
    } else if (b == B_EDIT) {
        en_seed((uint32_t)dice(0, 65535));
        en_regrow();
        say("DICE", "NEW LOOPS");
    } else {
        for (v = 0; v < V_MAIN && VIEW_BTN[v] != b; v++)
            ;
        if (v >= V_MAIN)
            return;
        if (view_vox(v) >= 0)
            dice_voice(view_vox(v));
        else
            dice_layer(v);
        if (v == V_GARDEN) {                     /* the garden: grown anew as well */
            en_seed((uint32_t)dice(0, 65535));
            en_regrow();
        }
        {
            char t[12];
            caps(t, MAIN_NAME[v], sizeof t);
            say("DICE", t);
        }
    }
    mark_dirty();
}
static void par_init(int p)                     /* a parameter to its default */
{
    const int v = en_param_info(p)->def;
    proj.par[p] = (int16_t)v;
    en_set(p, v);
    par_out(p);
}
/* SEL + SAVE: what the page is part of, to its defaults (SEL held undoes it): on a voice's MOD pages its five
 * modulators; on its other pages how it plays (Envelope .. the ADSR, its role kept), its Index and modulators (its
 * patch kept); on the reel's pages the reel's settings and modulators (its source kept); elsewhere every modulator */
static void dice_init(void)
{
    const int v = view_vox(ui.view), sub = view_sub(ui.view);
    int s, k;
    dice_keep();
    if (v >= 0 && sub >= S_MOD) {
        for (s = 0; s < EN_NSLOT; s++)
            for (k = 0; k < MS_N; k++)
                par_init(P_MS(v, s, k));
        say("INIT", "ITS MODULATORS");
    } else if (v >= 0) {
        for (k = 0; k < PL_N; k++)
            if (k != PL_ROLE)
                par_init(P_PL(v, k));
        par_init(P_VX(v, VX_INDEX));
        for (s = 0; s < EN_NSLOT; s++)
            for (k = 0; k < MS_N; k++)
                par_init(P_MS(v, s, k));
        say("INIT", "THE VOICE");
    } else if (tape_view(ui.view)) {
        for (k = P_TAPE; k < P_NPARAMS; k++)
            if (k != P_RL(SRC))
                par_init(k);
        say("INIT", "THE REEL");
    } else {
        for (s = 0; s < EN_NVOX; s++)
            for (k = 0; k < EN_NSLOT * MS_N; k++)
                par_init(P_VX(s, VX_SLOT) + k);
        for (k = P_TMOD; k < P_NPARAMS; k++)
            par_init(k);
        say("INIT", "ALL MODULATION");
    }
    mark_dirty();
}
static void dice_undo(void)
{
    if (!undo.ok) {
        warn("NOTHING", "TO UNDO");
        return;
    }
    memcpy(proj.par, undo.par, sizeof proj.par);
    proj.pool = undo.pool;
    memcpy(vpatch, undo.pat, sizeof vpatch);
    undo.ok = 0;
    project_apply();
    say("UNDO", "BEFORE THE DICE");
    mark_dirty();
}

/* ------------------------------------------------------------ buttons --- */
static void set_view(int v)
{
    int g = view_group(v);
    ui.view = (uint8_t)v;
    ui.touched = -1;
    if (g >= 0)                                  /* the group remembers its page */
        ui.gsub[view_vox(v)][g] = (uint8_t)(view_sub(v) - GROUP0[g]);
}

static void vox_title(void)                     /* which voice, and which of its pages, large on the screen (a
                                                 * screen reader hears the page itself) */
{
    char t[12], s[12];
    int x = view_vox(ui.view);
    if (x < 0)
        return;
    caps(t, MAIN_NAME[VOX_VIEW[x]], sizeof t);
    caps(s, ui.view < V_MAIN ? "Layer" : SUB_NAME[view_sub(ui.view)], sizeof s);
    say_as(t, s, 0);
}

static void reel_say(void)                     /* the reel as it is: recording, or its splices and seconds */
{
    char t[24];
    const uint32_t ds = (en_reel_len * 10u + 11025u) / 22050u;   /* tenths of a second */
    if (en_reel_state) {
        say("TAPE", en_reel_state == 1 ? "RECORDING" : "OVERDUB");
        return;
    }
    itoa_u(en_reel_nspl, t);
    {
        int k = 0;
        while (t[k])
            k++;
        t[k++] = ' ';
        t[k++] = 'S';
        t[k++] = 'P';
        t[k++] = 'L';
        t[k++] = ',';
        t[k++] = ' ';
        itoa_u(ds / 10u, t + k);
        while (t[k])
            k++;
        t[k++] = '.';
        t[k++] = (char)('0' + ds % 10u);
        t[k++] = ' ';
        t[k++] = 'S';
        t[k] = 0;
    }
    say("TAPE", t);
}
static void reel_button(int b)                  /* on the reel's pages: REC overdubs the splice picked (again: stop),
                                                 * EDIT records a new one at the end, SAVE keeps the reel too */
{
    if (project_reel_progress() >= 0) {         /* (the reel being saved: it stays as it is meanwhile) */
        warn("SAVING", "THE REEL");
        return;
    }
    if (b == B_SAVE) {
        if (en_reel_state) {
            warn("STOP THE TAPE", "FIRST");
            return;
        }
        if (project_save() != 0 || project_reel_save() != 0) {
            warn("NOT SAVED", "FLASH ERROR");
            return;
        }
        ui.dirty = 0;
        say("SAVING", "THE REEL");                /* (a sector a frame: reel_saving) */
        return;
    }
    if (b == B_EDIT) {
        const uint32_t end = (en_reel_state == 1 ? en_reel_w : en_reel_len) + 255u;   /* as the engine has it: */
        if (en_reel_nspl >= 8u || end / 256u * 256u + 2048u > 260096u) {   /* a new one from a block, 8 blocks at least */
            if (en_reel_state == 1)
                en_reel(EN_REEL_DUB);            /* (the one recording ends) */
            warn("THE REEL", "IS FULL");
            return;
        }
        if (en_reel_state == 1) {                /* (recording one: it ends, and the next begins) */
            en_reel(EN_REEL_NEW);
            say("TAPE", "A NEW SPLICE");
        } else {
            en_reel(EN_REEL_NEW);
            say("TAPE", "RECORDING");
        }
        proj.par[P_RL(ORG)] = (int16_t)(en_reel_nspl < 8u ? en_reel_nspl : 7);   /* (the new one plays) */
        en_set(P_RL(ORG), proj.par[P_RL(ORG)]);
        par_out(P_RL(ORG));
        mark_dirty();
        return;
    }
    if (en_reel_state) {
        en_reel(EN_REEL_DUB);                    /* (stops either) */
        say("TAPE", "STOPPED");
    } else {
        en_reel(EN_REEL_DUB);
        say("TAPE", en_reel_nspl ? "OVERDUB" : "RECORDING");
    }
}
static int page_on(int v)                       /* a layer page shown in the round (Osc's: Osc in use) */
{
    return (v != V_OSC && v != V_OSC2) || proj.par[P_ST(VMODE)] == VIS_OSC || proj.par[P_ST(VLAYER)] == VIS_OSC + 1;
}
static int page_at(int v)                       /* its place in PAGE_ORDER */
{
    int i;
    for (i = 0; i < V_MAIN; i++)
        if (PAGE_ORDER[i] == v)
            return i;
    return 0;
}
static int page_step(int v, int dir, int visual)   /* the next page shown, dir +1 / -1 (-1: none that way); visual:
                                                 * the video synth's alone */
{
    int i = page_at(v) + dir;
    for (; i >= 0 && i < V_MAIN; i += dir)
        if (page_on(PAGE_ORDER[i]) && (!visual || (PAGE_ORDER[i] >= V_VIS && PAGE_ORDER[i] <= V_XFORM)))
            return PAGE_ORDER[i];
    return -1;
}
static void button(int b)
{
    int v;
    if (b == B_HOME) {                           /* the garden; again: its visual's page, the video synth's, its
                                                  * shape's, round */
        /* (the visual's pages in PAGE_ORDER, from the Screen back to the garden) */
        const int nx = ui.view == V_GARDEN ? V_VIS : ui.view >= V_VIS && ui.view <= V_XFORM ? page_step((int)ui.view, 1, 1)
                                                                                         : V_GARDEN;
        set_view(nx < 0 ? V_GARDEN : nx);
        return;
    }
    if (b == B_GLO) {                            /* Setup; again: MIDI, the song's page, round */
        set_view(ui.view == V_SETUP ? V_MIDI : ui.view == V_MIDI ? V_SONG : V_SETUP);
        return;
    }
    if (b == B_SEQ) {                            /* the reel; again: its next page, round */
        set_view(tape_view(ui.view) ? (ui.view == V_TKEY ? V_TAPE : ui.view + 1) : V_TAPE);
        reel_say();
        return;
    }
    for (v = 0; v < V_MAIN; v++)                 /* a page's button: its page; a voice's again: its next group */
        if (VIEW_BTN[v] == b) {
            int x = view_vox(v), g = view_group(ui.view) + 1;
            if (x >= 0 && view_vox(ui.view) == x && g < NGROUP)
                set_view(V_MAIN + x * NSUB + GROUP0[g] + ui.gsub[x][g]);
            else
                set_view(v);
            if (x >= 0)
                vox_title();
            return;
        }
    if (tape_view(ui.view) && (b == B_REC || b == B_EDIT || b == B_SAVE)) {   /* the reel's transport */
        reel_button(b);
        return;
    }
    if (ui.view >= V_VIS && ui.view <= V_XFORM && (b == B_EDIT || b == B_REC)) {   /* the visual: a new genome; held */
        if (b == B_EDIT) {
            vis_mutate(1.0f);
            vhold = 0;
            say("VISUAL", "NEW STRUCTURE");
        } else {
            vhold = (uint8_t)!vhold;
            say("STRUCTURE", vhold ? "HELD" : "FREE");
        }
        return;
    }
    if (view_sub(ui.view) == S_GEN && (b == B_EDIT || b == B_REC)) {   /* the generator: a new voice, or varied */
        if (b == B_EDIT)
            gen_new(view_vox(ui.view));
        else
            gen_vary(view_vox(ui.view));
        return;
    }
    switch (b) {
    case B_EDIT:
        en_next();
        say("NEXT CHORD", en_playing ? "ON THE BAR" : 0);
        break;
    case B_PLAY:
        sp_tp.playing = !en_playing;
        sp_tp.play_t = plat_ms();
        if (en_playing) {
            en_play(0);
            say("RESTING", 0);
        } else {
            en_seed(plat_ms());
            en_play(1);
            say("GROWING", 0);
        }
        break;
    case B_REC:
        say("FREEZE", en_frozen ? "OFF" : "ON");
        en_freeze(!en_frozen);                   /* the engine takes it at its next block */
        break;
    case B_OCTDN: case B_OCTUP:
        {
            const int p = P_PL(kvox(), PL_OCT);
            char t[16] = "OCTAVE ", w[12];
            knob_set(p, proj.par[p] + (b == B_OCTUP ? 1 : -1));
            en_param_text(p, proj.par[p], t + 7);
            caps(w, MAIN_NAME[VOX_VIEW[kvox()]], sizeof w);
            say(w, t);
        }
        break;
    case B_SAVE:
        if (project_save() == 0) {
            ui.dirty = 0;
            say("SAVED", 0);
        } else {
            warn("NOT SAVED", "FLASH ERROR");
        }
        break;
    default: break;
    }
}

/* MIDI in (USB and the TRS jack): notes on voice 1's channel add or remove their note from the scale (Notes in Scale),
 * or each voice's channel plays it (Play); clock and start / stop drive the grid */
/* a CC in (cc_out's map): a parameter set, or a patch byte by NRPN, as the knob would (not sent out again) */
static int ch_vox(int ch)                       /* the voice on MIDI channel ch (0..15); -1 none */
{
    int v;
    for (v = 0; v < EN_NVOX; v++)
        if (proj.par[P_ST(CH1) + v] - 1 == ch)
            return v;
    return -1;
}
static void midi_cc(int mch, int cc, int v)
{
    int p = -1;
    const int vx = ch_vox(mch), ch = vx >= 0 ? vx : mch == 15 ? 15 : 99;   /* (a voice's index; 15 the rest's) */
    ui.cc_in = 1;
    if (ch < EN_NVOX && (cc == 99 || cc == 98)) {
        ui.nrpn[ch][cc == 99 ? 0 : 1] = (uint8_t)v;
    } else if (ch < EN_NVOX && cc == 6) {        /* NRPN data: a patch byte of the channel's voice */
        int off = ui.nrpn[ch][0] << 7 | ui.nrpn[ch][1];
        if (off < EN_PATCH) {
            project_patch_set(ch, off, v);
            mark_dirty();
        }
    } else if (ch < EN_NVOX && cc >= CC0 && cc < CC0 + VX_N - VX_INDEX) {
        p = P_VX(ch, VX_INDEX + cc - CC0);
    } else if (ch < EN_NVOX && cc >= CC_PL && cc < CC_PL + PL_N) {
        p = P_PL(ch, cc - CC_PL);
    } else if (ch == 15 && cc >= 50 && cc < 50 + P_NPARAMS - P_TAPE) {   /* the reel's and its modulators' */
        p = P_TAPE + cc - 50;
    } else if (ch == 15 && cc >= CC0 && cc < CC0 + 30) {
        p = cc - CC0;
    }
    if (p >= 0 && p != P_MIDI) {
        const en_param_t *pi = en_param_info(p);
        int x = is_src(p) ? v : pi->lo + (v * (pi->hi - pi->lo) + 63) / 127;
        if (is_src(p)) {
            uint8_t q[EN_PATCH + 1];
            if (x < EN_NSRC && bank_voice(src_vox(p), x, q) && x != proj.par[p]) {
                project_source(src_vox(p), x);
                mark_dirty();
            }
        } else if (x != proj.par[p]) {
            proj.par[p] = (int16_t)x;
            en_set(p, x);
            mark_dirty();
        }
    }
    ui.cc_in = 0;
}

static void midi_in(void)
{
    uint32_t pkt;
    int port;
    while (plat_midi_in(&pkt, &port)) {
        uint32_t st = (pkt >> 8) & 0xFFu, n = (pkt >> 16) & 0x7Fu, v = pkt >> 24;
        if ((st & 0xF0u) == 0x90u && v) {        /* (the MIDI page says where the last note came from) */
            ui.in_ch = (uint8_t)((st & 15u) + 1u);
            ui.in_port = (uint8_t)port;
        }
        if (st == 0xF8u || st == 0xFAu || st == 0xFBu || st == 0xFCu)
            en_clock(st == 0xF8u ? EN_CLK_TICK : st == 0xFAu ? EN_CLK_START : st == 0xFBu ? EN_CLK_CONTINUE : EN_CLK_STOP);
        else if ((st & 0xF0u) == 0x90u || (st & 0xF0u) == 0x80u) {   /* a note: the scale's, or a voice's */
            const int on = (st & 0xF0u) == 0x90u && v, x = ch_vox((int)(st & 15u));
            if (proj.par[P_ST(NOTES)] && x >= 0) {   /* Play: the voice of its channel, at its pitch */
                int q = (int)n - 12 * proj.par[P_PL(x, PL_OCT)];
                en_key(x, q < 0 ? 0 : q > 127 ? 127 : q, on);
            } else if (!proj.par[P_ST(NOTES)] && on && x == 0) {
                toggle_note((int)(n % 12u));
            }
        } else if ((st & 0xF0u) == 0xB0u)
            midi_cc((int)(st & 15u), (int)n, (int)(v & 0x7Fu));
    }
}

void ui_patch_in(int voice)                     /* a DX7 voice came in over USB: it plays on this voice */
{
    char t[12];
    int k;
    for (k = 0; k < 10; k++)
        t[k] = (char)vpatch[voice][EP_NAME + k];
    t[10] = 0;
    say(voice == VOX_PAD ? "VOICE 1: DX7 IN" : voice == VOX_BELLS ? "VOICE 2: DX7 IN" : "VOICE 3: DX7 IN", t);
    mark_dirty();
}
void ui_bank_in(int bank, int voices)          /* a bank came in */
{
    char a[12], b[16];
    itoa_u((uint32_t)bank + 1u, a + 5);
    a[0] = 'B', a[1] = 'A', a[2] = 'N', a[3] = 'K', a[4] = ' ';
    itoa_u((uint32_t)voices, b);
    cat_s(b + (voices > 9 ? 2 : 1), " VOICES");
    say(a, b);
}

/* HELP CARDS (X0X's): hold one button alone for HELP_HOLD and a card says what it does. A key, a
 * turn or another button puts it away; letting go after its card does nothing else. PLAY acts on
 * its press, and SEL has no job here: neither has a card. */
#define HELP_HOLD 900u
#define RESET_HOLD 1500u
static void help_tick(uint32_t btn, uint32_t keys)
{
    uint32_t now = plat_ms();
    int i;
    if (ui.help) {
        uint32_t m = 1u << (ui.help - 1);
        if (btn != m || keys || (int32_t)(ui.turn_t - ui.down_t[ui.help - 1]) > 0)
            ui.help = 0;
        return;
    }
    if (!btn || (btn & (btn - 1u)) || keys)
        return;
    for (i = 0; i < NB; i++)
        if (btn == 1u << i && i != B_PLAY && i != B_SEL && !(ui.btn_used & btn) && now - ui.down_t[i] >= HELP_HOLD &&
            (int32_t)(ui.turn_t - ui.down_t[i]) <= 0) {
            ui.help = (uint8_t)(i + 1);
            ui.btn_used |= btn;                  /* its release now does nothing */
        }
}

static void input(void)
{
    uint32_t btn = plat_buttons(), keys = plat_keys(), ch, i;
    int32_t e;
    ch = btn ^ ui.btn;
    ui.btn = btn;
    for (i = 0; i < NB; i++) {
        uint32_t m = 1u << i;
        if (!(ch & m))
            continue;
        if (btn & m) {
            ui.btn_used &= ~m;
            ui.down_t[i] = plat_ms();
            if (i == B_SAVE)
                ui.reset_done = 0;
            if (i == B_REC)
                ui.erase_done = 0;
            if (i == B_PLAY)                   /* transport on the press */
                button((int)i);
        } else if (!(ui.btn_used & m) && i != B_PLAY) {
            if (i == B_SEL) {                    /* SEL alone: this page */
                dice_roll(B_SEL);
            } else if (btn >> B_SEL & 1u) {      /* with SEL held: the dice on its layer, SAVE initialises (SEL's
                                                  * own release then does nothing) */
                ui.btn_used |= 1u << B_SEL;
                if (i == B_SAVE)
                    dice_init();
                else
                    dice_roll((int)i);
            } else {
                button((int)i);
            }
        }
    }
    ch = keys ^ ui.keys;
    ui.keys = keys;
    for (i = 0; i < NKEYS; i++) {                /* key 0 is F3 */
        if (!(ch >> i & 1u))
            continue;
        if (keys >> i & 1u) {
            const int v = view_vox(ui.view);
            if (v >= 0) {                        /* a voice's page: the key plays it, held */
                ui.key_vox[i] = (int8_t)v;
                en_key(v, KEY_NOTE0 + (int)i, 1);
            } else if (ui.view == V_GARDEN) {    /* the Partiture: the note in or out of the scale */
                toggle_note((int)((5u + i) % 12u));
            } else if (ui.view >= V_VIS && ui.view <= V_XFORM) {   /* the visual's pages: the keys play it */
                vis_key((int)i);
            } else if (tape_view(ui.view) && proj.par[P_RL(SRC)] >= 6) {   /* the reel's, SOURCE Play: the voice
                                                  * (or all three) played, as the reel records it */
                const int w = proj.par[P_RL(SRC)] - 6, vv = w < 3 ? w : 3;
                int x;
                ui.key_vox[i] = (int8_t)vv;
                for (x = 0; x < EN_NVOX; x++)
                    if (vv == 3 || vv == x)
                        en_key(x, KEY_NOTE0 + (int)i, 1);
            }
        } else if (ui.key_vox[i] >= 0) {         /* let go: by the voice it played (3: all), whatever page now */
            int x;
            for (x = 0; x < EN_NVOX; x++)
                if (ui.key_vox[i] == 3 || ui.key_vox[i] == x)
                    en_key(x, KEY_NOTE0 + (int)i, 0);
            ui.key_vox[i] = -1;
        }
    }
    if (btn || keys || ch)
        ui.act_t = plat_ms();
    midi_in();
    if ((e = plat_enc(EN_SELECT)) != 0) {        /* SELECT: the pages; with HOME held, the tempo */
        if (btn >> B_HOME & 1u) {
            turn(EN_SELECT, P_TEMPO, e);
            ui.btn_used |= 1u << B_HOME;
            if (ui.view == V_GARDEN) {           /* Tempo is the Garden's first knob */
                ui.touched = 0;
                ui.touch_until = plat_ms() + 1200u;
            } else {
                char t[8];
                itoa_u((uint32_t)proj.par[P_TEMPO], t);
                say("TEMPO", t);
            }
        } else if (ui.view < V_MAIN) {            /* the layers' pages, one after another (PAGE_ORDER) */
            const int v = page_step((int)ui.view, e > 0 ? 1 : -1, 0);
            if (v >= 0)
                set_view(v);
        } else {                                 /* a voice's own, one after another; before the first its layer's */
            int x = view_vox(ui.view), s = view_sub(ui.view) + (e > 0 ? 1 : -1);
            if (s < 0)
                set_view(VOX_VIEW[x]);
            else if (s < NSUB)
                set_view(V_MAIN + x * NSUB + s);
            vox_title();
        }
        ui.turn_t = plat_ms();
    }
    if ((e = plat_enc(EN_ALGO)) != 0) {
        set_mode(proj.par[P_MODE] + (e > 0 ? 1 : -1));
        ui.turn_t = plat_ms();
    }
    if ((e = plat_enc(EN_PRESET)) != 0) {
        set_piece(proj.piece + (e > 0 ? 1 : -1));
        ui.turn_t = plat_ms();
    }
    if ((btn >> B_SAVE & 1u) && !ui.reset_done && plat_ms() - ui.down_t[B_SAVE] > RESET_HOLD) {
        ui.reset_done = 1;                     /* held: the piece back to its recipe (not saved; SEL held undoes it) */
        dice_keep();
        ui.btn_used |= 1u << B_SAVE;
        ui.help = 0;
        project_piece_reset();
        project_apply();
        en_regrow();
        say("RESET", piece_name(proj.piece));
        mark_dirty();
    }
    for (i = 0; i < 4; i++)
        if ((e = plat_enc(EN_K1 + (int)i)) != 0) {
            turn(EN_K1 + (int)i, view_knob(ui.view, (int)i), e);
            ui.touched = (int8_t)i;
            ui.touch_until = plat_ms() + 1200u;
            ui.act_t = ui.turn_t = plat_ms();
        }
    if ((btn >> B_REC & 1u) && tape_view(ui.view) && !ui.erase_done && plat_ms() - ui.down_t[B_REC] >= RESET_HOLD) {
        ui.erase_done = 1;                       /* REC held on the reel's pages: the reel erased */
        ui.btn_used |= 1u << B_REC;
        ui.help = 0;
        if (project_reel_progress() >= 0) {      /* (not while it is being saved) */
            warn("SAVING", "THE REEL");
        } else {
            en_reel(EN_REEL_ERASE);
            warn("TAPE", "ERASED");
        }
    }
    if (btn == 1u << B_SEL && !(ui.btn_used >> B_SEL & 1u) && plat_ms() - ui.down_t[B_SEL] >= 1000u) {
        ui.btn_used |= 1u << B_SEL;              /* SEL held alone a second: undo the last roll */
        dice_undo();
    }
    if (ui.gen_dirty) {                          /* the generator's knobs turned: the voice made again, once a frame */
        gen_apply(kvox(), ui.gen_dirty == 2u);
        ui.gen_dirty = 0;
    }
    help_tick(btn, keys);
}

/* ------------------------------------------------------------- screen --- */
/* three bands: the header (28 px), the garden (144), the knobs (68) */
#define HDR_H 28
#define MAIN_Y 28
#define MAIN_H 144
#define KNB_Y 172
#define KNB_H 68

static uint32_t hash(uint32_t h, uint32_t v) { return (h ^ v) * 16777619u; }

/* a rectangle with rounded corners (radius r, up to 6) */
static void cv_round(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint16_t c)
{
    static const uint8_t IN[7][6] = {{0}, {1}, {1, 0}, {2, 1, 0}, {2, 1, 0, 0}, {3, 2, 1, 0, 0}, {4, 2, 1, 1, 0, 0}};
    int32_t j;
    if (r > 6)
        r = 6;
    if (r > h / 2)
        r = h / 2;
    cv_rect(x, y + r, w, h - 2 * r, c);
    for (j = 0; j < r; j++) {
        int32_t d = IN[r][j];
        cv_rect(x + d, y + j, w - 2 * d, 1, c);
        cv_rect(x + d, y + h - 1 - j, w - 2 * d, 1, c);
    }
}

/* a filled circle of radius r (0..6) centred on (cx, cy): HW[r][d] is the half width d rows from the middle */
static void cv_disc(int32_t cx, int32_t cy, int32_t r, uint16_t c)
{
    static const uint8_t HW[7][7] = {{0}, {1, 0}, {2, 1, 0}, {3, 2, 2, 0}, {4, 4, 3, 2, 0}, {5, 5, 4, 4, 3, 0},
                                     {6, 6, 5, 5, 4, 3, 0}};
    int32_t j;
    if (r <= 0) {
        cv_rect(cx, cy, 1, 1, c);
        return;
    }
    if (r > 6)
        r = 6;
    for (j = -r + 1; j < r; j++) {
        int32_t w = HW[r][j < 0 ? -j : j];
        cv_rect(cx - w, cy + j, 2 * w + 1, 1, c);
    }
}

static void text_c(int32_t cx, int32_t y, const felucca_font_t *f, const char *s, uint16_t c)
{
    cv_text(cx - text_w(f, s) / 2, y, f, s, c);
}

/* anti-aliased drawing (X0X's): a pixel blended over the canvas, a disc, and a knob's ring */
static void px_blend(int32_t x, int32_t y, uint16_t c, uint32_t a)
{
    uint16_t *q;
    uint32_t bg;
    y += cv_oy;
    if ((uint32_t)x >= cv_w || y < cv_y0 || y >= cv_y1 || !a)
        return;
    q = &cv_px[(uint32_t)y * cv_w + (uint32_t)x];
    if (a >= 16u) {
        *q = swap16(c);
        return;
    }
    bg = swap16(*q);
    *q = swap16((uint32_t)(((((c >> 11) * a + (bg >> 11) * (16u - a)) >> 4) << 11) |
                           (((((c >> 5) & 63u) * a + ((bg >> 5) & 63u) * (16u - a)) >> 4) << 5) |
                           (((c & 31u) * a + (bg & 31u) * (16u - a)) >> 4)));
}

static void disc(float cx, float cy, float r, uint16_t c)
{
    int32_t x0 = (int32_t)fm_floorf(cx - r - 1.0f), x1 = (int32_t)(cx + r + 1.0f);
    int32_t y0 = (int32_t)fm_floorf(cy - r - 1.0f), y1 = (int32_t)(cy + r + 1.0f), x, y;
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++) {
            float dx = (float)x + 0.5f - cx, dy = (float)y + 0.5f - cy, d = fm_sqrtf(dx * dx + dy * dy);
            px_blend(x, y, c, (uint32_t)(fm_clampf(r - d + 0.5f, 0.0f, 1.0f) * 16.0f + 0.5f));
        }
}

/* atan2 in turns, 0..1 from +x going clockwise on screen (y down) */
static float turns(float y, float x)
{
    float ax = fm_fabsf(x), ay = fm_fabsf(y), a = ax < ay ? ax / (ay + 1e-9f) : ay / (ax + 1e-9f), s = a * a, r;
    r = ((-0.0464964749f * s + 0.15931422f) * s - 0.327622764f) * s * a + a;   /* atan on [0, 1] */
    if (ay > ax)
        r = 1.57079637f - r;
    if (x < 0.0f)
        r = FM_PI - r;
    if (y < 0.0f)
        r = FM_TWO_PI - r;
    return r * (1.0f / FM_TWO_PI);
}

/* a knob: a 270-degree track from 7:30 to 4:30, anti-aliased, round ends; the value filled from
 * the start (or from the top, for a centred value) and a round handle */
static void arc(int32_t cx, int32_t cy, int32_t r, float v, uint16_t track, uint16_t fill, uint16_t bg, int bipolar)
{
    const float th = 3.4f, a0 = 0.375f, sweep = 0.75f;
    float ro = (float)r, ri = ro - th, rm = ro - th * 0.5f, fx = (float)cx + 0.5f, fy = (float)cy + 0.5f, lo, hi;
    int32_t x, y;
    v = fm_clampf(v, 0.0f, 1.0f);
    lo = bipolar ? (v < 0.5f ? v : 0.5f) : 0.0f;
    hi = bipolar ? (v < 0.5f ? 0.5f : v) : v;
    for (y = -r - 1; y <= r + 1; y++)
        for (x = -r - 1; x <= r + 1; x++) {
            float dx = (float)x, dy = (float)y, d = fm_sqrtf(dx * dx + dy * dy), t, a;
            a = fm_clampf(ro + 0.5f - d, 0.0f, 1.0f) * fm_clampf(d - ri + 0.5f, 0.0f, 1.0f);
            if (a <= 0.0f)
                continue;
            t = turns(dy, dx) - a0;
            if (t < 0.0f)
                t += 1.0f;
            t /= sweep;
            if (t > 1.0f)
                continue;
            px_blend(cx + x, cy + y, (t >= lo && t <= hi) ? fill : track, (uint32_t)(a * 16.0f + 0.5f));
        }
    {
        float e0 = a0 * FM_TWO_PI, e1 = (a0 + sweep) * FM_TWO_PI, ev = (a0 + sweep * v) * FM_TWO_PI;
        float el = (a0 + sweep * lo) * FM_TWO_PI;
        disc(fx + fm_cosf(e0) * rm - 0.5f, fy + fm_sinf(e0) * rm - 0.5f, th * 0.5f, lo <= 0.0f && hi > 0.0f ? fill : track);
        disc(fx + fm_cosf(e1) * rm - 0.5f, fy + fm_sinf(e1) * rm - 0.5f, th * 0.5f, hi >= 1.0f ? fill : track);
        if (hi > lo)
            disc(fx + fm_cosf(el) * rm - 0.5f, fy + fm_sinf(el) * rm - 0.5f, th * 0.5f, fill);
        disc(fx + fm_cosf(ev) * rm - 0.5f, fy + fm_sinf(ev) * rm - 0.5f, th * 0.5f + 1.3f, bg);
        disc(fx + fm_cosf(ev) * rm - 0.5f, fy + fm_sinf(ev) * rm - 0.5f, th * 0.5f + 0.6f, fill);
    }
}

static void caps(char *d, const char *s, int n)  /* s in capitals, at most n - 1 characters */
{
    int i;
    for (i = 0; i < n - 1 && s[i]; i++)
        d[i] = (char)(s[i] >= 'a' && s[i] <= 'z' ? s[i] - 32 : s[i]);
    d[i] = 0;
}

static uint16_t mix(uint16_t a, uint16_t b, int t)   /* t 0..16: a -> b */
{
    int r, g, bl;
    if (t <= 0)
        return a;
    if (t >= 16)
        return b;
    r = ((a >> 11) * (16 - t) + (b >> 11) * t) / 16;
    g = (((a >> 5) & 63) * (16 - t) + ((b >> 5) & 63) * t) / 16;
    bl = ((a & 31) * (16 - t) + (b & 31) * t) / 16;
    return (uint16_t)(r << 11 | g << 5 | bl);
}
static int q16(float x) { return x <= 0.0f ? 0 : x >= 1.0f ? 16 : (int)(x * 16.0f + 0.5f); }

static void tag(int32_t *x, const char *s, uint16_t bg, uint16_t fg)
{
    int32_t w = text_w(&FONT_XS, s) + 10;
    cv_round(*x, 6, w, 16, 6, bg);
    cv_text(*x + 5, 7, &FONT_XS, s, fg);
    *x += w + 4;
}

/* the battery (plat_battery): 0 (low) .. 3 bars, 4 a USB host powers the FM-1, -1 not read yet. The
 * simulator's "battery N" (host.c) stands in for it (AMB-1's) */
#ifdef PLAT_HOST
static int8_t batt_host = -2;                  /* -2: plat_battery's */
static int batt(void) { return batt_host > -2 ? batt_host : plat_battery(); }
#else
#define batt() plat_battery()
#endif
#define K_LOW RGB(196, 84, 52)                 /* the battery at one bar or none */
#define BATT_W 20                              /* the body 18 wide and its nub */
/* lying down at the header's right: three bars, rust at one or none; a bolt instead on USB power; nothing until it
 * is read */
static void draw_batt(int32_t x, int b)
{
    static const int8_t BOLT[7][2] = {{10, 2}, {9, 2}, {8, 2}, {7, 5}, {9, 2}, {8, 2}, {7, 2}};
    uint16_t c = b <= 1 ? K_LOW : K_DIM;
    int i;
    cv_round(x, 9, 18, 10, 2, c);
    cv_rect(x + 1, 10, 16, 8, K_BG);
    cv_rect(x + 18, 12, 2, 4, c);
    if (b == 4) {
        for (i = 0; i < 7; i++)
            cv_rect(x + BOLT[i][0], 10 + i, BOLT[i][1], 1, K_TEXT);
        return;
    }
    for (i = 0; i < b && i < 3; i++)
        cv_rect(x + 2 + i * 5, 11, 4, 6, b <= 1 ? K_LOW : K_PAD);
}
static const char *batt_words(int b)
{
    static const char *const W[5] = {"Battery low", "Battery one third", "Battery two thirds", "Battery full",
                                     "On USB power"};
    return b >= 0 && b <= 4 ? W[b] : 0;
}

/* the header: the piece in a tag, its root and mode; on the right, the battery, the tempo with a slow beat light
 * (a play triangle while resting) and a dot while there is something unsaved. Messages take its place
 * for a moment. */
static void draw_header(void)
{
    char t[24];
    int msg = plat_ms() < ui.msg_until;
    uint32_t h = hash(hash(hash(2166136261u, proj.piece | (uint32_t)proj.par[P_ROOT] << 8 | (uint32_t)proj.pool << 12),
                           en_playing | ui.dirty << 4 | (uint32_t)msg << 5 | (uint32_t)en_ext << 6 | (uint32_t)proj.par[P_MODE] << 8),
                      (uint32_t)proj.par[P_TEMPO] | ((ui.view == V_SETUP ? plat_cpu_pct() << 8 | plat_xruns() << 16 | en_sheds << 24 : 0u) ^
                      (ui.view == V_MIDI ? plat_midi_count(PORT_USB, 0) << 4 ^ plat_midi_count(PORT_TRS, 0) << 14 ^
                                               plat_midi_count(PORT_BLE, 0) << 24 ^ ui.in_ch : 0u)));
    h = hash(h, (uint32_t)theme_of(ui.view) ^ ui_lsig ^ (uint32_t)(batt() + 1) << 28);
    if (msg)
        h = hash(hash(h, (uint32_t)ui.msg[0][0] << 8 | ui.msg[0][1]), ui.msg_until);
    if (en_playing)
        h = hash(h, (en_bars / 4u) & 1u);
    h = hash(h, plat_ms() / 500u & 1u);         /* (the cursor) */
    if (!vis_over) {                             /* (over the visual: its caller set the canvas, and sends it) */
        if (h == ui.sig[0])
            return;
        ui.sig[0] = h;
        cv_begin(240, HDR_H, K_BG);
    }
    if (msg) {
        int32_t x = cv_text(10, 6, &FONT_B, ui.msg[0], K_TEXT);
        cv_text(x + 8, 6, &FONT_B, ui.msg[1], K_ACC);
    } else {
        int32_t x = 232, w;
        if (batt() >= 0) {                         /* the battery, at the right */
            x -= BATT_W;
            draw_batt(x, batt());
            x -= 6;
        }
        if (ui.dirty) {                            /* unsaved */
            cv_round(x - 5, 12, 5, 5, 2, K_DIM);
            x -= 11;
        }
        if (en_ext) {
            t[0] = 'E';
            t[1] = 'X';
            t[2] = 'T';
            t[3] = 0;
        } else {
            itoa_u((uint32_t)proj.par[P_TEMPO], t);
        }
        if (ui.view == V_MIDI) {                  /* MIDI: what comes in, "U12 J3 B0 ch1" */
            char c[32] = "U", *q;
            q = c + 1;
            itoa_u(plat_midi_count(PORT_USB, 0) % 1000u, q);
            while (*q)
                q++;
            *q++ = ' ', *q++ = 'J';
            itoa_u(plat_midi_count(PORT_TRS, 0) % 1000u, q);
            while (*q)
                q++;
            *q++ = ' ', *q++ = 'B';
            itoa_u(plat_midi_count(PORT_BLE, 0) % 1000u, q);
            while (*q)
                q++;
            if (ui.in_ch) {
                *q++ = ' ', *q++ = 'c', *q++ = 'h';
                itoa_u(ui.in_ch, q);
            }
            x -= text_w(&FONT_XS, c);
            cv_text(x, 8, &FONT_XS, c, K_DIM);
            x -= 10;
        }
        if (ui.view == V_SETUP) {                 /* Setup: the audio load too, and the overloads since it started */
            char c[24] = "CPU ";
            itoa_u(plat_cpu_pct(), c + 4);
            if (plat_xruns()) {
                int k = 0;
                while (c[k])
                    k++;
                c[k++] = ' ';
                c[k++] = 'X';
                itoa_u(plat_xruns() > 999u ? 999u : plat_xruns(), c + k);
            }
            if (en_sheds) {                       /* (notes let go to keep it from overrunning) */
                int k = 0;
                while (c[k])
                    k++;
                c[k++] = ' ';
                c[k++] = 'S';
                itoa_u(en_sheds > 999u ? 999u : en_sheds, c + k);
            }
            x -= text_w(&FONT_XS, c);
            cv_text(x, 8, &FONT_XS, c, K_DIM);
            x -= 10;
        }
        x -= text_w(&FONT_S, t);
        cv_text(x, 6, &FONT_S, t, en_playing ? K_TEXT : K_DIM);
        x -= 16;
        if (en_playing) {                         /* a slow beat light */
            int on = ((en_bars / 4u) & 1u) == 0u;
            cv_round(x, 9, 10, 10, 5, on ? K_PAD : K_PAD_T);
        } else {
            int32_t j;                            /* resting: a play triangle */
            for (j = 0; j < 5; j++)
                cv_rect(x + 2 + j, 9 + j, 1, 11 - 2 * j, K_DIM);
        }
        w = text_w(&FONT_B, piece_name(proj.piece)) + 12;   /* the piece, in a tag */
        cv_round(4, 4, w, 20, 6, K_TEXT);
        cv_text(10, 6, &FONT_B, piece_name(proj.piece), K_BG);
        {                                         /* its root, its mode, a cursor: as far as they fit left of x */
            const char *rt = EN_NOTE_NAME[proj.par[P_ROOT]], *md = mode_name();
            int32_t r = w + 10;
            if (r + text_w(&FONT_XS, rt) <= x - 4) {
                r = cv_text(r, 8, &FONT_XS, rt, K_TEXT);
                if (r + 3 + text_w(&FONT_XS, md) <= x - 4) {
                    r = cv_text(r + 3, 8, &FONT_XS, md, K_DIM);
                    if (r + 9 <= x - 4 && plat_ms() / 500u & 1u)   /* a terminal's cursor */
                        cv_rect(r + 3, 9, 6, 11, K_ACC);
                }
            }
        }
    }
    if (!vis_over)
        cv_blit(0, 0);
}

/* the garden: the scale as a ring of twelve, the chord lit in it; the four bell loops around it, each
 * a ring of its steps with its notes on it and its playhead going round; the three pulses on the
 * right, the four layers' levels on the left */
#define CX 120
#define CY 84

static void polar(int32_t cx, int32_t cy, float r, int i, int n, int32_t *x, int32_t *y)
{
    float a = (float)i * (FM_TWO_PI / (float)n);  /* step i of n, clockwise from the top */
    float sx = fm_sinf(a) * r, sy = fm_cosf(a) * r;
    *x = cx + (int32_t)(sx + (sx < 0.0f ? -0.5f : 0.5f));
    *y = cy - (int32_t)(sy + (sy < 0.0f ? -0.5f : 0.5f));
}

static const char *chord_now(char *nm)          /* the chord's name, as the garden shows it ("Em7") */
{
    int pcs[EN_NPAD + 1], k;
    pcs[0] = en_chord_root;
    for (k = 0; k < EN_NPAD; k++)
        pcs[k + 1] = en_chord[k] % 12;
    return en_chord_name(pcs, EN_NPAD + 1, nm);
}

/* the Garden's score: a grand staff (G and F clefs), right to left, 16 pixels a second, its reading line at x 176: each
 * note at its place on the staff (a sharp marked beside it), in its voice's colour, a trail while it sounds, bright
 * on the line; ledger lines as it goes past the staves; the chord named above. Staff steps: middle C 35, 3 px each */
#define SC_X0 60
#define SC_X1 176
static int32_t staff_y(int step) { return 140 - (step - 12) * 3; }
static int note_step(int note, int *sharp)
{
    static const uint8_t L[12] = {0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6}, S[12] = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0};
    *sharp = S[note % 12];
    return note / 12 * 7 + L[note % 12];
}
static void draw_score(void)
{
    const uint32_t now = en_now, w = en_score_w;
    const uint16_t VC[EN_NVOX] = {K_PAD, K_BELL, K_BASS};
    const uint16_t line = mix(K_BG, K_DIM, 9);
    uint32_t k;
    int st;
    for (st = 25; st <= 45; st += 2)             /* the staves: G2 .. A3, E4 .. F5 */
        if (st <= 33 || st >= 37)
            cv_rect(SC_X0, staff_y(st), SC_X1 - SC_X0 + 6, 1, line);
    cv_text(SC_X0 - 1, staff_y(43) - 6, &FONT_XS, "G", K_DIM);   /* the clefs, as letters */
    cv_text(SC_X0 - 1, staff_y(31) - 6, &FONT_XS, "F", K_DIM);
    cv_rect(SC_X1, staff_y(47), 1, staff_y(23) - staff_y(47), mix(K_BG, K_ACC, 7));   /* the reading line */
    for (k = 1; k <= EN_SCORE_N && k <= w; k++) {
        const en_note_t *e = &en_score[(w - k) % EN_SCORE_N];
        const uint32_t d0 = now - e->on, d1 = e->off ? now - e->off : 0u;
        const int32_t x0 = d0 > 44100u * 60u ? SC_X0 - 1 : SC_X1 - (int32_t)(d0 * 16u / 44100u),
                      x1 = !e->off ? SC_X1 : d1 > 44100u * 60u ? SC_X0 - 1 : SC_X1 - (int32_t)(d1 * 16u / 44100u);
        const uint16_t c = VC[e->voice < EN_NVOX ? e->voice : 0];
        int sh, s2;
        int32_t y;
        if (!e->on || x1 < SC_X0)                /* (one still sounding stays, however long ago it began) */
            continue;
        st = note_step(e->note, &sh);
        y = staff_y(st);
        if (y < 2 || y > MAIN_H - 4)
            continue;
        if (x1 > (x0 < SC_X0 ? SC_X0 : x0) + 3) {   /* its trail while it sounded */
            cv_rect(x0 < SC_X0 ? SC_X0 : x0, y - 1, x1 - (x0 < SC_X0 ? SC_X0 : x0), 1, mix(K_BG, c, 6));
            cv_rect(x0 < SC_X0 ? SC_X0 : x0, y, x1 - (x0 < SC_X0 ? SC_X0 : x0), 1, mix(K_BG, c, 10));
        }
        if (!e->off) {                           /* still sounding: on the reading line */
            cv_round(SC_X1 - 3, y - 2, 7, 5, 2, c);
            if (sh) {
                cv_rect(SC_X1 - 8, y - 1, 3, 1, c);
                cv_rect(SC_X1 - 7, y - 2, 1, 3, c);
            }
        }
        if (x0 < SC_X0)
            continue;
        for (s2 = 47; s2 <= st; s2 += 2)          /* ledger lines: above the G staff, below the F, middle C */
            cv_rect(x0 - 4, staff_y(s2), 9, 1, line);
        for (s2 = 23; s2 >= st; s2 -= 2)
            cv_rect(x0 - 4, staff_y(s2), 9, 1, line);
        if (st == 35)
            cv_rect(x0 - 4, staff_y(35), 9, 1, line);
        cv_round(x0 - 3, y - 2, 7, 5, 2, mix(K_BG, c, e->off ? 11 : 13));   /* where it began */
        if (sh) {                                /* a sharp: a small cross before it */
            cv_rect(x0 - 8, y - 1, 3, 1, c);
            cv_rect(x0 - 7, y - 2, 1, 3, c);
        }
    }
    if (ui.view == V_SONG && proj.par[P_SG(SONG)] > 0) {   /* the song: where it is */
        const uint8_t *r = song_rec(proj.par[P_SG(SONG)] - 1);
        char t[24];
        if (r) {
            char *q = t;
            q = cat_s(q, "BAR ");
            q = cat_i(q, (int)(en_song_pos / 16u) + 1, 0);
            q = cat_s(q, " / ");
            cat_i(q, ((r[5] << 7 | r[6]) + 15) / 16, 0);
            cv_text(SC_X0, 0, &FONT_XS, t, K_DIM);
        }
    }
    {   /* the chord's name, above the reading line */
        char nm[16];
        chord_now(nm);
        cv_text(SC_X1 - text_w(&FONT_B, nm) / 2, 0, &FONT_B, nm, en_frozen ? K_FROST : K_TEXT);
    }
}

static void draw_meters(void)                    /* left: the three voices, as loud as each is made; the master */
{
    const uint16_t C[4] = {K_PAD, K_BELL, K_BASS, K_TEXT}, T[4] = {K_PAD_T, K_BELL_T, K_BASS_T, K_LINE};
    float lv[4];
    int i;
    static const char *const NM[4] = {"1", "2", "3", "M"};
    for (i = 0; i < EN_NVOX; i++)               /* in dB: -42 .. 0 */
        lv[i] = en_vox_level[i];
    lv[3] = en_master_level;
    for (i = 0; i < 4; i++) {
        const float db = lv[i] > 1e-5f ? 6.0206f * fm_log2f(lv[i]) : -99.0f;
        int32_t x = 10 + i * 11, h = (int32_t)(fm_clampf((db + 42.0f) * (1.0f / 42.0f), 0.0f, 1.0f) * 76.0f + 0.5f);
        cv_round(x, 44, 7, 82, 3, T[i]);
        if (h > 0)
            cv_round(x, 44 + 82 - h - 2, 7, h + 2, 3, C[i]);
        text_c(x + 4, 128, &FONT_XS, NM[i], i == 3 ? K_TEXT : K_DIM);
    }
}

/* a button's card (help_tick): its name, then what it does, in the garden's place. A line starting with a
 * space goes on from the one before (a screen reader joins them: sp_line) */
#define HELP_LINES 4
static const char *const BTN_LABEL[NB] = {"FX", "SEL", "ENV", "LFO", "EDIT", "GLO", "HOME", "SAVE", "ARP", "SEQ",
                                          "PLAY", "REC", "OCT-", "OCT+"};
static int help_lines(int b, const char *l[HELP_LINES])
{
    int n = 0;
#define L(t) (l[n++] = (t))
    switch (b) {
    case B_HOME: L("THE CORTEX"); L("TEMPO, EVOLVE, MOTION, DRIFT"); L("AGAIN: ITS VISUAL"); L("+ SELECT: TEMPO"); break;
    case B_ENV: L("VOICE 1"); L("LEVEL, ROLE, OCTAVE, DENSITY"); L("AGAIN: GEN, PATCH, OP, MOD");
        L("GEN: EDIT NEW, REC VARY"); break;
    case B_ARP: L("VOICE 2"); L("LEVEL, ROLE, OCTAVE, DENSITY"); L("AGAIN: GEN, PATCH, OP, MOD");
        L("GEN: EDIT NEW, REC VARY"); break;
    case B_LFO: L("VOICE 3"); L("LEVEL, ROLE, OCTAVE, DENSITY"); L("AGAIN: GEN, PATCH, OP, MOD");
        L("GEN: EDIT NEW, REC VARY"); break;
    case B_SEQ: L("THE TAPE"); L("SOURCE, DRY/WET, SOS, SPEED"); L("AGAIN: GENE, DECK, ITS MOD,"); L(" KEY SYNC"); break;
    case B_FX: L("THE SPACE"); L("ECHO, ECHO TIME, REVERB,"); L("SIZE"); break;
    case B_GLO: L("SETUP"); L("ROOT, TUNE, MIDI OUT, LIGHTS"); L("AGAIN: MIDI CHANNELS,"); L(" SONG (A MIDI FILE)"); break;
    case B_EDIT: L("THE NEXT CHORD,"); L("ON THE NEXT BAR"); L("TAPE: A NEW SPLICE"); break;
    case B_REC: L("FREEZE: THE CHORD HOLDS"); L(" AND THE REVERB RINGS ON"); L("TAPE: OVERDUB, HELD: ERASE"); break;
    case B_OCTDN: L("THE VOICE SHOWN AN OCTAVE"); L(" DOWN"); L("WITH OCT+, 5 S: UPDATE MODE"); break;
    case B_OCTUP: L("THE VOICE SHOWN AN OCTAVE"); L(" UP"); L("WITH OCT-, 5 S: UPDATE MODE"); break;
    case B_SAVE: L("SAVES ALL EIGHT PIECES"); L("KEEP HOLDING: THE PIECE"); L(" BACK TO ITS RECIPE");
        L("SEL + SAVE: INIT"); break;
    default: break;
    }
#undef L
    return n;
}

static void draw_help(void)
{
    const char *l[HELP_LINES];
    int n = help_lines(ui.help - 1, l), k;
    cv_round(16, 14, 208, 116, 6, K_PANEL);
    cv_round(16, 14, 208, 26, 6, K_TEXT);
    cv_rect(16, 34, 208, 6, K_TEXT);
    text_c(120, 18, &FONT_B, BTN_LABEL[ui.help - 1], K_BG);
    for (k = 0; k < n; k++)
        text_c(120, 56 + k * 20, &FONT_S, l[k] + (l[k][0] == ' '), K_TEXT);
}

/* the reel's pages: a cassette (its two spools winding with the tape: the left full at the reel's start, spokes
 * turning at SPEED, either way), the reel below as a strip (its splices, the one playing bright, the cursor in it,
 * each gene where it reads, the record head), and how much is recorded */
static float tape_ang;                           /* the spools' turn, in 1/24ths */
static int tape_speed(void)                      /* SPEED as the engine has it, in hundredths (+-200) */
{
    const int v = proj.par[P_RL(SPEED)];
    return v >= 73 && v <= 77 ? 100 : v >= 23 && v <= 27 ? -100 : v >= 48 && v <= 52 ? 0 : (v - 50) * 4;
}
static void cv_bigdisc(int32_t cx, int32_t cy, int32_t r, uint16_t c)   /* a filled circle of any radius */
{
    int32_t dy, hw = r;
    for (dy = 0; dy <= r; dy++) {
        while (hw > 0 && hw * hw + dy * dy > r * r)
            hw--;
        cv_rect(cx - hw, cy - dy, 2 * hw + 1, 1, c);
        if (dy)
            cv_rect(cx - hw, cy + dy, 2 * hw + 1, 1, c);
    }
}
static uint32_t grain(uint32_t i) { return hash(0x9E3779B9u, i); }
/* behind the page, its record's look (drawn anew, its colours): Volume 85-92 black and white alone; Volume II's
 * front its rust; drukqs the rows of lit windows of its machines; Syro a list of
 * sums, as its cover; Richard D. James Album its sky; Collapse its warning stripes; the reel (Volume II's back) two of
 * its circles more */
static void draw_cover(int th)
{
    int k;
    switch (th) {
    case 0: {                                    /* a barcode and a catalogue number, the piece's */
        const uint32_t b = hash(0xC0DEu, proj.piece + 1u);
        int32_t x = 198;
        char t[12] = "FM6-00";
        for (k = 0; k < 18 && x < 234; k++) {
            const int32_t w = 1 + (int32_t)(b >> (k % 16 * 2) & 1u) + (k % 5 == 0);
            cv_rect(x, 108, w, 16, K_TEXT);
            x += w + 1 + (int32_t)(b >> (k % 13 + 3) & 1u);
        }
        t[6] = (char)('1' + proj.piece);
        t[7] = 0;
        cv_text(198, 126, &FONT_XS, t, K_DIM);
        break;
    }
    case 2:                                      /* rust: grain */
        for (k = 0; k < 520; k++) {
            const uint32_t g = grain((uint32_t)k), q = grain((uint32_t)k ^ 0x5bd1e995u);
            cv_rect((int32_t)(g % 240u), (int32_t)(q % (uint32_t)MAIN_H), 1 + (int32_t)(q >> 20 & 1u), 1 + (int32_t)(g >> 27 & 1u),
                    mix(K_BG, K_ACC, 2 + (int)(g >> 24 & 3u)));
        }
        break;
    case 3: {                                    /* drukqs: windows lit in rows, as the loops play */
        int r, c;
        for (r = 0; r < 9; r++)
            for (c = 0; c < 3; c++) {
                const int l = (r + c) % EN_NLOOP, on = en_playing && (en_loop_pos[l] + r * 3 + c) % 5 == 0;
                cv_round(194 + c * 13, 22 + r * 12, 10, 8, 2,
                         on ? mix(K_PAD_T, K_ACC, 8 + q16(en_loop_glow[l]) / 2) : mix(K_BG, K_PAD_T, 10));
            }
        break;
    }
    case 4: {                                    /* Syro: a list of sums, right, in its green */
        static const char *const SUM[8] = {"  42.00", " 118.50", "   9.99", "1203.00", "  65.40", " 300.00",
                                           "  17.25", "2440.14"};
        for (k = 0; k < 8; k++)
            cv_text(236 - text_w(&FONT_XS, SUM[k]), 20 + k * 13, &FONT_XS, SUM[k], mix(K_BG, K_DIM, 9));
        cv_rect(186, 125, 50, 1, mix(K_BG, K_DIM, 9));
        break;
    }
    case 5:                                      /* the sky: lighter toward the horizon */
        for (k = 0; k < 12; k++)
            cv_rect(0, k * 12, 240, 12, mix(K_BG, K_PAD_T, k * 16 / 11));
        break;
    case 6: {                                    /* a live hex dump of the settings, scrolling; warning stripes */
        static const char HX[] = "0123456789ABCDEF";
        const uint32_t t0 = plat_ms() / 300u;
        for (k = 0; k < 8; k++) {
            const uint32_t a = (t0 + (uint32_t)k) % P_NPARAMS;
            const uint16_t v = (uint16_t)proj.par[a];
            char t[10];
            t[0] = HX[a >> 8 & 15u], t[1] = HX[a >> 4 & 15u], t[2] = HX[a & 15u], t[3] = ':';
            t[4] = HX[v >> 12 & 15u], t[5] = HX[v >> 8 & 15u], t[6] = HX[v >> 4 & 15u], t[7] = HX[v & 15u], t[8] = 0;
            cv_text(184, 20 + k * 13, &FONT_XS, t, k == 7 ? K_ACC : mix(K_BG, K_DIM, 10));
        }
    }
        cv_rect(0, MAIN_H - 9, 240, 9, K_BG);
        for (k = -9; k < 240; k += 12) {
            int j;
            for (j = 0; j < 6; j++)
                cv_line(k + j, MAIN_H - 1, k + j + 8, MAIN_H - 9, K_ACC);
        }
        break;
    case 1: {                                    /* Volume II's back: two more of its circles, small */
        static const uint8_t SP[3] = {0, 9, 15};
        static const int32_t X[2] = {20, 220};
        const uint16_t INK[2] = {K_PULSE, K_TEXT};
        int c;
        for (c = 0; c < 2; c++) {
            cv_bigdisc(X[c], 18, 11, INK[c]);
            cv_bigdisc(X[c], 18, 10, K_BG);
            for (k = 0; k < 3; k++) {
                int32_t x, y;
                polar(X[c], 18, 10.0f, SP[k] + c * 5, 24, &x, &y);
                cv_line(X[c], 18, x, y, INK[c]);
            }
        }
        break;
    }
    default:
        break;
    }
}
/* over the picture: a CRT's scanlines (every other row a shade toward the ground's other end), registration marks
 * at its corners */
/* a voice's page: its sound on a scope, 120 x 120 around (120, 72): a graticule, the last trace dim (phosphor), this
 * one bright; voice 1 in time, voice 2 against itself a quarter later (Lissajous), voice 3 round a circle */
static int8_t sc_prev[EN_SCOPE_N];
static void scope_pt(int mode, const int8_t *w, int i, int32_t dx, int32_t *x, int32_t *y)
{
    if (mode == 1) {                             /* Lissajous */
        *x = 120 + w[i] * 56 / 127 + dx;
        *y = 72 - w[(i + EN_SCOPE_N / 4) % EN_SCOPE_N] * 56 / 127;
    } else if (mode == 2) {                      /* round a circle */
        polar(120 + dx, 72, 34.0f + (float)w[i] * (22.0f / 127.0f), i, EN_SCOPE_N, x, y);
    } else {                                     /* in time */
        *x = 60 + i * 120 / (EN_SCOPE_N - 1) + dx;
        *y = 72 - w[i] * 54 / 127;
    }
}
static void scope_trace(int mode, const int8_t *w, uint16_t c, int32_t off)
{
    int i;
    int32_t x0 = 0, y0 = 0;
    for (i = 0; i < EN_SCOPE_N; i++) {
        int32_t x, y;
        scope_pt(mode, w, i, off, &x, &y);
        if (i && (mode != 0 || x > x0 - 20))
            cv_line(x0, y0, x, y, c);
        x0 = x, y0 = y;
    }
    if (mode != 0) {                             /* (closed: the last back to the first) */
        int32_t x, y;
        scope_pt(mode, w, 0, off, &x, &y);
        cv_line(x0, y0, x, y, c);
    }
}
static void draw_scope(int v)
{
    const int mode = v;
    const uint16_t grid = mix(K_BG, K_LINE, 10);
    int32_t k;
    char t[16] = "CH1  FM";
    for (k = 0; k <= 8; k++) {                   /* the graticule: dotted, 8 x 8, the axes plain */
        int32_t j;
        for (j = 0; j <= 120; j += 4) {
            cv_rect(60 + k * 15, 12 + j, 1, 1, grid);
            cv_rect(60 + j, 12 + k * 15, 1, 1, grid);
        }
    }
    cv_rect(60, 72, 121, 1, mix(K_BG, K_LINE, 14));
    cv_rect(120, 12, 1, 121, mix(K_BG, K_LINE, 14));
    scope_trace(mode, sc_prev, mix(K_BG, K_ACC, 5), 0);   /* the last, fading */
    scope_trace(mode, en_scope, mix(K_BG, K_ACC, 9), 1);   /* the glow */
    scope_trace(mode, en_scope, K_ACC, 0);
    __builtin_memcpy(sc_prev, en_scope, sizeof sc_prev);
    t[2] = (char)('1' + v);                      /* its label, as a scope's channel readout */
    cv_text(62, -1, &FONT_XS, t, K_DIM);
    cv_text(62, 134 - 12, &FONT_XS, mode == 1 ? "X-Y" : mode == 2 ? "POLAR" : "Y-T", K_DIM);
}
static void draw_screenfx(void)
{
    const uint16_t sl = mix(K_BG, K_TEXT, 1);
    int32_t y;
    int k;
    (void)sl;                                    /* (1.5.1: no scanlines: the screen at its own definition) */
    (void)y;
    {
        static const int32_t X[4] = {4, 236, 4, 236}, Y[4] = {4, 4, MAIN_H - 5, MAIN_H - 5};
        for (k = 0; k < 4; k++) {               /* + at each corner */
            cv_rect(X[k] - 3, Y[k], 7, 1, K_LINE);
            cv_rect(X[k], Y[k] - 3, 1, 7, K_LINE);
        }
    }
}
static void draw_spool(int32_t cx, int32_t cy, int32_t r, int turn, uint16_t ink)
{
    static const uint8_t SPOKE[3] = {0, 7, 16};  /* in 24ths: three spokes, unevenly, as the circles have them */
    int k;
    cv_bigdisc(cx, cy, 28, ink);                 /* the circle: a ring two pixels wide */
    cv_bigdisc(cx, cy, 26, K_BG);
    cv_bigdisc(cx, cy, r, mix(K_BG, ink, 3));    /* the tape wound on it */
    for (k = 0; k < 3; k++) {
        int32_t x1, y1, x2, y2;
        polar(cx, cy, 27.0f, turn + SPOKE[k], 24, &x1, &y1);
        polar(cx + 1, cy, 27.0f, turn + SPOKE[k], 24, &x2, &y2);
        cv_line(cx, cy, x1, y1, ink);
        cv_line(cx + 1, cy, x2, y2, ink);
    }
}
static void draw_tape(void)
{
    const uint32_t len = en_reel_len, n = en_reel_nspl;
    const int org = proj.par[P_RL(ORG)] < (int)n ? proj.par[P_RL(ORG)] : (int)n - 1;
    const float filled = (float)len / (float)EN_REEL_N;
    float at = 0.0f;
    char t[24];
    int k;
    if (n > 0u && org >= 0)                      /* the cursor in the reel: where the spools stand */
        at = ((float)en_reel_s0[org] + en_reel_pos * (float)(en_reel_s1[org] - en_reel_s0[org])) / (float)EN_REEL_N;
    if (en_reel_state)
        at = (float)en_reel_w / (float)EN_REEL_N;
    draw_spool(72, 50, 8 + (int32_t)(18.0f * (1.0f - at)), (int)tape_ang % 24, K_PAD);
    draw_spool(168, 50, 8 + (int32_t)(18.0f * at), (int)tape_ang % 24, K_BASS);
    cv_line(72, 80, 168, 80, K_DIM);             /* the tape between, past the heads */
    cv_round(110, 74, 20, 10, 3, en_reel_state ? K_PULSE : K_DIM);
    {                                            /* the reel as a strip: 16 .. 224 */
        const int32_t x0 = 16, w = 208, y = 100, hgt = 14;
        cv_round(x0, y, w, hgt, 3, K_LINE);
        for (k = 0; k < (int)n; k++) {
            int32_t a = x0 + (int32_t)((float)en_reel_s0[k] * (float)w / (float)EN_REEL_N),
                    b = x0 + (int32_t)((float)en_reel_s1[k] * (float)w / (float)EN_REEL_N);
            if (b < a + 2)
                b = a + 2;
            cv_rect(a, y + 2, b - a - 1, hgt - 4, k == org ? K_BASS : (k & 1 ? K_BASS_T : mix(K_BASS_T, K_BASS, 5)));
        }
        for (k = 0; k < 4; k++)                  /* the genes */
            if (en_reel_gene[k] >= 0) {
                int32_t gx = x0 + (int32_t)((float)en_reel_gene[k] * (float)w / (float)EN_REEL_N);
                cv_rect(gx, y - 3, 2, 4, K_ACC);
            }
        if (n > 0u && !en_reel_state) {          /* the cursor */
            int32_t cxp = x0 + (int32_t)(at * (float)w);
            cv_rect(cxp, y - 2, 1, hgt + 4, K_TEXT);
        }
        if (en_reel_state) {                     /* the record head */
            int32_t rx = x0 + (int32_t)((float)en_reel_w * (float)w / (float)EN_REEL_N);
            cv_rect(rx - 1, y - 3, 3, hgt + 6, K_PULSE);
        }
    }
    {                                            /* recorded / the reel, and what it does */
        const uint32_t ds = (uint32_t)(filled * 118.0f + 0.5f);
        int i = 0;
        itoa_u(ds / 10u, t);
        while (t[i])
            i++;
        t[i++] = '.';
        t[i++] = (char)('0' + ds % 10u);
        t[i] = 0;
        cv_text(16, 120, &FONT_XS, t, K_TEXT);
        cv_text(16 + text_w(&FONT_XS, t) + 2, 120, &FONT_XS, "/ 11.8 s", K_DIM);
        if (en_reel_state)
            text_c(120, 120, &FONT_XS, en_reel_state == 1 ? "RECORDING" : "OVERDUB", K_PULSE);
        else if (n)
            text_c(120, 120, &FONT_XS, proj.par[P_RL(FX)] ? "FX AFTER" : "FX BEFORE", K_DIM);
        else
            text_c(120, 120, &FONT_XS, "EMPTY", K_DIM);
        itoa_u(n, t);
        cv_text(224 - text_w(&FONT_XS, "SPLICES") - 2 - text_w(&FONT_XS, t), 120, &FONT_XS, t, K_TEXT);
        cv_text(224 - text_w(&FONT_XS, "SPLICES"), 120, &FONT_XS, "SPLICES", K_DIM);
    }
}


/* ------------------------------------------------------------ the visual --- */
/* the first page's picture, on the whole screen (Garden, Visual, Video and Shape, but for the Score; the header and
 * the knobs drawn over it, darkened under them): light in a field (120 x 120, a byte a cell, half the screen's
 * resolution) that FEEDBACK turns back into itself and TRAILS fades, MIRROR folds; shown smooth (bilinear) through a
 * palette (COLOUR, turned by HUE, its planes apart by RGB, its rows bent by SYNC); over it, at the screen's own
 * resolution, a GEOMETRY of sharp lines. What it hears: the engine's ear (six bands of the mix, its last 128 samples),
 * each voice's level and the notes as they begin (the score); REACT how much, SPEED how fast it moves */
#define VF_W 120
#define VF_H 120
#define VCX 59.5f
#define VCY 59.5f
#define VIS_SCORE 9
#define VSEG_N 270
static uint8_t vf[VF_H][VF_W] PLAT_POOL;
static uint8_t vtmp[VF_H][VF_W] PLAT_POOL;      /* FEEDBACK: the last frame, turned and zoomed, before it comes back */
static uint16_t vpal[256] PLAT_POOL;            /* (RGB565, not yet swapped: its planes taken apart) */
static uint8_t vline[2][240] PLAT_POOL __attribute__((aligned(4)));         /* (a screen row of the field, smoothed; its colour index) */
static struct {
    float x, y, vx, vy;
    uint8_t on, v;
} vpart[48] PLAT_POOL;
static struct {
    float x, y, r, dr;
    int16_t v;
} vring[16] PLAT_POOL;
static struct {
    float s, a;
    int16_t v;
} vsq[20] PLAT_POOL;
static int8_t vhist[10][VF_W] PLAT_POOL;
static struct {
    int16_t x0, y0, x1, y1;
} vseg[VSEG_N] PLAT_POOL;                       /* GEOMETRY: this frame's lines, screen pixels */
static int vnseg, vpal_key = -1;
static uint8_t vhist_w, vis_ok;
static int8_t vtear[VF_H];                      /* SYNC: each row's tear now (a bass hit) */
static float vchroma[12], vorb[EN_NVOX][2], vt, vbass_prev, vband[EN_NBAND], vlv[EN_NVOX], vgain;
static float vpeak[EN_NBAND], vprev[EN_NBAND], vflux_avg, vbeat, vhue_kick;   /* the ear's: each band's recent peak,
                                                                                * the last frame's, the rise's average,
                                                                                * the beat (1 .. 0), HUE's jump */
static uint32_t vbeat_t;
/* the react matrix: the visual's settings as it reads them this frame (VPAR), each its knob's plus what its slots
 * give: a source (the ear's level, its bands, the beat, a voice, the notes, a chord's change, a slow LFO, a random
 * held on the beats) times its depth across the setting's range; a slot on All moves every setting of a range (not
 * the switches), each a third as much */
static int16_t veff[ST_N];
#define VPAR(k) veff[ST_##k]
static float vnote_p, vchord_p, vsh;
static uint8_t vchord_was = 255;
/* the genome: 16 values (0..1) the pictures, the geometry and the feedback take their shapes from, each drifting to a
 * target of its own (MUTATE: how fast, how far the next), thrown on a beat (CHAOS), held (REC), made anew (EDIT) */
#define VG_N 16
#define G(k, lo, hi) ((lo) + ((hi) - (lo)) * vg[k])
static float vg[VG_N] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f},
             vg_to[VG_N] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
/* MORPH: the picture now (vcur), the one it turns into (vnext, -1 none) and how far (vmw), the time to the next */
static int8_t vcur = -1, vnext = -1, vgeo = -1, vlast = -1, vgeo_now;
static uint8_t vlut[256];                       /* CHROMA: the field's light to its palette index (folded, stepped, keyed) */
static float vmw, vmorph_left;
static int vamp = 256;                          /* (each picture's light, as it fades in or out: 256 whole) */
static uint32_t vseen, vrng = 0x2545F491u;
static float vis_rnd(void)                       /* 0..1 */
{
    vrng ^= vrng << 13;
    vrng ^= vrng >> 17;
    vrng ^= vrng << 5;
    return (float)(vrng >> 8) * (1.0f / 16777216.0f);
}
static int vis_full(void)                        /* the visual on the whole screen now */
{
    return (ui.view == V_GARDEN || (ui.view >= V_VIS && ui.view <= V_XFORM)) && !ui.help &&
           proj.par[P_ST(VMODE)] < VIS_SCORE;   /* (the knob's: the matrix reads it in the frame) */
}
static void vis_palette(void)                    /* COLOUR: five stops, the page's theme or a palette of its own; HUE */
{
    static const uint16_t P[3][5] = {
        {0x0000, 0x6000, 0xC800, 0xFC00, 0xFFE0},   /* Ember: black, red, orange, yellow */
        {0x0000, 0x0010, 0x041F, 0x07FF, 0xFFFF},   /* Ice: navy, blue, cyan, white */
        {0x0000, 0x4810, 0xA014, 0x87E0, 0xEFF0},   /* Acid: purple, magenta, lime */
    };
    const int pi = VPAR(VPAL), hue = VPAR(VHUE) + (vhue_kick > 0.0f),
              key = hue ? -2 : pi * 16 + theme_of(ui.view);   /* (HUE: made again each frame, turned) */
    uint16_t st[5];
    int i;
    if (key == vpal_key && key != -2)
        return;
    vpal_key = key;
    if (pi == 0) {
        st[0] = K_BG, st[1] = TH->pad_t, st[2] = TH->pad, st[3] = TH->bell, st[4] = TH->text;
    } else if (pi == 4) {
        st[0] = K_BG, st[1] = mix(K_BG, TH->text, 4), st[2] = mix(K_BG, TH->text, 9), st[3] = mix(K_BG, TH->text, 13),
        st[4] = TH->text;
    } else {
        for (i = 0; i < 5; i++)
            st[i] = P[pi - 1][i];
    }
    if (hue) {                                   /* HUE: each stop turned round the grey axis, faster as it rises */
        const float a = vt * (float)hue * 0.03f + vhue_kick, c = fm_cosf(a), sn = fm_sinf(a), k = (1.0f - c) / 3.0f,
                    q = sn * 0.57735f;           /* (Rodrigues about (1,1,1)/sqrt 3) */
        for (i = 0; i < 5; i++) {
            const float r = (float)(st[i] >> 11) / 31.0f, gg = (float)(st[i] >> 5 & 63u) / 63.0f,
                        b = (float)(st[i] & 31u) / 31.0f;
            const float r2 = r * (c + k) + gg * (k - q) + b * (k + q), g2 = r * (k + q) + gg * (c + k) + b * (k - q),
                        b2 = r * (k - q) + gg * (k + q) + b * (c + k);
            st[i] = (uint16_t)((uint16_t)(fm_clampf(r2, 0.0f, 1.0f) * 31.0f + 0.5f) << 11 |
                               (uint16_t)(fm_clampf(g2, 0.0f, 1.0f) * 63.0f + 0.5f) << 5 |
                               (uint16_t)(fm_clampf(b2, 0.0f, 1.0f) * 31.0f + 0.5f));
        }
    }
    for (i = 0; i < 256; i++) {                  /* (four spans of 64 steps each, every one its own: no bands) */
        const uint16_t a = st[i >> 6], b = st[(i >> 6) + 1];
        const int t = i & 63;
        vpal[i] = (uint16_t)((((a >> 11) * (64 - t) + (b >> 11) * t + 32) >> 6) << 11 |
                             ((((a >> 5) & 63u) * (64 - t) + ((b >> 5) & 63u) * t + 32) >> 6) << 5 |
                             (((a & 31u) * (64 - t) + (b & 31u) * t + 32) >> 6));
    }
}
static uint8_t (*vdst)[VF_W] = vf;              /* what the pictures draw into: the field, or LAYER's (vtmp) */
static inline void vf_add(int x, int y, int v)
{
    v = (v * vamp) >> 8;
    if ((unsigned)x < VF_W && (unsigned)y < VF_H) {
        const int a = vdst[y][x] + v;
        vdst[y][x] = (uint8_t)(a > 255 ? 255 : a < 0 ? 0 : a);
    }
}
static void vf_addf(float x, float y, int v)    /* v at (x, y) between cells: spread on the four (no stairs) */
{
    const int ix = (int)fm_floorf(x), iy = (int)fm_floorf(y);
    const int fx = (int)((x - (float)ix) * 16.0f), fy = (int)((y - (float)iy) * 16.0f);
    vf_add(ix, iy, (v * (16 - fx) * (16 - fy)) >> 8);
    vf_add(ix + 1, iy, (v * fx * (16 - fy)) >> 8);
    vf_add(ix, iy + 1, (v * (16 - fx) * fy) >> 8);
    vf_add(ix + 1, iy + 1, (v * fx * fy) >> 8);
}
static void vf_disc(float cx, float cy, float r, int v)   /* soft: brightest in the middle */
{
    const int x0 = (int)(cx - r), x1 = (int)(cx + r) + 1, y0 = (int)(cy - r), y1 = (int)(cy + r) + 1;
    int x, y;
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++) {
            const float dx = (float)x - cx, dy = (float)y - cy, d2 = dx * dx + dy * dy, r2 = r * r;
            if (d2 < r2)
                vf_add(x, y, (int)((float)v * (1.0f - d2 / r2)));
        }
}
static void vf_line(float x0, float y0, float x1, float y1, int v)
{
    const float dx = x1 - x0, dy = y1 - y0, n = fm_maxf(fm_fabsf(dx), fm_fabsf(dy));
    int i, k = (int)n + 1;
    for (i = 0; i <= k; i++) {
        const float t = (float)i / (float)k;
        vf_addf(x0 + dx * t, y0 + dy * t, v);
    }
}
static void vf_circle(float cx, float cy, float r, int v)
{
    const int n = 8 + (int)(r * 5.0f);
    int i;
    for (i = 0; i < n; i++) {
        const float a = FM_TWO_PI * (float)i / (float)n;
        vf_addf(cx + r * fm_cosf(a), cy + r * fm_sinf(a), v);
    }
}
static void vseg_add(float x0, float y0, float x1, float y1)
{
    if (vnseg < VSEG_N) {
        vseg[vnseg].x0 = (int16_t)x0, vseg[vnseg].y0 = (int16_t)y0;
        vseg[vnseg].x1 = (int16_t)x1, vseg[vnseg].y1 = (int16_t)y1;
        vnseg++;
    }
}
static float vo_note = 60.0f;                   /* Osc's SYNC Notes: the last note's pitch, eased */
static int vo_note_to = 60;
static void vis_note(int mode, int voice, int note, int vel)   /* a note began: the picture answers it */
{
    int i, k;
    vo_note_to = note;
    if ((unsigned)voice >= EN_NVOX)
        voice = 0;
    if (mode == 0) {                             /* Bloom: a ring from its voice's orb, quicker the higher */
        for (i = 0; i < 16 && vring[i].v > 0; i++)
            ;
        if (i < 16) {
            vring[i].x = vorb[voice][0];
            vring[i].y = vorb[voice][1];
            vring[i].r = 2.0f;
            vring[i].dr = 0.6f + (float)(note - 36) * 0.025f;
            vring[i].v = (int16_t)(60 + vel);
        }
    } else if (mode == 2 || mode == 6) {         /* Prism, Spiral: its pitch class */
        vchroma[note % 12] = fm_minf(vchroma[note % 12] + (float)vel * 0.012f, 1.5f);
    } else if (mode == 3) {                      /* Swarm: sparks from where it sits on the keyboard */
        const int n = 4 + vel / 16;
        for (k = 0, i = 0; i < 48 && k < n; i++)
            if (!vpart[i].on) {
                vpart[i].on = 1;
                vpart[i].x = fm_clampf((float)(note - 30) * (float)VF_W / 72.0f, 1.0f, VF_W - 2.0f) +
                             (vis_rnd() - 0.5f) * 4.0f;
                vpart[i].y = VF_H - 2.0f;
                vpart[i].vx = (vis_rnd() - 0.5f) * 1.2f;
                vpart[i].vy = -(0.8f + (float)vel * 0.02f) * (voice == 2 ? 0.6f : 1.0f) * (0.7f + 0.6f * vis_rnd());
                vpart[i].v = (uint8_t)(120 + vel);
                k++;
            }
    }
}
static void vis_mirror(int m)                    /* MIRROR: the field folded (Mirror, Flip, Quad, Kaleido) */
{
    int x, y;
    if (m == 4)                                  /* Kaleido: the top-left quadrant's lower half its upper's mirror */
        for (y = 0; y < VF_H / 2; y++)
            for (x = 0; x < y; x++)
                vf[y][x] = vf[x][y];
    if (m == 1 || m >= 3)
        for (y = 0; y < VF_H; y++)
            for (x = 0; x < VF_W / 2; x++)
                vf[y][VF_W - 1 - x] = vf[y][x];
    if (m == 2 || m >= 3)
        for (y = 0; y < VF_H / 2; y++)
            memcpy(vf[VF_H - 1 - y], vf[y], VF_W);
}
static void vis_geometry(void)                   /* GEOMETRY's lines, this frame (screen pixels, 240 x 240) */
{
    static int16_t geo_was = -1;
    int geo;
    const int sides = VPAR(VSIDES);
    if (VPAR(VGEO) != geo_was) {       /* (GEOMETRY turned: that one, until MORPH picks another) */
        geo_was = VPAR(VGEO);
        vgeo = -1;
    }
    geo = VPAR(VGEO) && vgeo > 0 && VPAR(VMORPH) ? vgeo : VPAR(VGEO);
    vgeo_now = (int8_t)geo;
    const float lvl = fm_minf(en_master_level * vgain * 2.0f, 1.2f) + 0.4f * vbeat * (float)VPAR(VPULSE) * 0.01f;
    const int w = (int)en_wave_w;
    int i, k;
    vnseg = 0;
    switch (geo) {
    case 1:                                      /* Polygon: three nested, turning against each other */
        for (k = 0; k < 3; k++) {
            const float nr = G(8, 0.4f, 0.85f), r = (70.0f + 30.0f * lvl + 10.0f * vband[0]) * (k == 0 ? 1.0f : k == 1 ? nr : nr * nr),
                        a0 = vt * (k & 1 ? -0.4f : 0.3f) + (float)k * 0.3f;
            for (i = 0; i < sides; i++) {
                const float a = a0 + FM_TWO_PI * (float)i / (float)sides, b = a + FM_TWO_PI / (float)sides;
                vseg_add(120.0f + r * fm_cosf(a), 120.0f + r * fm_sinf(a), 120.0f + r * fm_cosf(b), 120.0f + r * fm_sinf(b));
            }
        }
        break;
    case 2: {                                    /* Lissajous: SIDES against SIDES + 1, its phase drifting */
        const float a = (float)sides * 0.5f, b = a + G(9, 0.25f, 1.5f), ph = vt * 0.3f, amp = 80.0f + 25.0f * lvl;
        float px = 120.0f + amp * fm_sinf(ph), py = 120.0f;
        for (i = 1; i <= 160; i++) {
            const float t = FM_TWO_PI * (float)i / 160.0f,
                        x = 120.0f + amp * (1.0f + 0.15f * vband[2]) * fm_sinf(a * t + ph), y = 120.0f + amp * fm_sinf(b * t);
            vseg_add(px, py, x, y);
            px = x, py = y;
        }
        break;
    }
    case 3: {                                    /* Grid: a floor to the horizon, running towards you */
        const float hz = G(10, 70.0f, 130.0f), run = vt * 0.5f;
        for (i = -8; i <= 8; i++)
            vseg_add(120.0f, hz, 120.0f + (float)i * 40.0f, 240.0f);
        for (k = 0; k < 8; k++) {
            float z = (float)k / 8.0f + run;
            z -= fm_floorf(z);
            {
                const float y = hz + 144.0f * z * z;
                vseg_add(0.0f, y, 239.0f, y);
            }
        }
        for (i = 0; i < 64; i++) {               /* the horizon: the sound's shape */
            const int s0 = en_wave[(unsigned)((int)w - 128 + i * 2) % EN_WAVE_N],
                      s1 = en_wave[(unsigned)((int)w - 128 + i * 2 + 2) % EN_WAVE_N];
            vseg_add((float)i * 3.75f, hz - fm_fabsf((float)s0) * 0.3f * vgain, (float)(i + 1) * 3.75f,
                     hz - fm_fabsf((float)s1) * 0.3f * vgain);
        }
        break;
    }
    case 4: {                                    /* Mandala: the sound's shape, SIDES times round, each fold mirrored */
        float px = 0.0f, py = 0.0f;
        const int n = sides * 20;
        for (i = 0; i <= n; i++) {
            const int f = i % 20, mi = (i / 20) & 1 ? 19 - f : f;
            const int s = en_wave[(unsigned)((int)w - 128 + mi * 5) % EN_WAVE_N];
            const float a = vt * 0.1f + FM_TWO_PI * (float)i / (float)n,
                        r = G(11, 25.0f, 65.0f) + 40.0f * lvl + fm_fabsf((float)s) * G(12, 0.2f, 0.9f) * vgain,
                        x = 120.0f + r * fm_cosf(a), y = 120.0f + r * fm_sinf(a);
            if (i)
                vseg_add(px, py, x, y);
            px = x, py = y;
        }
        break;
    }
    case 5: {                                    /* Scope: the sound, across */
        float px = 0.0f, py = 120.0f;
        for (i = 0; i < 128; i++) {
            const int s = en_wave[(unsigned)((int)w - 128 + i) % EN_WAVE_N];
            const float x = (float)i * (239.0f / 127.0f), y = 120.0f - (float)s * 0.7f * vgain;
            if (i)
                vseg_add(px, py, x, y);
            px = x, py = y;
        }
        break;
    }
    default:
        break;
    }
    {                                            /* (1.5.14) TRANSFORM on the lines too: turned on by ROTATE, carried by
                                                  * SHIFT (round the screen), grown or shrunk by ZOOM */
        static float ga, gx, gy;
        const float sc = fm_exp2f((float)VPAR(VXZOOM) * 0.006f);   /* (x1.5 at 100, x0.66 at -100) */
        float ca, sa, ox, oy;
        const float shang = (float)VPAR(VOANGLE) * (FM_PI / 100.0f);
        if (VPAR(VXTGT) == 0) {                  /* (TARGET Feedback: the lines as drawn) */
            ga = gx = gy = 0.0f;
            return;
        }
        ga += (float)VPAR(VXROT) * 0.0008f;
        ga -= ga > FM_PI ? FM_TWO_PI : ga < -FM_PI ? -FM_TWO_PI : 0.0f;
        gx += (float)VPAR(VXSHX) * 0.04f * fm_cosf(shang);   /* (2 cells a frame, as the feedback's, along ANGLE) */
        gy += (float)VPAR(VXSHX) * 0.04f * fm_sinf(shang);
        gx -= gx >= 120.0f ? 240.0f : gx < -120.0f ? -240.0f : 0.0f;
        gy -= gy >= 120.0f ? 240.0f : gy < -120.0f ? -240.0f : 0.0f;
        if (!VPAR(VXROT) && !VPAR(VXSHX))
            ga = gx = gy = 0.0f;                 /* (the knobs back at 0: the lines as drawn) */
        if (ga == 0.0f && gx == 0.0f && gy == 0.0f && sc == 1.0f)
            return;
        ca = fm_cosf(ga) * sc, sa = fm_sinf(ga) * sc, ox = 120.0f - gx, oy = 120.0f - gy;
        for (i = 0; i < vnseg; i++) {
            const float x0 = (float)vseg[i].x0 - 120.0f, y0 = (float)vseg[i].y0 - 120.0f, x1 = (float)vseg[i].x1 - 120.0f,
                        y1 = (float)vseg[i].y1 - 120.0f;
            vseg[i].x0 = (int16_t)(ox + x0 * ca - y0 * sa), vseg[i].y0 = (int16_t)(oy + x0 * sa + y0 * ca);
            vseg[i].x1 = (int16_t)(ox + x1 * ca - y1 * sa), vseg[i].y1 = (int16_t)(oy + x1 * sa + y1 * ca);
        }
    }
}
static void vis_mutate(float amount)             /* the genome's targets thrown (0..1 how far) */
{
    int k;
    for (k = 0; k < VG_N; k++)
        vg_to[k] = fm_clampf(vg[k] + (vis_rnd() - 0.5f) * 2.0f * amount, 0.0f, 1.0f);
}
static void vis_life(void)                       /* MUTATE, CHAOS: the genome on its way */
{
    const float mu = (float)VPAR(VMUTATE) * 0.01f, ch = (float)VPAR(VCHAOS) * 0.01f;
    int k;
    if (vhold)
        return;
    if (ch > 0.0f && vbeat > 0.95f)              /* (a beat: thrown, as far as CHAOS says) */
        vis_mutate(0.35f * ch);
    for (k = 0; k < VG_N; k++) {
        vg[k] += (vg_to[k] - vg[k]) * (0.004f + 0.04f * mu);
        if (mu > 0.0f && fm_fabsf(vg_to[k] - vg[k]) < 0.02f)   /* there: the next, further the more it mutates */
            vg_to[k] = fm_clampf(vg[k] + (vis_rnd() - 0.5f) * (0.2f + 0.8f * mu), 0.0f, 1.0f);
    }
}
/* MOSH, SORT, CORRUPT, SLICE: the picture broken as a compressed video breaks. MOSH: the field in 8 x 8 blocks, each
 * with a motion vector it keeps (a P-frame with no I-frame): every frame each block carries the last one along its
 * vector, so what was there smears; a beat throws new vectors (the bass: longer), blocks come to rest as their vectors
 * run out. SORT: runs of bright cells along the rows laid out as ramps (pixel sorting; the highs lower its threshold).
 * CORRUPT: blocks copied where they don't belong (a buffer's offset gone wrong), blocks flattened to their mean
 * (macroblocks), the low bits crushed, more on the beats. SLICE: bands of rows shifted for a few frames, the odd rows
 * apart (interlace) */
#define VB_N (VF_W / 8)
static int8_t vmv[VB_N][VB_N][2];               /* MOSH: each block's motion vector (cells a frame) */
static uint8_t vmv_life[VB_N][VB_N];            /* ... frames it keeps it */
static int8_t vchan[VF_H / 8];                  /* SLICE: each band of 8 rows' colour planes apart */
static uint8_t vslice_left;
static void vis_mosh(void)
{
    const float mo = (float)VPAR(VMOSH) * 0.01f, so = (float)VPAR(VSORT) * 0.01f,
                co = (float)VPAR(VCORRUPT) * 0.01f;
    int bx, by, x, y, k;
    if (mo > 0.0f) {
        const int keep = (int)(256.0f * (0.55f + 0.44f * mo));   /* (how much of the carried frame stays) */
        for (by = 0; by < VB_N; by++)            /* new vectors: a beat throws many, the bass makes them long */
            for (bx = 0; bx < VB_N; bx++) {
                if (vmv_life[by][bx])
                    vmv_life[by][bx]--;
                else
                    vmv[by][bx][0] = vmv[by][bx][1] = 0;
                if (vis_rnd() < mo * (0.01f + 0.25f * vbeat)) {
                    const float a = vis_rnd() * FM_TWO_PI, len = 1.0f + (2.0f + 6.0f * vband[0]) * mo * vis_rnd();
                    vmv[by][bx][0] = (int8_t)(len * fm_cosf(a));
                    vmv[by][bx][1] = (int8_t)(len * fm_sinf(a));
                    vmv_life[by][bx] = (uint8_t)(6 + (int)(40.0f * mo * vis_rnd()));
                }
            }
        memcpy(vtmp, vf, sizeof vf);
        for (by = 0; by < VB_N; by++)
            for (bx = 0; bx < VB_N; bx++) {
                const int dx = vmv[by][bx][0], dy = vmv[by][bx][1];
                if (!dx && !dy)
                    continue;
                for (y = by * 8; y < by * 8 + 8; y++)
                    for (x = bx * 8; x < bx * 8 + 8; x++) {
                        int sx = x - dx, sy = y - dy;
                        sx = sx < 0 ? sx + VF_W : sx >= VF_W ? sx - VF_W : sx;
                        sy = sy < 0 ? sy + VF_H : sy >= VF_H ? sy - VF_H : sy;
                        {
                            const int c = vf[y][x], m = (vtmp[sy][sx] * keep) >> 8;
                            vf[y][x] = (uint8_t)(m > c ? m : c);   /* (the carried and the new, the brighter) */
                        }
                    }
            }
    }
    if (so > 0.0f) {                             /* SORT: the bright runs along the rows, as ramps */
        const int th = (int)(250.0f - 190.0f * so - 40.0f * vband[4]), up = ((int)(vt * 0.7f)) & 1;
        for (y = 0; y < VF_H; y++) {
            uint8_t *r = vf[y];
            x = 0;
            while (x < VF_W) {
                int x0, lo = 255, hi = 0;
                while (x < VF_W && r[x] < th)
                    x++;
                x0 = x;
                while (x < VF_W && r[x] >= th) {
                    lo = r[x] < lo ? r[x] : lo;
                    hi = r[x] > hi ? r[x] : hi;
                    x++;
                }
                if (x - x0 >= 4) {
                    const int n = x - x0;
                    for (k = 0; k < n; k++)
                        r[x0 + k] = (uint8_t)(lo + (hi - lo) * ((y + up) & 1 ? k : n - 1 - k) / (n - 1));
                }
            }
        }
    }
    if (co > 0.0f) {                             /* CORRUPT: blocks misplaced, flattened, crushed */
        const int n = (int)(co * (1.0f + 10.0f * vbeat) + vis_rnd());
        for (k = 0; k < n; k++) {
            const int w = 8 << (int)(vis_rnd() * 2.0f), h = 2 + (int)(vis_rnd() * 10.0f),
                      sx = (int)(vis_rnd() * (VF_W - w)), sy = (int)(vis_rnd() * (VF_H - h)),
                      dx = (int)(vis_rnd() * (VF_W - w)), dy = (int)(vis_rnd() * (VF_H - h));
            const float what = vis_rnd();
            for (y = 0; y < h; y++) {
                if (what < 0.5f) {               /* (a buffer's offset: another place's bytes) */
                    for (x = 0; x < w; x++)
                        vf[dy + y][dx + x] = vtmp[sy + y][sx + x];
                } else if (what < 0.8f) {        /* (a macroblock: its mean) */
                    int s = 0;
                    for (x = 0; x < w; x++)
                        s += vf[dy + y][dx + x];
                    memset(&vf[dy + y][dx], s / w, (uint32_t)w);
                } else {                         /* (its low bits crushed) */
                    for (x = 0; x < w; x++)
                        vf[dy + y][dx + x] &= 0xE0u;
                }
            }
        }
    }
}
static void vis_slice(void)                      /* SLICE: bands of rows shifted a while; the odd rows apart */
{
    const float sl = (float)VPAR(VSLICE) * 0.01f;
    int k, y;
    if (sl <= 0.0f)
        return;
    if (vslice_left)
        vslice_left--;
    else if (vis_rnd() < sl * (0.15f + 0.8f * vbeat)) {
        const int bands = 1 + (int)(vis_rnd() * 4.0f * sl);
        for (k = 0; k < bands; k++) {
            const int y0 = (int)(vis_rnd() * VF_H), n = 1 + (int)(vis_rnd() * 14.0f * sl);
            const int8_t o = (int8_t)((vis_rnd() - 0.5f) * 90.0f * sl);
            for (y = y0; y < y0 + n && y < VF_H; y++)
                vtear[y] = o;
        }
        for (k = 0; k < VF_H / 8; k++)
            vchan[k] = (int8_t)(vis_rnd() < sl * 0.5f ? (vis_rnd() - 0.5f) * 12.0f * sl : 0.0f);
        vslice_left = (uint8_t)(2 + (int)(vis_rnd() * 8.0f));
    }
}
static float vis_src(int s)                      /* a react source now, 0..1 */
{
    switch (s) {
    case 1: return fm_minf(en_master_level * vgain * 2.0f, 1.0f);
    case 2: return fm_minf((vband[0] + vband[1]) * 0.5f, 1.0f);
    case 3: return fm_minf((vband[2] + vband[3]) * 0.5f, 1.0f);
    case 4: return fm_minf((vband[4] + vband[5]) * 0.5f, 1.0f);
    case 5: return vbeat;
    case 6: case 7: case 8: return fm_minf(vlv[s - 6], 1.0f);
    case 9: return vnote_p;
    case 10: return vchord_p;
    case 11: return 0.5f + 0.5f * fm_sinf(vt * 0.3f);
    case 12: return vsh;
    default: return 0.0f;
    }
}
static void vis_react(void)                      /* this frame's settings: the knobs, moved by the react slots */
{
    int k, sl;
    for (k = ST_VMODE; k < ST_N; k++)
        veff[k] = proj.par[P_SETP + k];
    vnote_p *= 0.7f;
    vchord_p *= 0.9f;
    if (en_chord_root != vchord_was) {
        vchord_was = en_chord_root;
        vchord_p = 1.0f;
    }
    if (vbeat > 0.95f)
        vsh = vis_rnd();
    for (sl = 0; sl < 4; sl++) {
        const int src = proj.par[P_ST(AR1S) + sl * 3], tg = proj.par[P_ST(AR1T) + sl * 3],
                  dp = proj.par[P_ST(AR1D) + sl * 3];
        float x;
        if (!src || !tg || !dp)
            continue;
        x = vis_src(src) * (float)dp * 0.01f;
        for (k = ST_VMODE; k <= ST_VXTGT; k++) {
            const en_param_t *pi = en_param_info(P_SETP + k);
            const int span = pi->hi - pi->lo;
            int v;
            if (tg < EN_AR_NTGT ? k != ST_VMODE + tg - 1 : span < 10)
                continue;
            v = veff[k] + (int)(x * (float)span * (tg < EN_AR_NTGT ? 1.0f : 0.33f) + (x < 0.0f ? -0.5f : 0.5f));
            veff[k] = (int16_t)(v < pi->lo ? pi->lo : v > pi->hi ? pi->hi : v);
        }
    }
}
static void vis_draw(int mode, float tr);
static void vis_step(int mode)                   /* a frame of the field and the geometry */
{
    static const float BW[EN_NBAND] = {1.0f, 1.4f, 2.0f, 2.8f, 4.0f, 6.0f};
    const float tr = (float)VPAR(VTRAIL) * 0.01f, fb = (float)VPAR(VFEED) * 0.01f,
                syn = (float)VPAR(VSYNC) * 0.01f;
    const int keep = mode == 1 ? 0 : (int)(140.0f + 114.0f * tr);
    int x, y, k;
    if (!vis_ok) {                               /* (the pool isn't cleared at the start) */
        memset(vf, 0, sizeof vf);
        memset(vtmp, 0, sizeof vtmp);
        memset(vpart, 0, sizeof vpart);
        memset(vring, 0, sizeof vring);
        memset(vsq, 0, sizeof vsq);
        memset(vhist, 0, sizeof vhist);
        vis_ok = 1;
    }
    vis_react();
    mode = VPAR(VMODE) < VIS_SCORE ? VPAR(VMODE) : mode;
    vgain = 0.4f + 2.6f * (float)VPAR(VREACT) * 0.01f;
    vt += 0.05f * (0.25f + 1.75f * (float)VPAR(VSPEED) * 0.01f) * G(0, 0.6f, 1.5f);   /* SPEED */
    if (vcur < 0)
        vcur = vlast = (int8_t)mode;
    if (mode != vlast) {                         /* (VISUAL turned, or a key: it turns into that picture, 3 s) */
        vlast = (int8_t)mode;
        if (vcur != mode) {
            vnext = (int8_t)mode;
            vmw = 0.0f;
        }
    }
    if (VPAR(VMORPH) && vnext < 0 && VPAR(VMODE) != VIS_OSC) {   /* (not Osc: a picture made by hand) */   /* MORPH: now and then another picture, the geometry with it */
        vmorph_left -= 0.05f;
        if (vmorph_left <= 0.0f) {
            int n = (int)(vis_rnd() * 8.0f);
            if (n == vcur)
                n = (n + 1) % 8;
            vnext = (int8_t)n;
            vmw = 0.0f;
            vmorph_left = 40.0f - 34.0f * (float)VPAR(VMORPH) * 0.01f + 6.0f * vis_rnd();
            if (VPAR(VGEO))
                vgeo = (int8_t)(1 + (int)(vis_rnd() * 6.0f));
        }
    }
    vis_palette();
    vis_life();
    {                                            /* the ear: each band against its own recent peak (loud and quiet
                                                  * alike), held down in silence; the beats: the bands' rise, the low
                                                  * ones counting most, over 1.6 times its own average */
        float flux = 0.0f;
        const float pres = fm_minf(en_master_level * 30.0f, 1.0f);
        for (k = 0; k < EN_NBAND; k++) {
            const float e = en_bands[k] * BW[k];
            float nb;
            vpeak[k] = fm_maxf(e, fm_maxf(vpeak[k] * 0.993f, 1e-4f));
            nb = e / vpeak[k];
            nb = nb * nb * (3.0f - 2.0f * nb);   /* (a softer knee) */
            vband[k] += (fm_minf(nb * pres * vgain * 0.55f + e * vgain * 2.0f, 1.5f) - vband[k]) *
                        (1.0f - (float)VPAR(VEAR) * 0.0085f);   /* EAR: smoothed (0 at once) */
            flux += fm_maxf(vband[k] - vprev[k], 0.0f) * (k < 2 ? 1.5f : 1.0f);
            vprev[k] = vband[k];
        }
        if (flux > vflux_avg * 1.6f + 0.08f && plat_ms() - vbeat_t > 120u) {
            vbeat = 1.0f;
            vbeat_t = plat_ms();
            vhue_kick += 0.6f * (float)VPAR(VPULSE) * 0.01f;
        } else {
            vbeat *= 0.75f;
        }
        vflux_avg += (flux - vflux_avg) * 0.08f;
    }
    for (k = 0; k < EN_NVOX; k++)
        vlv[k] = fm_minf(en_vox_level[k] * vgain * 2.0f, 1.5f);
    if (fb > 0.0f && keep) {                     /* FEEDBACK: the last frame back in, zoomed in and turned (the bass
                                                  * pushes it), fading by TRAILS */
        const float pu = (float)VPAR(VPULSE) * 0.01f, fl = (float)VPAR(VFLOW) * 0.01f,
                    z = (1.0f - 0.07f * fb * G(1, -0.6f, 1.4f) - 0.03f * fb * vband[0] - 0.08f * pu * vbeat) *
                        (1.0f - (float)(VPAR(VXTGT) != 1 ? VPAR(VXZOOM) : 0) * 0.0006f),   /* TRANSFORM's ZOOM (TARGET
                                                  * Feedback or Both): up to 6 % a frame, in or out */
                    an = fb * (0.05f * fm_sinf(vt * 0.37f) + 0.02f * vband[1] + G(2, -0.05f, 0.05f)) +
                         (float)(VPAR(VXTGT) != 1 ? VPAR(VXROT) : 0) * 0.0008f,   /* ROTATE: up to 0.08 rad a frame */
                    ca = fm_cosf(an) * z, sa = fm_sinf(an) * z, ff = G(3, 0.04f, 0.22f), fa = 3.0f * fl * (0.5f + vband[2]);
        /* SHIFT, along Osc's ANGLE: 2 cells a frame at 100 (TARGET Feedback or Both) */
        const float sh = VPAR(VXTGT) != 1 ? (float)VPAR(VXSHX) * 0.02f : 0.0f,
                    shang = (float)VPAR(VOANGLE) * (FM_PI / 100.0f), vshx = sh * fm_cosf(shang), vshy = sh * fm_sinf(shang);
        static int8_t flx[VF_H] PLAT_POOL, fly[VF_W] PLAT_POOL;   /* FLOW: the swirl the feedback is carried by
                                                                    * (sixteenths of a cell) */
        for (y = 0; y < VF_H; y++)
            flx[y] = (int8_t)fm_clampf(16.0f * fa * fm_sinf((float)y * ff + vt * 1.3f), -127.0f, 127.0f);
        for (x = 0; x < VF_W; x++)
            fly[x] = (int8_t)fm_clampf(16.0f * fa * fm_cosf((float)x * ff - vt * 1.1f), -127.0f, 127.0f);
        for (y = 0; y < VF_H; y++)
            for (x = 0; x < VF_W; x++) {
                const float dx = (float)x - VCX, dy = (float)y - VCY;
                const float fx = VCX + dx * ca - dy * sa + (float)flx[y] * 0.0625f - vshx,
                            fy = VCY + dx * sa + dy * ca + (float)fly[x] * 0.0625f - vshy;   /* (between
                                                                                                    * four cells) */
                const int sx = (int)fm_floorf(fx), sy = (int)fm_floorf(fy), wx = (int)((fx - (float)sx) * 256.0f),
                          wy = (int)((fy - (float)sy) * 256.0f);
                int v = 0;
                if (sx >= 0 && sy >= 0 && sx + 1 < VF_W && sy + 1 < VF_H) {
                    const int t0 = vf[sy][sx] * (256 - wx) + vf[sy][sx + 1] * wx,
                              t1 = vf[sy + 1][sx] * (256 - wx) + vf[sy + 1][sx + 1] * wx;
                    v = (t0 * (256 - wy) + t1 * wy) >> 16;
                }
                vrng = vrng * 1664525u + 1013904223u;
                vtmp[y][x] = (uint8_t)((v * keep + (int)(vrng >> 24)) >> 8);
            }
        memcpy(vf, vtmp, sizeof vf);
    } else {
        for (y = 0; y < VF_H; y++)               /* TRAILS: what was drawn fades (rounded at random: no steps) */
            for (x = 0; x < VF_W; x++) {
                vrng = vrng * 1664525u + 1013904223u;
                vf[y][x] = (uint8_t)((vf[y][x] * keep + (int)(vrng >> 24)) >> 8);
            }
    }
    if (en_score_w - vseen > EN_SCORE_N)
        vseen = en_score_w - EN_SCORE_N;
    while (vseen != en_score_w) {                /* the notes begun since the last frame */
        const en_note_t *e = &en_score[vseen % EN_SCORE_N];
        vseen++;
        if (e->on) {
            vnote_p = 1.0f;
            vis_note(mode, e->voice, e->note, e->vel);
            if (VPAR(VLAYER) - 1 != mode && VPAR(VLAYER))
                vis_note(VPAR(VLAYER) - 1, e->voice, e->note, e->vel);
        }
    }
    mode = vcur;
    if (vnext >= 0) {                            /* turning into the next: each at its share of the light */
        vmw += 0.05f / 3.0f;                     /* (3 s) */
        if (vmw >= 1.0f) {
            vcur = vnext;
            mode = vcur;
            vnext = -1;
            vmw = 0.0f;
        }
    }
    vamp = vnext >= 0 ? (int)(256.0f * (1.0f - vmw)) : 256;
    vis_draw(mode, tr);
    if (vnext >= 0) {
        memset(vtmp, 0, sizeof vtmp);
        vdst = vtmp;
        vamp = (int)(256.0f * vmw);
        vis_draw(vnext, tr);
        vdst = vf;
        for (y = 0; y < VF_H; y++)
            for (x = 0; x < VF_W; x++) {
                const int o = vf[y][x] + vtmp[y][x];
                vf[y][x] = (uint8_t)(o > 255 ? 255 : o);
            }
    }
    vamp = 256;
    {                                            /* LAYER: a second picture, drawn alone, blended in by BLEND, MIX */
        const int ly = VPAR(VLAYER) - 1, bl = VPAR(VBLEND);
        const int m = (int)(VPAR(VMIX) * 256 / 100);
        if (ly >= 0 && ly != mode) {
            memset(vtmp, 0, sizeof vtmp);
            vdst = vtmp;
            vis_draw(ly, tr);
            vdst = vf;
            for (y = 0; y < VF_H; y++)
                for (x = 0; x < VF_W; x++) {
                    const int a = vf[y][x], b = (vtmp[y][x] * m) >> 8;
                    int o;
                    switch (bl) {
                    case 1: o = 255 - (((255 - a) * (255 - b)) >> 8); break;   /* Screen */
                    case 2: o = a > b ? a : b; break;                           /* Max */
                    case 3: o = a > b ? a - b : b - a; break;                   /* Difference */
                    case 4: o = (a * (b + ((256 - m) * 255 >> 8))) >> 8; break;  /* Mask: the layer lets it through */
                    default: o = a + b; break;                                  /* Add */
                    }
                    vf[y][x] = (uint8_t)(o > 255 ? 255 : o);
                }
        }
    }
    if (vbeat > 0.05f && VPAR(VPULSE)) {   /* PULSE: the field flashed on the beat */
        const int fl = (int)(60.0f * vbeat * (float)VPAR(VPULSE) * 0.01f);
        for (y = 0; y < VF_H; y++)
            for (x = 0; x < VF_W; x++) {
                const int v = vf[y][x] + ((vf[y][x] * fl) >> 7);
                vf[y][x] = (uint8_t)(v > 255 ? 255 : v);
            }
    }
    vis_mirror(VPAR(VMIRROR));
    vis_geometry();
    {                                            /* the geometry into the field too, when it is to be broken: the
                                                  * blocks smear it, SORT runs it, CORRUPT misplaces it */
        const int br = VPAR(VMOSH) + VPAR(VSORT) + VPAR(VCORRUPT),
                  tf = VPAR(VFEED) && VPAR(VXTGT) && (VPAR(VXZOOM) || VPAR(VXROT) || VPAR(VXSHX));   /* (and with
                                                  * FEEDBACK and a TRANSFORM: its echoes carried by it, 1.5.14) */
        if (br > 0 || tf) {
            const int v = 60 + (br > 150 ? 150 : br) + (tf ? 60 : 0);
            int i;
            vdst = vf;
            vamp = 256;
            for (i = 0; i < vnseg; i++)
                vf_line((float)vseg[i].x0 * 0.5f, (float)vseg[i].y0 * 0.5f, (float)vseg[i].x1 * 0.5f,
                        (float)vseg[i].y1 * 0.5f, v);
        }
    }
    vis_mosh();
    vis_slice();
    if (syn > 0.0f && vband[0] > 0.45f && vband[0] > vbass_prev * 1.4f)   /* SYNC: a bass hit tears bands of rows */
        for (k = 0; k < 3; k++) {
            const int y0 = (int)(vis_rnd() * VF_H), n = 2 + (int)(vis_rnd() * 6.0f);
            const int8_t o = (int8_t)((vis_rnd() - 0.5f) * 40.0f * syn);
            for (y = y0; y < y0 + n && y < VF_H; y++)
                vtear[y] = o;
        }
    vbass_prev = vband[0];
}
/* Osc (1.5.9): LZX's ramps and oscillators, Hydra's osc() and modulate(): a wave across the field at ANGLE, FREQ
 * cycles across it, moving by SCROLL; a second (FREQ 2) square to it, met by COMBINE: added, multiplied, their
 * difference, or bending the first (Warp; Field: the picture as it is bends it, its feedback with it), WARP how far.
 * SYNC Notes: the notes' pitch its frequency (an octave up, twice the stripes); Beat: from the top on the beat. In
 * 16.16 phase, a table a wave: integer work, row by row */
static uint8_t vosin[256] PLAT_POOL, vonz[33] PLAT_POOL;   /* the sine, the noise's 32 points (the pool: made at the
                                                 * first frame); the other waves worked out */
static uint8_t vowave_ok;
static inline int osc_w(int w, int i)            /* wave w at i (a cycle in 256): 0 .. 255 */
{
    i &= 255;
    switch (w) {
    case 1: return i < 128 ? i * 2 : (255 - i) * 2;   /* Tri */
    case 2: return i;                                 /* Saw */
    case 3: return i < 128 ? 255 : 0;                 /* Square */
    case 4: return (vonz[i >> 3] * (8 - (i & 7)) + vonz[(i >> 3) + 1] * (i & 7)) >> 3;   /* Noise: 32 bumps, smoothed */
    default: return vosin[i];                         /* Sine */
    }
}
static uint32_t vo_ph1, vo_ph2;
static uint8_t vo_beat_was;
static inline float vo_atan2(float y, float x)   /* (an arctangent within 0.01 rad: enough for a picture) */
{
    const float ax = fm_fabsf(x), ay = fm_fabsf(y), a = fm_minf(ax, ay) / (fm_maxf(ax, ay) + 1e-9f), s = a * a;
    float r = ((-0.0464964749f * s + 0.15931422f) * s - 0.327622764f) * s * a + a;
    if (ay > ax)
        r = 1.57079637f - r;
    if (x < 0.0f)
        r = 3.14159274f - r;
    return y < 0.0f ? -r : r;
}
/* Osc's radial waves (WAVE 5 .. 7): its phase at (x, y) in a cycle's 256ths: Rings, out from the middle; Polygon, the
 * same on SIDES sides (Hydra's shape(), turned by ANGLE); Spiral, the rings wound into SIDES arms */
static int vo_radial(int w, int x, int y, float f1, float ang, int n, uint32_t ph)
{
    const float dx = (float)x - VCX, dy = (float)y - VCY, r = fm_sqrtf(dx * dx + dy * dy) * (1.0f / (float)VF_W);
    float q = r;
    if (w != 5) {
        const float th = vo_atan2(dy, dx) - ang;
        if (w == 6) {                           /* (the polygon's distance: its sides' apothem across the sector) */
            const float sec = FM_TWO_PI / (float)n, t = th - sec * fm_floorf(th / sec) - sec * 0.5f;
            q = r * fm_cosf(FM_PI / (float)n) / fm_cosf(t);
        } else {                                /* (n arms: the angle a turn of n cycles) */
            q = r + th * (float)n / (FM_TWO_PI * fm_maxf(f1, 0.1f));
        }
    }
    return (int)((q * f1 + (float)ph * (1.0f / 65536.0f)) * 256.0f) & 255;
}
static void vis_osc(void)
{
    const float ang = (float)VPAR(VOANGLE) * (FM_PI / 100.0f) + (vg[2] - 0.5f) * 0.6f,   /* (LIFE's genome turns it, */
                sc = (float)VPAR(VOSPEED) * 0.01f * (0.25f + 1.75f * (float)VPAR(VSPEED) * 0.01f) * 0.5f;   /* SPEED
                                                  * paces it, as the other pictures) */
    float f1 = 0.5f * fm_exp2f((float)VPAR(VOFREQ) * 0.06f), f2 = VPAR(VOFREQ2) ? 0.5f * fm_exp2f((float)VPAR(VOFREQ2) * 0.06f)
                                                                                : 0.0f;
    const int w = VPAR(VOSHAPE) < 8 ? VPAR(VOSHAPE) : 0, comb = VPAR(VOCOMB), wa = VPAR(VOWARP) * 2,
              ns = VPAR(VSIDES) < 3 ? 3 : VPAR(VSIDES),
              keep = VPAR(VFEED) * 255 / 100;   /* (drawn clean; FEEDBACK how much of the last frame stays under it) */
    int x, y;
    if (!vowave_ok) {                            /* the five waves, a cycle in 256 */
        uint32_t r = 0x9E3779B9u;
        int i;
        for (i = 0; i < 32; i++) {
            r = r * 1664525u + 1013904223u;
            vonz[i] = (uint8_t)(r >> 24);
        }
        vonz[32] = vonz[0];
        for (i = 0; i < 256; i++)
            vosin[i] = (uint8_t)(128.0f + 127.0f * fm_sinf(FM_TWO_PI * (float)i / 256.0f));
        vowave_ok = 1;
    }
    if (VPAR(VOSYNC) == 1) {                     /* Notes: the pitch the frequency (eased, as a slew) */
        vo_note += ((float)vo_note_to - vo_note) * 0.3f;
        f1 *= fm_exp2f((vo_note - 60.0f) * (1.0f / 12.0f));
        if (f2 > 0.0f)
            f2 *= fm_exp2f((vo_note - 60.0f) * (1.0f / 12.0f));
    } else if (VPAR(VOSYNC) == 2 && vbeat > 0.95f && !vo_beat_was) {
        vo_ph1 = vo_ph2 = 0;                     /* Beat: from the top */
    }
    vo_beat_was = vbeat > 0.95f;
    f1 = fm_minf(f1 * G(1, 0.8f, 1.25f), 60.0f);   /* (and LIFE's genome breathes in its frequencies) */
    f2 = fm_minf(f2 * G(3, 0.8f, 1.25f), 60.0f);
    vo_ph1 += (uint32_t)(int32_t)(sc * 0.2f * 65536.0f);   /* SCROLL: up to a fifth of a cycle a frame */
    vo_ph2 -= (uint32_t)(int32_t)(sc * 0.14f * 65536.0f);
    {
        const float ca = fm_cosf(ang), sa = fm_sinf(ang);
        const int32_t dx1 = (int32_t)(ca * f1 * 65536.0f / (float)VF_W), dy1 = (int32_t)(sa * f1 * 65536.0f / (float)VF_W),
                      dx2 = (int32_t)(-sa * f2 * 65536.0f / (float)VF_W), dy2 = (int32_t)(ca * f2 * 65536.0f / (float)VF_W);
        for (y = 0; y < VF_H; y++) {
            int32_t p1 = (int32_t)vo_ph1 - (int32_t)(VCX * (float)dx1) + (int32_t)(((float)y - VCY) * (float)dy1),
                    p2 = (int32_t)vo_ph2 - (int32_t)(VCX * (float)dx2) + (int32_t)(((float)y - VCY) * (float)dy2);
            for (x = 0; x < VF_W; x++, p1 += dx1, p2 += dx2) {
                const int b = f2 > 0.0f ? osc_w(w, (int)((uint32_t)p2 >> 8 & 255u)) : 128;
                int i1 = w >= 5 ? vo_radial(w, x, y, f1, ang, ns, vo_ph1) : (int)((uint32_t)p1 >> 8 & 255u), v;
                if (comb == 3 && f2 > 0.0f)       /* Warp: the second bends the first */
                    i1 += ((b - 128) * wa) >> 6;
                else if (comb == 4)               /* Field: the picture as it is bends it (Hydra's modulate(o0)) */
                    i1 += (vf[y][x] * wa) >> 6;
                {
                    const int a = osc_w(w, i1);
                    v = f2 <= 0.0f || comb >= 3 ? a : comb == 0 ? (a + b) >> 1 : comb == 1 ? (a * b) >> 8
                        : a > b ? a - b : b - a;
                }
                v = (v * vamp) >> 8;
                {
                    const int old = (vdst[y][x] * keep) >> 8;
                    vdst[y][x] = (uint8_t)(v > old ? v : old);
                }
            }
        }
    }
}
static void vis_draw(int mode, float tr)         /* picture mode into vdst (the field, or LAYER's) */
{
    int x, y, i, k;
    if (mode == VIS_OSC) {
        vis_osc();
        return;
    }
    switch (mode) {
    case 0:                                      /* Bloom: an orb a voice, wandering, as big as it is loud; rings */
        for (k = 0; k < EN_NVOX; k++) {
            const float of = G(4, 0.4f, 2.0f), orr = G(5, 22.0f, 52.0f);
            vorb[k][0] = VCX + orr * fm_sinf(vt * (0.21f + 0.07f * (float)k) * of + (float)k * 2.1f);
            vorb[k][1] = VCY + orr * fm_sinf(vt * (0.17f + 0.05f * (float)k) * of * G(6, 0.5f, 2.0f) + (float)k * 1.3f);
            vf_disc(vorb[k][0], vorb[k][1], (3.0f + 18.0f * vlv[k]) * G(7, 0.5f, 1.6f), (int)(30.0f + 110.0f * vlv[k]));
        }
        for (k = 0; k < EN_NBAND; k++)           /* the bands: sparks around the middle, higher further out */
            if (vband[k] > 0.25f && vis_rnd() < vband[k]) {
                const float a = vis_rnd() * FM_TWO_PI, r = 6.0f + (float)k * 9.0f;
                vf_addf(VCX + r * fm_cosf(a), VCY + r * fm_sinf(a), 200);
            }
        for (i = 0; i < 16; i++)
            if (vring[i].v > 0) {
                vf_circle(vring[i].x, vring[i].y, vring[i].r, vring[i].v / 2);
                vring[i].r += vring[i].dr;
                vring[i].v = (int16_t)(vring[i].r > 90.0f ? 0 : vring[i].v - 5);
            }
        break;
    case 1: {                                    /* Ridge: the sound's last moments, stacked, the nearest in front */
        const int n = 6 + (int)(4.0f * tr), w = (int)en_wave_w;
        for (x = 0; x < VF_W; x++) {
            const int s = en_wave[(unsigned)(w - 128 + x * 128 / VF_W) % EN_WAVE_N];
            vhist[vhist_w % 10][x] = (int8_t)fm_clampf((float)s * vgain, -127.0f, 127.0f);
        }
        vhist_w++;
        for (k = n - 1; k >= 0; k--) {           /* the oldest at the back (top), drawn first */
            const int8_t *hl = vhist[(vhist_w - 1u - (uint32_t)k) % 10u];
            const float base = G(4, 14.0f, 40.0f) + (float)(n - 1 - k) * (G(5, 60.0f, 100.0f) / (float)n),
                        sc = (0.15f + 0.25f * (float)(n - k) / (float)n) * G(6, 0.5f, 1.8f);
            const int vv = 70 + 185 * (n - k) / n;
            int py = -1;
            for (x = 4; x < VF_W - 4; x++) {
                const float win = fm_sinf(FM_PI * (float)(x - 4) / (float)(VF_W - 8)), wv = win * win;
                const int yy = (int)(base - fm_fabsf((float)hl[x]) * sc * wv);
                int j;
                for (j = yy + 1 < 0 ? 0 : yy + 1; j < VF_H; j++)   /* (what is behind it, hidden) */
                    vdst[j][x] = 0;
                if (py >= 0)
                    for (j = yy < py ? yy : py; j <= (yy < py ? py : yy); j++)
                        vf_add(x, j, vv);
                else
                    vf_add(x, yy, vv);
                py = yy;
            }
        }
        break;
    }
    case 2: {                                    /* Prism: twelve spokes, the notes sounding; the bands, rings */
        const float a0 = vt * 0.15f;
        for (k = 0; k < 12; k++) {
            const float a = a0 + FM_TWO_PI * (float)k / 12.0f, len = 6.0f + G(4, 25.0f, 60.0f) * vchroma[k];
            if (vchroma[k] > 0.02f)
                vf_line(VCX + 5.0f * fm_cosf(a), VCY + 5.0f * fm_sinf(a), VCX + len * fm_cosf(a), VCY + len * fm_sinf(a),
                        (int)(40.0f + 120.0f * vchroma[k]));
            vchroma[k] *= 0.94f;
        }
        for (k = 0; k < EN_NBAND; k++)
            if (vband[k] > 0.05f)
                vf_circle(VCX, VCY, 8.0f + (float)k * G(5, 4.0f, 11.0f) + vband[k] * 8.0f, (int)(20.0f + 130.0f * vband[k]));
        break;
    }
    case 3: {                                    /* Swarm: the sparks rise; the bass pushes them out, the highs stir */
        const float push = (vband[0] + vband[1]) * 0.08f * G(4, 0.3f, 2.0f), stir = (vband[3] + vband[4]) * G(5, 0.1f, 1.5f),
                    lvl = fm_minf(en_master_level * vgain * 2.0f, 1.0f);
        if (vis_rnd() < 0.2f + 0.8f * lvl)       /* (1.5.9: and sparks from the mix as it sounds, not only the notes) */
            for (i = 0; i < 48; i++)
                if (!vpart[i].on) {
                    vpart[i].on = 1;
                    vpart[i].x = 4.0f + vis_rnd() * (VF_W - 8.0f);
                    vpart[i].y = VF_H - 2.0f;
                    vpart[i].vx = (vis_rnd() - 0.5f) * 1.0f;
                    vpart[i].vy = -(0.6f + 1.6f * lvl) * (0.7f + 0.6f * vis_rnd());
                    vpart[i].v = (uint8_t)(110 + (int)(120.0f * lvl));
                    break;
                }
        for (i = 0; i < 48; i++)
            if (vpart[i].on) {
                vpart[i].vx += (vpart[i].x < VCX ? -push : push) + fm_sinf(vpart[i].y * 0.2f + vt * 3.0f) * stir * 0.1f;
                vpart[i].vx *= 0.97f;
                vpart[i].x += vpart[i].vx;
                vpart[i].y += vpart[i].vy;
                vpart[i].vy *= 0.995f;
                if (vpart[i].y < 0.0f || vpart[i].x < 0.0f || vpart[i].x >= VF_W || vpart[i].v < 8) {
                    vpart[i].on = 0;
                    continue;
                }
                vf_disc(vpart[i].x, vpart[i].y, 2.6f, vpart[i].v);   /* (a spark, and its streak) */
                vf_line(vpart[i].x - vpart[i].vx * 6.0f, vpart[i].y - vpart[i].vy * 6.0f, vpart[i].x, vpart[i].y,
                        vpart[i].v / 2);
                vpart[i].v = (uint8_t)(vpart[i].v - 1);
            }
        for (x = 0; x < VF_W; x += 2)            /* the ground glows with the mix */
            vf_add(x, VF_H - 1, (int)(160.0f * fm_minf(en_master_level * vgain * 2.0f, 1.0f)));
        break;
    }
    case 4: {                                    /* Tunnel: squares from the middle, as loud as the mix, turned by the chord */
        const float lvl = fm_minf(en_master_level * vgain * 2.5f, 1.2f), rot = (float)en_chord_root * (FM_PI / 6.0f);
        for (i = 0; i < 20 && vsq[i].v > 0; i++)
            ;
        if (i < 20 && lvl > 0.03f && (int)(vt * 20.0f + 0.5f) % 3 == 0) {
            vsq[i].s = 2.0f;
            vsq[i].a = rot + vt * 0.1f;
            vsq[i].v = (int16_t)(20.0f + 150.0f * lvl);
        }
        for (i = 0; i < 20; i++)
            if (vsq[i].v > 0) {
                const int ns = 3 + (int)(vg[4] * 5.99f);   /* (its sides: the genome's) */
                float px[8], py[8];
                for (k = 0; k < ns; k++) {
                    const float a = vsq[i].a + (float)k * (FM_TWO_PI / (float)ns) + FM_PI / 4.0f;
                    px[k] = VCX + vsq[i].s * fm_cosf(a);
                    py[k] = VCY + vsq[i].s * fm_sinf(a);
                }
                for (k = 0; k < ns; k++)
                    vf_line(px[k], py[k], px[(k + 1) % ns], py[(k + 1) % ns], vsq[i].v / 3);
                vsq[i].s *= G(5, 1.03f, 1.10f) + 0.06f * vband[0];
                vsq[i].v = (int16_t)(vsq[i].s > 110.0f ? 0 : vsq[i].v - 3);
            }
        break;
    }
    case 5: {                                    /* Plasma: four waves crossing, their scales and speeds the bands' */
        static int8_t sx[VF_W] PLAT_POOL, sy[VF_H] PLAT_POOL, sd[VF_W + VF_H] PLAT_POOL, se[VF_W + VF_H] PLAT_POOL;
        const float k1 = (0.06f + 0.05f * vband[0]) * G(4, 0.4f, 2.2f), k2 = (0.07f + 0.05f * vband[2]) * G(5, 0.4f, 2.2f),
                    k3 = (0.04f + 0.04f * vband[4]) * G(6, 0.4f, 2.2f),
                    lvl = 0.35f + 0.65f * fm_minf(en_master_level * vgain * 2.0f, 1.0f);
        for (x = 0; x < VF_W; x++)
            sx[x] = (int8_t)(127.0f * fm_sinf((float)x * k1 + vt * 1.1f));
        for (y = 0; y < VF_H; y++)
            sy[y] = (int8_t)(127.0f * fm_sinf((float)y * k2 - vt * 0.9f));
        for (i = 0; i < VF_W + VF_H; i++) {
            sd[i] = (int8_t)(127.0f * fm_sinf((float)i * k3 + vt * 0.7f));
            se[i] = (int8_t)(127.0f * fm_sinf((float)(i - VF_H) * k3 * 1.3f - vt * 0.5f));
        }
        for (y = 0; y < VF_H; y++)
            for (x = 0; x < VF_W; x++) {
                const int s = sx[x] + sy[y] + sd[x + y] + se[x - y + VF_H];   /* (four sines, 8 bits each) */
                const int v = ((int)((float)(s + 508) * 0.251f * lvl) * vamp) >> 8;
                if (v > vdst[y][x])
                    vdst[y][x] = (uint8_t)(v > 255 ? 255 : v);
            }
        break;
    }
    case 7: {                                    /* XOR: the cells' coordinates XORed, scaled and scrolled by the
                                                  * genome; two of its bit planes lit, one by the lows, one by the
                                                  * highs (LZX's logic, munching squares) */
        const int sx = 1 + (int)(vg[4] * 3.99f), sy = 1 + (int)(vg[5] * 3.99f), t = (int)(vt * 20.0f * G(6, 0.3f, 2.0f)),
                  p = 2 + (int)(vg[7] * 3.99f),
                  la = (int)(60.0f + 190.0f * fm_minf((vband[0] + vband[1] + vband[2]) * 0.5f, 1.0f)),
                  lb = (int)(120.0f * fm_minf((vband[3] + vband[4] + vband[5]) * 0.5f, 1.0f));
        for (y = 0; y < VF_H; y++)
            for (x = 0; x < VF_W; x++) {
                const int xv = ((x * sx + t) ^ (y * sy - t / 2)) & 255;
                int v = (xv >> p & 1 ? la : 0) + (xv >> (p + 2) & 1 ? lb : 0);
                v = (v * vamp) >> 8;
                vdst[y][x] = (uint8_t)(v > 255 ? 255 : v);   /* (drawn afresh: its pattern, not its trail) */
            }
        break;
    }
    default: {                                   /* Spiral: three arms out from the middle, each ring a band, the notes
                                                  * brightening their pitch class's turn */
        const float tw = G(4, 0.04f, 0.2f) + 0.05f * vband[1];
        const int arms = 2 + (int)(vg[5] * 4.99f);
        for (k = 0; k < arms; k++)
            for (i = 2; i < 84; i++) {
                const float r = (float)i * 0.75f, a = vt * 0.4f + r * tw + FM_TWO_PI * (float)k / (float)arms,
                            bnd = vband[(i / 14) % EN_NBAND], ch = vchroma[(i / 7) % 12];
                const int v = (int)(40.0f + 150.0f * bnd + 80.0f * ch);
                vf_addf(VCX + r * fm_cosf(a), VCY + r * fm_sinf(a), v);
            }
        for (k = 0; k < 12; k++)
            vchroma[k] *= 0.95f;
        break;
    }
    }
}
/* screen rows y0 .. y0 + h - 1 of the field, smooth (each screen pixel between four cells), through the palette:
 * RGB its planes apart (wider with the highs), SYNC its rows bent and torn; then the geometry's lines over it */
static int16_t vrow_o[MAIN_H];                  /* the band's rows' offsets (SYNC, SLICE): the sharp lines move with
                                                 * them */
static void vis_seg(int x0, int yy0, int x1, int yy1, uint16_t c, int y0, int h)   /* a sharp line, in the band */
{
    const int dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = yy1 > yy0 ? yy1 - yy0 : yy0 - yy1, sx = x0 < x1 ? 1 : -1,
              sy = yy0 < yy1 ? 1 : -1;
    int err = dx - dy, n = dx + dy + 1;
    if ((yy0 < y0 && yy1 < y0) || (yy0 >= y0 + h && yy1 >= y0 + h))
        return;
    while (n-- > 0) {
        if (yy0 >= y0 && yy0 < y0 + h) {
            int xs = x0 + vrow_o[yy0 - y0];
            xs = xs < 0 ? xs + 240 : xs >= 240 ? xs - 240 : xs;
            if (xs < 239) {
                uint16_t *p = cv_px + (uint32_t)(yy0 - y0) * 240u + (uint32_t)xs;
                p[0] = p[1] = c;
            }
        }
        if (x0 == x1 && yy0 == yy1)
            break;
        {
            const int e2 = 2 * err;
            if (e2 > -dy) {
                err -= dy;
                x0 += sx;
            }
            if (e2 < dx) {
                err += dx;
                yy0 += sy;
            }
        }
    }
}
static void vis_band(int y0, int h)
{
    const float syn = (float)VPAR(VSYNC) * 0.01f, rgb = (float)VPAR(VRGB) * 0.01f;
    const int sp = (int)(2.0f * rgb * (1.0f + 3.0f * vband[4] + 2.0f * vband[5]) + 0.5f);
    const uint16_t gc = swap16(vpal[236]);
    const int ps = 1 + (int)((float)VPAR(VPIXEL) * 0.09f * (1.0f + vband[0])),   /* PIXEL: its blocks, cells */
              dith = !VPAR(VPOSTER);
    int y, x, i;
    if (y0 == 0) {                               /* (once a frame) CHROMA's map: KEY, COLORAMA, POSTER; Rutt dims */
        const float fo = (float)VPAR(VFOLD) * 0.01f, po = (float)VPAR(VPOSTER) * 0.01f;
        const int kth = VPAR(VKEY) * 2, mul = 1 + (int)(fo * 7.0f), off = (int)(vt * 40.0f * fo),
                  lv = 2 + (int)((1.0f - po) * 14.0f), step = 255 / (lv - 1);
        for (i = 0; i < 256; i++) {
            int v = i;
            if (fo > 0.0f) {                     /* (folded back on itself: a triangle) */
                const int m = (v * mul + off) & 511;
                v = m < 256 ? m : 511 - m;
            }
            if (po > 0.0f)
                v = (v + step / 2) / step * step;
            if (i < kth)
                v = 0;
            if (vgeo_now == 6)
                v = v * 3 / 10;                  /* (Rutt: the picture under its lines) */
            v = v * (VPAR(VBRIGHT) + 50) / 100;  /* BRIGHT */
            vlut[i] = (uint8_t)(v > 255 ? 255 : v);
        }
    }
    for (y = y0; y < y0 + h; y++) {
        const int fy2 = y - 1, ya = fy2 < 0 ? 0 : fy2 >> 1, yb = ya + 1 < VF_H ? ya + 1 : VF_H - 1,
                  wb = fy2 < 0 ? 0 : (fy2 & 1) ? 3 : 1;   /* (screen row y sits a quarter or three quarters between) */
        const uint8_t *a = vf[ya], *b = vf[yb];
        uint8_t *col = vline[0], *ln = vline[1];
        uint16_t *out = cv_px + (uint32_t)(y - y0) * 240u;
        const int fi = y >> 1 < VF_H ? y >> 1 : VF_H - 1,
                  o = 2 * ((int)(syn * 5.0f * fm_sinf((float)fi * 0.19f + vt * 2.3f) + 0.5f) + vtear[fi]) +
                      ((fi & 1) ? (int)((float)VPAR(VSLICE) * 0.06f * (vband[3] + vbeat)) : 0),   /* (interlace) */
                  spr = sp + vchan[fi >> 3];
        vrow_o[y - y0] = (int16_t)o;
        if (ps > 1) {                            /* PIXEL: blocks of ps cells (Fairlight's mosaic) */
            const uint8_t *q = vf[fi / ps * ps];
            for (x = 0; x < VF_W; x++)
                ln[2 * x] = ln[2 * x + 1] = q[x / ps * ps];
        } else {
            for (x = 0; x < VF_W; x++)
                col[x] = (uint8_t)((a[x] * (4 - wb) + b[x] * wb) >> 2);
            for (x = 0; x < VF_W; x++) {         /* (and across: a quarter towards each side's neighbour) */
                const int l = col[x > 0 ? x - 1 : 0], r = col[x + 1 < VF_W ? x + 1 : VF_W - 1], c = col[x];
                ln[2 * x] = (uint8_t)((3 * c + l) >> 2);
                ln[2 * x + 1] = (uint8_t)((3 * c + r) >> 2);
            }
        }
        static const int8_t BAYER[4][4] = {{-6, 2, -4, 4}, {6, -2, 8, 0}, {-3, 5, -5, 3}, {9, 1, 7, -1}};
        const int8_t *dr = BAYER[y & 3];
        if (ps <= 1) {                           /* (sharpened along the row: what the smoothing softened) */
            memcpy(col, ln, 240);
            for (x = 1; x < 239; x++) {
                const int v = ln[x] + (2 * col[x] - col[x - 1] - col[x + 1]) / 3;
                ln[x] = (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v);
            }
        }
        for (x = 0; x < VF_W * 2; x++) {         /* (dithered between the palette's steps, unless POSTER; CHROMA) */
            const int d = ln[x] + (dith && ln[x] > 8 ? dr[x & 3] / 2 : 0);
            ln[x] = vlut[d < 0 ? 0 : d > 255 ? 255 : d];
        }
        for (x = 0; x < 240; x++) {
            int xg = x - o, xr, xb;
            xg = xg < 0 ? xg + 240 : xg >= 240 ? xg - 240 : xg;
            xr = xg - spr;
            xb = xg + spr;
            xr = xr < 0 ? xr + 240 : xr >= 240 ? xr - 240 : xr;
            xb = xb < 0 ? xb + 240 : xb >= 240 ? xb - 240 : xb;
            xr = xr < 0 ? xr + 240 : xr;
            xb = xb >= 240 ? xb - 240 : xb;
            out[x] = swap16((vpal[ln[xr]] & 0xF800u) | (vpal[ln[xg]] & 0x07E0u) | (vpal[ln[xb]] & 0x001Fu));
        }
        if ((y & 1) && y >> 1 < VF_H)
            vtear[y >> 1] = (int8_t)(vtear[y >> 1] * 3 / 4);
    }
    {                                            /* the geometry: sharp, at the screen's resolution, two pixels wide;
                                                  * broken as the picture is */
        const int mo = VPAR(VMOSH), co = VPAR(VCORRUPT), q = ps > 1 ? ps * 2 : 1;
        const uint16_t gcc = swap16(vpal[vlut[236]]);
        uint32_t hs = (uint32_t)(vt * 20.0f) * 2654435761u;   /* (the same throws in every band of a frame) */
        for (i = 0; i < vnseg; i++) {
            int xa = vseg[i].x0, ya = vseg[i].y0, xb = vseg[i].x1, yb = vseg[i].y1;
            hs = hs * 1664525u + 1013904223u;
            if (co && (int)(hs >> 25) < co / 3) {    /* CORRUPT: thrown somewhere else, or gone */
                if (hs >> 16 & 1u)
                    continue;
                {
                    const int dx = (int)(hs >> 8 & 63u) - 32, dy = (int)(hs >> 14 & 31u) - 16;
                    xa += dx, xb += dx, ya += dy, yb += dy;
                }
            }
            if (mo) {                            /* MOSH: each end carried by its block's vector */
                const int k = 1 + mo / 25;
                if (xa >= 0 && ya >= 0 && xa < 240 && ya < 240) {
                    const int8_t *m = vmv[ya >> 4][xa >> 4];
                    xa += m[0] * k, ya += m[1] * k;
                }
                if (xb >= 0 && yb >= 0 && xb < 240 && yb < 240) {
                    const int8_t *m = vmv[yb >> 4][xb >> 4];
                    xb += m[0] * k, yb += m[1] * k;
                }
            }
            if (q > 1) {                         /* PIXEL: its ends on the blocks' grid */
                xa = xa / q * q, ya = ya / q * q, xb = xb / q * q, yb = yb / q * q;
            }
            vis_seg(xa, ya, xb, yb, VPAR(VFOLD) || VPAR(VPOSTER) ? gcc : gc, y0, h);
        }
    }
    if (vgeo_now == 6) {                         /* Rutt: 32 scanlines, each lifted by the picture's light under it
                                                  * (the Rutt/Etra scan processor), coloured by it */
        const float lift = 0.10f + 0.12f * vgain;
        int j;
        for (j = 0; j < 32; j++) {
            const int by = 10 + j * 7, fy = by >> 1 < VF_H ? by >> 1 : VF_H - 1;
            int px = 0, py = by - (int)((float)vf[fy][0] * lift);
            for (x = 4; x < 240; x += 4) {
                const int b = vf[fy][x >> 1], yy = by - (int)((float)b * lift);
                vis_seg(px, py, x, yy, swap16(vpal[b < 64 ? 64 + b : b]), y0, h);
                px = x, py = yy;
            }
        }
    }
}
static void vis_dim(int h)                       /* the band's rows darkened (the header, the knobs: read over it) */
{
    uint32_t i, n = (uint32_t)h * 240u;
    for (i = 0; i < n; i++) {
        const uint16_t c = swap16(cv_px[i]);
        cv_px[i] = swap16((uint16_t)(((c >> 2) & 0x39E7u) + ((c >> 3) & 0x18E3u)));   /* (to 3/8: read over it) */
    }
}
/* the keys on the visual's pages: F3 .. B3 a picture each (it turns into it, 2 s), C4 .. F4 the geometries, F#4 ..
 * A#4 the folds, B4 .. D#5 the palettes, E5 a nudge, F5 a throw, F#5 a new genome, G5 held or free */
static void vis_key(int k)
{
    static const char *const VMN[7] = {"BLOOM", "RIDGE", "PRISM", "SWARM", "TUNNEL", "PLASMA", "SPIRAL"};
    if (k < 7) {
        proj.par[P_ST(VMODE)] = (int16_t)k;   /* (the next frame turns into it) */
        say("MORPH TO", VMN[k]);
    } else if (k < 13) {
        proj.par[P_ST(VGEO)] = (int16_t)(k - 7);
        vgeo = -1;
        say("GEOMETRY", k == 7 ? "OFF" : "ON");
    } else if (k < 18) {
        proj.par[P_ST(VMIRROR)] = (int16_t)(k - 13);
        say("MIRROR", k == 13 ? "OFF" : "ON");
    } else if (k < 23) {
        proj.par[P_ST(VPAL)] = (int16_t)(k - 18);
        say("COLOUR", "CHANGED");
    } else if (k < 25) {
        vis_mutate(k == 23 ? 0.15f : 0.5f);
        vhold = 0;
        say("MUTATE", k == 23 ? "A NUDGE" : "A THROW");
    } else if (k == 25) {
        vis_mutate(1.0f);
        vhold = 0;
        say("VISUAL", "NEW STRUCTURE");
    } else {
        vhold = (uint8_t)!vhold;
        say("STRUCTURE", vhold ? "HELD" : "FREE");
    }
    mark_dirty();
}
/* ------------------------------------------------------------ the start --- */
/* CORTEX's start (3 s, or until a button): static that settles, the gate of horn opening (in the Odyssey true dreams
 * come through the gate of horn, false ones through ivory) on an eye that opens, the name torn into place, the three
 * Oneiroi beneath it */
#define SPLASH_MS 3200u
static uint32_t splash_t0;
static uint8_t splash_on;
static int32_t sp_by, sp_bh;                    /* the band being drawn */
static inline void sp_px(int32_t x, int32_t y, uint16_t c)
{
    if ((uint32_t)x < 240u && y >= sp_by && y < sp_by + sp_bh)
        cv_px[(uint32_t)(y - sp_by) * 240u + (uint32_t)x] = swap16(c);
}
static uint16_t sp_mix(uint16_t a, uint16_t b, float t) { return mix(a, b, q16(t)); }
static void sp_band(int32_t by, int32_t bh, float t)
{
    static const uint16_t NIGHT = 0x0806, VIOLET = 0x611A, HORN = 0xE5A8, IVORY = 0xF7BB;
    int32_t x, y;
    uint32_t r = 0x9E3779B9u ^ (uint32_t)(t * 977.0f);
    sp_by = by, sp_bh = bh;
    cv_begin(240, (uint32_t)bh, NIGHT);
    {                                            /* static, thinning as it settles */
        const float dens = fm_clampf(1.0f - t * 2.2f, 0.0f, 1.0f);
        for (y = by; y < by + bh; y++) {
            for (x = 0; x < 240; x += 2) {
                r = r * 1664525u + 1013904223u;
                if ((float)(r >> 24) < dens * 150.0f)
                    sp_px(x, y, sp_mix(NIGHT, VIOLET, (float)(r >> 16 & 255u) / 255.0f));
            }
        }
    }
    if (t > 0.2f) {                              /* the eye opening: two lids, an iris of rings */
        const float o = fm_clampf((t - 0.2f) * 2.2f, 0.0f, 1.0f), cy = 104.0f;
        int32_t k;
        for (x = 30; x < 210; x++) {
            const float u = ((float)x - 120.0f) / 90.0f, lid = 38.0f * o * (1.0f - u * u);
            sp_px(x, (int32_t)(cy - lid), HORN);
            sp_px(x, (int32_t)(cy + lid), HORN);
            for (y = (int32_t)(cy - lid) + 1; y < (int32_t)(cy + lid); y++) {
                const float dx = (float)x - 120.0f, dy = (float)y - cy, d = fm_sqrtf(dx * dx + dy * dy);
                if (d < 30.0f * o) {
                    const int ring = (int)(d * 0.4f + t * 12.0f) & 1;
                    sp_px(x, y, d < 10.0f * o ? NIGHT : ring ? sp_mix(VIOLET, IVORY, 1.0f - d / 30.0f) : VIOLET);
                }
            }
        }
        for (k = 0; k < 9; k++) {                /* lashes */
            const float a = -2.6f + (float)k * 0.65f, x0 = 120.0f + 92.0f * fm_cosf(a) * 0.9f;
            const float y0 = cy - 30.0f * o * fm_sinf(-a) * 0.9f;
            cv_line((int32_t)x0, (int32_t)y0 - sp_by, (int32_t)(x0 + 8.0f * fm_cosf(a)),
                    (int32_t)(y0 - 10.0f * o) - sp_by, HORN);
        }
    }
    if (t > 0.45f) {                             /* the name, its rows torn into place */
        const float g = fm_clampf(1.0f - (t - 0.45f) * 3.0f, 0.0f, 1.0f);
        const char *nm = "CORTEX";
        const int32_t w = text_w(&FONT_L, nm), x0 = 120 - w / 2, y0 = 156;
        cv_oy = -sp_by;
        (void)g;
        cv_text(x0, y0, &FONT_L, nm, IVORY);   /* (1.5.3: still, no torn copy) */
        if (t > 0.65f) {
            const char *s = "MORPHEUS  PHOBETOR  PHANTASOS";
            cv_text(120 - text_w(&FONT_XS, s) / 2, 196, &FONT_XS, s, HORN);
            cv_text(120 - text_w(&FONT_XS, "FM-1  1.5") / 2, 216, &FONT_XS, "FM-1  1.5", VIOLET);
        }
        cv_oy = 0;
    }
    cv_blit(0, (uint32_t)by);
}
static int draw_splash(void)                     /* the start: 1 while it plays */
{
    const uint32_t e = plat_ms() - splash_t0;
    float t;
    if (!splash_on)
        return 0;
    if (e >= SPLASH_MS || ui.btn) {
        splash_on = 0;
        ui.sig[0] = ui.sig[1] = ui.sig[2] = 0;
        return 0;
    }
    if (ui.frame % 2u)
        return 1;
    t = (float)e / (float)SPLASH_MS;
    sp_band(0, HDR_H, t);
    sp_band(MAIN_Y, MAIN_H, t);
    sp_band(KNB_Y, KNB_H, t);
    return 1;
}
static void draw_header(void);
static void draw_knobs(void);
static void draw_vis_screen(void)                /* the whole screen: the visual, the header and knobs over it */
{
    char nm[16];
    const uint32_t idle = plat_ms() - ui.act_t;
    const int bars = proj.par[P_ST(VBARS)],
              hide = plat_ms() >= ui.msg_until &&
                     (bars == 2 ? idle > 1500u : bars == 1 ? idle > (uint32_t)proj.par[P_ST(VDELAY)] * 1000u : 0);
    vis_step(proj.par[P_ST(VMODE)]);
    ui.sig[0] = ui.sig[1] = ui.sig[2] = 0;       /* (the bands drawn afresh when it leaves) */
    vis_over = 1;
    cv_begin(240, HDR_H, K_BG);
    vis_band(0, HDR_H);
    if (!hide) {                                 /* BARS: the header and the knobs, or the picture alone */
        vis_dim(HDR_H);
        cv_rect(0, HDR_H - 1, 240, 1, K_LINE);
        draw_header();
    }
    cv_blit(0, 0);
    cv_begin(240, MAIN_H, K_BG);
    vis_band(MAIN_Y, MAIN_H);
    if (!hide)
        cv_text(6, 4, &FONT_XS, chord_now(nm), TH->text);   /* the chord, and the way to the next */
    if (hide) {
    } else if (en_frozen) {
        cv_text(232 - text_w(&FONT_XS, "held"), 4, &FONT_XS, "held", K_FROST);
    } else {
        int32_t w = 30, f = (int32_t)(en_drift * (float)w + 0.5f);
        cv_round(232 - w, 8, w, 6, 3, K_LINE);
        if (f > 0)
            cv_round(232 - w, 8, f < 6 ? 6 : f, 6, 3, K_PAD);
    }
    cv_blit(0, MAIN_Y);
    cv_begin(240, KNB_H, K_BG);
    vis_band(KNB_Y, KNB_H);
    if (!hide) {
        vis_dim(KNB_H);
        cv_rect(0, 0, 240, 1, K_LINE);
        draw_knobs();
    }
    cv_blit(0, KNB_Y);
    vis_over = 0;
}

static void draw_main(void)
{
    uint32_t h = hash(2166136261u, pool_abs() | (uint32_t)proj.par[P_ROOT] << 12 | (uint32_t)en_frozen << 16 | proj.piece << 24 |
                                        (uint32_t)en_playing << 17 | (uint32_t)proj.par[P_MODE] << 20), i;
    if (ui.frame % 3u)                           /* the garden moves slowly: 20 frames a second is plenty */
        return;
    h = hash(h, (uint32_t)theme_of(ui.view) ^ ui_lsig);
    if (theme_of(ui.view) == 6)                 /* (Setup's hex dump scrolling) */
        h = hash(h, plat_ms() / 300u);
    en_scope_v = (int8_t)view_vox(ui.view);      /* (the scope: the voice shown, each new trace) */
    if (en_scope_v < 0)                          /* (the score: scrolling, a pixel each 1/16 s; the song's bar) */
        h = hash(h, en_now / 2756u ^ en_score_w << 20 ^ en_song_pos << 4);
    if (en_scope_v >= 0)
        h = hash(h, en_scope_seq);
    if (ui.help) {
        h = hash(0x48454C50u, ui.help | (uint32_t)theme_of(ui.view) << 8);
        if (h != ui.sig[1]) {
            ui.sig[1] = h;
            cv_begin(240, MAIN_H, K_BG);
            draw_help();
            cv_blit(0, MAIN_Y);
        }
        return;
    }
    if (tape_view(ui.view)) {                   /* the reel's pages: the reel */
        if (en_playing && en_reel_nspl)
            tape_ang += (float)tape_speed() * 0.006f;
        if (en_reel_state)
            tape_ang += 0.6f;
        while (tape_ang < 0.0f)
            tape_ang += 24.0f;
        while (tape_ang >= 240.0f)
            tape_ang -= 240.0f;
        h = hash(0x5441504Eu, (uint32_t)tape_ang | en_reel_state << 8 | en_reel_nspl << 10 |
                                  (uint32_t)proj.par[P_RL(ORG)] << 14 | (uint32_t)proj.par[P_RL(FX)] << 18);
        h = hash(h, en_reel_len ^ en_reel_w << 1 ^ (uint32_t)(en_reel_pos * 200.0f) << 20);
        for (i = 0; i < 4u; i++)
            h = hash(h, (uint32_t)en_reel_gene[i] >> 9);
        if (h == ui.sig[1])
            return;
        ui.sig[1] = h;
        cv_begin(240, MAIN_H, K_BG);
        draw_cover(theme_of(ui.view));
        draw_tape();
        draw_screenfx();
        cv_blit(0, MAIN_Y);
        return;
    }
    for (i = 0; i < EN_NPAD; i++)
        h = hash(h, en_chord[i]);
    for (i = 0; i < EN_NLOOP; i++) {
        uint32_t s;
        h = hash(h, en_loop_pos[i] | (uint32_t)q16(en_loop_glow[i]) << 8);
        for (s = 0; s < EN_LOOP_LEN[i]; s++)
            h = hash(h, (uint32_t)(en_loop_note[i][s] >= 0));
    }
    h = hash(h, (uint32_t)q16(en_reel_level) | (uint32_t)q16(en_master_level) << 5 | (uint32_t)q16(en_vox_level[0]) << 10 |
                    (uint32_t)q16(en_vox_level[1]) << 15 | (uint32_t)q16(en_vox_level[2]) << 20);
    h = hash(h, (uint32_t)(en_drift * 40.0f) | (uint32_t)q16(en_pad_level) << 8 | (uint32_t)q16(en_bass_level) << 13);
    for (i = 0; i < 4; i++)
        h = hash(h, (uint32_t)proj.par[i]);
    if (h == ui.sig[1])
        return;
    ui.sig[1] = h;
    cv_begin(240, MAIN_H, K_BG);
    draw_cover(theme_of(ui.view));
    if (en_frozen) {                             /* the way to the next chord, top right (held: frozen) */
        cv_text(232 - text_w(&FONT_XS, "held"), 4, &FONT_XS, "held", K_FROST);
    } else {
        int32_t w = 30, f = (int32_t)(en_drift * (float)w + 0.5f);
        cv_round(232 - w, 8, w, 6, 3, K_LINE);
        if (f > 0)
            cv_round(232 - w, 8, f < 6 ? 6 : f, 6, 3, K_PAD);
    }
    if (view_vox(ui.view) >= 0)                  /* a voice's pages: its scope; the others: the score */
        draw_scope(view_vox(ui.view));
    else
        draw_score();
    draw_meters();
    draw_screenfx();
    cv_blit(0, MAIN_Y);
}

/* a knob's value in its cell (56 px): spaces run together as one (a DX7 name's "STRINGS  1"), bold, else small,
 * else cut to fit */
static void text_fit(int32_t cx, int32_t y, const char *s, uint16_t c)
{
    char b[16];
    int n = 0, k;
    for (k = 0; s[k] && n < 15; k++)
        if (s[k] != ' ' || (n && b[n - 1] != ' '))
            b[n++] = s[k];
    while (n && b[n - 1] == ' ')
        n--;
    b[n] = 0;
    if (text_w(&FONT_B, b) <= 56) {
        text_c(cx, y, &FONT_B, b, c);
        return;
    }
    while (n > 1 && text_w(&FONT_XS, b) > 57)
        b[--n] = 0;
    text_c(cx, y + 2, &FONT_XS, b, c);
}

/* the knob strip (X0X's): a rule with the page's name and five page dots at its right end; under
 * it a cell a knob: its name, a ring (or pips, for a switch of a few positions), the value. The
 * knob being turned is drawn on a tint. Only redrawn when what it shows changes (the rings cost a
 * square root and an arctangent per pixel). */
static void draw_knobs(void)
{
    uint32_t h = hash(2166136261u, ui.view | (uint32_t)ui.touched << 8), i;
    int now_touch = plat_ms() < ui.touch_until;
    for (i = 0; i < 4; i++)
        h = hash(h, (uint32_t)knob_get(view_knob(ui.view, (int)i)));
    h = hash(h, (uint32_t)now_touch ^ ui_lsig);
    if (!vis_over) {
        if (h == ui.sig[2])
            return;
        ui.sig[2] = h;
        cv_begin(240, KNB_H, K_BG);
    }
    {   /* the rule; at its right end, where you are: the page, then the dots (a voice's own pages on its dot) */
        char t[20], nm[20];
        int k;
        int32_t xd = 237 - V_MAIN * 6, xt;
        view_name(ui.view, nm);
        caps(t, nm, sizeof t);
        xt = xd - 5 - text_w(&FONT_XS, t);
        cv_rect(4, 5, xt - 9, 1, K_LINE);
        cv_text(xt, -1, &FONT_XS, t, K_ACC);
        for (k = 0; k < V_MAIN; k++)              /* (in PAGE_ORDER) */
            cv_round(xd + k * 6, 3, 4, 4, 2, PAGE_ORDER[k] == view_main(ui.view) ? K_ACC : K_LINE);
    }
    for (i = 0; i < 4; i++) {
        int k = view_knob(ui.view, (int)i), lo, hi, v, hot = now_touch && ui.touched == (int)i;
        int32_t x = (int32_t)i * 60, cx = x + 30;
        char t[16];
        uint16_t bg = hot ? K_PANEL : K_BG, fill = hot ? K_ACC : ui_lc[i % 3u];   /* (the chord's colours) */
        if (k == K_NONE)
            continue;
        if (i)
            cv_rect(x, 16, 1, 46, K_LINE);
        if (hot)
            cv_round(x + 3, 11, 54, 57, 6, K_PANEL);
        knob_range(k, &lo, &hi);
        v = knob_get(k);
        caps(t, knob_label(ui.view, (int)i), sizeof t);
        text_c(cx, 12, &FONT_XS, t, (K_BG >> 11) > 16u ? (hot ? 0x8A00u : 0xA440u) : hot ? 0xFFE0u : 0xFEA0u);   /* (the name: yellow, bright when turned; ochre on a light page) */
        if (k != K_CPU && hi - lo < 6) {         /* a switch: its positions as pips */
            int n = hi - lo + 1, j, pw = n > 4 ? 6 : 9, gap = 3, w0 = n * (pw + gap) - gap;
            for (j = 0; j < n; j++)
                cv_round(cx - w0 / 2 + j * (pw + gap), 33, pw, 8, 2, j == v - lo ? fill : K_LINE);
        } else if (k != K_CPU) {
            arc(cx, 37, 12, hi > lo ? (float)(v - lo) / (float)(hi - lo) : 0.0f, K_LINE, fill, bg, lo < 0);
        }
        knob_text(k, t);
        text_fit(cx, 50, t, hot ? K_ACC : K_TEXT);
    }
    if (!vis_over)
        cv_blit(0, KNB_Y);
}

static void leds(void)
{
    uint32_t b = 0, k = 0, i, ap = pool_abs();
    b |= 1u << VIEW_BTN[view_main(ui.view)];
    if (en_playing)
        b |= 1u << B_PLAY;
    if (en_frozen)
        b |= 1u << B_REC;
    if (proj.leds == 3 && vis_ok && vis_full()) {   /* Visual: the keys the picture across them, its middle third:
                                                  * each key its column's brightest, against the frame's; lit,
                                                  * half, dim, off (the buttons glow as On) */
        uint8_t col[32];
        uint32_t kb = 0, km = 0, kd = 0;
        int mx = 1, x, y;
        for (i = 0; i < NKEYS; i++) {
            int m = 0;
            x = (int)((i * VF_W + VF_W / 2) / NKEYS);
            for (y = VF_H / 3; y < VF_H * 2 / 3; y += 2)
                m = vf[y][x] > m ? vf[y][x] : m;
            col[i] = (uint8_t)m;
            mx = m > mx ? m : mx;
        }
        for (i = 0; i < NKEYS; i++) {
            const int l = col[i] * 255 / (mx < 96 ? 96 : mx);   /* (a dark frame stays dark) */
            if (l > 170)
                kb |= 1u << i;
            else if (l > 100)
                km |= 1u << i;
            else if (l > 40)
                kd |= 1u << i;
        }
        plat_lights(b, kb, ~b & ((1u << NB) - 1u), kd, 1);
        plat_lights_mid(0, km);
        return;
    }
    if (proj.leds)
        for (i = 0; i < NKEYS; i++)
            if (ap >> ((5u + i) % 12u) & 1u)
                k |= 1u << i;
    plat_lights_mid(0, 0);
    plat_glow(proj.leds == 1 || proj.leds == 3);
    plat_leds(b, k);
}

/* AUTOSAVE: a few seconds after a change, while resting and quiet, nothing touched (a flash erase
 * silences the audio for a moment: never while the garden grows; SAVE saves then) */
#define AUTOSAVE_QUIET 4000u
static void autosave(void)
{
    uint32_t now = plat_ms();
    if (!ui.dirty || en_playing || ui.btn || ui.keys || now - ui.act_t < AUTOSAVE_QUIET || en_pad_level > 0.01f)
        return;
    if (project_save() == 0) {
        ui.dirty = 0;
        ui.saved_t = now;
    }
}

/* ------------------------------------------------------ screen reader --- */
/* The screen as text for a screen reader on the computer (announce.c; fm1-reader): a page once it settles,
 * with its knobs; a knob's value as it turns; a help card as it comes up; the chord as it moves (queued: the
 * garden moves it by itself, every 20 s at the quickest); growing and resting from MIDI, and a MIDI clock
 * arriving or going; what a button did (say). WHERE (the reader asks, or it has just started listening) is
 * the whole screen in two: the page, its knobs and the header, then the garden (the chord, the scale).
 * Nothing is built while no reader listens. */
static struct {
    uint8_t view, help, ext, started;
    int16_t val[4];
    uint32_t chord;                            /* the chord's name, hashed */
} sp;
static char sp_buf[ANN_MAX + 1];
static uint32_t sp_n;
/* the chord names (en_chord_name's, after the root) in words: "m" and "M7" differ only in capitals, which
 * the words file ignores */
static const char *const CHORD_Q[][2] = {
    {"", "major"}, {"m", "minor"}, {"7", "7"}, {"M7", "major 7"}, {"m7", "minor 7"}, {"6", "6"},
    {"m6", "minor 6"}, {"add9", "add 9"}, {"madd9", "minor add 9"}, {"aug", "augmented"}, {"dim", "diminished"},
    {"dim7", "diminished 7"}, {"m7b5", "half diminished"}, {"mM7", "minor major 7"}, {"sus2", "suspended 2"},
    {"sus4", "suspended 4"}, {"7sus4", "7 suspended 4"}, {"5", "5"},
};
#define NCHORD_Q (sizeof CHORD_Q / sizeof CHORD_Q[0])

static void sp_cat(const char *s)
{
    while (*s && sp_n < ANN_MAX)
        sp_buf[sp_n++] = *s++;
    sp_buf[sp_n] = 0;
}
static void sp_int(uint32_t v)
{
    char t[12];
    itoa_u(v, t);
    sp_cat(t);
}
static int sp_same(const char *a, const char *b)
{
    while (*a && *a == *b)
        a++, b++;
    return *a == *b;
}

/* a value as it is said: Drift in minutes and seconds (the screen's "1:05", "45s"), the rest as shown */
static void sp_value(int k)
{
    char t[16];
    knob_text(k, t);
    if (k == P_DRIFT) {
        uint32_t m = 0, s = 0, i;
        for (i = 0; t[i] >= '0' && t[i] <= '9'; i++)
            s = s * 10u + (uint32_t)(t[i] - '0');
        if (t[i] == ':') {
            m = s;
            s = 0;
            for (i++; t[i] >= '0' && t[i] <= '9'; i++)
                s = s * 10u + (uint32_t)(t[i] - '0');
        }
        if (m) {
            sp_int(m);
            sp_cat(m == 1u ? " minute" : " minutes");
            if (!s)
                return;
            sp_cat(" ");
        }
        sp_int(s);
        sp_cat(s == 1u ? " second" : " seconds");
        return;
    }
    if (k != P_DRIFT) {                          /* "2.7s" 2.7 seconds, "40ms" 40 milliseconds, "+6dB", "440Hz" */
        uint32_t i = 0;
        while (t[i])
            i++;
        if (i >= 2 && t[i - 1] == 's' && t[i - 2] >= '0' && t[i - 2] <= '9') {
            t[i - 1] = 0;
            sp_cat(t);
            sp_cat(" seconds");
            return;
        }
        if (i >= 3 && t[i - 1] == 's' && t[i - 2] == 'm') {
            t[i - 2] = 0;
            sp_cat(t);
            sp_cat(" milliseconds");
            return;
        }
        if (i >= 3 && t[i - 1] == 'B' && t[i - 2] == 'd') {
            t[i - 2] = 0;
            sp_cat(t);
            sp_cat(" dB");
            return;
        }
    }
    sp_cat(t);
}
static void sp_knob(int view, int i)           /* "Swell 60": the name the page gives it, then its value */
{
    sp_cat(knob_label(view, i));
    sp_cat(" ");
    sp_value(view_knob(view, i));
}

static void sp_chord_q(int root, const char *q)   /* "E minor 7" */
{
    uint32_t i;
    sp_cat(EN_NOTE_NAME[root % 12]);
    sp_cat(" ");
    for (i = 0; i < NCHORD_Q; i++)
        if (sp_same(q, CHORD_Q[i][0])) {
            sp_cat(CHORD_Q[i][1]);
            return;
        }
    sp_cat(q);
}
static void sp_chord(void)                     /* the chord in the middle of the garden */
{
    char nm[16];
    const char *q = chord_now(nm), *r = EN_NOTE_NAME[en_chord_root % 12u];
    while (*r && *q == *r)
        q++, r++;
    sp_chord_q(en_chord_root % 12u, q);
}

/* the page: its name and its knobs; all: the header too (the piece, its key, the tempo, growing or resting) */
static void sp_page(int all)
{
    int i;
    sp_n = 0;
    sp_buf[0] = 0;
    {
        char nm[20];
        view_name(ui.view, nm);
        sp_cat(nm);
    }
    sp_cat(" page");
    for (i = 0; i < 4; i++) {
        sp_cat(i ? ", " : ": ");
        sp_knob(ui.view, i);
    }
    if (!all)
        return;
    sp_cat(". Piece ");
    sp_cat(piece_name(proj.piece));
    sp_cat(", ");
    sp_cat(EN_NOTE_NAME[proj.par[P_ROOT]]);
    sp_cat(" ");
    sp_cat(mode_name());
    if (en_ext) {
        sp_cat(", tempo EXT");
    } else if (ui.view != V_GARDEN) {          /* (the Garden's first knob says it) */
        sp_cat(", tempo ");
        sp_int((uint32_t)proj.par[P_TEMPO]);
    }
    sp_cat(sp_tp.playing ? ", growing" : ", resting");
    if (ui.dirty)
        sp_cat(", not saved yet");
    if (ui.view == V_MIDI) {                     /* what comes in: each port's messages, the last note's channel */
        static const char *const PN[NPORTS] = {"USB", "Bluetooth", "jack"};
        sp_cat(". In: USB ");
        sp_int(plat_midi_count(PORT_USB, 0));
        sp_cat(", jack ");
        sp_int(plat_midi_count(PORT_TRS, 0));
        sp_cat(", Bluetooth ");
        sp_int(plat_midi_count(PORT_BLE, 0));
        if (ui.in_ch) {
            sp_cat("; last note channel ");
            sp_int(ui.in_ch);
            sp_cat(" from ");
            sp_cat(PN[ui.in_port < NPORTS ? ui.in_port : 0]);
        }
    }
    if (ui.view == V_SETUP) {                    /* (and the audio's overruns, the notes shed to avoid them) */
        sp_cat(". CPU ");
        sp_int(plat_cpu_pct());
        sp_cat("%, overruns ");
        sp_int(plat_xruns());
        sp_cat(", shed ");
        sp_int(en_sheds);
    }
    if (batt_words(batt())) {                    /* the battery, at the end */
        sp_cat(". ");
        sp_cat(batt_words(batt()));
    }
}

/* the garden: the chord and how far to the next, the scale from the root up ("F and F sharp": one letter's
 * two notes joined, so they are heard as two), the pulse sitting out */
static void sp_garden(void)
{
    uint32_t ap = pool_abs();
    int i, last = -1;
    sp_n = 0;
    sp_buf[0] = 0;
    sp_cat("Chord ");
    sp_chord();
    if (en_frozen) {
        sp_cat(", frozen");
    } else {
        sp_cat(", next chord ");
        sp_int((uint32_t)(fm_clampf(en_drift, 0.0f, 1.0f) * 100.0f + 0.5f));
        sp_cat("% of the way");
    }
    sp_cat(". Scale");
    for (i = 0; i < 12; i++) {
        int pc = (proj.par[P_ROOT] + i) % 12;
        if (ap >> pc & 1u) {
            sp_cat(last < 0 ? " " : EN_NOTE_NAME[last][0] == EN_NOTE_NAME[pc][0] ? " and " : ", ");
            sp_cat(EN_NOTE_NAME[pc]);
            last = pc;
        }
    }
}

/* a help card: its button, then its lines ("ENV button. THE PAD. LEVEL, SWELL, BRIGHT, COLOUR") */
static void sp_line(const char *s)
{
    char p = sp_n ? sp_buf[sp_n - 1] : 0;
    if (*s == ' ')                             /* it goes on from the line before */
        s++;
    else if (p != ',' && p != ':')
        sp_cat(".");
    sp_cat(" ");
    sp_cat(s);
}
static void sp_help(void)
{
    const char *l[HELP_LINES];
    int n = help_lines(ui.help - 1, l), k;
    sp_n = 0;
    sp_buf[0] = 0;
    sp_cat(BTN_LABEL[ui.help - 1]);
    sp_cat(" button");
    for (k = 0; k < n; k++)
        sp_line(l[k]);
}

static void sp_piece_text(void)                /* "PIECE Departures, C# Ionian, tempo 60" */
{
    sp_n = 0;
    sp_buf[0] = 0;
    sp_cat("PIECE ");
    sp_cat(piece_name(proj.piece));
    sp_cat(", ");
    sp_cat(EN_NOTE_NAME[proj.par[P_ROOT]]);
    sp_cat(" ");
    sp_cat(mode_name());
    sp_cat(", tempo ");
    sp_int((uint32_t)proj.par[P_TEMPO]);
}
static void sp_piece(void)
{
    if (!ann_on())
        return;
    sp_piece_text();
    ann_say(sp_buf, 0, ANN_NOW);
    sp_said = 1;
}

static uint32_t sp_chord_sig(void)
{
    char nm[16];
    const char *s = chord_now(nm);
    uint32_t h = 2166136261u;
    while (*s)
        h = hash(h, (uint8_t)*s++);
    return h;
}
static void sp_snap(void)
{
    int i;
    for (i = 0; i < 4; i++)
        sp.val[i] = (int16_t)knob_get(view_knob(ui.view, i));
    sp.view = ui.view;
    sp.help = ui.help;
    sp.ext = en_ext;
    sp.chord = sp_chord_sig();
}

static void speak(void)
{
    uint32_t now = plat_ms(), c;
    int where = ann_where(), i, changed = -1;
    if (!ann_on()) {
        sp.started = 0;
        sp_said = 0;
        return;
    }
    if (!sp.started || where) {                       /* everything on the screen, in two */
        sp.started = 1;
        sp_snap();
        if (ui.help) {
            sp_help();
            ann_say(sp_buf, 0, ANN_NOW);
            sp_page(1);
        } else {
            sp_page(1);
            ann_say(sp_buf, 0, ANN_NOW);
            sp_garden();
        }
        ann_say(sp_buf, 0, 0);
        sp_said = 0;
        return;
    }
    if (sp_said) {                                    /* a button said what it did (and what it changed) */
        sp_said = 0;
        sp_snap();
        return;
    }
    if (en_playing != sp_tp.playing && now - sp_tp.play_t > 150u) {   /* MIDI start / stop */
        sp_tp.playing = en_playing;
        ann_say(en_playing ? "GROWING" : "RESTING", 0, 0);
    }
    if (en_ext != sp.ext) {                           /* a MIDI clock arrived, or stopped */
        sp.ext = en_ext;
        sp_n = 0;
        sp_cat("Tempo ");
        if (en_ext)
            sp_cat("EXT");
        else
            sp_int((uint32_t)proj.par[P_TEMPO]);
        ann_say(sp_buf, 0, 0);
    }
    if (ui.help != sp.help) {                         /* a help card: at once */
        sp.help = ui.help;
        if (ui.help) {
            sp_help();
            ann_say(sp_buf, 0, ANN_NOW);
            return;
        }
    }
    if (ui.view != sp.view) {                         /* a new page: said when it settles */
        sp_snap();
        sp_page(0);
        ann_later(sp_buf, ANN_NOW, 200u, now);
        return;
    }
    for (i = 0; i < 4; i++) {                         /* a knob turning */
        int v = knob_get(view_knob(ui.view, i));
        if (v != sp.val[i]) {
            sp.val[i] = (int16_t)v;
            if (changed < 0)
                changed = i;
        }
    }
    if (changed >= 0) {
        sp_n = 0;
        sp_knob(ui.view, changed);
        ann_later(sp_buf, ANN_NOW, 120u, now);
    }
    c = sp_chord_sig();
    if (c != sp.chord) {                              /* the chord moved: queued, after what is being said */
        sp.chord = c;
        sp_n = 0;
        sp_cat("Chord ");
        sp_chord();
        ann_say(sp_buf, 0, 0);
    }
}

#ifdef PLAT_HOST
/* every page, knob value, help card, chord and message as a screen reader gets them (the "speakdump"
 * command): KIND <tab> the text sent (fm1-reader/tools/audit.mjs puts it in words with web/words/ and checks it) */
static void sp_print(const char *kind, const char *t) { printf("%s\t%s\n", kind, t); }
static void speak_dump(void)
{
    static const char *const MSG[][2] = {{"UPDATE MODE IN", "3.."}, {"UPDATE MODE", "CANCELLED"},
                                         {"NEXT CHORD", "ON THE BAR"}, {"NEXT CHORD", 0}, {"RESTING", 0},
                                         {"GROWING", 0}, {"FREEZE", "ON"}, {"FREEZE", "OFF"}, {"SAVED", 0}};
    static const char *const WARN[][2] = {{"THE ROOT STAYS", "(SETUP)"}, {"3 NOTES", "AT LEAST"},
                                          {"NOT SAVED", "FLASH ERROR"}};
    project_t keep = proj;
    uint8_t keep_view = ui.view, keep_help = ui.help, keep_dirty = ui.dirty, keep_play = sp_tp.playing;
    uint8_t keep_ext = en_ext, keep_frozen = en_frozen;
    int v, i, x, lo, hi;
    char t[48];
    for (v = 0; v < NVIEWS; v++) {
        ui.view = (uint8_t)v;
        sp_page(0);
        sp_print("SCREEN", sp_buf);
        for (i = 0; i < 4; i++) {
            static uint8_t keep_pat[EN_NVOX][EN_PATCH];
            const int k = view_knob(v, i), p = knob_par(k), off = knob_off(k), vx = kvox();
            uint8_t keep_pick[4];
            memcpy(keep_pat, vpatch, sizeof vpatch);
            keep_pick[0] = ui.op[vx], keep_pick[1] = ui.stage[vx], keep_pick[2] = ui.pstage[vx];
            keep_pick[3] = ui.slot[vx];
            knob_range(k, &lo, &hi);
            if (p >= 0 && is_src(p))             /* (the own six: a bank's are the user's) */
                hi = EN_NFACT - 1;
            for (x = lo; x <= hi && k != K_NONE && k != K_CPU; x++) {
                if (p >= 0)
                    proj.par[p] = (int16_t)x;
                else if (off >= 0)
                    vpatch[vx][off] = (uint8_t)x;
                else if (k == K_LEDS)
                    proj.leds = (uint8_t)x;
                else if (k == K_OP)
                    ui.op[vx] = (uint8_t)x;
                else if (k == K_STAGE || k == K_PSTAGE || k == K_SLOT)
                    (k == K_STAGE ? ui.stage : k == K_PSTAGE ? ui.pstage : ui.slot)[vx] = (uint8_t)(x - 1);
                else if (k == K_TSLOT)
                    ui.tslot = (uint8_t)(x - 1);
                sp_n = 0;
                sp_knob(v, i);
                sp_print("KNOB", sp_buf);
            }
            proj = keep;
            memcpy(vpatch, keep_pat, sizeof vpatch);
            ui.op[vx] = keep_pick[0], ui.stage[vx] = keep_pick[1], ui.pstage[vx] = keep_pick[2];
            ui.slot[vx] = keep_pick[3];
        }
    }
    for (x = 0; x < 8; x++) {                         /* WHERE: growing, resting, unsaved, a clock, frozen, custom */
        ui.view = (uint8_t)(x * 3 % NVIEWS);
        sp_tp.playing = (uint8_t)(x & 1);
        ui.dirty = (uint8_t)(x >> 1 & 1);
        en_ext = (uint8_t)(x == 3);
        en_frozen = (uint8_t)(x == 5);
        proj.pool = x == 6 ? 0xFFFu : x == 7 ? 0x91u : keep.pool;
        proj.par[P_ROOT] = (int16_t)(x * 5 % 12);
        sp_page(1);
        sp_print("WHERE", sp_buf);
        sp_garden();
        sp_print("WHERE", sp_buf);
    }
    proj = keep;
    ui.dirty = keep_dirty;
    sp_tp.playing = keep_play;
    en_ext = keep_ext;
    en_frozen = keep_frozen;
    ui.view = keep_view;
    for (i = 0; i < NB; i++) {
        const char *l[HELP_LINES];
        if (!help_lines(i, l))
            continue;
        ui.help = (uint8_t)(i + 1);
        sp_help();
        sp_print("HELP", sp_buf);
    }
    ui.help = keep_help;
    for (x = 0; x < 12; x++)
        for (i = 0; i < (int)NCHORD_Q; i++) {
            sp_n = 0;
            sp_cat("Chord ");
            sp_chord_q(x, CHORD_Q[i][0]);
            sp_print("CHORD", sp_buf);
        }
    for (i = 0; i < (int)(sizeof MSG / sizeof MSG[0]); i++) {
        msg_text(MSG[i][0], MSG[i][1], t);
        sp_print("MSG", t);
    }
    for (i = 0; i < (int)(sizeof WARN / sizeof WARN[0]); i++) {
        msg_text(WARN[i][0], WARN[i][1], t);
        sp_print("WARN", t);
    }
    for (x = 0; x < 12; x++) {
        msg_text("ADDED", EN_NOTE_NAME[x], t);
        sp_print("MSG", t);
        msg_text("REMOVED", EN_NOTE_NAME[x], t);
        sp_print("MSG", t);
    }
    for (x = 0; x < EN_NMODES; x++) {
        msg_text("MODE", EN_MODE_NAME[x], t);
        sp_print("MSG", t);
    }
    for (x = -2; x <= 2; x++) {
        char o[8];
        char oo[16] = "OCTAVE ";
        en_param_text(P_PL(VOX_PAD, PL_OCT), x, oo + 7);
        (void)o;
        msg_text("VOICE 1", oo, t);
        sp_print("MSG", t);
    }
    for (x = 30; x <= 120; x += 45) {
        char o[8];
        itoa_u((uint32_t)x, o);
        msg_text("TEMPO", o, t);
        sp_print("MSG", t);
    }
    for (x = 0; x < NPIECES; x++) {
        project_piece(x);
        sp_piece_text();
        sp_print("MSG", sp_buf);
        msg_text("RESET", piece_name(x), t);
        sp_print("MSG", t);
    }
    proj = keep;
    sp_print("MSG", "Tempo EXT");
    sp_print("MSG", "Tempo 72");
    for (x = 0; x <= 4; x++)                          /* the battery, at the end of WHERE's first part */
        sp_print("WHERE", batt_words(x));
}
#endif

void ui_init(void)
{
    memset(&ui, 0, sizeof ui);
    memset(ui.key_vox, -1, sizeof ui.key_vox);   /* (no key held on a voice) */
    splash_on = 1;                               /* (the start screen) */
    splash_t0 = plat_ms();
    {                                            /* the generators: a pad, a bell, a bass, half bright, half moving */
        static const uint8_t KIND0[EN_NVOX] = {0, 4, 7};
        int v;
        for (v = 0; v < EN_NVOX; v++) {
            ui.gen[v][0] = KIND0[v];
            ui.gen[v][1] = ui.gen[v][2] = 50;
            ui.gen[v][3] = 30;
            ui.gen_seed[v] = (uint32_t)v + 1u;
        }
    }
    ui.touched = -1;
    lcd_fill(0, 0, 240, 240, K_BG);
    if (!en_playing)
        en_play(1);                              /* it grows from the start */
    sp_tp.playing = 1;
    sp_tp.play_t = plat_ms();
}

static void reel_saving(void)                  /* the reel being saved: a sector a frame, how far it is */
{
    int r = project_reel_step(), pc;
    char t[12];
    if (r == 2) {
        say("SAVED", "PIECES AND REEL");
    } else if (r < 0) {
        warn("REEL NOT SAVED", "FLASH ERROR");
    } else if (r == 1 && (pc = project_reel_progress()) >= 0 && ui.frame % 8u == 0u) {
        itoa_u((uint32_t)pc, t);
        {
            int k = 0;
            while (t[k])
                k++;
            t[k++] = '%';
            t[k] = 0;
        }
        say_as("SAVING THE REEL", t, 0);
    }
}

/* Key sync Garden: the garden's root to fit its scale to the splice playing, as SPEED transposes it (once it has held
 * 0.4 s: a sweep of SPEED doesn't drag it through every key); the cents SPEED leaves over tune the garden */
static void key_follow(void)
{
    static int cand = -1, sent;
    static uint32_t since;
    float kv[12], w[12], sh, sum = 0.0f, mx = 0.0f, sc[12];
    const float a = fm_fabsf(en_reel_rate);
    int pc, r, t, best, dc;
    if (proj.par[P_ST(KSYNC)] != 1 || !en_reel_nspl || a < 0.05f || !en_reel_keyvec(en_reel_spl, kv)) {
        if (sent) {
            en_detune(0);
            sent = 0;
        }
        cand = -1;
        return;
    }
    sh = 12.0f * fm_log2f(a);
    t = (int)fm_floorf(sh + 0.5f);
    dc = (int)((sh - (float)t) * 1000.0f);       /* (tenths of a cent) */
    for (pc = 0; pc < 12; pc++) {
        w[((pc + t) % 12 + 12) % 12] = kv[pc];
        sum += kv[pc];
        mx = fm_maxf(mx, kv[pc]);
    }
    best = proj.par[P_ROOT];
    for (r = 0; r < 12; r++) {                   /* each root: how much of the tape sits on the scale from it */
        float in = 0.0f;
        for (pc = 0; pc < 12; pc++)
            if (proj.pool >> ((pc - r + 12) % 12) & 1u)
                in += w[pc];
        sc[r] = in / sum + 0.2f * w[r] / mx;
    }
    for (r = 0; r < 12; r++)                     /* (the root kept unless another fits clearly better) */
        if (sc[r] > sc[best] + 0.04f)
            best = r;
    if (best != proj.par[P_ROOT]) {
        if (cand != best) {
            cand = best;
            since = plat_ms();
        } else if (plat_ms() - since > 400u) {
            knob_set(P_ROOT, best);
            say("KEY FROM TAPE", EN_NOTE_NAME[best]);
            cand = -1;
        }
    } else {
        cand = -1;
    }
    if (dc - sent > 20 || sent - dc > 20) {      /* (2 cents) */
        en_detune(dc ? dc : 0);
        sent = dc;
    }
}

/* the live colours: the chord's three first tones, each pitch class a hue (C red round to B), the voices' loudness
 * their brightness; eased, taken every fourth frame */
static uint16_t hsv565(float h, float s, float v)
{
    const float h6 = (h - (float)(int)h) * 6.0f, f = h6 - (float)(int)h6;
    const float p = v * (1.0f - s), q = v * (1.0f - s * f), t = v * (1.0f - s * (1.0f - f));
    float r, g, b;
    switch ((int)h6) {
    case 0: r = v, g = t, b = p; break;
    case 1: r = q, g = v, b = p; break;
    case 2: r = p, g = v, b = t; break;
    case 3: r = p, g = q, b = v; break;
    case 4: r = t, g = p, b = v; break;
    default: r = v, g = p, b = q; break;
    }
    return RGB((int)(r * 255.0f), (int)(g * 255.0f), (int)(b * 255.0f));
}
static void ui_live(void)
{
    static float hue[3] = {0.0f, 0.33f, 0.66f}, lv[3];
    int i;
    th_live = THEMES[theme_of(ui.view)];
    if (ui.frame % 4u == 0u) {
        const int n = en_chord_n ? en_chord_n : 3;
        uint32_t sg = 0;
        for (i = 0; i < 3; i++) {
            const float want = (float)(en_chord[i % n] % 12u) / 12.0f + 0.04f * (float)i,
                        l = fm_minf(en_vox_level[i] * 3.0f + en_master_level, 1.0f);
            float d = want - hue[i];
            d -= (float)(int)(d + (d < 0.0f ? -0.5f : 0.5f));   /* (the short way round) */
            hue[i] += d * 0.25f;
            hue[i] -= hue[i] < 0.0f ? -1.0f : hue[i] >= 1.0f ? 1.0f : 0.0f;
            lv[i] += (l - lv[i]) * 0.3f;
            ui_lc[i] = hsv565(hue[i] < 0.0f ? hue[i] + 1.0f : hue[i], 0.75f, 0.6f + 0.4f * lv[i]);
            sg = sg * 131u + (uint32_t)(ui_lc[i] & 0xE71Cu);   /* (coarsely) */
        }
        if (ui.frame % 32u == 0u)                /* (the pages drawn again with them twice a second) */
            ui_lsig = sg;
    }
    th_live.line = mix(th_live.line, ui_lc[0], 9);   /* the rules and the tracks: half the first tone */
    th_live.pad = ui_lc[1];
    th_live.bell = mix(th_live.bell, ui_lc[2], 8);
}
/* the ground washed: each pixel left at the page's ground colour takes a band of the live colours, faint (a fifth
 * on a dark page, a ninth on a light one), in slow diagonal bands that drift as the bands are drawn again */
static void ui_blit(uint32_t x, uint32_t y)
{
    if (ui_wash_on && !vis_over && cv_w == 240u && x == 0u) {
        static const uint8_t WV[32] = {0, 1, 2, 3, 4, 5, 6, 6, 7, 7, 7, 6, 6, 5, 4, 3, 2, 1, 0, 0, 0, 0, 1, 1,
                                       1, 1, 0, 0, 0, 0, 0, 0};
        const uint16_t bg = swap16(K_BG), b0 = K_BG;
        const int light = (b0 >> 11) > 16u, top = light ? 2 : 4;
        const uint32_t ph = plat_ms() / 160u;
        uint32_t yy, xx;
        for (yy = 0; yy < cv_h; yy++) {
            const uint32_t sy = y + yy;
            const uint16_t lc = mix(ui_lc[(sy / 80u) % 3u], ui_lc[(sy / 80u + 1u) % 3u], (int)(sy % 80u) / 5);
            uint16_t tone[8];
            uint16_t *p = cv_px + yy * 240u;
            int k;
            for (k = 0; k < 8; k++)
                tone[k] = swap16(mix(b0, lc, (k * top + 3) / 7));
            for (xx = 0; xx < 240u; xx++)
                if (p[xx] == bg)
                    p[xx] = tone[WV[((xx + sy) / 9u + ph) & 31u]];
        }
    }
    (cv_blit)(x, y);
}

void ui_frame(void)
{
    input();
    en_reel_study();                             /* (a splice's key: a frame of it) */
    key_follow();
    ui_live();
    TH = &th_live;
    reel_saving();
    autosave();
    if (draw_splash()) {                         /* the start screen */
    } else if (vis_full()) {                     /* the visual: the whole screen, 20 frames a second */
        if (ui.frame % 3u == 0u)
            draw_vis_screen();
    } else {
        ui_wash_on = 1;
        draw_header();
        draw_main();
        draw_knobs();
        ui_wash_on = 0;
    }
    leds();
    speak();
    ann_tick(plat_ms());
    ui.frame++;
}

void ui_input_only(void) { input(); }
