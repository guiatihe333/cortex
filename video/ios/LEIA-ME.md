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

