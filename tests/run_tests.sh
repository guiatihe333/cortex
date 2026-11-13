#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Charles Vestal
# AMB-1's tests: the instrument (tests/host/amb_test.c), the whole app in fm1-base's simulator (tests/scenarios/*.amb:
# UI, chords, rhythm, persistence, the screen reader), the browser builds, and the update path against AMB-1's package
# (./build.sh first). Screenshots and audio in app/build/scenarios/. fm1-base's own tests: base/tests/run_tests.sh.
#   tests/run_tests.sh
set -u
cd "$(dirname "$0")/.."
CC="${CC:-cc}"
PY="${PYTHON:-python3}"
export AC79_SDK="${AC79_SDK:-$HOME/fw-AC79_AIoT_SDK}"   # the package's SDK files, as ./build.sh
B=app/build
OUT=$B/tests
FAIL=0
mkdir -p "$OUT" $B/scenarios
run() {
    name="$1"; shift
    if "$@" > "$OUT/$name.log" 2>&1; then
        echo "  ok   $name"
    else
        echo "  FAIL $name (see $OUT/$name.log)"
        tail -15 "$OUT/$name.log" | sed 's/^/       /'
        FAIL=1
    fi
}
run amb sh -c "$CC -O2 -ffp-contract=off -std=c99 -Wall -Wextra -Wdouble-promotion -Werror -DEN_HOST -Iapp/dsp -Ibase/include \
    -o $OUT/amb_test tests/host/amb_test.c app/dsp/amb.c app/dsp/fm_morph.c -lm && $OUT/amb_test"
run morph sh -c "$CC -O2 -ffp-contract=off -std=c99 -Wall -Wextra -Wdouble-promotion -Werror -DEN_HOST -Iapp/dsp -Ibase/include \
    -o $OUT/morph_test tests/host/morph_test.c app/dsp/fm_morph.c app/dsp/amb.c -lm && $OUT/morph_test"
run expression sh -c "$CC -O2 -ffp-contract=off -std=c99 -Wall -Wextra -Wdouble-promotion -Werror -DEN_HOST -Iapp/dsp -Ibase/include \
    -o $OUT/expression_test tests/host/expression_test.c app/dsp/fm_morph.c -lm && $OUT/expression_test"
run expression-bypass "$PY" -c 'import subprocess,sys; p=sys.argv[1]; a=subprocess.check_output([p,"hash-plain"]); b=subprocess.check_output([p,"hash-configured"]); assert a==b,(a,b); print("Amount 0: sample-identical in fresh processes",a.decode().strip())' "$OUT/expression_test"
run filter sh -c "$CC -O2 -ffp-contract=off -std=c99 -Wall -Wextra -Wdouble-promotion -Werror -DEN_HOST -Iapp/dsp -Ibase/include \
    -o $OUT/filter_test tests/host/filter_test.c app/dsp/fm_morph.c -lm && $OUT/filter_test"
run seq sh -c "$CC -O2 -ffp-contract=off -std=c99 -Wall -Wextra -Wdouble-promotion -Werror -DEN_HOST -Iapp/dsp -Ibase/include \
    -o $OUT/seq_test tests/host/seq_test.c app/dsp/fm_morph.c -lm && $OUT/seq_test"
if [ -f $B/cortex.fwsc ]; then
    run ota-entry sh -c "$CC -w -o $OUT/ota_test base/tests/ota_test.c && $OUT/ota_test $B/cortex.fwsc"
    run update-loader sh -c "head -c 100000 $B/cortex.bin > $OUT/old_app.bin && \
        $PY base/tools/fm1pkg_make.py $OUT/old_app.bin $B/loader/ota.bin $OUT/old.fwsc >/dev/null && \
        $CC -w -o $OUT/ldr_test base/tests/ldr_test.c && $OUT/ldr_test $OUT/old.fwsc $B/cortex.fwsc"
else
    echo "  skip update path (no $B/cortex.fwsc: ./build.sh)"
fi
if command -v node >/dev/null 2>&1; then              # tools/FITA.html's reel coding (its CORE), against the FM-1's
    run fita node tests/host/fita_test.js tools/FITA.html $OUT/fita.syx
else
    echo "  skip fita (no node)"
fi
run host-build sh build.sh --host
run video-canvas sh -c "$CC -O2 -std=c99 -Wall -Wextra -Wno-unused-function -Werror -I$B/gen \
    -o $OUT/video_canvas_test tests/host/video_canvas_test.c && $OUT/video_canvas_test"
if command -v emcc >/dev/null 2>&1; then
    run emu sh -c "sh build.sh --web && node tests/host/emu_test.mjs $B/emu/app.wasm"
    echo "  skip app (web/app, AMB-1's engine-only web app, is not kept up in CORTEX: no pulse since 1.4.0)"
else
    echo "  skip emu, app (no emcc)"
fi
for s in tests/scenarios/*.amb; do
    n=$(basename "$s" .amb)
    mkdir -p "$B/scenarios/$n"
    run "scenario-$n" $B/host/cortex_host "$s" "$B/scenarios/$n"
done
[ $FAIL -eq 0 ] && echo "all tests passed" || echo "TESTS FAILED"
exit $FAIL
