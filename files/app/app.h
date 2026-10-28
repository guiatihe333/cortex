/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal */
/* AMB-1 app: the project (everything that is saved), the app's entry points and its half of the contract with
 * fm1-base (device/unity.c, host/plat_host.c). */
#pragma once
#include <stdint.h>
#include "plat.h"
#include "amb.h"

#define APP_NAME "AMB-1"
#define APP_BLOCK 256u                   /* frames per audio block: 5.8 ms at 44.1 kHz */
#ifdef APP_VERSION                       /* a release build passes its own (tools/build.py --release) */
#define EN_VERSION APP_VERSION
#else
#define EN_VERSION "DEV"
#endif

/* the store: the project at 0xFC000 and the panel calibration at 0xDC000, where AMB-1 0.9 kept them (an update keeps
 * the pieces) */
enum { OBJ_PROJ, OBJ_PANEL, OBJ_PATCH, OBJ_TAPE, OBJ_NOBJ };
#define APP_STORE_MAP {0xFC000u, 0xDC000u, 0xDE000u, 0xDA000u}   /* the pieces' patches after the panel; the pieces'
                                                                   * reel settings before it, in the data area */
#define PROJ_MAGIC 0x314F4E45u           /* "ENO1" */
#define PROJ_FORMAT 14u                  /* 14: Off, every modulator's first target; 13: the reel's five modulators, its settings in OBJ_TAPE (P_SPLIT);
                                          * 12: the reel's modulators, no pulse; 11: the reel's dry/wet and deck; 10: the reel's settings; 9: the voices
                                          * alike (a ROLE each, its own octave, density, pace, glide,
                                          * an ADSR), AMB-1's eighteen patches; 8: how each voice plays (ENV, GATE); 7:
                                          * each voice's own patch
                                          * (OBJ_PATCH) and five modulators; 6: each
                                          * voice's patch choice, FM edits and MOD; 5: one MOD for all; 4: the bass's
                                          * and the pulse's own settings (9 down to 3 are carried over; older formats
                                          * give the recipes) */
#define NPIECES EN_NPIECES

/* the DX7 banks (bank.c): EN_NBANK sectors of the data area, each a 32-voice bulk dump's voices (VMEM, 4096 bytes),
 * sent from a computer over USB-MIDI (tools/amb_syx.py) */
#define BANK_BASE 0x97000u
/* the reel (amb.h en_reel_mem): its splices at REEL_BASE, its blocks' states a sector on, its samples three on; the
 * splices written last (a reel half written has none) */
#define REEL_BASE 0xA7000u
int project_reel_save(void);              /* starts saving the reel (-1: it already is) */
int project_reel_step(void);              /* a sector more: 0 nothing to do, 1 going on, 2 done, -1 a flash error */
int project_reel_progress(void);          /* 0..100 while saving, -1 not */
void project_reel_load(void);
int bank_voice(int voice, int src, uint8_t *p);   /* source src's patch for the voice (a bank's: from the flash);
                                                 * 0: none there */
int bank_name(int src, char *b);         /* its name (10 characters, trimmed) into b; 0: an empty slot */
int bank_next(int src, int dir);         /* the next source that holds a voice, dir +1 / -1 (the own six always) */
#define APP_SYSEX
#define APP_OVERLOAD                     /* the render over 90 % of its time: it sheds (en_overload) */
int app_sysex(const uint8_t *p, uint32_t n);   /* a bank's chunk, a DX7 single voice (bank.c) */

/* the playing piece's patches, a voice each (as the engine has them) and the pieces' (OBJ_PATCH) */
typedef struct {
    uint32_t magic, format;
    uint8_t pat[NPIECES][EN_NVOX][EN_PATCH];
} patchstore_t;
extern uint8_t vpatch[EN_NVOX][EN_PATCH];
void project_source(int voice, int src);  /* a source's patch into the voice (its parameter, as loaded) */
void project_patch_set(int voice, int off, int v);   /* one byte edited (OWN) */
void project_patch_send(int voice, int fresh);       /* the voice's patch to the engine */

typedef struct {
    int16_t par[P_NPARAMS];
    uint16_t pool;
} slot_t;
/* in the store, the settings from P_SPLIT (the reel's) go in an object of their own (OBJ_TAPE): a store object holds
 * 3840 bytes, nine pieces' worth of everything does not fit one */
#define P_SPLIT P_TAPE

typedef struct {
    uint32_t magic, format;
    int16_t par[P_NPARAMS];              /* the piece playing, as it is now (amb.h P_*) */
    uint16_t pool;                       /* its scale, relative to the root (bit 0 = the root) */
    uint8_t leds;                        /* lights: 0 off, 1 keys and the buttons' glow, 2 keys only */
    uint8_t piece;                       /* which piece */
    uint8_t rsv0[2];
    slot_t slot[NPIECES];                /* every piece as it was left (Tune and MIDI out are global) */
    uint8_t rsv[16];                     /* room to grow */
} project_t;

extern project_t proj;

void project_defaults(void);
int project_load(void);                  /* 0 = loaded; else defaults are in place */
int project_save(void);                  /* 0 = saved and verified */
void project_apply(void);                /* every value to the engine (boot, load) */
const char *piece_name(int i);
void project_piece(int i);               /* the playing piece kept, piece i taken up as it was left */
void project_piece_reset(void);          /* the playing piece back to its recipe */
void ui_patch_in(int voice);             /* a voice's patch came in over USB (bank.c): the screen says so */
void ui_bank_in(int bank, int voices);   /* a bank came in (bank.c): the screen says so */
int ui_voice(void);                      /* the voice whose pages are shown (VOX_*), else the pad */

void ui_init(void);
void ui_frame(void);
void ui_input_only(void);
int ui_dirty(void);
void ui_say(const char *a, const char *b);

/* the contract: the platform calls these */
void app_init(void);                     /* boot: the engine, then the saved project (or the defaults) */
void app_render(int32_t *lr, uint32_t n, uint32_t gain_q12);   /* audio interrupt: n stereo frames, 24-bit */
void app_panic(void);                    /* silence now (before an update rewrites the flash) */
void app_ui_init(void);
void app_frame(void);
void app_input_only(void);
void app_say(const char *a, const char *b);

/* the host simulator's part (app/host.c): AMB-1's checks, its commands, the screen reader's dump */
#define APP_HOST_EXPECT
int app_expect(const char *what, int *got);
#define APP_HOST_CMD
int app_host_cmd(const char *cmd, const char *a, const char *b, const char *c, const char *d);
#define APP_HOST_SPEAKDUMP
void app_speak_dump(void);
#define APP_HOST_REBOOT
void app_host_reboot(void);              /* the project forgotten, as RAM is at a restart */
