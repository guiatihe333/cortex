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
/* The Cortex preview service is advertised only while an external Visual is selected. */
#define APP_BLE_VIDEO
#define APP_BLE_AT_BOOT 0
#define APP_BLE_STORE
#define BLE_DEVICE_NAME "Cortex Video"
#define APP_BLE_VIDEO_SERVICE 0x01,0x4E,0x3B,0x22,0xF7,0x78,0x06,0x9C,0xE1,0x47,0x64,0xA8,0x2E,0x15,0x5F,0xC0
#define APP_BLE_VIDEO_VALUE   0x02,0x4E,0x3B,0x22,0xF7,0x78,0x06,0x9C,0xE1,0x47,0x64,0xA8,0x2E,0x15,0x5F,0xC0
int app_ble_video_write(const uint8_t *data, uint16_t length);

#define APP_HOST_EXPECT
int app_expect(const char *what, int *got);
