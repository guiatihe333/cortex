# AMB-1 licensing

AMB-1 is free software under the **Mozilla Public License 2.0** (`MPL-2.0`, full text in `LICENSE`), file by file.
If you change one of its files and share the result, share that file's source under the MPL. It is built on
[fm1-base](https://github.com/charlesvestal/fm1-base) and [felucca-bsp](https://github.com/hugelton/felucca-bsp)
(`base/`, a git submodule), both MPL-2.0.

Until 0.9, AMB-1 was GPL-3.0-only, built on Felucca's platform layer. That code is gone from this tree: the platform
is fm1-base's now, and AMB-1's own code (the engine, the UI, the pieces) is relicensed by its author. AMB-1 0.9 and
earlier stay under the GPL.

## Where the code comes from

| What | Origin | Licence |
| --- | --- | --- |
| The platform: `base/` (boot, USB and USB audio, the TRS jack, storage, the update path, screen, panel, the screen reader, the host simulator and the browser emulator, build and install tools) | [fm1-base](https://github.com/charlesvestal/fm1-base) and [felucca-bsp](https://github.com/hugelton/felucca-bsp), from [Felucca](https://github.com/hugelton/Felucca) by Leo Kuroshita (@kurogedelic), Hügelton Instruments, under MPL-2.0 with his permission ([felucca-bsp#4](https://github.com/hugelton/felucca-bsp/issues/4)) | MPL-2.0 |
| `app/` (`app.c`, `app.h`, `ui.c`, `project.c`, `host.c`, `dsp/amb.c` with its Dattorro plate from FoMni-1, `dsp/fm6_tables.h` made by `tools/gen_fm6_tables.py`), `web/app/`, `tests/`, `tools/` | AMB-1, Charles Vestal | MPL-2.0 |
| AMB-1 FM6's own: `app/bank.c` (the DX7 banks over SysEx), `app/dsp/reel.c` (the reel: IMA ADPCM, after the public IMA/DVI algorithm; its genes after Make Noise's Morphagene, the idea only, no code), `tools/amb_syx.py`, and the changes through `app/` | AMB-1 FM6 | MPL-2.0 |
| The idea of a browser emulator for FM-1 firmware (fm1-base's emulator) | [Groove OS](https://www.groove-os.com/) by Peter Gombos, whose site runs its firmware in the browser; no code from it | credit only |

## Third-party material

| What | Licence | Where |
| --- | --- | --- |
| Fonts: Barlow Semi Condensed (The Barlow Project Authors), Terminus (Dimitar Toshkov Zhekov) | SIL OFL 1.1 | `base/assets/fonts/` |
| JieLi AC79 SDK: `uboot.boot`, `cfg_tool.bin`, `eq_cfg_hw.bin` are read from your SDK checkout at build time and placed in the package; no SDK files are in this tree | Apache-2.0 | <https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK> |
| The FM6 engine of the voices (pad, bells, bass): msfa by Google Inc. and Pascal Gauthier, from [Dexed](https://github.com/asb2m10/dexed), as ported to integer C by Leo Kuroshita (Hügelton Instruments) and carried in [JIANT FM](https://github.com/juanjiant-bit/JIANT) by Juan Ignacio Antonucci (`firmware/src/fm6_core.c` at 8580aea), changed here to keep 64-bit multiplies out of line, and by AMB-1 FM6 to let a sounding note take its patch's edits (its header says how). Only this file is taken: none of JIANT's or Felucca's GPL code (the FM6 engine around it, its factory patches, its table generator) is in this tree; AMB-1's patches and `tools/gen_fm6_tables.py` are written anew | Apache-2.0 | `app/dsp/fm6_core.c`, `LICENSES/Apache-2.0-msfa.txt` |

DX7 voices are not in this tree or in the firmware: the banks are the user's own, sent over USB (`tools/amb_syx.py`).

## Trademarks

"Felucca" and "Hügelton Instruments" are names of Hügelton Instruments. "M-VAVE" and "FM-1", "Yamaha" and "DX7", "Make Noise" and "Morphagene", "TASCAM" are trademarks of their
respective owners, used here only to say what hardware AMB-1 runs on. AMB-1 is independent firmware; it is not
affiliated with, endorsed by or supported by any of them.

## Radio

AMB-1 never enables the Bluetooth / Wi-Fi radio of the hardware.
