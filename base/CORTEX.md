# base/ in CORTEX

This is [fm1-base](https://github.com/charlesvestal/fm1-base) at `3ae2a65` with
[felucca-bsp](https://github.com/hugelton/felucca-bsp) at `f878800` in `bsp/`, copied into the tree, with two changes:

1. `include/plat.h`, `device/plat_fm1.c`, `host/plat_host.c`: `plat_usb_audio_in()`, the USB audio a computer or a
   phone plays into the FM-1, mono, for the app (the reel records it: SOURCE USB); the simulator's `usbaudio HZ`
   plays a tone into it.
2. `bsp/chip/usb.c`: `uac_out_tap()`, the last render's USB audio as it was played, and the USB output ring moved to
   the pool to keep 8 KiB of RAM.

Everything else is fm1-base's and felucca-bsp's, under their licences (MPL-2.0).
