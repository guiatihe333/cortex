/* SPDX-License-Identifier: MPL-2.0 */
(() => {
  // Runs only in the bundled iPhone app. The desktop mirror does not load this bridge.
  const native = window.cortexNative;
  if (!native) return;
  $("iphoneUsb").hidden = true;
  $("connect").textContent = "Conectar FM-1 (USB)";
  // Keep an already selected microphone alive when connecting the FM-1.
  startAudio = async () => {
    if (!audioStream && navigator.mediaDevices?.enumerateDevices) await listAudio();
  };
  $("connect").onclick = async () => {
    stopBluetooth();
    await connect().catch(() => {});
    if (!midi) $("linkState").textContent = "Conecte o FM-1 por USB ou use Bluetooth abaixo.";
  };
  // The desktop defaults to high-resolution export; 720 square is a better starting point on iPhone.
  $("size").value = "720x720";
  $("hd").value = "2";
  layout();
  if (wx) setHD();

  function button(text, parent, callback) {
    const control = document.createElement("button");
    control.type = "button";
    control.textContent = text;
    control.onclick = callback;
    parent.append(control);
    return control;
  }
  let loop = null;
  let writing = false;
  const bluetooth = button("Enviar por Bluetooth", $("openCamera").parentElement, toggleBluetooth);
  // the picture the FM-1 gets: sharp 120 x 120 (its effects all on), or clean 240 x 240; filled, cut at the long
  // sides, as the app's own camera sends it (VideoCodec codes the RGBA)
  const sizeChoice = document.createElement("select");
  for (const [value, text] of [["120", "Nítida 120 × 120 (todos os efeitos)"], ["240", "Limpa 240 × 240"]]) {
    const option = document.createElement("option");
    option.value = value;
    option.textContent = text;
    sizeChoice.append(option);
  }
  $("openCamera").parentElement.append(sizeChoice);
  const sendCanvas = document.createElement("canvas");
  const sendContext = sendCanvas.getContext("2d", { willReadFrequently: true });
  function stopBluetooth() {
    if (!loop) return;
    clearInterval(loop);
    loop = null;
    bluetooth.textContent = "Enviar por Bluetooth";
    native("stopBLE").catch(() => {});
  }
  function toggleBluetooth() {
    if (loop) { stopBluetooth(); return; }
    if (!hasExternalFrame()) {
      videoStatus("Abra a câmera ou importe um clipe antes de enviar.");
      return;
    }
    send(OFF);
    clearInterval(keepTimer);
    if (inPort) inPort.onmidimessage = null;
    inPort = outPort = midi = null;
    synthFrame = false;
    bluetooth.textContent = "Parar Bluetooth";
    loop = setInterval(sendBluetooth, 70);
    sendBluetooth();
  }
  async function sendBluetooth() {
    if (!hasExternalFrame()) { stopBluetooth(); return; }
    if (writing || !loop) return;
    writing = true;
    try {
      const output = $("out"), side = sizeChoice.value === "240" ? 240 : 120;
      if (sendCanvas.width !== side) sendCanvas.width = sendCanvas.height = side;
      sendContext.fillStyle = "#000";
      sendContext.fillRect(0, 0, side, side);
      const scale = side / Math.min(output.width, output.height);   // (fill: cut at the long sides)
      const width = output.width * scale, height = output.height * scale;
      sendContext.drawImage(output, (side - width) / 2, (side - height) / 2, width, height);
      const data = sendContext.getImageData(0, 0, side, side).data;
      let encoded = "";
      for (let i = 0; i < data.length; i += 8192)
        encoded += String.fromCharCode.apply(null, data.subarray(i, i + 8192));
      await native("ble", { source: sourceKind === "camera" ? 1 : 2, pixels: btoa(encoded) });
    } catch (error) { videoStatus(error.message); stopBluetooth(); }
    finally { writing = false; }
  }

  const audio = $("audioIn").closest("section");
  const audioState = document.createElement("p");
  audioState.className = "note";
  audioState.role = "status";
  const audioButton = button("Capturar áudio do Zoom R20", audio, openZoom);
  audio.append(audioState);
  let usbAudio = null;

  const networkForm = document.createElement("div");
  networkForm.className = "row";
  const address = document.createElement("input");
  address.placeholder = "IP do PC:porta";
  address.setAttribute("aria-label", "Endereço do PC");
  address.autocapitalize = "off";
  address.autocomplete = "off";
  const code = document.createElement("input");
  code.placeholder = "Código da ponte";
  code.setAttribute("aria-label", "Código da ponte R20");
  code.autocapitalize = "off";
  code.autocomplete = "off";
  code.maxLength = 16;
  networkForm.append(address, code);
  const wifi = button("Conectar R20 por Wi-Fi", networkForm, toggleNetwork);
  audio.append(networkForm);
  let network = null;
  let pendingZoom = false;

  function releaseNetwork(message = "") {
    if (!network) return;
    const finished = network;
    network = null;
    native("r20Disconnect").catch(() => {});
    if (audioStream === finished.destination.stream) {
      if (recorder) stopRec();
      audioStream = analyser = null;
    }
    finished.destination.stream.getTracks().forEach(track => track.stop());
    finished.delay.disconnect();
    const select = $("audioIn");
    const option = [...select.options].find(item => item.value === "__r20wifi");
    if (option) option.remove();
    wifi.textContent = "Conectar R20 por Wi-Fi";
    if (message) audioState.textContent = message;
  }

  async function toggleNetwork() {
    if (network) { releaseNetwork("Áudio por Wi-Fi desligado."); return; }
    if (recorder || pendingZoom) return;
    const ctx = audioCtx || new AudioContext();
    try { await ctx.resume(); }
    catch (error) { audioState.textContent = error.message; return; }
    if (network) return;
    audioCtx = ctx;
    const destination = ctx.createMediaStreamDestination();
    const meter = ctx.createAnalyser();
    const delay = ctx.createDelay(1);
    delay.delayTime.value = Number($("avDelay").value) / 1000;
    delay.connect(destination);
    delay.connect(meter);
    const state = {ctx, destination, delay, meter, next: 0, ready: false};
    network = state;
    wifi.textContent = "Desligar R20 por Wi-Fi";
    audioState.textContent = "Conectando à ponte do R20…";
    try {
      await native("r20Connect", {address: address.value.trim(), code: code.value.trim().toLowerCase()});
    } catch (error) {
      if (network === state) releaseNetwork(error.message || "Não foi possível conectar ao PC.");
    }
  }

  window.cortexNetworkAudio = (encoded, rate, channels) => {
    const state = network;
    if (!state || ![44100, 48000].includes(rate) || ![1, 2].includes(channels)) return;
    const pcm = atob(encoded);
    const frames = rate / 25;
    if (pcm.length !== frames * channels * 2) return;
    const now = state.ctx.currentTime;
    if (state.next > now + 0.30) return;
    // Keep a small jitter buffer. Late packets re-prime rather than accumulating delay.
    if (state.next < now) state.next = now + 0.075;
    const buffer = state.ctx.createBuffer(channels, frames, rate);
    for (let channel = 0; channel < channels; channel++) {
      const samples = buffer.getChannelData(channel);
      for (let i = 0; i < frames; i++) {
        const byte = 2 * (i * channels + channel);
        const unsigned = pcm.charCodeAt(byte) | (pcm.charCodeAt(byte + 1) << 8);
        samples[i] = ((unsigned << 16) >> 16) / 32768;
      }
    }
    const audioSource = state.ctx.createBufferSource();
    audioSource.buffer = buffer;
    audioSource.connect(state.delay);
    audioSource.onended = () => audioSource.disconnect();
    audioSource.start(state.next);
    state.next += frames / rate;
    if (!state.ready) {
      if (audioStream && audioStream !== routeDest?.stream) audioStream.getTracks().forEach(track => track.stop());
      audioStream = state.destination.stream;
      analyser = state.meter;
      state.ready = true;
      const select = $("audioIn");
      if (!select.options.length) select.add(new Option("(sem áudio)", ""));
      select.add(new Option("Zoom R20 (Wi-Fi)", "__r20wifi"));
      select.value = "__r20wifi";
      audioState.textContent = "Zoom R20 por Wi-Fi ativo. Confira o medidor e toque em Gravar aqui.";
    }
  };
  window.cortexNetworkError = message => releaseNetwork(message);
  $("avDelay").addEventListener("input", () => {
    if (network) network.delay.delayTime.value = Number($("avDelay").value) / 1000;
  });
  const originalStartRecording = startRec;
  startRec = () => {
    if (network && !network.ready) {
      audioState.textContent = "Aguarde o áudio do R20 antes de gravar.";
      return;
    }
    originalStartRecording();
  };

  async function openZoom() {
    if (recorder || pendingZoom) return;
    releaseNetwork();
    pendingZoom = true;
    audioButton.disabled = wifi.disabled = true;
    let input = null;
    try {
      await native("zoom");
      if (!navigator.mediaDevices?.getUserMedia) throw Error("Captura de áudio indisponível nesta versão do iOS.");
      const devices = await navigator.mediaDevices.enumerateDevices();
      const device = devices.find(d => d.kind === "audioinput" && /zoom|r20/i.test(d.label));
      const onlyZoom = device?.deviceId ? { deviceId: { exact: device.deviceId } } : {};
      input = await navigator.mediaDevices.getUserMedia({ audio: {
        ...onlyZoom, channelCount: { ideal: 2 }, echoCancellation: false, noiseSuppression: false, autoGainControl: false
      } });
      if (!await native("zoomActive")) throw Error("O iOS não manteve a rota do R20. Confira o USB e tente novamente.");
      const ctx = audioCtx || new AudioContext();
      await ctx.resume();
      const meter = ctx.createAnalyser();
      meter.fftSize = 1024;
      ctx.createMediaStreamSource(input).connect(meter);
      if (audioStream && audioStream !== routeDest?.stream) audioStream.getTracks().forEach(track => track.stop());
      audioStream = usbAudio = input;
      audioCtx = ctx;
      analyser = meter;
      const select = $("audioIn");
      if (!select.options.length) select.add(new Option("(sem áudio)", ""));
      if (![...select.options].some(option => option.value === "__zoom")) select.add(new Option("Zoom R20 (USB)", "__zoom"));
      select.value = "__zoom";
      input.getAudioTracks().forEach(track => track.addEventListener("ended", () => {
        if (usbAudio === input) {
          usbAudio = null;
          if (recorder) stopRec();
          audioState.textContent = "O áudio do R20 foi interrompido.";
        }
      }));
      audioState.textContent = "Zoom R20 ativo na gravação. Confira o medidor.";
    } catch (error) {
      if (input && input !== usbAudio) input.getTracks().forEach(track => track.stop());
      audioState.textContent = error.message || "Não foi possível abrir o R20.";
    } finally {
      pendingZoom = false;
      audioButton.disabled = wifi.disabled = Boolean(recorder);
    }
  }
  $("audioIn").onchange = () => {
    const selected = $("audioIn").value;
    if (selected === "__r20wifi") return;
    releaseNetwork();
    if (selected === "__zoom") openZoom();
    else openAudio().catch(error => { audioState.textContent = error.message; });
  };
  new MutationObserver(() => {
    const active = $("recBtn").classList.contains("on");
    audioButton.disabled = wifi.disabled = active || pendingZoom;
    $("audioIn").disabled = active;
    native("recording", { active }).catch(() => {});
  }).observe($("recBtn"), { attributes: true, attributeFilter: ["class"] });
  window.cortexPause = () => {
    if (recorder) stopRec();
    releaseNetwork();
    stopBluetooth();
    send(OFF);
    clearInterval(keepTimer);
    releaseVideo();
    if (audioStream && audioStream !== routeDest?.stream) audioStream.getTracks().forEach(track => track.stop());
    if (routeStream) routeStream.getTracks().forEach(track => track.stop());
    audioStream = usbAudio = null;
    analyser = null;
  };
})();
