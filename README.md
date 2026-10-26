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
| 2 | **Rate** (0–100) | Duração de um ciclo, de 60 s a 0,12 s (até 8,5 Hz, velocidade de vibrato). Cada voz anda um pouco mais rápido ou mais devagar. |
| 3 | **Chaos** (0–100) | Forma do movimento: 0 é uma onda lenta; 50 é um passeio aleatório, deslizando entre valores sorteados; 100 é o mapa logístico na borda do caos, com saltos suavizados por 1/20 do segmento (8 a 150 ms). |
| 4 | **Target** | O que se move; veja a tabela abaixo. |

### Os targets: os parâmetros FM primeiro

| Target | Parâmetro FM6 | Faixa | Forma |
| --- | --- | --- | --- |
| **Timbre** | Nível dos moduladores (o índice FM) | ±12 dB | Bipolar, em dB, como o ouvido percebe o índice |
| **Ratio** | Razão de frequência dos moduladores | ×1, ×2, ×3, ×4, ×5 | Degraus harmônicos só para cima, com histerese e glide de 15 ms |
| **Metal** | Moduladores desafinados entre si (um sobe, o outro desce) | ±80 cents | Bipolar: bandas laterais inarmônicas, som de sino |
| **Feedback** | Feedback do operador 6, **contínuo** | até +9 passos | Unipolar, a partir do feedback do patch |
| **Decay** | Envelopes de cada nota nova: decaimento dos moduladores (o brilho) e das portadoras (a duração) | ±20 / ±6 rates | Aplicado no ataque de cada nota |
| **Pitch** | Afinação da voz | ±30 cents | Suavizado em 15 ms; no Rate máximo vira vibrato |
| **Space** | Posição no estéreo | — | Suavizado em 30 ms, com rampa de ganho por bloco |
| **All** | Timbre, Metal, Feedback, Decay, Pitch e Space, um pouco menos de cada | — | Sem Ratio, por causa dos degraus |

Como funciona:
- **Um gerador independente por voz**: 4 do pad, 6 dos sinos e 1 do baixo. Os ritmos são espalhados pela razão
  áurea, para que as vozes nunca se movam juntas.
- **Ligado ao arco do AMB-1**: a modulação se aprofunda quando o jardim está cheio e se acalma quando está esparso.
- **Gerador aleatório próprio**: notas, loops e acordes continuam exatamente como antes.
- **Receitas das peças**: cada peça ganhou a sua. Garden usa Timbre, Glass usa Ratio, Night Bus usa Metal, Voices
  usa Feedback, Shoreline usa Decay, Departures usa Pitch, Afternoon usa Space e Tides usa All.
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

`amb1-fm6.patch` traz tudo de uma vez. Em `patches/` estão os quatro passos separados (vozes FM6, página MOD,
targets FM, correção do Metal), para `git am`.

## A modulação é adequada para síntese FM? O que foi medido

Ferramentas próprias em `analysis/`. Elas medem cada camada sozinha, sem efeitos, por 30 s: centroide espectral
(brilho), periodicidade (1,0 = perfeitamente harmônico), pitch e um detector de cliques que varre todas as amostras.

| Problema encontrado | Correção | Medida depois |
| --- | --- | --- |
| **Feedback** do msfa é inteiro (0..7, degraus de 6 dB): modular dá zipper | Feedback contínuo no núcleo (ganho Q12 em vez de shift) | Idêntico ao JIANT com feedback 0 (bit a bit) e -58 a -104 dB de diferença com 1..7 |
| **Feedback** sem efeito: os patches tinham o operador com feedback mudo, e no DX7 o feedback é proporcional à saída do próprio operador | Pares renumerados (o modulador principal virou o op6, sem mudar o som) e faixa até ganho 1 | Sinos: oscilação do centroide de ±605 para ±1112 Hz |
| **Ratio** ×1/2 fazia a voz soar uma oitava abaixo (bandas laterais em f/2) | Só degraus para cima, ×1 a ×5 | Periodicidade 0,876 → 0,904 (sem modulação: 0,925); picos dos sinos 13,8x → 4,1x |
| **Space** mudava o ganho em degraus a cada bloco de 0,7 ms (zipper, ~-66 dB) | Rampa de ganho dentro do bloco | Pico 11,3x → 4,8x, igual ao sinal sem modulação |
| Pitch e Space suavizados demais (40/80 ms cortavam o vibrato pela metade) | 15/30 ms | Pitch medido: ±0,21 st, exatamente o teórico de ±0,3 st senoidal |
| Rate só ia até 0,5 s | Até 0,12 s (8,5 Hz) | — |

O que já estava adequado e foi confirmado:
- **Timbre** em dB move o centroide (pad +5 a +9%, sinos ±1195 Hz). O FM6 interpola o ganho dos operadores dentro
  de cada bloco, então o índice se move sem degraus.
- **Metal** reduz a periodicidade (som inarmônico) sem mudar o brilho, que é o papel dele.
- **Decay** atua nos sinos (centroide +38%); no pad e no baixo, que são sustentados, quase não tem efeito.

Limites honestos:
- **No baixo, Ratio, Metal e Feedback mudam pouco.** O baixo é quase um seno puro (modulador em OL 45), e não há
  índice FM para mexer. Isso é de propósito.
- **Chaos 100 dá saltos de timbre de verdade** (até 18 dB em ~20 ms). O detector marca "quinas" de -62 dB nas
  fronteiras de bloco, o mesmo comportamento dos envelopes do msfa original. Não são cliques.

Níveis: cada camada ficou a até 0,3 dB do AMB-1 original, medido com `analysis/layers.c`.

**Licença:** o resultado continua **MPL-2.0** como o AMB-1. Do JIANT entrou só o `fm6_core.c`, que é Apache-2.0
(compatível). Nada do código GPL do JIANT ou do Felucca entrou: nem o `eng_fm6.c`, nem os patches de fábrica, nem o
gerador de tabelas.

## Como compilar e instalar: um comando

Num Linux x86-64 ou num Mac (com Docker), com acesso a `github.com`, `pkgman.jieliapp.com` e `gitee.com`:

```sh
sh build_release.sh               # versão padrão 1.3.90-fm6
```

O script faz tudo, em `./work`:
1. Clona o fm1-amb no commit `188b0a4` e aplica os patches de `patches/`.
2. Baixa o toolchain da JieLi (clang 4.0.1 para pi32v2).
3. Baixa o SDK AC79 e confere os hashes dos três arquivos que entram no pacote.
4. Roda os testes.
5. Gera o `amb-1.3.90-fm6.fwsc` e roda os testes de novo, agora incluindo o caminho de atualização.
6. Se houver `emcc`, gera o emulador e o site com o **instalador web**, em `work/site/`.

Para instalar, com o FM-1 no USB:

```sh
pip3 install mido python-rtmidi
python3 work/fm1-amb/base/tools/fm1_install.py work/fm1-amb/app/build/amb-1.3.90-fm6.fwsc
```

Ou sirva `work/site` (`python3 -m http.server`) e abra `install/` no Chrome ou Edge (Web MIDI). Se o FM-1 não
responder, segure OCT- e OCT+ por 5 s para o modo de atualização. Instalar firmware é por sua conta e risco; o
instalador web consegue devolver o firmware de fábrica da M-VAVE.

**Versão 1.3.90-fm6:** a identidade do pacote fica `FM-1_7010390`, longe das versões oficiais do AMB-1 (1.3 hoje).
O fork mantém o prefixo de identidade e o lugar de gravação do AMB-1. Isso é de propósito: instalado sobre o
AMB-1, ele herda as peças salvas (o projeto formato 4 é migrado para o 5).

## Revisão final

- Interface, gravação, app web e motor revisados linha a linha.
- **1 defeito encontrado e corrigido:** no Metal, os moduladores deveriam se desafinar em sentidos opostos, mas o
  código alternava pelo índice do operador. No algoritmo 5 os moduladores são os operadores 6, 4 e 2, todos pares,
  então todos iam para o mesmo lado. Agora alterna por contagem.
- Conferido no simulador: o cartão de ajuda mais longo cabe na tela, e o leitor de tela fala "Mod page: Depth 20,
  Rate 11 seconds, Chaos 25, Target Timbre".
- Os 4 patches aplicados num clone limpo reproduzem a árvore testada sem diferenças.

## O que ainda NÃO foi verificado

- O código compila sem avisos (gcc e clang, com as flags estritas do teste de host do AMB-1: `-Wall -Wextra
  -Wdouble-promotion -Werror`). A interface (`ui.c`, `project.c`) também passou pelo build do simulador de host, com
  cabeçalhos de fonte provisórios só para a checagem.
- **Os testes passam todos** (`tests/run_tests.sh`: o teste do instrumento, o build do simulador e os 8 cenários,
  incluindo o `mod.amb` novo), no Linux, via o wrapper `analysis/cc-linux`. Os scripts do AMB-1 assumem macOS: no
  glibc faltam `-lm` e `_DEFAULT_SOURCE`, e isso falha igual no AMB-1 original. Ficaram de fora o emulador web
  (falta o emcc) e o caminho de atualização (falta o `.fwsc`).
- **Não houve build para o aparelho.** A política de rede do ambiente onde isto foi feito bloqueia
  `pkgman.jieliapp.com` (toolchain) e `gitee.com` (SDK). Por isso o `.fwsc`, o instalador web e as checagens de
  imagem, RAM, pool e "64-bit beside loads" do `build.py` ainda não rodaram. Rode o `build_release.sh`.
- O emulador no navegador e o app web também não foram compilados: a instalação do Emscripten não foi permitida
  nesse ambiente.
- **Os timbres não foram ouvidos por uma pessoa.** Os níveis foram medidos e igualados ao original, e cada target
  foi verificado por análise espectral, mas o gosto é seu: os patches estão em `fm_patches()` e `bell_patches()`,
  e as faixas da modulação em `mod_of()`.
- **CPU**: no pior caso são 11 notas FM6 (4 do pad, 6 dos sinos e 1 do baixo), mais 11 geradores de modulação por
  bloco (barato). A estimativa é de uns 10 % a mais de CPU que as vozes originais a 360 MHz, ainda não medida no
  aparelho. Se faltar fôlego, reduza `NBELL`.
