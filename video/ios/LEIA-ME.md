# Cortex Video para iPhone

App iOS 16 ou posterior para usar a câmera traseira ou um vídeo de Fotos ou Arquivos com o Cortex Video do M‑VAVE FM‑1. Requer a versão do firmware com Live Video / Video import. Use a compilação com efeitos da câmera para processar o sinal na própria tela do FM‑1.

## Instalação

Abra `CortexVideo.xcodeproj` no Xcode em um Mac. Selecione o alvo CortexVideo e escolha a equipe em Signing & Capabilities. Se necessário, troque `org.cortexvideo.ios` por um identificador próprio. Selecione o iPhone como destino e clique em Run. Se o iOS solicitar o Modo Desenvolvedor, siga as instruções no aparelho. A instalação exige assinatura para o iPhone.

## Uso

1. Ligue o FM‑1 ao iPhone com o cabo/adaptador USB de dados que já funciona para MIDI. O USB do FM‑1 fica ligado ao iPhone durante o uso.
2. Abra Cortex Video no iPhone. Toque em Câmera, Fotos ou Arquivos. O app procura o dispositivo CORTEX VIDEO e seleciona o modo correspondente no FM‑1.
3. Na tela do FM‑1 você pode ajustar Colour em VISUAL, Mirror em SHAPE, Poster, Pixel e Key em CHROMA, Bright e Invert em INPUT. Os efeitos são calculados pelo FM‑1. O app envia uma prévia em cores de 60 × 60 em blocos USB‑MIDI, com aproximadamente dois quadros por segundo como alvo.
4. Para gravar no iPhone, selecione Câmera e toque em Gravar vídeo. O iPhone salva o MOV com o microfone quando autorizado. A gravação preserva o vídeo da câmera em resolução própria. Toque em Salvar em Fotos ou Compartilhar após parar. Os arquivos também aparecem em Arquivos, na pasta local do app.

A câmera e o player trabalham enquanto o app está em primeiro plano. Os efeitos do FM‑1 aparecem no LCD. A gravação local contém a câmera do iPhone.

## Protocolo

USB-MIDI SysEx conforme `video/vlink.h` do Cortex Video. O app ativa a sessão, seleciona o visual 9 ou 10, mantém a conexão e transmite quadros RGB332 divididos em 20 partes. O FM‑1 mostra o último quadro completo da fonte selecionada.



## Bluetooth

No FM-1 selecione VISUAL → Live Video para câmera ou Video import. para clipe. Isso abre o serviço Bluetooth
Cortex Video enquanto a fonte externa estiver selecionada. No app escolha Bluetooth, permita o acesso e abra
a câmera ou o clipe. A conexão é feita no próprio app. Em USB, continua valendo o cabo de dados e USB-MIDI.
O desempenho da prévia Bluetooth depende do tamanho de pacote negociado e deve ser medido no aparelho.


## Espelho e Zoom R20

No Zoom R20, escolha **Audio Interface → Stereo Mix** para gravar a mixagem estéreo. Ligue o R20 ao iPhone com
um cabo de dados. Em iPhone Lightning, use o adaptador de câmera USB. Para usar os dois aparelhos ao mesmo tempo,
o FM-1 pode receber a imagem por Bluetooth enquanto o R20 ocupa o USB.

- Na câmera nativa, selecione **Áudio da gravação → Zoom R20 (USB)** e confira **Entrada ativa** antes de gravar.
- Em **Espelho**, use os controles e efeitos do espelho do PC. **Conectar FM-1 (USB)** libera os controles do
  motor e das cenas. Para enviar a câmera ou o clipe sem cabo, escolha o Visual correspondente no FM-1 e toque
  em **Enviar por Bluetooth**. O espelho envia prévia de 60 × 60; a câmera nativa oferece o modo nítido de 120 × 120.
- Em **Áudio gravado**, toque em **Capturar áudio do Zoom R20** e confira o medidor. **Gravar aqui** salva a
  composição com o áudio em Arquivos. Use o menu **Gravação** do iPhone para compartilhar ou salvar o MP4 em Fotos.

O espelho usa as APIs de câmera, áudio e gravação do WebKit. A visibilidade do R20 e os formatos de gravação
dependem da versão do iOS e da rota USB. Para atualizar o espelho incorporado, rode `video/build.sh --test` e
copie `video/build/mirror/cortex-video-mirror.html` para `video/ios/CortexVideo/Mirror.html`.

### Áudio sem cabo no iPhone

1. No R20 use **Audio Interface → Stereo Mix** e conecte-o por USB ao Windows.
2. No PC abra **Abrir ponte R20.cmd**. O terminal mostra o endereço e o código desta sessão. Se o Windows
   pedir permissão de firewall para Python, libere apenas em rede privada.
3. Com PC e iPhone na mesma rede, abra **Espelho → Áudio gravado**, preencha o endereço e o código e toque
   em **Conectar R20 por Wi-Fi**. Confira o medidor antes de gravar. A gravação salva vídeo e áudio no iPhone.
4. Para enviar o vídeo ao FM-1, use **Enviar por Bluetooth** com Live Video ou Video import. no aparelho.

O BTA-1 continua disponível para controle do R20. A ponte usa a entrada USB do R20 no Windows e envia PCM
só após o código da sessão ser aceito. Use em rede local de confiança. Feche o terminal para parar a ponte.
