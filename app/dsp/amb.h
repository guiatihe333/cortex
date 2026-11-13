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
    P_TAPE = P_PLAY + 3 * 11,                         /* then the reel's (TP_*, P_RL) */
    P_TMOD = P_TAPE + 12,                             /* then the reel's five modulators (TM_*, P_TM) */
    P_SONGP = P_TMOD + 5 * 6,                         /* then the song's (SG_*) */
    P_SETP = P_SONGP + 4,                             /* then MIDI's and Key sync (ST_*) */
    P_VCF = P_SETP + 64,                              /* then each voice's filter: EN_NVOX of VF_N (P_VF, 1.5.20) */
    P_EXPR = P_VCF + 3 * 4,
    P_SEQ = P_EXPR + 3 * 73,
    P_SEQ_MOTIF = P_SEQ, P_SEQ_ROTATE, P_SEQ_ACCENT, P_SEQ_PASS, P_SEQ_HUMAN, P_SEQ_SPACE, P_SEQ_RANGE, P_SEQ_DIALOGUE, P_SEQ_LENGTH, P_SEQ_TENSION, P_SEQ_RESOLVE, P_NPARAMS
};
/* the three FM6 voices, each with its own patch (en_patch: a DX7 voice, edited operator by operator), its own FM
 * index and five generative modulators */
enum { VOX_PAD, VOX_BELLS, VOX_BASS, EN_NVOX };
/* a patch's source: AMB-1's own eighteen, any voice any of them (EN_NFACT, made here: six pads, six bells, six basses)
 * or a voice of the banks loaded from .syx files (EN_NBANK banks of 32 in the flash: source EN_NFACT + bank * 32 + voice) */
#define EN_NFACT 18
#define EN_NBANK 16
#define EN_NSRC (EN_NFACT + (EN_NBANK + 1) * 32)   /* (and B17, the pads' bank: built in, bank_pads.h) */
#define EN_PADS_SRC (EN_NFACT + EN_NBANK * 32)
/* the voice generator (the Gen page): a patch of a kind (EN_GEN_NAME's) made from a seed, BRIGHT and MOTION 0..100 (the
 * same seed and settings, the same patch); a patch varied by an amount 0..100, from a seed */
#define EN_NGEN 10
extern const char *const EN_GEN_NAME[EN_NGEN];
void en_gen(int kind, int bright, int motion, uint32_t seed, uint8_t *p);
void en_gen_vary(uint8_t *p, int amount, uint32_t seed);
#ifdef EN_HOST
int en_probe(const uint8_t *p, int note, float on_s, float off_s, float *env, int nenv, float *harm);
#endif
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
/* a voice's filter (1.5.20): a low-pass after the Moog ladder on each of its notes: ON, CUTOFF (30 Hz .. 18 kHz, half
 * following the note), RESONANCE (to self-oscillation), ENV (the cutoff opened by the note's loudness, up to 6 octaves) */
enum { VF_ON, VF_CUT, VF_RES, VF_ENV, VF_N };
#define P_VF(v, k) (P_VCF + (v) * VF_N + (k))
/* Interpretation: per-voice controls and eight compact matrix routes. All OFF
 * through EX_AMOUNT=0; morph is independent and captured at the next attack. */
#define EN_XROUTES 8
#define EN_XSTRIDE 73
enum { EX_AMOUNT, EX_STYLE, EX_PHRASE, EX_VARIATION, EX_MEMORY, EX_LINK,
       EX_MORPH_SRC, EX_MORPH, EX_MORPH_KIND, EX_N };
enum { XR_SOURCE, XR_TARGET, XR_DEPTH, XR_WHEN, XR_CURVE, XR_SMOOTH, XR_MIN, XR_MAX, XR_N };
#define P_EX(v,k) (P_EXPR + (v) * EN_XSTRIDE + (k))
#define P_XR(v,r,k) P_EX(v, EX_N + (r) * XR_N + (k))
enum { XS_OFF, XS_ENERGY, XS_LINK, XS_OPEN, XS_BREATH, XS_DENSITY, XS_DURATION,
       XS_ACCENT, XS_ARCH, XS_SILENCE, XS_LEAP, XS_REGISTER, XS_OTHER, XS_MEMORY, XS_CALL, XS_TENSION, XS_ANSWER, XS_RECALL, XS_N };
enum { XT_OFF, XT_ATTACK, XT_DECAY, XT_SUSTAIN, XT_RELEASE, XT_GATE, XT_LEVEL, XT_CUTOFF, XT_INDEX, XT_MORPH,
       XT_DENSITY, XT_EVOLVE, XT_DRIFT, XT_MOTIF, XT_ROTATION, XT_ACCENT, XT_PASSING, XT_SWING, XT_HUMAN, XT_SPACE, XT_RANGE, XT_DIALOGUE, XT_LENGTH, XT_TENSION, XT_RESOLVE, XT_N };   /* (1.5.26: the last three aim at the sequencer) */
enum { XW_NOTE, XW_PHRASE, XW_LIVE, XW_N };
enum { XC_LINEAR, XC_SOFT, XC_STEP, XC_CENTER, XC_N };
enum { XSTYLE_NATURAL, XSTYLE_BREATH, XSTYLE_PLUCK, XSTYLE_LEGATO, XSTYLE_CLOUD, XSTYLE_N };
enum { XM_COLOR, XM_COMPATIBLE, XM_N };
_Static_assert(EX_N + EN_XROUTES * XR_N == EN_XSTRIDE, "expression layout");
/* B is staged by the UI/loader, then copied only by the audio command queue. */
void en_morph_load(int voice, const uint8_t *patch);
int en_morph_compatible(const uint8_t *a, const uint8_t *b);
void en_morph_patch(const uint8_t *a, const uint8_t *b, int amount, int kind, uint8_t *out);
/* Read-only audio snapshots, 0..100: energy/link/open/breath, for UI meters. */
extern volatile uint8_t en_expression[EN_NVOX][4];
/* Latest applied route contributions (signed percent) and phrase position, 0..100. */
extern volatile int16_t en_expr_route[EN_NVOX][EN_XROUTES];
extern volatile uint8_t en_expr_phase[EN_NVOX];
extern volatile uint8_t en_phrase_stage, en_phrase_leader, en_phrase_active;

const char *en_expr_target_scope(int target, int role);
enum { ROLE_CHORD, ROLE_LOOPS, ROLE_BASS, EN_NROLE };
/* the settings of before each voice had its own (Swell, Density, Bell oct, Bass oct, Glide, Fifth: still in the table,
 * unshown) into each voice's (a piece's recipe, a project of an older format) */
void en_upgrade(int16_t *par);
/* the reel (reel.c): one track of tape, 11.8 s, with genes. SOURCE (Mix, Voice 1..3, Pulse), MIX (dry/wet: 0 the
 * garden alone, 50 both, 100 the reel alone), SOS (of what was there, how much stays as it is overdubbed), SPEED
 * (-2x .. 0 .. +2x), GENE (a gene: the whole splice .. 1/256), SLIDE (where in the splice), MORPH (gaps, joined,
 * overlapping, spread), ORGANIZE (the splice played, 1..8); the deck: FX (the space before the reel, recorded with
 * it, or after it, the reel through it), WOW (and flutter), DRIVE (onto tape), TONE (its highs) */
enum { TP_SRC, TP_MIX, TP_SOS, TP_SPEED, TP_GENE, TP_SLIDE, TP_MORPH, TP_ORG, TP_FX, TP_WOW, TP_DRIVE, TP_TONE, TP_N };
#define EN_REEL_N 260096u                         /* the reel's samples (22.05 kHz): 11.8 s */
/* the reel's modulators, five as a voice's: TARGET (one of its settings, or another modulator's depth or rate), DEPTH
 * (of its range, half either way at 100), RATE (60 s .. 0.12 s a cycle; with SYNC Tempo a note value), SHAPE (the
 * voices' MOD's), SYNC (Free, Tempo), SMOOTH (a glide, 2 .. 500 ms) */
#define EN_NTMOD 5
enum { TM_TARGET, TM_DEPTH, TM_RATE, TM_SHAPE, TM_SYNC, TM_SMOOTH, TM_N };
#define EN_NTMSET 10                              /* Speed, Gene, Slide, Morph, Dry/Wet, Tone, Wow, Drive, SOS, Organize */
#define EN_NTMDEST (EN_NTMSET + 2 * EN_NTMOD)     /* then M1..M5's depth, M1..M5's rate */
#define P_TM(s, k) (P_TMOD + (s) * TM_N + TM_##k)
/* the song (a MIDI file's, tools/amb_mid.py): SONG (0 Off, 1..EN_NSONG: its slot), FOLLOW (0 .. 49: the garden's own
 * lines, their notes from the song's phrases, more as it rises; 50 .. 100: the song's melody on the Loops voice and
 * its bass on the Bass voice, more of them, all at 100; the chords the song's either way), VARY (its melody's notes
 * now and then a scale step off, a sixteenth late, left out), START (the bar it begins at and comes round to) */
enum { SG_SONG, SG_FOLLOW, SG_VARY, SG_START, SG_N };
#define P_SG(k) (P_SONGP + SG_##k)
#define EN_NSONG 4
/* a song's record: "ASNG", 1, its sixteenths (14 bits: two 7-bit bytes), its tempo (two), its root (0..11), its scale
 * (relative to the root, two 6-bit bytes), its name (12), 8 spare; then a chord a half bar (its pitch classes, two
 * 6-bit bytes, absolute; its root 0..11, 127 none), then the melody's notes (0 none, a note where it begins), its
 * lengths, the bass's notes, its lengths: a byte a sixteenth each. Every byte 7-bit (it comes as SysEx) */
#define EN_SONG_HDR 32
#define EN_SONG_MAXN 2048
void en_song(const uint8_t *rec);               /* the song to play (a valid record, read in place), 0 none */
extern volatile uint32_t en_song_pos;           /* where it is, in sixteenths */
#define P_RL(k) (P_TAPE + TP_##k)
_Static_assert(P_TMOD == P_TAPE + TP_N, "the reel's");
/* MIDI (global: Setup's MIDI page): each voice's channel (1..16, out and in), what notes in do (0 Scale: those on
 * voice 1's channel add or remove their note from the scale; 1 Play: each voice's channel plays that voice); Key
 * sync (a piece's: 0 Off, 1 Garden: the garden's root follows the splice playing as SPEED transposes it, and its
 * tuning the cents left over; 2 Tape: SPEED snaps to a semitone that sets the splice in the garden's key) */
enum { ST_CH1, ST_CH2, ST_CH3, ST_NOTES, ST_KSYNC, ST_VMODE, ST_VPAL, ST_VTRAIL, ST_VREACT, ST_VFEED, ST_VRGB, ST_VHUE,
       ST_VSYNC, ST_VGEO, ST_VSIDES, ST_VMIRROR, ST_VSPEED, ST_VLAYER, ST_VBLEND, ST_VMIX, ST_VPULSE,
       ST_VMORPH, ST_VMUTATE, ST_VFLOW, ST_VCHAOS, ST_VMOSH, ST_VSORT, ST_VCORRUPT, ST_VSLICE,
       ST_VFOLD, ST_VPOSTER, ST_VPIXEL, ST_VKEY, ST_VOFREQ, ST_VOSHAPE, ST_VOANGLE, ST_VOSPEED, ST_VOFREQ2,
       ST_VOCOMB, ST_VOWARP, ST_VOSYNC, ST_VXZOOM, ST_VXROT, ST_VXSHX, ST_VXTGT, ST_VGEO2, ST_VSIDES2, ST_VSIZE2, ST_VBARS, ST_VDELAY, ST_VBRIGHT, ST_VEAR,
       ST_AR1S, ST_AR1T, ST_AR1D, ST_AR2S, ST_AR2T, ST_AR2D, ST_AR3S, ST_AR3T, ST_AR3D, ST_AR4S, ST_AR4T, ST_AR4D, ST_N };
/* (1.4.17, global) the first page's visual: VISUAL (0 Bloom .. 4 Tunnel, 5 the Score), COLOUR (0 the theme, Ember,
 * Ice, Acid, Mono), TRAILS, REACT (0..100); (1.4.18) as a video synth: FEEDBACK, RGB, HUE, SYNC (0..100); (1.4.19)
 * VISUAL 0 Bloom .. 6 Spiral, 7 the Score; its shape: GEOMETRY (0 Off, Polygon, Lissajous, Grid, Mandala, Scope),
 * SIDES (3..12), MIRROR (0 Off, Mirror, Flip, Quad, Kaleido), SPEED (0..100); (1.4.20) LAYER (0 Off, 1.. a
 * second picture, Bloom .. Spiral), BLEND (0 Add, Screen, Max, Difference, Mask), MIX, PULSE (0..100); (1.4.22) its
 * life: MORPH, MUTATE, FLOW, CHAOS (0..100); (1.4.23) broken as data: MOSH, SORT, CORRUPT, SLICE (0..100); (1.4.24)
 * VISUAL 7 XOR, 8 the Score; GEOMETRY 6 Rutt; its chroma: COLORAMA, POSTER, PIXEL, KEY (0..100); (1.5.0) the screen:
 * BARS (0 Show, 1 Auto: hidden after DELAY s untouched, 2 Hide), DELAY (1..15), BRIGHT, EAR (its smoothing) (0..100);
 * the react matrix: four slots, each a SOURCE (EN_AR_NSRC: 0 Off, Level, Bass, Mid, High, Beat, Voice 1..3, Notes,
 * Chord, LFO, Random), a TARGET (0 Off, 1.. the visual's settings VMODE .. VOSYNC, then All) and a DEPTH
 * (-100..100); (1.5.9) VISUAL 8 Osc, 9 the Score, LAYER 9 Osc; Osc's: FREQ (0..100: half a cycle .. 32 across), WAVE
 * (0 Sine, Tri, Saw, Square, Noise), ANGLE (0..100: 0 .. 180 degrees), SCROLL (-100..100), FREQ 2 (0 none; a second,
 * square to it), COMBINE (0 Add, Multiply, Difference, Warp: the second bends the first, Field: the picture bends it),
 * WARP (0..100), SYNC (0 Free, Notes: the notes' pitch its frequency, Beat: from the top on the beat) */
#define EN_AR_NSRC 13
#define EN_AR_NTGT (ST_VSIZE2 - ST_VMODE + 2)     /* (1.5.15: GEOMETRY 0 .. 16; SHAPE 2's GEOMETRY 2, SIDES 2,
                                                  * SIZE 2) */
#define EN_AR_NTGT27 (ST_VXTGT - ST_VMODE + 2)    /* (format 27's All) */
/* (Off, the settings, All); (1.5.12) TRANSFORM's ZOOM, ROTATE,
                                                  * SHIFT (-100..100; 1.5.14: along Osc's ANGLE), TARGET (1.5.14: 0
                                                  * Feedback, 1 Lines, 2 Both); Osc's WAVE 5 Rings, 6 Polygon, 7 Spiral */
#define EN_AR_NTGT25 (ST_VOSYNC - ST_VMODE + 2)   /* (format 25's All) */
#define EN_AR_NTGT24 (ST_VKEY - ST_VMODE + 2)     /* (format 24's All) */
#define P_ST(k) (P_SETP + ST_##k)
_Static_assert(P_SONGP == P_TMOD + EN_NTMOD * TM_N && P_SETP == P_SONGP + SG_N && P_VCF == P_SETP + ST_N &&
                   P_EXPR == P_VCF + EN_NVOX * VF_N && P_SEQ == P_EXPR + EN_NVOX * EN_XSTRIDE && P_NPARAMS == P_SEQ + 11,
               "the reel's modulators, the song's, MIDI's");
/* the splices' keys (reel.c): each studied once recorded (en_reel_study, from the UI, a frame of it a call: the splice
 * studied, -1 none to study), its 12 pitch classes' weights (en_reel_keyvec: 0 not known yet, or silent); or set by
 * hand, 1..12 C..B major, 13..24 C..B minor (0: as studied); the key a weighting is most like (its tonic, *minor) */
int en_reel_study(void);
int en_reel_keyvec(int spl, float *v);
int en_reel_keyman(int spl);
void en_reel_keyset(int spl, int k);
int en_key_guess(const float *v, int *minor);
extern volatile float en_reel_rate;               /* the reel's speed as it plays (Key sync Tape: snapped), signed */
extern volatile int8_t en_reel_spl;               /* the splice playing */
void en_detune(int dcents);
/* the engine's ear (the screen's visuals): the mix, mono, a sample in four: six bands' levels (one-pole splits at 150,
 * 400, 1 k, 2.5 k, 5 kHz; fast up, ~0.2 s down) and its last EN_WAVE_N samples (x 160, a ring, en_wave_w next) */
#define EN_NBAND 6
#define EN_WAVE_N 128
extern volatile float en_bands[EN_NBAND];
extern volatile int8_t en_wave[EN_WAVE_N];
extern volatile uint32_t en_wave_w;                       /* the garden's tuning moved by dcents tenths of a cent (Key sync) */
enum { EN_REEL_NEW = 1, EN_REEL_DUB, EN_REEL_ERASE, EN_REEL_TAKE, EN_REEL_CHOP };
/* a reel from a file (FITA.html): its blocks and their states written in place (en_reel_mem 1, 3) while it is erased,
 * then its splices (its 80-byte metadata, as en_reel_mem 0 has it) taken at once */
void en_reel_take(const void *meta);
/* what plays into the FM-1 over USB (a phone, a computer), mono, 16-bit, n frames for the next en_render (0: none):
 * the reel's SOURCE USB records it */
void en_usb(const int16_t *mono, uint32_t n);
void en_reel(int op);                             /* a new splice at the end; overdub the one picked (again: stop); erase */
/* the splice cut at its attacks (EDIT held on the tape's pages): a frame a call (en_reel_chop_step), the audio left as
 * it is. 0 started; -1 no splice, -2 too short, -3 recording, -4 already cutting, -5 the reel is full */
int en_reel_chop(int spl);
int en_reel_chop_step(void);                      /* 1 working, 0 cut (the engine does it), -1 idle, -2 too short */
extern volatile uint8_t en_reel_state, en_reel_nspl;   /* 0 resting, 1 recording a new splice, 2 overdubbing; splices */
extern volatile uint32_t en_reel_len, en_reel_w;  /* samples recorded (22.05 kHz, of 260096); where it records */
extern volatile float en_reel_pos;                /* the cursor in the splice playing, 0..1 */
extern volatile uint32_t en_reel_s0[8], en_reel_s1[8];   /* each splice's first sample, one past its last */
extern volatile int32_t en_reel_gene[4];          /* where each gene reads (a sample of the reel); -1 none */
extern volatile float en_reel_level;              /* how loud the reel plays */
extern volatile float en_reel_mod[EN_NTMOD];      /* each modulator now, -1 .. 1 times its depth */
void *en_reel_mem(int part, uint32_t *n);         /* the reel's memory: 0 its splices, 1 its blocks' states, 2 its
                                                   * samples recorded (3: all of them), for saving and loading */
int en_reel_valid(void);                          /* after loading: 1 a reel, 0 none (then empty) */
#define P_PL(v, k) (P_PLAY + (v) * PL_N + (k))
_Static_assert(P_PLAY == P_VOX + EN_NVOX * VX_N, "a block a voice");
_Static_assert(P_TAPE == P_PLAY + EN_NVOX * PL_N, "how each voice plays");
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
void en_tape(int on);                          /* the reel's own transport: the tape alone, the garden left resting */
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
extern volatile uint8_t en_tape_playing;        /* the reel plays while the garden rests (PLAY on its pages) */
extern volatile uint8_t en_chord[EN_NPAD];     /* the chord's notes (MIDI), pad voices' targets */
extern volatile uint8_t en_chord_root;         /* the chord's root, MIDI pitch class 0..11 */
extern volatile uint8_t en_chord_n;            /* its distinct tones: 3 or 4 */
extern volatile float en_drift;                /* 0..1: how far to the next chord */
extern volatile float en_pad_level, en_bass_level;
extern volatile float en_vox_level[EN_NVOX], en_master_level;   /* each voice's peak (its sound alone), the mix's before
                                                                 * the volume (1: full scale), held a moment: the meters */
extern volatile float en_tempo;                   /* the tempo playing: Tempo's, or an external clock's as measured */
extern volatile uint8_t en_loop_pos[EN_NLOOP];
extern volatile int8_t en_loop_note[EN_NLOOP][EN_LOOP_MAX];   /* -1 rest, else the slot's offset */
extern volatile float en_loop_glow[EN_NLOOP];  /* 0..1: a bell just rang from this loop */
extern volatile uint32_t en_bars;              /* sixteenths played since start (for tests) */
extern volatile float en_arc;                  /* the arc, -1 sparse .. +1 full (times Motion) */
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
#define MOD_MSLOT (MOD_OP + 6 * OPT_N)            /* then the voice's own modulators: M1..M5's depth, M1..M5's rate */
#define MOD_SEQ (MOD_MSLOT + 2 * 5)
#define MOD_EXPR (MOD_SEQ + 15)
#define EN_NMODDEST (MOD_EXPR + 2)
/* a modulator's target as its setting has it: 0 Off (the default), then 1 + one of the EN_NMODDEST (the voices') or of
 * the EN_NTMDEST (the reel's) */
#define EN_MOD_OFF 0
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
void en_moddest_text(int d, char *buf);
/* the scope: the voice the screen shows (-1 none) is captured from a rising zero crossing, every 2nd sample, scaled to
 * its peak (-127 .. 127); en_scope_seq counts the traces */
/* the score: the last EN_SCORE_N notes played, every voice's (as their MIDI goes out, sent or not): its note, its
 * voice, when it began and ended (en_now's samples; off 0 while it sounds); en_score_w the next to write */
#define EN_SCORE_N 48
typedef struct {
    uint32_t on, off;
    uint8_t note, voice, vel, rsv;
} en_note_t;
extern en_note_t en_score[EN_SCORE_N];
extern volatile uint32_t en_score_w, en_now;
#define EN_SCOPE_N 128
/* the render near its time (over 90 %, from the audio interrupt, after it): at the next render's start a gene of the
 * reel (the one furthest through), else the softest bell ringing, is let go; en_sheds counts them */
void en_overload(void);
/* the audio's load (%, the app's measure, each render): over 72 % the voices hold fewer notes (the loops three each,
 * the chord one fewer; over 80 % the loops two, the chord two fewer), the ones over faded out, before an overload sheds */
void en_load(int pct);
/* the keys played on a voice's page: note (MIDI) of voice v down (on 1) or up (on 0), apart from the garden */
void en_key(int v, int note, int on);
uint32_t en_alg_carriers(int alg);              /* algorithm alg (0..31)'s carriers: bit k operator 6 - k */
void en_keys_off(void);
/* the second core (1.5.22): the app's plat_cpu2_run / plat_cpu2_wait when it runs, else 0 (one core). The render
 * hands it the reel's genes while this core makes the voices */
extern int (*en_job_run)(void (*fn)(uint32_t), uint32_t arg);
extern uint32_t (*en_core)(void);                     /* (1.5.37) which core runs the caller: 0, 1 (plat_core) */
extern int (*en_job_wait)(void);                         /* every key's note let go (a new piece; STOP does it too) */
extern volatile uint32_t en_sheds;
extern volatile int8_t en_scope_v;
extern volatile uint32_t en_scope_seq;
extern int8_t en_scope[EN_SCOPE_N];        /* a target's name, as the screen shows it (10 bytes) */
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
