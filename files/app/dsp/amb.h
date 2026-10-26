/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal */
/* The instrument: a generative ambient garden. Four layers draw on one pool of notes (a scale:
 * the root plus up to eleven more pitch classes) and one chord that drifts slowly through it:
 *
 *   PAD     four breathing voices: each swells in, hangs and fades on its own cycle (unequal lengths,
 *           as the loops of Music for Airports), taking its note of the chord as it starts, so the
 *           chord forms and dissolves. Two colours, by piece, both FM6 patches (JIANT FM's 6-operator
 *           engine, fm6_core.c): a Rhodes (blooming brighter as it swells) or Voices (two carriers apart,
 *           one fed back, vibrato); then a soft low-pass. The chord moves every DRIFT (20 s .. 5 min): voices on shared
 *           notes hold, the others breathe in anew.
 *   BELLS   soft FM6 bells played by four tape loops of unequal length (13, 17, 23, 29
 *           quarter notes), each holding a short phrase that shifts against the others, each its own
 *           shade of bell. EVOLVE changes a note now and then; DENSITY sets how many notes a loop holds.
 *   BASS    an FM6 sine and its sub-octave on the chord's root (now and then its fifth), gliding.
 *   PULSE   a soft thump, a closed hat and an open hat, each on its own Euclidean pattern (16, 12 and 7
 *           sixteenths): polyrhythms. HITS sets how many; the patterns turn and change around it.
 *
 * Over it all, the ARC: a slow tide, minutes long, between sparse (fewer bells, a darker vowel, a quieter
 * pulse, more reverb) and full; MOTION sets how far it goes. And rests: with EVOLVE, the pulse
 * sits out a few bars, the bass a chord, a bell loop a pass.
 * Then, after Discreet Music's chain, an Echoplex (160 ms, fed back) and a tape loop (to ~5.6 s, two
 * heads, each repeat darker), and Dattorro's plate (from FoMni), fed generously: the space is most of
 * what is heard.
 * FREEZE holds the chord and lets the plate ring on forever.
 *
 * Threads: en_render() runs in the audio interrupt. Everything else posts commands, which the render
 * drains at the start of a block; the state below marked (UI) is written by the render and only read
 * elsewhere. Float only, no libm: fastmath.h. */
#pragma once
#include <stdint.h>

#define EN_SR 44100.0f
#define EN_NLOOP 4                     /* bell tape loops */
#define EN_LOOP_MAX 29
#define EN_NDRUM 3                     /* pulse: thump, closed hat, open hat */
#define EN_NPAD 4                      /* pad voices */

/* modes: pools relative to the root (bit i: root + i semitones) */
enum { MODE_IONIAN, MODE_DORIAN, MODE_PHRYGIAN, MODE_LYDIAN, MODE_MIXOLYDIAN, MODE_AEOLIAN, MODE_PENT_MAJ,
       MODE_PENT_MIN, EN_NMODES };
extern const char *const EN_MODE_NAME[EN_NMODES];
extern const uint16_t EN_MODE_POOL[EN_NMODES];
extern const char *const EN_NOTE_NAME[12];
extern const uint8_t EN_LOOP_LEN[EN_NLOOP];
extern const uint8_t EN_DRUM_N[EN_NDRUM];

/* parameters: value ranges in en_param_info() */
enum {
    P_PAD, P_BELLS, P_BASS, P_PULSE,                  /* layer levels */
    P_DENSITY, P_EVOLVE, P_DRIFT, P_HITS,             /* motion */
    P_ECHO, P_ETIME, P_REVERB, P_SIZE,                /* space */
    P_BRIGHT, P_SWELL, P_BTONE, P_MOTION,             /* tone */
    P_TUNE, P_MIDI, P_TEMPO, P_ROOT, P_MODE, P_BOCT,  /* setup, panel encoders, bell octave */
    P_PADTONE,                                        /* the pad's patch (its source, EN_NSRC): Rhodes, Voices, ... */
    P_BASSOCT, P_GLIDE, P_FIFTH,                      /* bass: its octave, its glide, how often it leans on the fifth */
    P_SWING, P_HATS,                                  /* pulse: swing, the hats' colour */
    P_BELLPATCH, P_BASSPATCH,                         /* the bells' and the bass's patches (sources) */
    P_VOX,                                            /* then each voice's own: EN_NVOX blocks of VX_N (P_VX) */
    P_PLAY = P_VOX + 3 * 42,                          /* then how each voice plays its notes: EN_NVOX of PL_N (P_PL) */
    P_NPARAMS = P_PLAY + 3 * 11
};
/* the three FM6 voices, each with its own patch (en_patch: a DX7 voice, edited operator by operator), its own FM
 * index and five generative modulators */
enum { VOX_PAD, VOX_BELLS, VOX_BASS, EN_NVOX };
/* a patch's source: AMB-1's own eighteen, any voice any of them (EN_NFACT, made here: six pads, six bells, six basses)
 * or a voice of the banks loaded from .syx files (EN_NBANK banks of 32 in the flash: source EN_NFACT + bank * 32 + voice) */
#define EN_NFACT 18
#define EN_NBANK 16
#define EN_NSRC (EN_NFACT + EN_NBANK * 32)
#define EN_PADPATCHES 6
#define EN_BELLPATCHES 6
#define EN_BASSPATCHES 6
#define EN_FACT_BELL 6                            /* the first bell (AMB-1's own, shaded by Bell tone) */
#define EN_FACT_BASS 12
/* a voice's block: OWN (the patch edited since its source was loaded: kept as it is), INDEX (dB on the modulators,
 * heard at once), then EN_NSLOT modulators of MS_N: MOD (depth, rate, shape, target) and MOD 2 (spread, fade, sync,
 * smooth) */
#define EN_NSLOT 5
enum { MS_DEPTH, MS_RATE, MS_SHAPE, MS_DEST, MS_SPREAD, MS_FADE, MS_SYNC, MS_SMOOTH, MS_N };
enum { VX_OWN, VX_INDEX, VX_SLOT, VX_N = VX_SLOT + EN_NSLOT * MS_N };
#define P_VX(v, k) (P_VOX + (v) * VX_N + (k))
#define P_MS(v, s, k) P_VX(v, VX_SLOT + (s) * MS_N + (k))
/* how a voice plays its notes, the three alike: ENV (AMB-1's own: a chord's breath, the loops let go after a beat, the
 * bass held and gliding; the patch's: each note struck and let go, its envelopes as a DX7 plays them; ADSR: the same,
 * its carriers' envelope the four knobs), GATE (how long a note is held down: of a breath, a bar, a chord or a step),
 * ROLE (Chord, Loops or Bass), OCTAVE, DENSITY (a chord's voices, the loops' notes, the bass's leaning on the fifth),
 * PACE (a breath's length, the loops' step, the bass struck again each bar .. eighth), GLIDE, and the ADSR */
enum { PL_ENV, PL_GATE, PL_ROLE, PL_OCT, PL_DENS, PL_PACE, PL_GLIDE, PL_A, PL_D, PL_S, PL_R, PL_N };
enum { ENV_AMB, ENV_PATCH, ENV_ADSR, EN_NENV };
enum { ROLE_CHORD, ROLE_LOOPS, ROLE_BASS, EN_NROLE };
/* the settings of before each voice had its own (Swell, Density, Bell oct, Bass oct, Glide, Fifth: still in the table,
 * unshown) into each voice's (a piece's recipe, a project of an older format) */
void en_upgrade(int16_t *par);
#define P_PL(v, k) (P_PLAY + (v) * PL_N + (k))
_Static_assert(P_PLAY == P_VOX + EN_NVOX * VX_N, "a block a voice");
_Static_assert(P_NPARAMS == P_PLAY + EN_NVOX * PL_N, "how each voice plays");
extern const uint8_t EN_VOX_PATCH[EN_NVOX];       /* each voice's patch parameter */
/* the old global MOD (format 5 and the recipes: depth, rate, chaos, target) as every voice's first modulator */
void en_mod_legacy(int16_t *par, int depth, int rate, int chaos, int dest);

/* the patches: 155 bytes, a DX7 voice as its VCED edit buffer has it (fm6_core.c FP_*: six operators of 21, OP6
 * first; then the pitch envelope, algorithm 0..31, feedback, the LFO, transpose, the name) */
#define EN_PATCH 155
enum { EP_R1 = 0, EP_L1 = 4, EP_BP = 8, EP_LD, EP_RD, EP_LC, EP_RC, EP_RS, EP_AMS, EP_KVS, EP_OL, EP_MODE, EP_FC,
       EP_FF, EP_DET, EP_OP = 21, EP_PR1 = 126, EP_PL1 = 130, EP_ALG = 134, EP_FB, EP_OKS, EP_LFS, EP_LFD, EP_LPMD,
       EP_LAMD, EP_LKS, EP_LFW, EP_LPMS, EP_TRNSP, EP_NAME };
#define EP_OPN(n) ((6 - (n)) * EP_OP)                /* operator n's (1..6) first byte */
uint8_t en_patch_max(int off);                    /* a byte's highest value (its lowest is 0) */
void en_factory(int voice, int sel, int tone, uint8_t *p);   /* the voice's own patch sel (bells: at Bell tone) */
int en_unpack(const uint8_t *vmem, uint8_t *p);  /* a DX7 bank's packed voice (128 bytes) into a patch; 0: not one */
void en_bake(uint8_t *p, int ratio, int fb, int atk, int dec, int sus, int rel);   /* format 6's FM edits, into p */
/* the voice's patch, for the render: its bytes (each posted, applied together by en_patch_done); done: a new patch
 * (the pad crosses over to it, the bass is struck anew, the bells take it at their next strike), or 0 an edit (heard
 * at once on the notes sounding) */
void en_patch_byte(int voice, int off, int v);
void en_patch_done(int voice, int fresh);
/* the same, the whole patch at once: copied into the voice's mailbox and one command posted (the engine takes the
 * mailbox's newest: a quick run of patches is never more than a command each, never a byte dropped) */
void en_patch_load(int voice, const uint8_t *p, int fresh);
typedef struct {
    const char *name;
    int16_t lo, hi, def;
} en_param_t;
const en_param_t *en_param_info(int i);
void en_param_text(int i, int v, char *buf);   /* the value as the screen shows it (10 bytes) */

/* commands (main loop -> render) */
void en_init(void);
void en_set(int param, int value);
void en_pool(uint32_t pool);                   /* the scale, relative to the root (bit 0 always set) */
void en_play(int on);                          /* the garden grows / rests (the tails ring on) */
void en_next(void);                            /* the next chord: on the next bar (now while resting) */
void en_freeze(int on);
void en_seed(uint32_t s);                      /* stir some entropy into the generators */
void en_panic(void);                           /* everything quiet, now */
void en_regrow(void);                          /* a new piece: fresh loops, the home chord */
/* MIDI clock in (as FoMni): ticks drive the 1/16 grid (6 a step) while they keep coming */
enum { EN_CLK_TICK, EN_CLK_START, EN_CLK_CONTINUE, EN_CLK_STOP };
void en_clock(int msg);

/* the render (audio ISR): n stereo frames, 24-bit in int32; gain Q12 (the MASTER pot) */
void en_render(int32_t *out_lr, uint32_t n, uint32_t gain_q12);

/* MIDI out, from the render: the app sends it (USB); status, data 1, data 2 */
void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2);

/* state for the screen (UI) */
extern volatile uint8_t en_playing, en_frozen, en_ext;
extern volatile uint8_t en_chord[EN_NPAD];     /* the chord's notes (MIDI), pad voices' targets */
extern volatile uint8_t en_chord_root;         /* the chord's root, MIDI pitch class 0..11 */
extern volatile uint8_t en_chord_n;            /* its distinct tones: 3 or 4 */
extern volatile float en_drift;                /* 0..1: how far to the next chord */
extern volatile float en_pad_level, en_bass_level;
extern volatile float en_tempo;                   /* the tempo playing: Tempo's, or an external clock's as measured */
extern volatile uint8_t en_loop_pos[EN_NLOOP];
extern volatile int8_t en_loop_note[EN_NLOOP][EN_LOOP_MAX];   /* -1 rest, else the slot's offset */
extern volatile float en_loop_glow[EN_NLOOP];  /* 0..1: a bell just rang from this loop */
extern volatile uint8_t en_drum_pos[EN_NDRUM];
extern volatile uint32_t en_drum_pat[EN_NDRUM];/* bit i: a hit on step i */
extern volatile float en_drum_glow[EN_NDRUM];
extern volatile uint32_t en_bars;              /* sixteenths played since start (for tests) */
extern volatile float en_arc;                  /* the arc, -1 sparse .. +1 full (times Motion) */
extern volatile uint8_t en_pulse_rest;         /* the pulse is sitting a few bars out */
extern volatile uint8_t en_pad_note[EN_NPAD];  /* the note each voice sings now (MIDI) */
extern volatile float en_pad_amp[EN_NPAD];     /* and how loud */

/* MOD: the voices' generative modulation, five modulators for each of the three voices (pad, bells, bass), each set on
 * its own and summed. In each modulator every sounding voice (the four pad voices, the six bells, the bass) has a
 * generator of its own, at its own pace around RATE
 * (60 s .. 0.12 s a cycle, or with SYNC Tempo a note value, 8 bars .. 1/16). SHAPE what it makes (EN_MODSHAPE_NAME):
 * the periodic waves, sample and hold, a random walk (glides between random values), the logistic map at the edge
 * of chaos (jumps), or an envelope that falls from each note's start. DEPTH how far it goes (the arc deepens and calms
 * it); SPREAD how far each voice goes its own way (0: the layer's voices move as one); FADE the modulation coming in
 * after each note (0 .. 8 s); SYNC Free, Note (each note starts the shape again) or Tempo; SMOOTH a glide on what it
 * makes (2 ms .. 0.5 s: a square softened). TARGET what it moves, the FM6 voice's own parameters first: */
enum { SHAPE_SINE, SHAPE_TRI, SHAPE_UP, SHAPE_DOWN, SHAPE_SQUARE, SHAPE_SH, SHAPE_WALK, SHAPE_CHAOS, SHAPE_ENV,
       EN_NMODSHAPE };
extern const char *const EN_MODSHAPE_NAME[EN_NMODSHAPE];
enum { SYNC_FREE, SYNC_NOTE, SYNC_TEMPO, EN_NMODSYNC };
enum { MOD_TIMBRE, MOD_RATIO, MOD_METAL, MOD_FEEDBACK, MOD_DECAY, MOD_PITCH, MOD_SPACE, MOD_ALL, MOD_LEVEL,
       MOD_LFOPM, MOD_OP };
/* then each operator's own, MOD_OP + (n - 1) * OPT_N + kind for operator n (1..6): */
enum { OPT_LEVEL, OPT_RATIO, OPT_FINE, OPT_ATK, OPT_DEC, OPT_SUS, OPT_REL, OPT_N };
#define EN_NMODDEST (MOD_OP + 6 * OPT_N)
/*   TIMBRE    the modulators' output level (+-12 dB): the FM index, brighter and darker
 *   RATIO     the modulators' frequency ratio, in harmonic steps up (x1, x2, x3, x4, x5), gliding 15 ms
 *   METAL     the modulators' ratio pulled apart (+-80 cents, one up, the next down): inharmonic sidebands
 *   FEEDBACK  the feedback operator's feedback (up to 9 steps more, continuous): from pure to noisy, saw-like
 *   DECAY     each note's envelopes as it starts: the modulators' decay (the brightness fading) +-20 rates,
 *             the carriers' (the ring) +-6
 *   PITCH     the voice's pitch (+-30 cents, slewed 15 ms): tape wow, a choir's drift, at the top a vibrato
 *   SPACE     the voice's place in the stereo field (slewed 30 ms, ramped across a block)
 *   ALL       timbre, metal, feedback, decay, pitch and space, a little less of each (not ratio: its steps)
 *   LEVEL     the voice's loudness, down to -10 dB (one-sided: a tremolo, a swell, never louder than the voice)
 *   LFO PM    the patch's own LFO's pitch depth (one-sided, up to 30 cents more: vibrato)
 *   an operator's:
 *     LEVEL   its output level, +-12 dB (a carrier: down only, to -12 dB, never louder than the patch)
 *     RATIO   its frequency ratio in harmonic steps up (x1 .. x5), gliding 15 ms
 *     FINE    its pitch, +-50 cents, slewed 15 ms
 *     ATTACK, DECAY, SUSTAIN, RELEASE   its envelope as each note starts: rates +-30 (+ slower), sustain +-30 */
void en_moddest_text(int d, char *buf);        /* a target's name, as the screen shows it (10 bytes) */
#define EN_NMOD (EN_NVOX * EN_NPAD)             /* the generators of a modulator: each voice's four notes */
extern volatile float en_mod[EN_NSLOT][EN_NMOD];   /* each generator's value now, -1 .. 1 (times the depth) */

/* the pure parts, for the tests and the screen */
uint32_t en_euclid(int k, int n, int rot);     /* Bjorklund's pattern, as a bitmask (bit 0 = step 0) */
int en_scale(uint32_t pool, int *steps);       /* the pool's pitch classes (relative), ascending; count */
/* the chord on scale degree deg: its tones (MIDI pitch classes, absolute), 3 or 4 of them; count */
int en_chord_tones(uint32_t pool, int root, int deg, int *pcs);
/* voice leading: new pad notes for the tones, each voice moving as little as possible, all in lo..hi */
void en_voice_lead(const int *from, const int *pcs, int ntones, int *to, int lo, int hi);
const char *en_chord_name(const int *pcs, int n, char *buf);   /* "Em7", "Dsus4", "C" ... */

/* the pieces: recipes for every parameter (Tune and MIDI out included, but they are the player's) */
#define EN_NPIECES 8
const char *en_piece_name(int i);
void en_piece(int i, int16_t *par, uint16_t *pool);
