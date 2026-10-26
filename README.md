# AMB-1 com as vozes do motor FM6 do JIANT

Modificação do firmware alternativo **AMB-1** (Charles Vestal, <https://github.com/charlesvestal/fm1-amb>) para o
M-VAVE FM-1: as vozes passam a ser geradas pelo **motor FM6 do JIANT FM** (<https://github.com/juanjiant-bit/JIANT>),
FM de 6 operadores no estilo DX7 (msfa, o núcleo do Dexed, portado para C inteiro).

Base usada: `charlesvestal/fm1-amb` no commit `188b0a4`. Motor: `juanjiant-bit/JIANT`,
`firmware/src/fm6_core.c` no commit `8580aea`.

## O que mudou

| Camada | Antes (AMB-1) | Agora |
| --- | --- | --- |
| PAD (4 vozes) | Rhodes FM de 2 operadores em float / "Voices": serras em filtros de formante | Nota FM6 sustentada com patch próprio: **AMB RHODES** ou **AMB VOICES** (2 portadoras com ~2,4 cents de diferença, uma com feedback, vibrato) |
| BELLS (6 vozes) | Sino FM de 2 operadores em float | Sino FM6, um patch por loop (4 timbres); Bell tone controla razão, índice e ataque |
| BASS | Seno + suboitava + 2º harmônico | Patch FM6 **AMB BASS** (seno, suboitava -4,4 dB, 2º harmônico -16 dB, um pouco de modulação) |
| PULSE | Bumbo e hats sintetizados | **Sem mudança** (percussão, não é voz) |

Continua igual a parte do AMB-1 que *toca* as vozes: respirações do pad, loops de fita, glide do baixo, acordes,
eco de fita, Echoplex e o plate de Dattorro. Os controles também: **Bright** e o arco agora controlam o nível dos
moduladores FM6 (o pad fica mais brilhante quando cresce), **Bell tone** reconstrói os patches dos sinos, **Pad tone**
alterna entre Rhodes e Voices, **Tune** chega ao FM6.

## Motor de modulação generativa (página MOD)

Uma seção nova nos parâmetros das vozes. **Como chegar:** aperte de novo o botão da página de uma voz (ENV no Pad,
ARP nos Bells, LFO no Bass) ou gire o SELECT depois de Setup. Na página MOD, os três botões das vozes ficam acesos.

| Knob | Parâmetro | O que faz |
| --- | --- | --- |
| 1 | **Depth** (0–100) | Profundidade da modulação. Em 0 as vozes soam exatamente como sem o motor. |
| 2 | **Rate** (0–100) | Duração de um ciclo, de 60 s a 0,5 s. Cada voz anda um pouco mais rápido ou mais devagar. |
| 3 | **Chaos** (0–100) | Forma do movimento: 0 é uma onda lenta; 50 é um passeio aleatório, deslizando entre valores sorteados; 100 é o mapa logístico na borda do caos, com saltos suavizados em ~5 ms. |
| 4 | **Target** | O que se move. **Timbre:** nível dos moduladores FM, ±12 dB. **Metal:** moduladores desafinados entre si, ±60 cents, dando bandas laterais inarmônicas. **Pitch:** afinação da voz, ±30 cents (wow de fita). **Space:** posição no estéreo. **All:** um pouco de cada. |

Como funciona:
- **Um gerador independente por voz**: 4 do pad, 6 dos sinos e 1 do baixo. Os ritmos são espalhados pela razão
  áurea, para que as vozes nunca se movam juntas.
- **Ligado ao arco do AMB-1**: a modulação se aprofunda quando o jardim está cheio e se acalma quando está esparso.
- **Gerador aleatório próprio**: notas, loops e acordes continuam exatamente como antes.
- **Receitas das peças**: cada peça ganhou a sua. Garden usa Timbre suave, Night Bus e Glass usam Metal, Afternoon e
  Shoreline usam Space, Departures usa Pitch e Tides usa All.
- **Projetos salvos**: o formato do projeto foi para o 5. Projetos salvos nos formatos 4 (AMB-1 atual) e 3 (0.9) são
  migrados, e os parâmetros novos vêm da receita da peça.
- **Também incluídos**: o leitor de tela fala a página nova, o app web ganhou o grupo "Mod" e o manual foi atualizado.

Arquivos (em `files/`, os mesmos que o patch cria ou altera):

- `app/dsp/fm6_core.c`: o núcleo FM6 do JIANT (Apache-2.0). Única alteração: as multiplicações de 64 bits saíram das
  linhas de código quente. O fm1-base documenta (`bsp/docs/CPU.md`) que a CPU do FM-1 às vezes corrompe um registrador
  quando multiplicações 32x32→64 ficam ao lado de leituras de 16 bits, exatamente o que o loop dos operadores faz.
  Agora o loop multiplica em 32 bits com uma decomposição exata (`fm6_mulg`), e os produtos por bloco passam por
  funções `noinline`. O som é idêntico bit a bit.
- `app/dsp/fm6_tables.h` e `tools/gen_fm6_tables.py`: tabelas do msfa (exp2, LFO, detune, fine, seno Q15),
  geradas por um script próprio.
- `app/dsp/amb.c`: as vozes em FM6. Elas são renderizadas em blocos de 32 amostras (o bloco do FM6), e uma nota
  começa no bloco seguinte, no máximo 0,7 ms depois. Patches escritos do zero em `fm_patches()` e `bell_patches()`.
- `app/dsp/amb.h`, `README.md`, `LICENSING.md`, `LICENSES/Apache-2.0-msfa.txt`: documentação e licenças.
- Página MOD: o motor fica em `amb.c` (`mod_step`, `mod_of`). Também mudaram `amb.h` (parâmetros `P_MDEPTH` a
  `P_MDEST`), `ui.c` (página e leitor de tela), `project.c` e `app.h` (formato 5 com migração), `web/app/index.html`,
  `docs/MANUAL.md`, `tests/host/amb_test.c` (checagens do motor) e `tests/scenarios/mod.amb` (navegação).

`amb1-fm6.patch` traz tudo de uma vez. Em `patches/` estão os dois passos separados (vozes FM6 e depois MOD), para
`git am`.

**Licença:** o resultado continua **MPL-2.0** como o AMB-1. Do JIANT entrou só o `fm6_core.c`, que é Apache-2.0
(compatível). Nada do código GPL do JIANT ou do Felucca entrou: nem o `eng_fm6.c`, nem os patches de fábrica, nem o
gerador de tabelas.

## Como compilar e instalar

```sh
git clone --recursive https://github.com/charlesvestal/fm1-amb
cd fm1-amb
git checkout 188b0a4 && git submodule update --init --recursive
git apply /caminho/para/amb1-fm6.patch       # ou copie o conteúdo de files/ por cima da árvore
sh tests/run_tests.sh                        # testes do instrumento e simulador
./build.sh                                   # gera app/build/amb.fwsc (precisa do toolchain JieLi + SDK AC79, ver BUILDING.md)
python3 base/tools/fm1_install.py app/build/amb.fwsc
```

O emulador no navegador (`./build.sh --web`, precisa do emcc) deixa ouvir tudo antes de gravar no aparelho.
Instalar firmware no FM-1 é por sua conta e risco: o instalador web do AMB-1 consegue restaurar o firmware de fábrica
da M-VAVE.

## O que ainda NÃO foi verificado

- O código compila sem avisos (gcc e clang, com as flags estritas do teste de host do AMB-1: `-Wall -Wextra
  -Wdouble-promotion -Werror`). A interface (`ui.c`, `project.c`) também passou pelo build do simulador de host, com
  cabeçalhos de fonte provisórios só para a checagem.
- **Os testes não foram executados** (`tests/run_tests.sh`, o teste do instrumento, o simulador). O ambiente onde a
  modificação foi feita não tinha permissão para executar código vindo de repositórios externos.
- **Não houve build para o aparelho**: faltam o toolchain JieLi e o SDK AC79. Por isso as checagens de imagem, RAM e
  CPU do `build.py` ainda não rodaram.
- **Os timbres não foram ouvidos.** Os níveis e envelopes dos patches foram calculados a partir da matemática do msfa
  para reproduzir o volume e o decaimento das vozes originais, mas vale ouvir e ajustar: os números estão em
  `fm_patches()` e `bell_patches()`, e `PAD_OUT` define o volume do pad.
- **CPU**: no pior caso são 11 notas FM6 (4 do pad, 6 dos sinos e 1 do baixo). A estimativa é de uns 10 % a mais de
  CPU que as vozes originais a 360 MHz. Se faltar fôlego, reduza `NBELL`.
