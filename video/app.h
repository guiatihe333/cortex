/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal, guiatihe333 */
/* CORTEX VIDEO: the FM-1 as an audio-reactive video synth, nothing else (no voices, no tape, no songs). It hears what a
 * computer or a phone plays into it over USB (and passes it on to the headphones and the speaker), takes notes, CC and
 * clock over MIDI, and plays CORTEX's video synth (vsynth.c) on its screen. A computer's mirror (mirror/index.html)
 * follows it over USB-MIDI (vlink.h) and records the pictures with the sound (REC on the FM-1).
 * This header is the app's half of the contract with fm1-base (device/unity.c, host/plat_host.c). */
#pragma once
#include <stdint.h>
#include "plat.h"

#define APP_NAME "CORTEX VIDEO"
#define APP_SPLASH_RGB RGB(184, 50, 90)
#define APP_BLOCK 256u                   /* frames per audio block: 5.8 ms at 44.1 kHz */
#ifdef APP_VERSION
#define CV_VERSION APP_VERSION
#else
#define CV_VERSION "DEV"
#endif

/* the store: the settings (the picture now, the screen's), the panel's calibration where every CORTEX left it, the
 * sixteen scenes. Beside CORTEX on the same FM-1: its project (0xFC000), patches (0xDE000), tape, reel and banks are
 * left alone; CORTEX VIDEO's two objects take CORTEX's fourth song slot (0xD6000 .. 0xDA000, an A/B pair each) */
enum { OBJ_SET, OBJ_PANEL, OBJ_SCENES, OBJ_NOBJ };
#define APP_STORE_MAP {0xD6000u, 0xDC000u, 0xD8000u}

/* the contract: the platform calls these */
void app_init(void);
void app_render(int32_t *lr, uint32_t n, uint32_t gain_q12);
void app_panic(void);
void app_ui_init(void);
void app_frame(void);
void app_input_only(void);
void app_say(const char *a, const char *b);
#define APP_SYSEX                        /* the mirror's link (vlink.h) */
int app_sysex(const uint8_t *p, uint32_t n);

/* the host simulator's checks (expect): page, parN (a setting), link, rec, frames, sent, held, scene, bpm */
#define APP_HOST_EXPECT
int app_expect(const char *what, int *got);
