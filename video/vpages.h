/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 guiatihe333 */
/* CORTEX VIDEO's pages: four knobs each, in groups (SOURCE, OBJECT, SHAPE, FX, REACT, PERFORM, SETUP). The FM-1's panel
 * (app.c) and the computer's mirror (mirror/: its controls) read the same table. A knob is a setting (ST_*) or one of
 * the FM-1's own (K_*: the slot, the tempo, the scenes, the matrix), which the mirror leaves out */
#pragma once
#include "vsynth.h"

enum { K_SLOT = -1, K_TEMPO = -2, K_NONE = -3, K_SCN = -4, K_SQ = -5, K_SMORPH = -6, K_SCHAIN = -7, K_MKEY = -8,
       K_MFN = -9, K_MPAR = -10, K_MVAL = -11 };
enum { GR_SOURCE, GR_OBJECT, GR_SHAPE, GR_FX, GR_REACT, GR_PERFORM, GR_SETUP, GR_N };
static const char *const GROUP_NAME[GR_N] = {"SOURCE", "OBJECT", "SHAPE", "FX", "REACT", "PERFORM", "SETUP"};
static const struct {
    const char *name;
    uint8_t group;
    int8_t k[4];
} PAGES[] = {
    {"VISUAL", GR_SOURCE, {ST_VMODE, ST_VPAL, ST_VTRAIL, ST_VREACT}},
    {"OSC", GR_SOURCE, {ST_VOFREQ, ST_VOSHAPE, ST_VOANGLE, ST_VOSPEED}},
    {"OSC 2", GR_SOURCE, {ST_VOFREQ2, ST_VOCOMB, ST_VOWARP, ST_VOSYNC}},
    {"OBJECT", GR_OBJECT, {ST_OSHAPE, ST_OSIZE, ST_ODRAW, ST_OCOLOR}},
    {"OBJ MOTION", GR_OBJECT, {ST_OSPIN, ST_OTILT, ST_OZOOM, ST_OMORPH}},
    {"OBJ ARRAY", GR_OBJECT, {ST_OCOUNT, ST_OARRAY, ST_OSPREAD, ST_OREACT}},
    {"OBJ LOOK", GR_OBJECT, {ST_OSIDES, ST_OBLEND, ST_OLIGHT, ST_OPULSE}},
    {"SHAPE", GR_SHAPE, {ST_VGEO, ST_VSIDES, ST_VMIRROR, ST_VSPEED}},
    {"SHAPE 2", GR_SHAPE, {ST_VGEO2, ST_VSIDES2, ST_VSIZE2, ST_VPULSE}},
    {"MIX", GR_SHAPE, {ST_VLAYER, ST_VBLEND, ST_VMIX, ST_VSTROBE}},
    {"VIDEO", GR_FX, {ST_VFEED, ST_VRGB, ST_VHUE, ST_VSYNC}},
    {"TRANSFORM", GR_FX, {ST_VXZOOM, ST_VXROT, ST_VXSHX, ST_VXTGT}},
    {"LIFE", GR_FX, {ST_VMORPH, ST_VMUTATE, ST_VFLOW, ST_VCHAOS}},
    {"MOSH", GR_FX, {ST_VMOSH, ST_VSORT, ST_VCORRUPT, ST_VSLICE}},
    {"CHROMA", GR_FX, {ST_VFOLD, ST_VPOSTER, ST_VPIXEL, ST_VKEY}},
    {"REACT", GR_REACT, {K_SLOT, ST_AR1S, ST_AR1S + 1, ST_AR1S + 2}},
    {"REACT 2", GR_REACT, {K_SLOT, ST_AR1S + 3, ST_AR1S + 4, ST_AR1S + 2}},
    {"INPUT", GR_REACT, {ST_VGAIN, ST_VEAR, ST_VBRIGHT, ST_VINVERT}},
    {"SCENES", GR_PERFORM, {K_SCN, K_SQ, K_SMORPH, K_SCHAIN}},
    {"MATRIX", GR_PERFORM, {K_MKEY, K_MFN, K_MPAR, K_MVAL}},
    {"SCREEN", GR_SETUP, {ST_VBARS, ST_VDELAY, K_TEMPO, K_NONE}},
};
#define NPAGE ((int)(sizeof PAGES / sizeof *PAGES))
