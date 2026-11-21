/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal, guiatihe333 */
/* CORTEX VIDEO's video synth: the engine alone, with no platform in it. The FM-1 runs it (video/app.c: the ear, the
 * panel, the screen), and so does the PC's mirror (video/mirror: the same C in WebAssembly), frame for frame: given
 * the same settings and the same inputs, it draws the same pictures (every build -ffp-contract=off, fastmath.h's own
 * functions, its own random numbers). That is how a take is recorded on the computer from what the FM-1 plays.
 *
 * A frame: vs_frame(in) moves the picture on (the field, the geometry), then vs_paint(y0, h, out) paints screen rows
 * y0 .. y0 + h - 1 into out (240 x h pixels, RGB565 in the LCD's byte order); every row once a frame, top to bottom.
 *
 * Its pieces come from CORTEX's video synth (app/ui.c, 1.4.17 .. 1.6): a field of 120 x 120 light fed back into itself,
 * ten pictures, sixteen geometries, the Osc, a layer, life, data moshing, chroma, a react matrix. CORTEX VIDEO adds:
 * eight react slots and their new sources (the tempo's LFO, the bar, CC, velocity), STROBE, INVERT, input GAIN and ten
 * palettes. */
#pragma once
#include <stdint.h>

/* the engine's version: changes whenever the same input would draw another picture (the mirror must be the same) */
#define VS_ENGINE 7
#define VS_NBAND 6
#define VS_WAVE_N 128
#define VS_NVOX 3                                 /* (the lows, the mids, the highs: the old voices' orbs) */
#define VS_NNOTE 8                                /* the notes begun a frame, at most */
#define VS_NSLOT 8                                /* the react matrix's slots */
#define VS_SLOT_W 5                               /* a slot's settings: SOURCE, TARGET, DEPTH, MODE, LAG */
#define VS_NSPEC 24                               /* the spectrum's bands (the FM-1's FFT: 43 Hz .. 5.5 kHz, log spaced) */

/* the settings (vs_par_info: name, range, default). The first part keeps CORTEX's order and meaning (its looks and
 * its pieces read the same), then CORTEX VIDEO's */
enum {
    ST_VMODE, ST_VPAL, ST_VTRAIL, ST_VREACT, ST_VFEED, ST_VRGB, ST_VHUE, ST_VSYNC, ST_VGEO, ST_VSIDES, ST_VMIRROR,
    ST_VSPEED, ST_VLAYER, ST_VBLEND, ST_VMIX, ST_VPULSE, ST_VMORPH, ST_VMUTATE, ST_VFLOW, ST_VCHAOS, ST_VMOSH, ST_VSORT,
    ST_VCORRUPT, ST_VSLICE, ST_VFOLD, ST_VPOSTER, ST_VPIXEL, ST_VKEY, ST_VOFREQ, ST_VOSHAPE, ST_VOANGLE, ST_VOSPEED,
    ST_VOFREQ2, ST_VOCOMB, ST_VOWARP, ST_VOSYNC, ST_VXZOOM, ST_VXROT, ST_VXSHX, ST_VXTGT, ST_VGEO2, ST_VSIDES2,
    ST_VSIZE2, ST_VSTROBE, ST_VINVERT,
    /* the objects (vobj.inc): FORM, SIZE, DRAW, TINT; SYMMETRY, POINT, CURVE, SYMMETRY 2; HOLE, TWIST, DEPTH, DETAIL;
     * SPIN, TILT, ZOOM, MORPH; EXPLODE, NOISE, SPECTRUM, PULSE; COUNT, ARRAY, SPREAD, REACT; the copies' steps: TURN,
     * SCALE, HUE, LAG; BLEND, LIGHT, SYNC */
    ST_OFORM, ST_OSIZE, ST_ODRAW, ST_OCOLOR, ST_OSYM, ST_OPINCH, ST_OCURVE, ST_OSYM2, ST_OHOLE, ST_OTWIST, ST_ODEPTH,
    ST_ODETAIL, ST_OSPIN, ST_OTILT, ST_OZOOM, ST_OMORPH, ST_OEXPLODE, ST_ONOISE, ST_OSPEC, ST_OPULSE, ST_OCOUNT,
    ST_OARRAY, ST_OSPREAD, ST_OREACT, ST_OSTROT, ST_OSTSCALE, ST_OSTHUE, ST_OSTTIME, ST_OBLEND, ST_OLIGHT,
    ST_OKEY, ST_OLIFE,                            /* the objects' KEY (BLEND Behind's level), LIFE (the genome on them) */
    ST_MLENS, ST_MSWIRL, ST_MGLOW, ST_MVEC,       /* MODULATE: the objects on the field: LENS (pushed out, pulled in),
                                                   * SWIRL (turned round them), GLOW (their halo in it), VECTORS (MOSH's
                                                   * blocks moved by their motion) (vobj.inc) */
    ST_OSYNC,                                     /* (the react matrix's targets: VMODE .. OSYNC, then All) */
    ST_FXV, ST_FXM, ST_FXC,                       /* TARGETS: VIDEO's (RGB, SYNC, SLICE's rows), MOSH's (MOSH, CORRUPT,
                                                   * SORT), CHROMA's (COLORAMA, POSTER, PIXEL, KEY, INVERT, STROBE):
                                                   * 0 the field alone, 1 the whole picture, the objects too */
    ST_TR1S, ST_TR1A, ST_TR1N,                    /* three triggers: a SOURCE, an ACTION, an AMOUNT (vobj.inc) */
    ST_TR3N = ST_TR1S + 8,
    ST_VBRIGHT, ST_VEAR, ST_VGAIN, ST_VBARS, ST_VDELAY,
    ST_AR1S, ST_AR1T, ST_AR1D, ST_AR1M, ST_AR1L,   /* a slot: SOURCE, TARGET, DEPTH, MODE (Direct, Invert, Beat hold,
                                                    * Gate), LAG (its smoothing); VS_NSLOT of them */
    ST_N = ST_AR1S + VS_SLOT_W * VS_NSLOT
};
#define VS_TGT_LAST ST_OSYNC
#define VS_NTGT (VS_TGT_LAST - ST_VMODE + 2)      /* a slot's targets: 0 Off, 1 .. the settings VMODE .. OPULSE, All */
enum { SRC_OFF, SRC_LEVEL, SRC_BASS, SRC_MID, SRC_HIGH, SRC_BEAT, SRC_LOWS, SRC_MIDS, SRC_HIGHS, SRC_NOTES,
       SRC_ROOT, SRC_LFO, SRC_RANDOM, SRC_TEMPO, SRC_BAR, SRC_CC, SRC_VEL, SRC_KICK, SRC_SNARE, SRC_HAT, SRC_BRIGHT,
       SRC_N };
#define VS_NMODE 9                                /* built-in synthesis modes: Bloom .. Osc */
enum { VS_LIVE_VIDEO = VS_NMODE, VS_IMPORTED_VIDEO, VS_NVISUAL };
#define VS_OSC 8
#define VS_NPAL 10

typedef struct {
    const char *name;
    int16_t lo, hi, def;
} vs_par_t;
const vs_par_t *vs_par_info(int k);
const char *vs_par_text(int k, int v, char *buf);   /* a value as the screen says it ("Plasma", "-40", "Off") */

/* what the engine hears and is told, a frame */
typedef struct {
    uint32_t ms;                                  /* the FM-1's clock (the beats' spacing) */
    float level;                                  /* the input's peak (fast up, 0.25 s down), 0 .. ~1 */
    float bands[VS_NBAND];                        /* six bands' levels (one-pole splits at 150, 400, 1 k, 2.5 k, 5 kHz) */
    float phase;                                  /* the tempo, in beats since it started (MIDI clock, or the taps) */
    int8_t wave[VS_WAVE_N];                       /* the last samples, the oldest first */
    uint8_t nnote, root, cc, vel;                 /* the notes begun since the last frame; the last note's pitch class;
                                                   * the last CC value (the learned one); the last velocity */
    uint8_t clock_beat;                           /* 1: the MIDI clock's beat fell in this frame (a beat for sure) */
    uint8_t hold;                                 /* the structure held (the genome stays) */
    uint8_t strobe;                               /* a key held: the picture flashed white (STROBE by hand) */
    uint8_t mutate;                               /* the genome thrown this frame: 0 no, 1 a nudge, 2 a throw, 3 anew */
    uint8_t spec[VS_NSPEC];                       /* the spectrum, 0 .. 255 each (each band against its own recent peak) */
    uint8_t kick, snare, hat;                     /* the drums heard: each 255 at its hit, falling */
    uint8_t bright;                               /* the sound's brightness (its spectral centroid), 0 .. 255 */
    struct {
        uint8_t voice, note, vel;
    } note[VS_NNOTE];
} vs_in_t;

void vs_reset(uint32_t seed);                     /* the whole state anew: the field black, the genome, the randoms */
void vs_set(int k, int v);                        /* a setting (clamped to its range) */
int vs_get(int k);
int vs_eff(int k);                                /* as the last frame read it (its react slots added) */
void vs_frame(const vs_in_t *in);
void vs_paint(int y0, int h, uint16_t *out);      /* rows y0 .. y0 + h - 1, every row once a frame, top to bottom */
uint32_t vs_sum(void);
/* CORTEX VIDEO's camera (Live Video, Video import): the picture this frame, 60 x 60 RGB332 (NULL: none; then the
 * synthesis under it goes on). Its light goes into the field, so every effect breaks it as it breaks a synthesis;
 * its colour comes back when painting (COLORAMA turns it to the palette's). Before vs_frame */
void vs_camera(const uint8_t *pic, int sharp);   /* sharp: vlink.h's VLINK_HQ layout */
#ifdef VS_HD
void vs_paint_hd(int S, uint8_t *rgba);           /* (the mirror's) the whole frame at 240 S square, RGBA, instead of
                                                   * vs_paint: the same picture, sharper (vhd.inc) */
#endif                            /* the field and the random state, hashed: two engines agree */
float vs_beat(void);                              /* the beat now, 1 .. 0 */
float vs_band(int k);                             /* a band as the picture reads it (0 .. 1.5) */
/* everything that changes the picture comes in through vs_set and vs_frame's input, nothing else: the mirror gets
 * the same and draws the same */
