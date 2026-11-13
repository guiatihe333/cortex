# Expression CPU measurements

Host microbenchmark, 1,000,000 calls per sample; median of seven alternating runs. Units: ns/call. Not FM-1 CPU percentages. Baseline changes only Density/Duration source reads back to knob values; observation overhead is not included in this comparison.

| Source implementation | Sources | Amount 0 | Routes off | 8 routes |
|---|---:|---:|---:|---:|
| knob-sources | 63.6 | 71.4 | 73.8 | 144.8 |
| audio-sources | 57.8 | 76.9 | 72.6 | 144.4 |

Density = rendered note count / 4, capped at 100%. Duration = average sounded age of rendered notes, capped at four seconds. Keyboard and release tails count. Observations are published with one 32-sample block of latency (0.73 ms at 44.1 kHz). Added persistent state: 4 bytes per note plus 18 bytes of per-voice aggregates before alignment. No additional synthesis voices or flash format change.

Potential optimizations: cache route smoothing coefficients on parameter changes; update live meters less often than audio routes; profile FM rendering, space and video independently on the device before moving jobs between cores.

Additional observation benchmark (four rendered notes plus publication/reset): median 13.0 ns per simulated audio block over seven runs. This is an isolated helper benchmark, not a whole-engine before/after timing.

Reproduce: `cc -O2 -ffp-contract=off -std=c99 -DEN_HOST -Iapp/dsp -Ibase/include -o app/build/tests/expression_bench tests/host/expression_bench.c app/dsp/fm_morph.c -lm && app/build/tests/expression_bench`.

Validation: `sh tests/run_tests.sh` passed all enabled tests, including sequencer, expression bypass, expression recipes, migration, audio, and navigation. Browser builds skipped because emcc was unavailable.
