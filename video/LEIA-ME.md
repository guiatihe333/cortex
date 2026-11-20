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

## O painel

| Controle | Função |
| --- | --- |
| SELECT, OCT− / OCT+ | página (21, em 7 grupos): SOURCE (Visual, Osc, Osc 2) · OBJECT (Object, Obj Motion, Obj Array, Obj Look) · SHAPE (Shape, Shape 2, Mix) · FX (Video, Transform, Life, Mosh, Chroma) · REACT (React, React 2, Input) · PERFORM (Scenes, Matrix) · SETUP (Screen) |
| SEL + SELECT | pula de grupo em grupo |
| K1–K4 | os quatro ajustes da página |
| PRESET, ARP / SEQ | os 27 looks |
| ALGO | as cenas |
| 16 primeiras teclas | cenas 1–16; SEL + tecla guarda |
| teclas 17–27 | a matriz: cada tecla faz o que a página MATRIX diz |
| segurar uma tecla da matriz e girar um knob | a tecla aprende: vira um “Jump” para aquele valor enquanto segurada |
| REC | gravação no PC (precisa do espelho aberto) |
| PLAY | segura a estrutura · EDIT nova estrutura · LFO mutação leve · GLO sorteia a página |
| ENV | desfaz (até 8 gestos: um knob depois de uma pausa, um look, uma cena, o sorteio, um controle do espelho) |
| FX | tap tempo · HOME esconde as barras · SAVE salva ajustes, cenas e matriz |

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
