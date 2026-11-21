# CORTEX VIDEO

O FM-1 como video synth reativo ao áudio, e só isso: sem vozes, sem fita, sem songs. É um firmware à parte
(identidade `FM-1_77…`). O CORTEX (`FM-1_76…`) continua como está.

## Como funciona

- **Áudio de entrada:** o computador ou o celular toca no FM-1 pelo USB (saída “CORTEX VIDEO FM-1”). O FM-1 escuta
  esse som (seis bandas, pico, forma de onda, batida) e o repassa para o fone e o alto-falante. MASTER controla o
  volume.
- **MIDI** (USB e TRS): as notas acendem a imagem, CC 1 é a fonte “CC” da matriz, CC 14–17 são os quatro knobs da
  página, program change chama uma cena e o clock dá o tempo e as batidas.
- **Gravação:** a página `cortex-video-mirror.html` (Chrome ou Edge) conecta ao FM-1 por USB-MIDI. Ela roda o mesmo
  motor de vídeo em WebAssembly e refaz cada quadro a partir do que o FM-1 manda (os ajustes e o que ele ouviu, cerca de
  200 bytes por quadro). Cada quadro traz uma soma de verificação. Se um quadro se perde ou diverge, os dois recomeçam
  juntos. O REC do FM-1 inicia e para a gravação no PC, que sai como MP4 (H.264 + AAC) ou WebM, com o áudio da entrada
  USB do FM-1.

## Vídeo de arquivo e câmera

- **PC:** abra o espelho no Chrome ou Edge. Em **Entrada de vídeo**, importe um clipe ou ligue a câmera. O clipe
  repete em loop. MP4 (H.264) é um bom ponto de partida. Para câmeras USB ou virtuais, escolha o dispositivo
  no seletor **Câmera**. Os nomes costumam aparecer depois de permitir o acesso.
- **iPhone via cabo no Windows:** instale o [Camo Camera e o Camo Studio](https://reincubate.com/camo/).
  Conecte o iPhone ao PC com um cabo Lightning de dados, desbloqueie o aparelho e toque em **Confiar** se solicitado.
  Abra o Camo Camera no iPhone e selecione o aparelho no Camo Studio pela conexão USB. Quando a prévia aparecer,
  abra **Entrada de vídeo → iPhone via USB / Lightning → Ligar câmera do iPhone**. O espelho seleciona a câmera
  virtual Camo e aplica os mesmos efeitos ao vídeo recebido por cabo. Use o Camo Studio para trocar a lente.
- **No próprio iPhone:** abra a página do espelho no Safari por HTTPS, toque em **Ligar câmera** e escolha
  a frontal ou a traseira.
- **Na tela do FM-1:** conecte o aparelho por USB ao PC e use o firmware e o espelho desta mesma compilação.
  Na página **VISUAL**, gire K1 até **Live Video** para câmera ou **Video import.** para clipe. Escolha a fonte
  correspondente no espelho. O PC aplica os efeitos da entrada e envia uma prévia colorida de 60 × 60 para
  a tela do FM-1. O espelho continua com a saída de gravação em resolução maior.
  O mesmo envio USB-MIDI funciona com o app no iPhone ligado diretamente. A tela indica quando ainda não há
  link MIDI, quando falta abrir a fonte ou quando estão chegando partes sem um quadro completo.
- **Efeitos:** o espelhamento dobra a imagem em duas ou quatro partes. **Rastro de vídeo** realimenta a imagem
  anterior e **Zoom do rastro** faz o eco expandir ou contrair. Com o FM-1 conectado e sincronizado, a síntese
  forma uma segunda camada, com intensidade e mistura **Sobrepor**, **Trama**, **Diferença** ou **Multiplicar**.
- **Saída:** **Gravar aqui** e **Foto (PNG)** capturam a composição. O áudio continua vindo de **Áudio gravado**.
  O clipe e a câmera entram como imagem. Use **Preencher** para decidir entre preservar todo o quadro e
  cortar para preencher o formato da saída.

## O painel

| Controle | Função |
| --- | --- |
| SELECT, OCT− / OCT+ | página (28, em 7 grupos): SOURCE (Visual, Osc, Osc 2) · OBJECT (Object, Profile, Structure, Deform, Obj Motion, Clone, Clone Step, Obj Look) · SHAPE (Shape, Shape 2, Mix) · FX (Video, Transform, Life, Mosh, Chroma, Targets, Modulate) · REACT (React, React 2, Triggers, Input) · PERFORM (Scenes, Matrix) · SETUP (Screen) |
| SEL segurado | o mapa de páginas: ALGO escolhe o grupo, SELECT a página; soltar SEL vai até ela |
| HOME | toque: volta à página anterior (e de novo, à outra); segurado: esconde ou mostra as barras |
| K1–K4 | os quatro ajustes da página |
| SEL + knob | ajuste fino (de 1 em 1) |
| GLO + knob | o ajuste volta ao padrão |
| PRESET, ARP / SEQ | os 27 looks |
| ALGO | as cenas |
| 16 primeiras teclas | cenas 1–16; SEL + tecla guarda |
| teclas 17–27 | a matriz: cada tecla faz o que a página MATRIX diz |
| segurar uma tecla da matriz e girar um knob | a tecla aprende: vira um “Jump” para aquele valor enquanto segurada |
| REC | gravação no PC (precisa do espelho aberto) |
| PLAY | segura a estrutura · EDIT nova estrutura · LFO mutação leve · GLO (toque) sorteia a página |
| ENV | desfaz (até 8 gestos: um knob depois de uma pausa, um look, uma cena, o sorteio, um controle do espelho) |
| FX | tap tempo · SAVE salva ajustes, cenas e matriz |

### Objetos: formas a partir de parâmetros
Não há figuras prontas: a forma sai de alguns números estruturais.
- **PROFILE**: a superformula de Gielis. Symmetry (0 redondo; 3, 4, 5… pontas ou lados), Point (baixo: espinhos;
  alto: cheio), Curve (baixo: beliscado; alto: quadrado) e Symmetry 2 (o segundo perfil, das latitudes ou da seção).
- **OBJECT · Form**: Flat (o perfil como disco; Depth dá espessura, Hole faz um anel), Solid (o produto dos dois
  perfis: esfera, cubo, estrela, concha, flor), Ring (o segundo perfil varrido ao longo do primeiro: toros e
  parentes), Terrain e Wave (pisos do espectro e da forma de onda).
- **STRUCTURE**: Hole, Twist (torce o perfil ao longo da forma), Depth e Detail (facetas).
- **DEFORM**: Spectrum (cada parte da forma empurrada pela sua banda: em volta de um disco ou anel, ao longo das
  latitudes de um sólido), Noise, Morph (a forma de onda) e Explode (as faces lançadas para fora no tempo).
- **CLONE**: até 16 cópias em grade, anel, linha, espiral, espalhadas, órbita ou grade 3D; cada uma reage à sua
  faixa do espectro. **CLONE STEP**: Turn, Scale, Hue e Lag (as cópias atrasadas no giro) somados de cópia em cópia.
- **OBJ MOTION**: Spin, Tilt, Zoom e Sync (o giro preso ao tempo: 1 tempo, 2, 1 compasso, 2).
- **TRIGGERS** (3): um ataque (Kick, Snare, Hat, Beat, Note, Bar) dispara uma ação (Explode, Spin, Colour, Shuffle,
  Throw, Flash, Invert, Zoom) com uma intensidade. Shuffle sacode os números do perfil, e a forma volta sozinha.
- Looks novos: Orb, Supershell, Cubes, Flower, Torus Ring, Terrain, Spectrum, Clone Spiral, Kick Shatter.

### Um todo: os efeitos na imagem inteira (página TARGETS)
- **Video on**, **Mosh on** e **Chroma on**: cada família de efeitos age só no campo (Field) ou na imagem inteira
  (All, o padrão), objetos incluídos.
  - Video All: RGB, Sync e Slice rasgam e separam as cores da linha já composta (campo, linhas e objetos).
  - Mosh All: os vértices dos objetos andam com os vetores dos blocos, o rastro deles entra no campo para ser
    arrastado, e o Corrupt joga ou some com faces.
  - Chroma All: os objetos passam pelo mesmo mapa de cor do campo (Colorama, Poster, Key), por Invert e Strobe, e
    o Pixel encaixa os vértices na grade.
- **Life** (na mesma página): o genoma que muta o campo (Mutate, Chaos, as teclas Nudge, Throw, New) muta também o
  perfil dos objetos, e os dois mudam juntos.
- **OBJ LOOK · Blend**: Over, Add, Feed, Behind (o campo claro cobre o objeto além do **Key**, com borda suave, como
  um keyer de luma), Difference e Multiply.

### Os objetos modulam o campo (página MODULATE)
Como o modulate do Hydra, com os objetos no papel de modulador:
- **Lens**: o campo empurrado para fora das formas (positivo) ou puxado para dentro (negativo), como uma lente.
- **Swirl**: o campo torcido em volta das formas.
- **Glow**: um halo das formas no campo, que o feedback carrega.
- **Vectors**: com o Mosh ligado, os blocos sob cada cópia andam com o movimento dela na tela, e o borrão segue o
  que se move (como os vetores de um datamosh de verdade, em vez de sorteados).

### A tela
- Os knobs são arcos: o valor da mão em laranja; por dentro, até onde a modulação o leva agora, na cor da fonte
  (vermelho: grave, bumbo; amarelo: médios, caixa; azul: agudos, chimbal, brilho; verde: LFO, tempo, compasso;
  branco: batida, notas).
- À direita do cabeçalho: o espectro em 24 bandas, as luzes do bumbo, da caixa e do chimbal, e o tempo piscando.

### Cenas (página SCENES)
- **Scene**: escolhe e chama uma cena.
- **Launch**: Now (na hora), Beat (no próximo tempo) ou Bar (no próximo compasso). A tecla da cena pisca enquanto espera.
- **Morph**: Cut (corte) ou 1–16 tempos de transição suave entre as cenas.
- **Chain**: troca sozinha para a próxima cena guardada a cada 1–16 compassos.
- O tempo vem do MIDI clock, se houver, ou do tap tempo.

### Matriz (página MATRIX), teclas 17–27
- **Key** escolhe a tecla, **Does** o que ela faz, **Setting** e **To** o ajuste e o valor (para Jump e Toggle).
- Funções: Nudge, Throw, New, Hold, Strobe, Invert, Mosh, Freeze, Blackout (enquanto segura), próximo visual,
  objeto, paleta, look, cena (próxima, anterior, qualquer), Tap, Chain, Jump (enquanto segura) e Toggle (liga e
  desliga).

### Audiorreatividade
- FFT de 256 pontos no FM-1: 24 bandas de 43 Hz a 5,5 kHz (as 14 primeiras com 43 Hz cada, para os graves).
- Detectores de bumbo, caixa e chimbal e o brilho do som (centroide). Na matriz React são as fontes Kick, Snare, Hat e
  Bright.
- Cada slot React tem Mode (Direct, Invert, Beat hold, Gate) e Lag (suavização) na página REACT 2.
- As cópias dos objetos (Bars, a grade de cubos) e o Terrain leem cada uma uma faixa do espectro.

### Abrir o espelho
Dê um clique duplo em **Abrir espelho.cmd** (na mesma pasta da página). Ele serve a página em
http://127.0.0.1:8770 e a abre no navegador; deixe a janela preta aberta enquanto usa. Aberta direto do disco
(file://), a página não consegue escolher a saída de áudio nem lembrar as permissões: o R20 não chega ao FM-1.

### Áudio de uma placa ou gravador (Zoom R20, microfone)
O FM-1 só recebe áudio pelo USB do computador, e uma placa USB não se liga direto nele (os dois são dispositivos USB).
Na página do espelho, em “Áudio para o FM-1”, escolha a entrada (o R20) e a saída “Alto-falantes (CORTEX VIDEO
FM-1)”: a página toca a entrada no FM-1, que reage a ela. Em “Áudio gravado”, “A entrada enviada ao FM-1” grava esse
mesmo som direto, na qualidade da placa, atrasado (Atraso do áudio, 50 ms de partida) para cair junto com a imagem.

### Resolução do espelho
O espelho redesenha cada quadro em HD (480, 720 ou 960) a partir do mesmo estado do FM-1: o campo interpolado, as
linhas e os objetos 3D nítidos nessa resolução. “240” mostra a tela do FM-1 como ela é.

### Controle pelo espelho
A página do espelho mostra os mesmos grupos e páginas do FM-1, com sliders. Mexer num slider muda o FM-1, como um knob,
e o valor volta com os quadros. Há também os botões das 16 cenas (respeitam o Launch e o Morph do FM-1) e o Desfazer.

## O que há de novo em relação ao vídeo do CORTEX

- 8 slots na matriz React (eram 4), com novas fontes: Tempo, Bar, CC e Velocity, além de Level, Bass, Mid, High,
  Beat, Lows, Mids, Highs, Notes, Root, LFO e Random.
- Strobe (na batida ou por tecla), Invert, ganho de entrada (Gain) e 10 paletas (Cortex, Ember, Ice, Acid, Mono,
  Neon, Vapor, Phosphor, Thermal, Gold).
- 16 cenas, tap tempo, sincronia com MIDI clock e os 20 looks do CORTEX mais Strobe e Negative.

## Compilar e testar (WSL)

```sh
video/build.sh          # testes, depois o firmware (video/build/cortex-video.fwsc) e o espelho (video/build/mirror/)
video/build.sh --test   # só os testes
```

Os testes: o motor (determinismo, todas as imagens), dois cenários do FM-1 simulado (painel e link) e o espelho, em C
e em WebAssembly. O espelho refaz o link gravado pelo FM-1 simulado, e cada quadro precisa bater.

## Limites

- O SAVE do CORTEX VIDEO usa o espaço da song 4 do CORTEX (0xD6000–0xDA000). A peça, os patches, a fita e os
  bancos do CORTEX ficam intactos. A calibração do painel é a mesma nos dois.

- A tela do FM-1 tem 240 × 240 (o campo interno, 120 × 120). O espelho redesenha em até 960 × 960 e amplia para
  1080p ou 4K; o campo continua com o detalhe de 120 × 120, suavizado.
- Cerca de 30 quadros por segundo (um a cada dois ciclos do loop do FM-1); com LAYER ligado, cerca de 20. O
  movimento segue o relógio. O MP4 é gravado a 30
  fps e repete quadros.
- Quando o espelho conecta, a imagem recomeça do zero, nos dois lados ao mesmo tempo.
- O áudio no arquivo é o da entrada USB do FM-1 no PC. A defasagem entre áudio e vídeo é a do USB, de alguns
  milissegundos, e ainda não foi medida no aparelho.
- O FM-1 só recebe áudio pelo USB. Áudio Bluetooth (receber de um celular) não existe na base.
- A igualdade entre o FM-1 e o espelho foi provada no simulador (x86 e WebAssembly dão o mesmo resultado bit a bit).
  No aparelho real, o contador “Divergências” do espelho mostra se continua igual.
