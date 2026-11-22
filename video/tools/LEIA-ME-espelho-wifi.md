# Espelho do Cortex Video → iPhone (Wi-Fi)

1. Ligue o Zoom R20 ao PC por USB e selecione **Audio Interface → Stereo Mix** no R20.
2. Extraia todos os arquivos desta pasta juntos. Abra **Abrir espelho e ponte.cmd**.
3. No espelho do Chrome ou Edge, abra a câmera, importe um clipe ou conecte o FM-1 para ter vídeo.
   Em **Áudio gravado**, selecione a entrada do Zoom R20. Clique em **Transmitir ao iPhone**.
4. No Cortex Video atualizado do iPhone, entre em **Espelho → Receber do PC (Wi-Fi)**. Digite o endereço
   e o código mostrados no PC. Permita o acesso do app à rede local.
5. Quando aparecer **Recebendo vídeo e som do PC**, toque em **Gravar**. Toque em **Ouvir no iPhone** para
   monitorar a prévia. Ao parar, use **Gravação** para compartilhar ou salvar MP4 em Fotos.

O PC e o iPhone precisam estar na mesma rede. A ponte fornece o enlace inicial; vídeo e áudio vão
juntos por WebRTC na rede local. O áudio para gravação vem da fonte escolhida no espelho do PC.
Se o firewall perguntar, permita Python e o navegador apenas na rede privada. Feche as janelas do
servidor e da ponte para encerrar. Para repetir a sessão, inicie uma nova transmissão no espelho.
