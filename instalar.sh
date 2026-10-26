#!/bin/sh
# Instala o AMB-1 FM6 no FM-1 ligado neste computador (Linux x86-64, ou macOS com Docker para o toolchain).
#
#   sh instalar.sh [VERSAO]          VERSAO padrao 1.3.91-fm6
#
#   1. prepara um Python isolado (work/.venv) com mido, python-rtmidi e Pillow
#   2. procura o FM-1 no USB e mostra o firmware que ele roda agora (antes do build: falha cedo)
#   3. compila o pacote se ainda nao existir (build_release.sh: toolchain JieLi, SDK AC79, testes, .fwsc)
#   4. instala, pedindo confirmacao. Nao desligue o FM-1 enquanto grava.
#
# O FM-1 nao responde? Segure OCT- e OCT+ por 5 s (modo de atualizacao) e rode de novo.
# Voltar atras: o instalador web do AMB-1 (https://charlesvestal.github.io/fm1-amb/install/, Chrome ou Edge) tem
# "Back to the stock firmware", que devolve o firmware de fabrica da M-VAVE.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
V="${1:-1.3.91-fm6}"
export WORK="${WORK:-$HERE/work}"
mkdir -p "$WORK"

echo "== 1. Python (mido, python-rtmidi, Pillow)"
[ -x "$WORK/.venv/bin/python" ] || python3 -m venv "$WORK/.venv"
export PYTHON="$WORK/.venv/bin/python"
if ! "$PYTHON" -c 'import mido, rtmidi, PIL' 2>/dev/null; then
    "$WORK/.venv/bin/pip" install -q mido python-rtmidi Pillow || {
        echo "nao consegui instalar o python-rtmidi (o acesso ao MIDI do Python)."
        echo "  Linux (Debian/Ubuntu): sudo apt install build-essential libasound2-dev libjack-dev python3-dev"
        echo "  macOS: xcode-select --install"
        echo "e rode de novo."
        exit 1
    }
fi

if [ ! -d "$WORK/fm1-amb" ]; then             # so o codigo (o build_release.sh aplica os patches e compila)
    git clone -q https://github.com/charlesvestal/fm1-amb "$WORK/fm1-amb"
    (cd "$WORK/fm1-amb" && git checkout -q 188b0a4 && git submodule update -q --init --recursive &&
        git -c user.name="AMB-1 FM6" -c user.email="noreply@example.com" am -q "$HERE"/patches/*.patch)
fi
INSTALL="$WORK/fm1-amb/base/tools/fm1_install.py"

echo "== 2. o FM-1"
set +e
INFO="$("$PYTHON" "$INSTALL" --info 2>&1)"
RC=$?
set -e
echo "$INFO"
if [ $RC -eq 3 ]; then
    echo "FM-1 nao encontrado. Confira o cabo USB (de dados, nao so de carga), ligue o FM-1 e feche outros"
    echo "programas que usem MIDI (DAW, editor web). Se ainda nao aparecer: OCT- + OCT+ por 5 s e rode de novo."
    exit 3
elif [ $RC -ne 0 ]; then
    case "$INFO" in
        *"ALSA sequencer"*)
            echo "O Linux nao deu acesso ao MIDI (sequenciador ALSA). Tente: sudo modprobe snd-seq"
            echo "e confira se seu usuario esta no grupo audio (sudo usermod -aG audio \$USER, depois saia e entre)." ;;
        *) echo "erro ao falar com o FM-1 pelo MIDI (codigo $RC): veja a mensagem acima" ;;
    esac
    exit $RC
fi
case "$INFO" in
    *FM-1_015*) ;;                               # M-VAVE's V15: what every FM-1 installer was made and tested on
    *FM-1_0[0-9][0-9]*)
        if [ -z "$FORCE_STOCK" ]; then
            echo "O FM-1 roda outro firmware de fabrica da M-VAVE (V15 e o unico em que os instaladores foram testados;"
            echo "o V16, de 21/10/2026, nao foi testado por ninguem). Para nao arriscar, volte ao V15 primeiro: no"
            echo "M-UPGRADE da M-VAVE, com o arquivo oficial FM-1.fwsc da versao V15, e rode este script de novo."
            echo "(Por sua conta e risco: FORCE_STOCK=1 sh instalar.sh tenta mesmo assim.)"
            exit 6
        fi ;;
    *FM1_OFFICIAL_OPEN*|*FM1_COMMUNITY_*)
        echo "O FM-1 roda o firmware FM1 Open da M-VAVE: ele so aceita o firmware de fabrica V15 por cima."
        echo "Volte primeiro ao V15 (M-UPGRADE da M-VAVE, ou o instalador web do AMB-1: Back to the stock"
        echo "firmware) e rode este script de novo."
        exit 6 ;;
esac

PKG="$WORK/fm1-amb/app/build/amb-$V.fwsc"
if [ ! -f "$PKG" ]; then
    echo "== 3. o pacote (alguns minutos)"
    sh "$HERE/build_release.sh" "$V"
fi

echo "== 4. instalar $PKG"
"$PYTHON" "$INSTALL" "$PKG"
