# AMB-1 FM6

O AMB-1 FM6 transforma o M-VAVE FM-1 numa máquina de música ambiente generativa, inspirada em *Music for Airports*,
de Brian Eno. Ele toca sozinho desde o momento em que é ligado: três vozes FM (cada uma respirando acordes, tocando
loops ou deslizando um baixo, como você escolher), um eco curto, um loop de fita e um reverb de placa, e uma fita que grava tudo isso e toca de volta em grãos. Você não toca o AMB-1, você cuida dele:
escolhe uma peça e uma escala, tira notas, põe de volta, e deixa crescer.

É um fork não oficial do AMB-1 de Charles Vestal: o mesmo jardim, com vozes FM no estilo DX7 (patches editados
operador por operador, seus próprios bancos DX7), três vozes iguais, os dados e a fita.

![Um desenho do FM-1 com o AMB-1 FM6, cada controle com o que ele faz: a seção Controles abaixo diz o mesmo em palavras](img/fm1-panel.svg "O FM-1 (um desenho), cada controle com seu rótulo")

---

## Conteúdo

1. Instalar
2. Primeiros passos
3. Controles
4. A tela
5. O jardim
6. As peças
7. As páginas
8. A fita
9. MIDI
10. Salvar
11. Problemas
12. Parâmetros
13. Créditos

---

## 1. Instalar

Este é o **AMB-1 FM6**, com pacote próprio (identidade `FM-1_76…`), diferente do que está no site do AMB-1. O FM-1
precisa estar antes com o firmware da M-VAVE versão **V15**: se estiver na V16, volte para a V15 com o atualizador
da M-VAVE, o M-UPGRADE, usando o arquivo V15 (`firmware-original-M-VAVE/FM-1_V15.fwsc` na pasta do pacote).

**Instalar:** com o Python 3 e seus módulos de MIDI (`pip install mido python-rtmidi`; no Windows, também o Visual
C++ Redistributable), ligue o FM-1 com um cabo USB de dados e rode, na pasta do pacote:

```
python fm1_install.py amb-1.4.1-fm6.fwsc
```

Não desconecte enquanto grava. No fim o FM-1 reinicia no AMB-1 FM6 e o instalador confere a identidade
(`FM-1_76010401`). `python fm1_install.py --info` mostra o que um FM-1 está rodando.

**Voltar ao original:** o [instalador web do AMB-1](https://charlesvestal.github.io/fm1-amb/install/) tem "Back to
the stock firmware" no fim da página, para qualquer FM-1, rode ele o que rodar: escolha o arquivo V15 e aperte o
botão. O M-UPGRADE da M-VAVE também funciona.

> Instalar outro firmware é por sua conta e risco. Se o FM-1 não ligar, veja
> [Recuperar um FM-1 que não liga](#recuperar-um-fm-1-que-n-o-liga).

---

## 2. Primeiros passos

O AMB-1 FM6 começa a tocar assim que é ligado. Alguns minutos com ele:

1. **Ouça.** A primeira peça, Garden, está tocando: Ré dórico, um acorde respirando (Voice 1), sinos em loops
   (Voice 2) e um baixo (Voice 3). MASTER é o volume.
2. **Experimente outra peça.** Gire PRESETS. Cada uma das oito peças tem tom, andamento, sons e movimento próprios.
3. **Mude a escala.** Aperte uma tecla acesa: a nota sai da escala. Aperte de novo para trazê-la de volta. Uma tecla
   apagada acrescenta a nota dela.
4. **Siga adiante.** Aperte EDIT: o acorde muda no começo do próximo compasso.
5. **Congele.** Aperte REC: o acorde fica parado e o reverb soa sem fim, enquanto o resto continua por cima. REC de
   novo solta.
6. **Molde uma voz.** ENV, ARP e LFO são as Voices 1, 2 e 3: Level, Role, Octave, Density. Aperte o botão de novo
   para o patch da voz, seus operadores e seus moduladores.
7. **Role os dados.** Aperte SEL: a página na tela ganha valores novos, em faixas que continuam musicais. Segure o
   SEL um segundo para desfazer. SEL + SAVE volta a parte da página ao padrão (desfeito do mesmo jeito).
8. **Grave.** SEQ abre a página Tape; EDIT grava, REC para, e a fita toca o que gravou.
9. **Descanse.** Aperte PLAY: o jardim para de crescer e os sons somem devagar. PLAY de novo e ele volta a crescer.

O que você muda fica guardado com a peça. Ele salva sozinho enquanto descansa, ou aperte SAVE.

Não sabe o que um botão faz? Segure-o por um segundo: aparece um cartão explicando. Solte e nada acontece.

---

## 3. Controles

| Controle | O que faz |
|---|---|
| Teclas | A escala. Cada tecla põe ou tira sua nota da escala; as teclas acendem para as notas que estão nela. Todas as oitavas de uma nota são a mesma tecla. A tônica fica, e a escala mantém pelo menos três notas. |
| PLAY | Cresce / descansa. Descansando, os sons somem devagar e o reverb se apaga. Verde enquanto cresce. |
| REC | **Freeze**: o acorde fica parado e o reverb soa sem fim. De novo: descongela. Nas páginas da fita: grava (veja [A fita](#s8)). |
| EDIT | O próximo acorde, no começo do próximo compasso. Nas páginas da fita: um splice novo. |
| HOME | A página Garden. |
| ENV · ARP · LFO | As Voices 1, 2 e 3. De novo: as páginas PATCH, OPERATOR e MOD da voz, e volta. |
| SEQ | A fita: Tape. De novo: Gene, Deck, Tape Mod, Tape Mod 2, e dá a volta. |
| FX · GLO | As páginas Space e Setup. |
| SEL | **Os dados.** Um toque: a página na tela é sorteada. Segurado com ENV, ARP, LFO, HOME, SEQ, FX ou GLO: a camada inteira (com HOME e SEQ, os loops e padrões crescem de novo também). Com EDIT: loops novos, os ajustes mantidos. Com SAVE: **init** (veja abaixo). Segurado sozinho um segundo: **desfaz** a última rolagem, init ou volta à receita. |
| SELECT | A página anterior ou a próxima. Segure HOME e gire para o andamento. |
| ALGORITHM | Modo: jônio, dórico, frígio, lídio, mixolídio, eólio, pentatônica maior e menor. Editar a escala pelas teclas a torna Custom. |
| PRESETS | A peça (veja [As peças](#s6)). |
| KNOB 1–4 | Os quatro valores na tela. Poucos cliques são passos de um em um; um giro rápido acelera. |
| OCT− / OCT+ | A oitava da voz na tela (a Voice 1 nas outras páginas), de duas abaixo a duas acima. |
| MASTER | Volume. |
| SAVE | Salva as oito peças (nas páginas da fita, a fita também). Segurado 1,5 s: a peça volta à receita (SEL segurado desfaz). |

**Init:** SEL + SAVE volta ao padrão aquilo de que a página faz parte: nas páginas MOD de uma voz, seus cinco
moduladores; nas outras páginas da voz, como ela toca (do Envelope ao ADSR, mantendo o papel), seu Index e seus
moduladores (mantendo o patch); nas páginas da fita, os ajustes e moduladores dela (mantendo a fonte); nas demais,
todos os moduladores da peça. **Desfazer:** segure o SEL um segundo: a última rolagem, init ou volta à receita é
desfeita.

**Ajuda:** segure qualquer botão menos o PLAY por um segundo e um cartão diz o que ele faz. Uma tecla, um knob ou
outro botão tira o cartão; soltar depois dele não faz mais nada.

![O cartão do SAVE: SAVES ALL EIGHT PIECES. KEEP HOLDING: THE PIECE BACK TO ITS RECIPE](img/screen-help.png "O cartão do SAVE")

---

## 4. A tela

![A página Garden: Garden, D Dorian, andamento 72; o anel com o acorde Dm7, as barras de nível à esquerda](img/screen-garden.png "A página Garden (HOME)")

- **Linha de cima:** a peça, sua tônica e seu modo. À direita, uma luz lenta do tempo, o andamento (EXT quando segue
  o clock MIDI) e um ponto quando há mudanças não salvas. As mensagens ("ADDED F#", "VOICE 2 / PATCH") aparecem aqui
  por um momento.
- **Canto de cima à direita do jardim:** uma barra fina enche até o próximo acorde; congelado, diz "held".
- **O anel:** as doze notas, Dó no alto. As notas da escala em verde, as do acorde mais escuras, a tônica circulada.
  O nome do acorde no meio.
- **Em volta:** os quatro loops da primeira voz em Loops, de 13, 17, 23 e 29 passos, de dentro para fora. Os pontos
  azuis têm nota; uma agulha percorre cada loop e brilha quando toca.
- **Esquerda:** os níveis como soam: o acorde (verde), os loops (azul), o baixo (oliva), a fita (escuro).
- **Embaixo:** o nome da página e um ponto para cada uma das onze páginas, depois seus quatro valores, um por knob: um
  dial (ou, para uma chave, suas posições) e o valor. O que você está girando acende.

Nas páginas da fita, o jardim dá lugar à fita (veja [A fita](#s8)).

![A página Space com ECHO TIME aceso enquanto gira](img/screen-turning.png "Girando Echo time na página Space")

---

## 5. O jardim

O AMB-1 FM6 faz crescer suas vozes a partir de uma escala e um acorde. Cada uma das três vozes toca seu próprio patch
FM (seis operadores, como um DX7) num de três **papéis** (Role), e qualquer voz pode fazer qualquer papel:

- **Chord:** até quatro notas que respiram. Cada uma entra, fica e some no seu próprio ciclo (de tamanhos diferentes,
  como nos loops de fita de "2/1"), pegando sua nota do acorde ao começar, de modo que o acorde se forma e se desfaz
  em vez de ficar parado. De tempos em tempos (**Drift**) o acorde muda para outro da escala; as notas que o novo
  acorde também tem ficam até o fim da respiração, as outras somem e respiram de novo nas notas novas.
- **Loops:** quatro loops de fita de 13, 17, 23 e 29 passos. Cada um guarda algumas notas, então os loops deslizam
  uns contra os outros e a melodia nunca se repete igual. Cada loop é um tom diferente do patch, e de vez em quando
  uma nota soa com sua oitava.
- **Bass:** a tônica do acorde, grave e suave, deslizando; às vezes se apoia na quinta.

Por padrão a Voice 1 é um acorde, a Voice 2 os loops e a Voice 3 o baixo, como eram o pad, os sinos e o baixo do
AMB-1.

Depois o espaço, como na cadeia de Eno em *Discreet Music*: um eco curto de fita, um loop de fita (até 2,8 segundos,
duas cabeças, cada repetição mais escura e mais fina) e um reverb de placa. Com **Motion** alto, as cabeças do eco
passeiam pelo estéreo e de vez em quando a segunda escorrega para outra parte do loop.

Por cima de tudo:

- **O arco:** uma maré lenta, de alguns minutos de ponta a ponta, entre esparso (menos notas, mais escuro, pulso mais
  baixo, mais reverb) e cheio. **Motion** define o quanto ele oscila, e o quanto a afinação, o pan e as cabeças do eco
  passeiam.
- **Pausas:** com **Evolve** alto, as vozes tomam fôlego: um baixo pula um acorde, um loop fica quieto por uma volta.

---

## 6. As peças

PRESETS escolhe uma de oito peças, cada uma uma receita para o jardim inteiro: as vozes e seus sons, andamento, tom e
modo, o quanto é movimentada e o quanto muda, e uma modulação própria (o segundo modulador de cada voz, o primeiro da
fita). Cada uma lembra como você a deixou. Segure SAVE 1,5 s para a peça voltar
à receita. Tune, MIDI out e Lights são os mesmos para todas.

| Peça | O que é |
|---|---|
| **Garden** | Ré dórico, 72: tudo, com calma. Rhodes. |
| **Departures** | Dó# jônio, 60: sinos suaves uma oitava abaixo sobre um Rhodes quieto, como "1/1". |
| **Voices** | Fá eólio, 56: só vozes cantadas e um baixo grave, como "2/1". |
| **Afternoon** | Sol lídio, 84: um Rhodes brilhante, sinos mais ocupados. |
| **Night Bus** | Lá eólio, 96: sinos metálicos, ecos curtos, modulação caótica. |
| **Shoreline** | Mi pentatônica maior, 64: sinos escuros e redondos perdidos em ecos longos, como "2/2". |
| **Glass** | Mib pentatônica menor, 76: sinos metálicos uma oitava acima, densos. |
| **Tides** | Sib mixolídio, 70: vozes cantadas, a que mais muda. |

---

## 7. As páginas

Uma página em cada botão, quatro valores nos KNOB 1–4; o SELECT passa por elas: Garden, Voice 1, 2, 3, Space, Setup,
Tape, Gene, Deck, Tape Mod, Tape Mod 2. Cada voz tem mais páginas próprias (ENV, ARP ou LFO de novo).

### HOME: Garden

- **Tempo:** 30 a 120 (também HOME + SELECT, de qualquer página).
- **Evolve:** o quanto o jardim é inquieto. Em 0 os loops se repetem exatamente; mais alto, notas mudam ou trocam de
  lugar no fim de uma volta, os padrões do pulso giram, e as vozes tomam fôlego.
- **Motion:** o quanto o arco oscila, e o quanto a afinação, o pan e o eco passeiam.
- **Drift:** quanto tempo dura um acorde antes de mudar, de 20 segundos a 5 minutos. EDIT muda agora.

### ENV · ARP · LFO: Voices 1, 2, 3

![A página da Voice 2: LEVEL 65, ROLE Loops, OCTAVE 0, DENSITY 40](img/screen-voice.png "A página de uma voz")

As três iguais:

- **Level:** o volume.
- **Role:** Chord, Loops ou Bass (veja [O jardim](#s5)). Mudar faz a voz sumir e voltar.
- **Octave:** de duas abaixo a duas acima (OCT− e OCT+ também).
- **Density:** as notas de um acorde (1 a 4); as notas dos loops (nenhuma a cinco por loop); o quanto o baixo se
  apoia na quinta.

### FX: Space

- **Echo:** quanto vai para os ecos.
- **Echo time:** o tamanho do loop de fita: semínima pontuada, mínima, mínima pontuada, 1, 2 ou 4 compassos,
  encurtado para caber nos seus 2,8 segundos em andamentos lentos. A segunda cabeça repete em três quartos dele (com
  Motion, em outras partes).
- **Reverb:** quanto vai para a placa.
- **Size:** quanto a placa soa, de uma sala a um salão longo, e quantas vezes o eco volta.

### GLO: Setup

![A página Setup: ROOT D, TUNE 0, MIDI OUT On, LIGHTS On](img/screen-setup.png "A página Setup")

- **Root:** a tônica do tom.
- **Tune:** afinação fina, ±50 cents.
- **MIDI out:** ligado ou desligado.
- **Lights:** On: as teclas acendem a escala e os botões apagados brilham de leve. Keys: só as teclas. Off.

O canto de cima à direita desta página mostra quanto do tempo do FM-1 o som está usando (CPU).

### ENV, ARP ou LFO de novo: as páginas de uma voz

O patch de cada voz é uma voz de DX7 de seis operadores, editada como um DX7 edita, com cinco moduladores
generativos. As páginas vêm em três grupos: aperte o botão da voz de novo para **PATCH**, de novo para **OPERATOR**,
de novo para **MOD**, e mais uma vez para voltar. O SELECT passa pelas páginas do grupo; cada grupo volta na página
em que foi deixado. O alto da tela diz qual voz e qual página. Cada peça guarda tudo isso.

**PATCH**

| Página | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
| --- | --- | --- | --- | --- |
| Patch | Patch | Algorithm (1–32) | Feedback (0–7) | Index (±24 dB nos moduladores) |
| Play | Envelope (AMB, Patch, ADSR) | Gate (5–100 %) | Pace | Glide |
| ADSR | Attack | Decay | Sustain | Release |
| LFO | Speed | Delay | Pitch mod | Amp mod |
| LFO 2 | Wave | Pitch sens | Key sync | Transpose (±24 semitons, como no DX7) |
| Pitch EG | Stage (1–4) | Rate | Level (±50) | Osc sync |

![A página Play de uma voz: ENVELOPE Patch, GATE 25%, PACE 1/4, GLIDE 260ms](img/screen-play.png "A página Play")

- **Patch:** primeiro os dezoito próprios do AMB-1 FM6 (pads: Rhodes, Voices, Glass, Strings, Warm, Halo; sinos:
  Bell, moldado pelo tom da peça, Vibes, Marimba, Tubular, Kalimba, Celesta; baixos: Sine, Round, Pick, Sub, Reed,
  Fretless), qualquer voz qualquer um deles, depois as vozes dos bancos DX7 que você mandou (veja [MIDI](#s9)), pelo
  nome. Girar carrega o patch como ele é; qualquer edição o torna da peça, marcado com `*` ("E.PIANO 1*").
- **Envelope:** como as notas são moldadas. **AMB:** o do AMB-1 (um acorde respira, os loops soltam uma nota presa
  depois de um tempo, o baixo é uma nota segurada que entra devagar). **Patch** (o padrão): cada nota é tocada e
  solta depois do **Gate**, então os envelopes do próprio patch são ouvidos, e editá-los muda o som. **ADSR:** o
  mesmo, com o envelope dos carriers vindo dos quatro knobs da página ADSR (os moduladores mantêm os do patch).
- **Gate:** quanto tempo a nota fica pressionada: da respiração de um acorde, de um compasso nos loops, de um acorde
  (ou um passo) no baixo.
- **Pace:** a respiração de um acorde (4 a 16 s); o passo dos loops (1/16, 1/8, 1/4, 1/2); o baixo tocado a cada
  acorde, ou a cada compasso, 1/2, 1/4, 1/8 (com Patch ou ADSR).
- **Glide:** o quanto o baixo desliza até a próxima nota, de 20 ms a 1,2 s.

![A página ADSR de uma voz: ATTACK 5, DECAY 40, SUSTAIN 70, RELEASE 40](img/screen-adsr.png "A página ADSR")

**OPERATOR** (o KNOB 1 escolhe o operador, OP1–OP6, em todas as páginas do grupo)

| Página | KNOB 2 | KNOB 3 | KNOB 4 |
| --- | --- | --- | --- |
| OP | Ratio (×0,50 a ×31; modo fixo: Hz) | Fine | Level |
| OP Env | Stage (1–4) | Rate | Level |
| OP 2 | Mode (Ratio, Fixed) | Detune (±7) | Velocity |
| OP 3 | Break (uma nota) | L depth | R depth |
| OP 4 | L curve | R curve | Rate scale |
| OP 5 | Amp sens | | |

Ratio e Fine mostram juntos a frequência do operador ("x1.50"); no modo fixo, em Hz. As edições são ouvidas na hora,
nas notas que estão soando.

**MOD** (o KNOB 1 escolhe o modulador, 1–5, em todas as páginas do grupo)

| Página | KNOB 2 | KNOB 3 | KNOB 4 |
| --- | --- | --- | --- |
| Mod | Target | Depth | Rate |
| Mod 2 | Shape | Sync | Smooth |
| Mod 3 | Spread | Fade | |

Cada uma das quatro notas de uma voz tem seu próprio gerador em cada modulador, cada um no seu ritmo em torno do
Rate; os cinco se somam; o arco os aprofunda quando o jardim está cheio e os acalma quando está esparso.

- **Depth:** o quanto vai. 0: desligado.
- **Rate:** um ciclo de 60 segundos a um oitavo de segundo. Com Sync em Tempo: uma figura, de 8 compassos a 1/16.
- **Shape:** Sine, Triangle, Saw up, Saw down, Square; S&H (um valor aleatório por ciclo, mantido); Walk (deslizando
  entre valores aleatórios); Chaos (o mapa logístico, saltando); Env (a partir do começo de cada nota, caindo até nada
  ao longo de um ciclo).
- **Target:** da voz: **Timbre** (o nível dos moduladores, ±12 dB), **Ratio** (passos harmônicos, ×1 a ×5),
  **Metal** (os moduladores desafinados entre si), **Feedback**, **Decay** (os envelopes de cada nota nova),
  **Pitch** (±30 cents), **Space** (o estéreo), **All**, **Level** (um tremolo, até −10 dB), **LFO PM** (o vibrato
  do patch, mais fundo). E os de qualquer operador, **OP1**..**OP6**: **Level**, **Ratio**, **Fine**, **Atk**,
  **Dec**, **Sus**, **Rel**. E os próprios moduladores da voz: **M1**..**M5**, **depth** (±50) ou **rate** (±2
  oitavas): um modulador movendo outro.
- **Spread:** 100: cada nota do seu jeito. 0: as notas da voz se movem juntas.
- **Fade:** a modulação entrando depois de cada nota, na hora (Off) até 8 segundos.
- **Sync:** Free; Note (cada nota recomeça a forma); Tempo (o Rate é uma figura).
- **Smooth:** um deslize na forma, de 2 ms a meio segundo.

---

## 8. A fita

Uma faixa de fita, 11,8 segundos, mono, inspirada na fita de um portastudio TASCAM e nos genes do Morphagene da Make
Noise (a ideia, não o código deles). SEQ abre a **Tape**; de novo **Gene**, **Deck**, **Tape Mod**, **Tape Mod 2**, e
dá a volta.

![A página Tape: um cassete com seus carretéis, a fita como uma tira com dois splices e o cursor; 4.2 / 11.8 s, FX BEFORE, 2 SPLICES; SOURCE Mix, DRY/WET 50, SOS 70, SPEED +1.00x](img/screen-tape.png "A página Tape")

Nessas páginas a tela mostra a fita: um cassete cujos carretéis enchem, esvaziam e giram no Speed, a fita como uma
tira (os splices, o que está tocando em destaque, o cursor, onde cada gene lê, a cabeça de gravação), os segundos
gravados e os splices.

| Página | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
| --- | --- | --- | --- | --- |
| Tape | Source (Mix, Voice 1, 2, 3) | Dry/Wet | SOS | Speed (−2x .. Stop .. +2x) |
| Gene | Gene (o splice inteiro .. 1/256) | Slide | Morph | Organize (splice 1–8) |
| Deck | FX (Before, After) | Wow | Drive | Tone |
| Tape Mod | Mod (1–5) | Target | Depth | Rate |
| Tape Mod 2 | Mod (1–5) | Shape | Sync | Smooth |

**Gravar** (nessas três páginas):

- **EDIT** grava um **splice** novo no fim da fita (até 8). EDIT de novo durante a gravação fecha o splice e já
  começa o próximo; **REC** para.
- **REC** faz overdub no splice escolhido no Organize, dando voltas nele, som sobre som (**SOS**: quanto do que havia
  fica; 0 substitui). REC de novo para.
- **SAVE** guarda as peças e a fita; a tela mostra o progresso, e a fita volta depois de reiniciar. **REC segurado**
  1,5 s apaga a fita (salve depois para apagar também a salva).
- Um splice não pode ser apagado sozinho: faça overdub nele com SOS 0, ou apague a fita.

**O que grava** é o do **Source**: a mistura inteira (sem a própria fita) ou uma voz sozinha, um pouco saturado
como numa fita (**Drive**).

**Como toca:** o splice escolhido no Organize, em **genes**: cada um uma janela do splice (**Gene**), começando onde
a fita está, adiante dela pelo **Slide**, no **Speed** (altura e tempo juntos, como fita; 1x e −1x ficam presos; em
Stop silencia), com **Wow** e flutter. **Morph**: silêncios entre os genes (0–40), um depois do outro (40–55), de
dois a quatro sobrepostos (55–100), espalhados no estéreo (acima de 75), uma oitava acima ou abaixo de vez em quando
(acima de 85). **Tone** escurece (de 1,5 kHz a aberto).

![A página Gene: GENE 100, SLIDE 0, MORPH 45, ORGANIZE 3](img/screen-gene.png "A página Gene")

**Dry/Wet:** o jardim contra a fita: Dry, só o jardim; 50, os dois cheios; Wet, só a fita (o jardim continua sendo
gravado, sem ser ouvido, como um envio).

**FX** define onde fica o espaço (eco e reverb): **Before** a fita, o jardim passa por ele, a fita o grava já com o
espaço e toca seca ao lado; **After** a fita, ela grava o jardim seco e toca através do espaço, como o jardim.

![A página Deck: FX Before, WOW 25, DRIVE 20, TONE 85](img/screen-deck.png "A página Deck")

**Tape Mod:** cinco moduladores dos ajustes da fita, como o MOD de uma voz (o KNOB 1 escolhe um, nas duas páginas):
**Target** (Speed, Gene, Slide, Morph, Dry/Wet, Tone, Wow, Drive, SOS, Organize; ou a **profundidade** ou a
**velocidade** de outro modulador, M1..M5), **Depth** (da faixa do ajuste, metade para cada lado em 100: no Speed, de
−2x a +2x; na profundidade de um modulador ±50, na velocidade ±2 oitavas), **Rate** (de 60 s a um oitavo de segundo
por ciclo; com Sync em Tempo, uma figura), **Shape** (Sine, Triangle, Saw up, Saw down, Square, S&H, Walk, Chaos),
**Sync** (Free, Tempo), **Smooth** (um deslize, de 2 a 500 ms). Eles movem o ajuste enquanto a fita toca (e grava:
Drive, SOS); o knob fica onde você o deixou. Cada peça tem o seu primeiro.

![A página Tape Mod: MOD 1, TARGET Morph, DEPTH 25, RATE 28s](img/screen-tapemod.png "A página Tape Mod")

---

## 9. MIDI

O MIDI entra pelo USB e pelo conector TRS, e sai pelo USB.

- **Entrada, canal 1:** uma nota põe ou tira sua nota da escala, como uma tecla.
- **Entrada, clock:** o tempo segue o clock MIDI, e start / stop começam e param o jardim. A tela mostra EXT no
  andamento. O que depende do andamento o segue também, medido a cada tempo: o eco, os moduladores em Sync Tempo, o
  Gate dos loops, o swing.
- **Saída** (ligada por padrão): as Voices 1, 2 e 3 nos canais 1, 2 e 3, e o clock com start / stop quando roda no
  próprio andamento. Desligar o MIDI out manda todas as notas pararem.

### CC e NRPN, entrada e saída

O que os knobs ajustam entra e sai como MIDI CC (sai com MIDI out ligado; o que entra não é reenviado):

| Canal | CC | O quê |
| --- | --- | --- |
| 16 | 20–49 | os parâmetros 0–29 (as camadas, o espaço, o setup; uma fonte de patch pelo seu número) |
| 16 | 50–61 | a fita: Source, Dry/Wet, SOS, Speed, Gene, Slide, Morph, Organize, FX, Wow, Drive, Tone |
| 16 | 62–91 | os cinco moduladores da fita: Target, Depth, Rate, Shape, Sync, Smooth do primeiro, depois os do próximo |
| 1, 2, 3 (as vozes) | 20 | o Index da voz |
| 1, 2, 3 | 21–60 | seus cinco moduladores, oito cada: Depth, Rate, Shape, Target, Spread, Fade, Sync, Smooth |
| 1, 2, 3 | 102–112 | como toca: Envelope, Gate, Role, Octave, Density, Pace, Glide, Attack, Decay, Sustain, Release |
| 1, 2, 3 | NRPN: 99 = 0/1, 98 = 0..127, 6 = valor | seu patch, byte a byte (a ordem VCED do DX7, 0..154) |

Um valor é ajustado à sua faixa. Os bytes do patch vão como são.

### Vozes e bancos DX7 de um computador

O firmware não traz vozes da Yamaha: você manda as suas. O `amb_syx.py` as envia pelo USB (Python com mido e
python-rtmidi; feche antes outros programas que usem o MIDI do FM-1):

```sh
python amb_syx.py ROM1A.syx --bank 1      # um banco de 32 vozes no banco 1 (de 16)
python amb_syx.py EPIANO.syx              # uma voz avulsa na voz cujas páginas estão na tela
python amb_syx.py --info                  # quantas vozes cada banco tem
python amb_syx.py --erase 1
```

Um banco pode ser um bulk dump de DX7 (4104 bytes), 4096 bytes de vozes compactadas, ou até 32 vozes avulsas num
arquivo; valores fora das faixas do DX7 (comuns em bancos antigos) são limitados. A tela diz BANK n e quantas vozes
chegaram. Uma voz avulsa de um editor de DX7 (o Send do Dexed) entra direto na voz cujas páginas estão na tela. Os
bancos (16 de 32 vozes) ficam na memória do FM-1, separados das peças; uma peça guarda uma cópia de cada voz que toca,
então mandar outro banco depois nunca a muda.

### Leitor de tela

O AMB-1 pode dizer a um leitor de tela o que sua tela mostra: ligue o FM-1 pelo USB, abra o
[FM-1 Screen Reader](https://charlesvestal.github.io/fm1-reader/) no Chrome ou Edge e clique em Connect. Ele lê a
página, seus knobs, o valor de um knob enquanto gira, a peça e seu tom, os cartões de ajuda, as mensagens e cada
acorde. Aperte W para ouvir a tela inteira de novo.

---

## 10. Salvar

O AMB-1 FM6 guarda as oito peças na memória do FM-1, cada uma como você a deixou: os knobs, a escala, o andamento, o
patch e os ajustes de cada voz, além de Tune, MIDI out e Lights.

- **Sozinho:** alguns segundos depois de uma mudança, mas só enquanto descansa (gravar na memória interrompe o som
  por um instante). Um ponto no alto da tela indica mudanças não salvas.
- **SAVE:** salva agora. Nas páginas da fita, a fita também.
- **SAVE segurado** 1,5 s: a peça volta à receita (só fica salvo no próximo salvamento).
- As peças de versões anteriores carregam como estavam, cada voz no papel que sua camada tinha.

---

## 11. Problemas

**Estalos com tudo ligado:** o som usa a maior parte do tempo do FM-1 com as três vozes ocupadas, os moduladores
ligados e a fita tocando muitos genes (CPU na página Setup, canto de cima à direita). Menos genes sobrepostos (Morph
mais baixo), menos moduladores ou uma Density menor ajudam. (A 1.4.0 abriu espaço: sem o pulso, a placa e os genes
a 22 kHz.)

**Modo de atualização:** segure OCT- e OCT+ juntos por cinco segundos. Aparece uma contagem; solte antes do fim para
cancelar.

**Calibração:** se um botão ou knob faz a coisa errada, segure OCT- e OCT+ enquanto liga o FM-1. A tela pede para
apertar cada botão e girar cada knob, um de cada vez.

**Depois de um travamento:** uma tela vermelha CRASH, e ele reinicia sozinho alguns segundos depois. O próximo início
mostra RESTARTED AFTER A CRASH e um endereço (PC ...) por um instante: anote-o, e o que você estava fazendo.

**Modo seguro:** se travar duas vezes seguidas ao iniciar, ele começa em SAFE MODE: sem som, mas o USB funciona, então
o instalador o alcança: instale de novo, ou volte ao firmware original. Aperte PLAY para tentar iniciar normalmente.
Se até o modo seguro falhar duas vezes, o FM-1 cai no modo de atualização do próprio chip, e só o resgate abaixo o
alcança.

### Recuperar um FM-1 que não liga

Um FM-1 no modo de atualização do chip mostra a tela vazia, e nem os instaladores nem o atualizador da M-VAVE o
enxergam. Um script consegue pôr de volta o firmware da M-VAVE, num Mac, com o mesmo cabo USB. Abra o Terminal, cole
esta linha e aperte Return:

```
bash -c "$(curl -fsSL https://raw.githubusercontent.com/hugelton/felucca-bsp/main/tools/fm1_rescue.sh)"
```

1. Se uma janela pedir para instalar as ferramentas de linha de comando, clique em Instalar e cole a linha de novo.
2. Se pedir uma senha, digite a senha do Mac (nada aparece) e aperte Return.
3. Quando disser que está esperando, ligue o FM-1 no USB e acenda-o. O script o pega, confere o chip, salva um backup
   da memória do FM-1 na pasta `fm1-rescue` da sua pasta pessoal, e pergunta antes de gravar qualquer coisa.
4. Digite **yes** e aperte Return. Não desconecte nada até ele dizer que está reiniciando o FM-1. Ele então inicia no
   firmware original, e você pode instalar o AMB-1 FM6 de novo.

O script baixa o firmware V15 da M-VAVE e o carregador de flash da JieLi e confere os dois contra assinaturas
conhecidas. Ele grava só a área do firmware, nunca o bootloader nem os ajustes do próprio FM-1. Se o FM-1 nunca
aparecer, o [FM-1-transporter](https://github.com/kurogedelic/FM-1-transporter) do Felucca ainda o alcança.

---

## 12. Parâmetros

Os padrões são os da Garden; cada peça tem os seus.

| Página | Valor | Faixa | Garden |
|---|---|---|---|
| Garden | Tempo | 30–120 | 72 |
| Garden | Evolve | 0–100 | 35 |
| Garden | Motion | 0–100 | 50 |
| Garden | Drift | 20 s – 5:00 | 25 (cerca de 40 s) |
| Voice 1, 2, 3 | Level | 0–100 | 60, 65, 45 |
| Voice 1, 2, 3 | Role | Chord, Loops, Bass | Chord, Loops, Bass |
| Voice 1, 2, 3 | Octave | −2 – +2 | 0 |
| Voice 1, 2, 3 | Density | 0–100 | 100, 40, 35 |
| Space | Echo | 0–100 | 45 |
| Space | Echo time | 1/4., 1/2, 1/2., 1 compasso, 2, 4 | 1 compasso |
| Space | Reverb | 0–100 | 65 |
| Space | Size | 0–100 | 80 |
| Setup | Root | Dó – Si | Ré |
| Setup | Tune | −50 – +50 cents | 0 |
| Setup | MIDI out | Off, On | On |
| Setup | Lights | Off, On, Keys | On |
| Tape | Source | Mix, Voice 1, 2, 3 | Mix |
| Tape | Dry/Wet | 0–100 | 50 |
| Tape | SOS | 0–100 | 70 |
| Tape | Speed | −2x .. Stop .. +2x | +1x |
| Gene | Gene, Slide, Morph | 0–100 | 100, 0, 45 |
| Gene | Organize | 1–8 | 1 |
| Deck | FX | Before, After | Before |
| Deck | Wow, Drive, Tone | 0–100 | 25, 20, 85 |
| Tape Mod (×5) | Target | Speed, Gene, Slide, Morph, Dry/Wet, Tone, Wow, Drive, SOS, Organize, M1–M5 depth, M1–M5 rate | Slide (Garden), Morph, Gene, Tone, Speed |
| Tape Mod (×5) | Depth, Rate, Shape | 0–100, 60 s – 0,12 s (Tempo: 8 compassos – 1/16), as formas do MOD | 30 (o primeiro, Garden), 0 |
| Tape Mod (×5) | Sync, Smooth | Free, Tempo; 2–500 ms | Free, 2 ms |
| PATCH de uma voz | Patch | os dezoito, depois as vozes dos bancos | Rhodes, Bell, Sine |
| PATCH de uma voz | Algorithm, Feedback, LFO, Pitch EG, Transpose | como no DX7 | os do patch |
| PATCH de uma voz | Index | −24 – +24 dB | 0 dB |
| Play de uma voz | Envelope | AMB, Patch, ADSR | Patch |
| Play de uma voz | Gate | 5–100 % | 70 %, 25 %, 80 % |
| Play de uma voz | Pace | 0–100 | 60, 60 (1/4), 0 (cada acorde) |
| Play de uma voz | Glide | 20 ms – 1,2 s | 260 ms |
| ADSR de uma voz | Attack, Decay, Sustain, Release | 0–100 | 5, 40, 70, 40 |
| OPERATOR de uma voz | Ratio, Fine, Level, Envelope, Mode, Detune, Velocity, Break, Depths, Curves, Rate scale, Amp sens | como no DX7 | os do patch |
| MOD de uma voz (×5) | Depth | 0–100 | 20 (o primeiro), 0 |
| MOD de uma voz (×5) | Rate | 0–100 (60 s – 0,12 s; Tempo: 8 compassos – 1/16) | 27 (11 s) |
| MOD de uma voz (×5) | Shape | Sine, Triangle, Saw up, Saw down, Square, S&H, Walk, Chaos, Env | Sine |
| MOD de uma voz (×5) | Target | os dez da voz, os sete de cada operador, M1–M5 depth e rate | Timbre |
| MOD de uma voz (×5) | Spread, Fade, Sync, Smooth | 0–100, Off – 8 s, Free/Note/Tempo, 2–500 ms | 100, Off, Free, 2 ms |
| ALGORITHM | Modo | 8 modos, ou Custom | Dórico |

---

## 13. Créditos

O AMB-1 FM6 é software livre sob a Mozilla Public License 2.0: se você compartilhar uma versão modificada,
compartilhe também o código dos arquivos modificados. O código vem junto (`codigo-fonte/`), com o `LICENSING.md`.

- **AMB-1**, o jardim, suas peças e sua tela: Charles Vestal. O AMB-1 FM6 é um fork não oficial, sem ligação com ele.
- A ideia vem da música generativa de Brian Eno: os loops de fita de tamanhos diferentes de *Music for Airports* e a
  cadeia de eco e delay de fita de *Discreet Music*.
- As vozes FM: o **msfa**, o núcleo DX7 do Dexed (Google Inc., Pascal Gauthier), Apache-2.0, como portado para C
  inteiro por Leo Kuroshita e levado no JIANT FM de Juan Ignacio Antonucci.
- Os genes da fita seguem a ideia do Morphagene da Make Noise, e sua fita a de um portastudio TASCAM; nenhum código
  deles. A codificação é IMA ADPCM.
- O reverb é a placa de Jon Dattorro ("Effect Design, Part 1", JAES 1997), via FoMni-1. Os ritmos euclidianos são a
  leitura de Godfried Toussaint do algoritmo de Bjorklund.
- A plataforma é o [fm1-base](https://github.com/charlesvestal/fm1-base) e o
  [felucca-bsp](https://github.com/hugelton/felucca-bsp), do **Felucca** de Leo Kuroshita, Hügelton Instruments.
- As fontes são Barlow Semi Condensed e Terminus (SIL Open Font License).

M-VAVE, FM-1, Yamaha, DX7, Make Noise, Morphagene e TASCAM são marcas de seus donos. O AMB-1 FM6 não tem ligação com
nenhum deles, com Brian Eno nem com a Hügelton Instruments.
