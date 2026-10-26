#!/bin/sh
# Build AMB-1 FM6 (AMB-1 with JIANT FM's FM6 voices and the MOD page) into an installable FM-1 package, and its web
# installer when Emscripten is there. Linux x86-64 (natively) or macOS (the JieLi toolchain then runs in Docker).
#
#   sh build_release.sh [VERSION]          VERSION X.Y[.Z][-suffix], default 1.3.97-fm6
#
# What it does, in WORK (default ./work):
#   1. fm1-amb at 188b0a4 with its submodules, and the patches in patches/ (git am)
#   2. JieLi's Linux toolchain (pkgman.jieliapp.com) into ~/.jieli, unless JIELI_TOOLCHAIN is set
#   3. JieLi's AC79 SDK (gitee.com), sparse, into ~/fw-AC79_AIoT_SDK, unless AC79_SDK is set; the three files the
#      package takes from it are checked against the hashes fm1-base was tested with
#   4. AMB-1's tests (the instrument, the simulator, the scenarios)
#   5. ./build.sh --release VERSION  ->  work/fm1-amb/app/build/amb-VERSION.fwsc
#   6. with emcc on the PATH: the browser emulator and the site with the web installer, in work/site/
#
# Then install it (FM-1 on USB; OCT- and OCT+ held 5 s give the update mode if it does not answer):
#   pip3 install mido python-rtmidi
#   python3 work/fm1-amb/base/tools/fm1_install.py work/fm1-amb/app/build/amb-VERSION.fwsc
# or open work/site/install/index.html in Chrome or Edge (Web MIDI; serve the folder: python3 -m http.server).
# Installing firmware is at your own risk; the web installer can put M-VAVE's own firmware back.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
V="${1:-1.3.97-fm6}"
WORK="${WORK:-$PWD/work}"
PY="${PYTHON:-python3}"
mkdir -p "$WORK"
cd "$WORK"

echo "== 1. fm1-amb + the FM6 patches"
if [ ! -d fm1-amb ]; then
    git clone -q https://github.com/charlesvestal/fm1-amb
    (cd fm1-amb && git checkout -q 188b0a4 && git submodule update -q --init --recursive)
    (cd fm1-amb && git -c user.name="AMB-1 FM6" -c user.email="noreply@example.com" am -q "$HERE"/patches/*.patch)
fi
cd fm1-amb

echo "== 2. the JieLi toolchain"
if [ -z "$JIELI_TOOLCHAIN" ]; then
    [ -e "$HOME/.jieli/toolchain/pi32v2/bin/clang" ] || sh base/tools/get_toolchain.sh "$HOME/.jieli"
    export JIELI_TOOLCHAIN="$HOME/.jieli/toolchain"
fi

echo "== 3. the AC79 SDK"
if [ -z "$AC79_SDK" ]; then
    export AC79_SDK="$HOME/fw-AC79_AIoT_SDK"
    if [ ! -f "$AC79_SDK/cpu/wl82/tools/uboot.boot" ]; then
        git clone -q --depth 1 --filter=blob:none --sparse --branch AC79NN_SDK_V1.2.1_2023-12-13 \
            https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK.git "$AC79_SDK"
        (cd "$AC79_SDK" && git sparse-checkout set cpu/wl82/tools)
    fi
fi
check() {                                   # sha256 of a file of the SDK, portably
    [ "$("$PY" -c 'import hashlib,sys; print(hashlib.sha256(open(sys.argv[1],"rb").read()).hexdigest())' "$AC79_SDK/$1")" = "$2" ]
}
check cpu/wl82/tools/uboot.boot        4e3b4c220dc96641cb5a723f41e68ce41d5261ae9434bb33fbd7f2c59976ded4 &&
check cpu/wl82/tools/cfg_tool.bin      276579954f076886a6a7694f65dc71c034a63a2c204b76749065c0ac7b010d1b &&
check cpu/wl82/tools/cfg/eq_cfg_hw.bin 41167491bffed4651750719c973d2758adeb9021a5670d02d6a53c85ed80ea7d ||
    { echo "the AC79 SDK's files are not the ones fm1-base was tested with: stop"; exit 1; }

echo "== 4. tests"
if [ "$(uname -s)" = Linux ]; then          # AMB-1's scripts assume macOS: glibc needs -lm and _DEFAULT_SOURCE
    export CC="$HERE/analysis/cc-linux"
fi
sh tests/run_tests.sh

echo "== 5. the package"
"$PY" -c 'import PIL' 2>/dev/null || { echo "needs Pillow: pip3 install Pillow"; exit 1; }
sh build.sh --release "$V"
PKG="app/build/amb-$V.fwsc"
ls -l "$PKG"
sh tests/run_tests.sh                       # again, now with the package: the update path's tests run too

echo "== 6. the web installer"
if command -v emcc >/dev/null 2>&1; then
    sh build.sh --web --release "$V"
    rm -rf "$WORK/site"
    "$PY" tools/make_pages.py "$PKG" "$V" "$WORK/site"
    echo "site: $WORK/site (install/ is the web installer)"
else
    echo "(no emcc: no browser emulator and web installer; install with base/tools/fm1_install.py)"
fi
echo "done: $WORK/fm1-amb/$PKG"
