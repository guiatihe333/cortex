/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal */
/* AMB-1 on fm1-base: the app's unity build (compiled into fm1-base's, after the platform) and the glue between
 * fm1-base's contract (app.h) and AMB-1's parts: the project (project.c), the UI (ui.c) and the engine (dsp/amb.c,
 * its own -O2 unit). */
/* Timing only the second core's audio jobs: one writer on that core, aligned
 * 32-bit snapshots on the UI core. The dispatcher barriers publish the callback. */
static volatile uint32_t cpu2_job_ticks, cpu2_job_max_ticks;
static void (*cpu2_job_fn)(uint32_t);
static void cpu2_timed_job(uint32_t arg)
{
    const uint32_t t = plat_ticks24();
    uint32_t dt;
    cpu2_job_fn(arg);
    dt = plat_ticks24() - t;
    cpu2_job_ticks += dt;
    if (dt > cpu2_job_max_ticks)
        cpu2_job_max_ticks = dt;
}
static int cpu2_audio_run(void (*fn)(uint32_t), uint32_t arg)
{
    if (!plat_cpu2_on() || plat_cpu2_busy())
        return 0;
    cpu2_job_fn = fn;
    return plat_cpu2_run(cpu2_timed_job, arg);
}

#include "bank.c"
#include "project.c"
#include "ui.c"

/* the engine's MIDI out (clock, notes when MIDI out is on): to USB */
void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2)
{
    if (st < 0xF0u && (st & 15u) < EN_NVOX)      /* a voice's: on its channel (Setup's MIDI page) */
        st = (st & 0xF0u) | ((uint32_t)(proj.par[P_ST(CH1) + (int)(st & 15u)] - 1) & 15u);
    plat_midi_out(st >> 4 | st << 8 | d1 << 16 | d2 << 24);
}

/* ---- the contract */
void app_init(void)
{
    en_init();
    if (plat_cpu2_set(1) == 0 && plat_cpu2_on()) {   /* (1.5.22: the second core, a part of the render; else one core) */
        en_job_run = cpu2_audio_run;
        en_job_wait = plat_cpu2_wait;
    }
    project_load();                      /* defaults when nothing (or another format) is saved */
    project_reel_load();                 /* the reel, as it was saved */
    project_apply();
}
void app_render(int32_t *lr, uint32_t n, uint32_t gain_q12)
{
    const int16_t *usb;
    const uint32_t k = plat_usb_audio_in(&usb);   /* (what plays in over USB, the last render's: SOURCE USB) */
    en_usb(k ? usb : 0, k < n ? k : n);
    en_load((int)plat_cpu_pct());               /* (the voices held to it) */
    en_render(lr, n, gain_q12);
}
void app_overload(void) { en_overload(); }
void app_panic(void)
{
    en_play(0);
    en_panic();
}
void app_ui_init(void) { ui_init(); }
void app_frame(void) { ui_frame(); }
void app_input_only(void) { ui_input_only(); }
void app_say(const char *a, const char *b) { ui_say(a, b); }

#ifdef PLAT_HOST
#include "host.c"                        /* the host simulator's part: checks and commands */
#endif
