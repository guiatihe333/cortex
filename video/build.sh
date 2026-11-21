#!/bin/bash
# SPDX-License-Identifier: MPL-2.0
# CORTEX VIDEO: build, test, and the mirror.
#   video/build.sh            the tests, then the firmware (video/build/cortex-video.fwsc) and the mirror page
#   video/build.sh --release X.Y.Z   a release's package
#   video/build.sh --test     the tests alone (host: no toolchain)
#   JIELI_TOOLCHAIN, AC79_SDK, PYTHON as for CORTEX's build.sh; Emscripten (~/emsdk) for the mirror
set -e
cd "$(dirname "$0")/.."
export JIELI_TOOLCHAIN="${JIELI_TOOLCHAIN:-$HOME/.jieli/toolchain}"
export AC79_SDK="${AC79_SDK:-$HOME/fw-AC79_AIoT_SDK}"
export X0X_FONTSET="${X0X_FONTSET:-terminus}"
PY="${PYTHON:-python3}"
V=video
T="${TMPDIR:-/tmp}/cortex-video-test"
mkdir -p "$T" $V/build/mirror
[ -f "$HOME/emsdk/emsdk_env.sh" ] && . "$HOME/emsdk/emsdk_env.sh" >/dev/null 2>&1 || true

echo "== the engine"
${CC:-cc} -O2 -ffp-contract=off -std=gnu99 -Wall -Wextra -Werror -Wno-unused-function -I$V -Ibase/include \
    $V/tests/vsynth_test.c $V/vsynth.c -lm -o "$T/vsynth_test"
"$T/vsynth_test"
echo "== the simulated FM-1"
"$PY" base/tools/build.py $V --host >/dev/null
fail=0
for s in $V/tests/scenarios/*.cvx; do
    n=$(basename "$s" .cvx)
    if $V/build/host/cortex-video_host "$s" "$T" >"$T/$n.log" 2>&1 && ! grep -q FAIL "$T/$n.log"; then
        echo "scenario $n: ok"
    else
        echo "scenario $n: FAILED"; grep -E 'FAIL|line' "$T/$n.log"; fail=1
    fi
done
echo "== the mirror (natively, then in WebAssembly) against the FM-1's link"
${CC:-cc} -O2 -ffp-contract=off -std=gnu99 -Wall -Wextra -Werror -Wno-unused-function -I$V -Ibase/include \
    $V/tests/follow_test.c $V/vsynth.c -lm -o "$T/follow_test"
"$T/follow_test" "$T/link.syx" || fail=1
"$T/follow_test" "$T/stall.syx" 0 || fail=1   # (the computer stopped reading a while: no frame lost, no SYNC again)
if command -v emcc >/dev/null; then
    emcc -O2 -ffp-contract=off -std=gnu99 -Wall -Werror -Wno-unused-function -I$V -Ibase/include \
        -DVS_HD $V/mirror/mirror.c $V/vsynth.c --no-entry -sSTANDALONE_WASM -sINITIAL_MEMORY=24MB \
        -sEXPORTED_FUNCTIONS=_mx_in,_mx_rgba,_mx_msg,_mx_stat,_mx_lost_clear,_mx_beat,_mx_npage,_mx_page_name,_mx_page_group,_mx_page_knob,_mx_ngroup,_mx_group_name,_mx_npar,_mx_slot_w,_mx_nslot,_mx_par_name,_mx_par_lo,_mx_par_hi,_mx_par_get,_mx_par_eff,_mx_par_text,_mx_set_hd,_mx_size,_mx_tr1s,_mx_video_source \
        -o $V/build/mirror/mirror.wasm
    node $V/tests/mirror_test.mjs $V/build/mirror/mirror.wasm "$T/link.syx" || fail=1
    node $V/tests/mirror_test.mjs $V/build/mirror/mirror.wasm "$T/stall.syx" 0 || fail=1
    "$PY" - "$V/mirror/index.html" "$V/build/mirror/mirror.wasm" "$V/build/mirror/cortex-video-mirror.html" <<'EOF'
import base64, sys
page = open(sys.argv[1], encoding="utf-8").read()
b64 = base64.b64encode(open(sys.argv[2], "rb").read()).decode()
assert page.count("/*WASM_BASE64*/") == 1
open(sys.argv[3], "w", encoding="utf-8", newline="\n").write(page.replace("/*WASM_BASE64*/", b64))
print("mirror   " + sys.argv[3])
EOF
else
    echo "mirror: no emcc (Emscripten): the page is not built"
fi
[ $fail = 0 ] || { echo "TESTS FAILED"; exit 1; }
echo "tests: ok"
[ "$1" = "--test" ] && exit 0
exec "$PY" base/tools/build.py $V "$@"
