# CORTEX

Firmware generativo para o **M-VAVE FM-1** (versão atual **1.5.3**), nascido como AMB-1 FM6: o AMB-1 de Charles
Vestal com as vozes no motor FM6 do JIANT FM, e depois um sintetizador de vídeo, músicas a partir de arquivos MIDI,
a fita com fontes Play, as letras derretidas e as páginas nas cores do acorde que toca.

- **Simulador no navegador:** a pasta [`simulator/`](simulator/) roda o mesmo código do FM-1 (WebAssembly); abra
  `simulator/index.html` por um servidor (`python -m http.server`) ou pelo GitHub Pages.
- **Código:** `patches/0001..0022` sobre `charlesvestal/fm1-amb` @ `188b0a4` (e `base-patches/` sobre o fm1-base);
  `amb1-fm6.patch` é o mesmo em um arquivo; `files/` traz os arquivos já modificados.
- **Compilar:** `build_release.sh` (Linux/WSL). Licença: MPL-2.0, como o AMB-1 (veja `files/LICENSING.md`).

O texto abaixo é o histórico das versões AMB-1 FM6.

## AMB-1 com as vozes do motor FM6 do JIANT

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

## Edição FM completa, bancos DX7, 5 moduladores e MIDI CC (1.3.92-fm6)

Cada voz (Pad, Bells, Bass) tem **o próprio patch DX7 de 6 operadores**, editável no aparelho como no JIANT FM, e
**cinco moduladores generativos** com alvo em qualquer parâmetro. Tudo fica guardado com a peça.

**Como navegar:** aperte de novo o botão da voz (ENV no Pad, ARP nos Bells, LFO no Bass). Cada toque vai para o próximo
grupo: **VOICE → OPERATOR → MOD →** volta à página da voz. Dentro de um grupo, o **SELECT** passa de página em página, e
cada grupo volta na página em que você o deixou.

| Grupo | Página | Knob 1 | Knob 2 | Knob 3 | Knob 4 |
| --- | --- | --- | --- | --- | --- |
| VOICE | Voice | **Patch** | Algorithm 1–32 | Feedback 0–7 | Index ±24 dB |
| VOICE | LFO | Speed | Delay | Pitch mod | Amp mod |
| VOICE | LFO 2 | Wave | Pitch sens | Key sync | Transpose ±24 |
| VOICE | Pitch EG | Stage 1–4 | Rate | Level ±50 | Osc sync |
| OPERATOR | OP | **Operador 1–6** | Ratio (×0,50..×31, ou Hz no modo fixo) | Fine | Level |
| OPERATOR | OP Env | Operador | Stage 1–4 | Rate | Level |
| OPERATOR | OP 2 | Operador | Mode (Ratio/Fixed) | Detune ±7 | Velocity |
| OPERATOR | OP 3 | Operador | Break (nota) | L depth | R depth |
| OPERATOR | OP 4 | Operador | L curve | R curve | Rate scale |
| OPERATOR | OP 5 | Operador | Amp sens | | |
| MOD | Mod | **Modulador 1–5** | Target | Depth | Rate |
| MOD | Mod 2 | Modulador | Shape | Sync | Smooth |
| MOD | Mod 3 | Modulador | Spread | Fade | |

- **Patch:** primeiro os seis próprios de cada voz (os da 1.3.91), depois as vozes dos bancos DX7 que você mandou, pelo
  nome; slots vazios são pulados. Girar carrega o patch como ele é; qualquer edição o torna da peça, marcado com `*`.
- **Quando se ouve:** edições de operador soam **na hora**, nas notas que estão tocando (o núcleo reaplica nível e
  frequência sem reiniciar o envelope). Patch novo: o pad faz crossfade, os sinos trocam no próximo toque, o baixo é
  tocado de novo com fade de 10 ms.
- **Alvos dos moduladores:** os da voz (Timbre, Ratio, Metal, Feedback, Decay, Pitch, Space, All, Level, **LFO PM**) e
  os de **cada operador**, OP1..OP6: **Level** (±12 dB; portadora só para baixo), **Ratio** (degraus harmônicos),
  **Fine** (±50 cents), **Atk**, **Dec**, **Sus**, **Rel**. Os cinco moduladores somam.
- **Formas:** Sine, Triangle, Saw up, Saw down, Square, S&H, Walk, Chaos e Env; Spread, Fade, Sync (Free, Note,
  Tempo) e Smooth por modulador.

**Bancos DX7 (.syx):** 16 bancos de 32 vozes (512) numa área própria da flash. O FM-1 só recebe SysEx de até 640 bytes,
então o script `tools/amb_syx.py` (na árvore do fm1-amb) manda o banco em 16 pedaços, cada um confirmado:

```sh
python3 tools/amb_syx.py ROM1A.syx --bank 1      # banco de 32 vozes no banco 1 (de 16)
python3 tools/amb_syx.py EPIANO.syx              # uma voz direto na voz da página mostrada
python3 tools/amb_syx.py --info                  # quantas vozes cada banco tem
```

Aceita bulk dump DX7 (4104 bytes), 4096 bytes de vozes empacotadas ou até 32 vozes avulsas num arquivo. Uma voz avulsa
mandada por um editor DX7 (o "Send" do Dexed) também entra direto na voz da página mostrada. Os bancos de fábrica da
Yamaha e os do Dexed/JIANT têm direitos autorais e não vão no firmware: você carrega os seus.

**MIDI CC e NRPN (entrada e saída):**

| Canal | CC | O quê |
| --- | --- | --- |
| 16 | 20–49 | parâmetros 0–29 (camadas, espaço, tom, setup; a fonte de patch pelo número) |
| 1 (pad), 2 (bells), 3 (bass) | 20 | Index da voz |
| 1, 2, 3 | 21–60 | os 5 moduladores, 8 parâmetros cada (Depth, Rate, Shape, Target, Spread, Fade, Sync, Smooth) |
| 1, 2, 3 | NRPN (99 = 0/1, 98 = byte, 6 = valor) | o patch da voz, byte a byte (ordem VCED do DX7, 0..154) |

Girar um knob manda o CC/NRPN correspondente (com MIDI out ligado); o que chega por MIDI não volta.

**Projeto formato 7:** os parâmetros num objeto e os patches das 8 peças em outro (0xDE000). Projetos da 1.3.91
(formato 6) migram: o MOD vira o primeiro modulador de cada voz e as edições Ratio/Feedback/envelope são aplicadas
no patch (que passa a ser da peça).

## Cada voz com suas páginas: como era na 1.3.91-fm6


Pad, Bells e Bass têm, cada um, **quatro páginas próprias**, guardadas com a peça. **Como chegar:** aperte de novo o
botão da página da voz (ENV no Pad, ARP nos Bells, LFO no Bass): FM → Env → Mod → Mod 2 → volta à página da voz. O
SELECT também chega nelas depois de Setup. O botão da voz fica aceso, e o ponto da voz mostra onde você está.

| Página | Knob 1 | Knob 2 | Knob 3 | Knob 4 |
| --- | --- | --- | --- | --- |
| **FM** | **Patch** (6 por voz) | **Ratio**: ×1 (do patch), ×2, ×3, ×4, ×5, ×7, ×1,41, ×2,76, ×3,5 | **Index**: ±24 dB nos moduladores | **Feedback**: ±7 passos |
| **Env** | **Attack** ±50 | **Decay** ±50 | **Sustain** ±50 | **Release** ±50 |
| **Mod** | **Depth** 0–100 | **Rate**: 60 s a 0,12 s, ou valor de nota com Sync Tempo | **Shape** (forma de onda) | **Target** |
| **Mod 2** | **Spread** 0–100 | **Fade** Off a 8 s | **Sync**: Free, Note, Tempo | **Smooth** 2 a 500 ms |

**Patches** (escritos do zero; o volume de cada um igualado ao primeiro da voz com `analysis/patchlev.c`):

| Voz | Patches |
| --- | --- |
| Pad | Rhodes, Voices, Glass, Strings, Warm, Halo (o knob Colour da página Pad é o mesmo parâmetro) |
| Bells | Bell (o original, moldado por Tone), Vibes, Marimba, Tubular, Kalimba, Celesta |
| Bass | Sine (o original), Round, Pick, Sub, Reed, Fretless |

Quando cada edição é ouvida: **Ratio, Index e Feedback na hora**. O envelope e o patch entram na próxima nota: no
pad, a próxima respiração (troca de patch faz o crossfade de troca de tom); nos sinos, o próximo toque. O baixo é uma
nota só sustentada: ele é tocado de novo quando o knob para (0,1 s), com fade de 10 ms para não estalar. Um sino com
Sustain acima de 0 é solto depois de uma batida.

**Formas de onda (Shape):** Sine, Triangle, Saw up, Saw down, Square, S&H (valor aleatório segurado por ciclo), Walk
(desliza entre valores aleatórios), Chaos (mapa logístico, r 3,95, saltos suavizados) e **Env** (de cada nota, cai de
1 a 0 em um ciclo: um "pluck" de brilho, uma queda de afinação).

**Mod 2:**
- **Spread**: 100 cada voz do layer anda do seu jeito; 0 as vozes do layer se movem juntas.
- **Fade**: a modulação entra devagar depois de cada nota.
- **Sync**: Free; Note (cada nota recomeça a forma); Tempo (o Rate vira valor de nota: 8 compassos a 1/16).
- **Smooth**: glide na forma (2 ms no mínimo: a borda do Square nunca estala).

**Target Level (novo):** volume da voz, até −10 dB, só para baixo (tremolo, swell; nunca mais alto que a voz).

## Motor de modulação generativa: como era na 1.3.90 (uma página MOD para as três vozes)

A versão anterior tinha uma página MOD só, compartilhada:

| Knob | Parâmetro | O que faz |
| --- | --- | --- |
| 1 | **Depth** (0–100) | Profundidade da modulação. Em 0 as vozes soam exatamente como sem o motor. |
| 2 | **Rate** (0–100) | Duração de um ciclo, de 60 s a 0,12 s (até 8,5 Hz, velocidade de vibrato). Cada voz anda um pouco mais rápido ou mais devagar. |
| 3 | **Chaos** (0–100) | Forma do movimento: 0 é uma onda lenta; 50 é um passeio aleatório, deslizando entre valores sorteados; 100 é o mapa logístico na borda do caos, com saltos suavizados por 1/20 do segmento (8 a 150 ms). |
| 4 | **Target** | O que se move; veja a tabela abaixo. |

Na 1.3.91 esses quatro valores viram os de cada voz (Chaos abaixo de 30 vira Sine, até 85 Walk, acima Chaos).

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
- **Projetos salvos**: o formato do projeto foi para o 6. Projetos salvos no formato 5 (1.3.90-fm6) viram o MOD de
  cada voz; nos formatos 4 (AMB-1 atual) e 3 (0.9) os parâmetros novos vêm da receita da peça.
- **Também incluídos**: o leitor de tela fala as páginas novas, o app web ganhou os grupos de cada voz (e guarda o
  estado numa chave nova) e o manual foi atualizado.
- **Fila de comandos**: foi de 64 para 256. Com 75 parâmetros, trocar de peça manda todos de uma vez, e a fila
  antiga perderia parte deles (inclusive no aparelho).

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

- Páginas por voz (1.3.91): bancos de patches (`pad_bank`, `bell_bank`, `bass_bank`), edições de FM e envelope,
  formas e funções do MOD (`vox_t`, `vox_apply`, `gen_shape`, `gen_note`) em `amb.c`; `amb.h` (blocos `P_VX`),
  `ui.c` (12 páginas, navegação), `project.c` e `app.h` (formato 6), `host.c` (`storev5`), testes e cenários.
- Edição completa e bancos (1.3.92): `fm6_core.c` (`fm6_note_refresh` e nível por operador), `amb.c` (patch próprio
  por voz `vpat`/`vstage`, comandos `en_patch_byte`/`en_patch_done`, `en_unpack` da VMEM do DX7, `en_bake`, matriz de
  5 moduladores com alvos por operador, LFO em todas as vozes, `en_init` reiniciando o acaso para testes A/B),
  `app/bank.c` (bancos na flash, SysEx), `project.c` (formato 7, `OBJ_PATCH`), `ui.c` (grupos VOICE/OPERATOR/MOD,
  knobs virtuais, MIDI CC/NRPN), `host.c` (`storev6`, `bankfake`, `dx7voice`, `patV_OFF`), `tools/amb_syx.py`.

`amb1-fm6.patch` traz tudo de uma vez. Em `patches/` estão os seis passos separados (vozes FM6, página MOD,
targets FM, correção do Metal, páginas por voz, edição completa e bancos), para `git am`.

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

## Instalar no FM-1 ligado ao seu computador

Antes de tudo: o FM-1 precisa estar no firmware de fábrica **V15** da M-VAVE. Todos os instaladores (fm1-base,
AMB-1, JIANT) foram feitos e testados sobre o V15.
- **V16** (21/10/2026): ninguém confirmou instalação de firmware alternativo sobre ele. Volte ao V15 pelo
  **M-UPGRADE** da M-VAVE, com o arquivo oficial `FM-1.fwsc` da versão V15.
- **FM1 Open**: ele só aceita o V15 por cima.

O `instalar.sh` detecta os dois casos e para antes de gravar.

**Linux (x86-64) ou macOS** (no Mac, com o Docker aberto), com o FM-1 no USB:

```sh
git clone -b claude/amb1-jiant-fm6 https://github.com/guiatihe333/lostgear
cd lostgear/fm1-amb-fm6
sh instalar.sh
```

O script:
1. Prepara um Python isolado com mido, python-rtmidi e Pillow.
2. Encontra o FM-1 e mostra o firmware atual, antes de gastar tempo no build.
3. Compila o pacote (`build_release.sh`).
4. Instala, pedindo confirmação.

Se o `python-rtmidi` não instalar: no Linux, `sudo apt install build-essential libasound2-dev libjack-dev python3-dev`;
no Mac, `xcode-select --install`.

**Windows:** o toolchain da JieLi só roda em Linux, e o WSL não enxerga o USB-MIDI. Então:
1. No **WSL (Ubuntu)**: clone como acima e rode `sh build_release.sh`. O pacote sai em
   `work/fm1-amb/app/build/amb-1.3.92-fm6.fwsc`.
2. No **PowerShell do Windows**, com Python instalado:
   ```powershell
   py -m pip install mido python-rtmidi
   cd \\wsl$\Ubuntu\home\<seu-usuario>\lostgear\fm1-amb-fm6\work\fm1-amb
   py base\tools\fm1_install.py app\build\amb-1.3.92-fm6.fwsc
   ```

Feche outros programas que usem MIDI (DAW, editores web) antes de instalar. Não desligue o FM-1 enquanto grava. Se
uma instalação for interrompida, o FM-1 fica no modo de atualização: rode o mesmo comando de novo para terminar.

## Como compilar e instalar: um comando

Num Linux x86-64 ou num Mac (com Docker), com acesso a `github.com`, `pkgman.jieliapp.com` e `gitee.com`:

```sh
sh build_release.sh               # versão padrão 1.3.92-fm6
```

O script faz tudo, em `./work`:
1. Clona o fm1-amb no commit `188b0a4` e aplica os patches de `patches/`.
2. Baixa o toolchain da JieLi (clang 4.0.1 para pi32v2).
3. Baixa o SDK AC79 e confere os hashes dos três arquivos que entram no pacote.
4. Roda os testes.
5. Gera o `amb-1.3.92-fm6.fwsc` e roda os testes de novo, agora incluindo o caminho de atualização.
6. Se houver `emcc`, gera o emulador e o site com o **instalador web**, em `work/site/`.

Para instalar, com o FM-1 no USB:

```sh
pip3 install mido python-rtmidi
python3 work/fm1-amb/base/tools/fm1_install.py work/fm1-amb/app/build/amb-1.3.92-fm6.fwsc
```

Ou sirva `work/site` (`python3 -m http.server`) e abra `install/` no Chrome ou Edge (Web MIDI). Se o FM-1 não
responder, segure OCT- e OCT+ por 5 s para o modo de atualização. Instalar firmware é por sua conta e risco; o
instalador web consegue devolver o firmware de fábrica da M-VAVE.

**Versão 1.3.92-fm6:** a identidade do pacote fica `FM-1_7010392`, longe das versões oficiais do AMB-1 (1.3 hoje).
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
