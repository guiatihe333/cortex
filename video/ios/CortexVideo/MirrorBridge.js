/* SPDX-License-Identifier: MPL-2.0 */
(() => {
  // Runs only in the bundled iPhone app. The desktop mirror does not load this bridge.
  const native = window.cortexNative;
  if (!native) return;
  $("iphoneUsb").hidden = true;
  if ($("toPhone")) $("toPhone").hidden = true;                    // (the PC's: here the app receives, below)
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
  // Receive the PC mirror's picture and sound over the local network (WebRTC; the PC's bridge passes the offer and
  // the answer): shown here in place of this mirror's own, recorded to an MP4 as the mirror's takes are.
  (() => {
    const section = document.createElement("section");
    section.innerHTML = '<h2>Receber do PC (Wi-Fi)</h2><p class="note">No PC: abra “Abrir ponte espelho.cmd”, ' +
      'e no espelho do PC toque em “Transmitir ao iPhone”. Digite aqui o endereço e o código mostrados.</p>';
    const address = document.createElement("input"), code = document.createElement("input");
    address.placeholder = "192.168.0.10:8783"; code.placeholder = "código de 16 caracteres";
    address.setAttribute("aria-label", "Endereço do espelho");
    code.setAttribute("aria-label", "Código do espelho");
    address.autocapitalize = code.autocapitalize = "off";
    address.autocomplete = code.autocomplete = "off";
    for (const field of [address, code]) { field.style.width = "100%"; field.style.margin = "4px 0"; section.append(field); }
    const row = document.createElement("div");
    row.className = "row";
    section.append(row);
    const connectButton = button("Conectar ao PC", row, () => pcLink ? stopPc() : startPc());
    const recButton = button("● Gravar", row, () => pcRec ? stopPcRec() : startPcRec());
    recButton.disabled = true;
    const listen = button("Ouvir no iPhone", row, () => {
      video.muted = !video.muted;
      listen.textContent = video.muted ? "Ouvir no iPhone" : "Silenciar prévia";
      if (!video.muted) video.play().catch(() => say("Não foi possível tocar a prévia."));
    });
    listen.disabled = true;
    const state = document.createElement("p");
    state.className = "note"; state.role = "status"; state.textContent = "desconectado";
    section.append(state);
    const target = document.querySelector("main") || document.body;
    section.style.gridColumn = "1 / -1";
    target.prepend(section);
    const video = document.createElement("video");
    video.playsInline = true; video.autoplay = true; video.muted = true;
    video.style.width = "100%"; video.style.background = "#000"; video.hidden = true;
    $("out").parentElement.prepend(video);
    let pcLink = null, pcStream = null, pcRec = null;
    try {
      address.value = localStorage.getItem("cortexPcAddress") || "";
    } catch (e) {}

    function say(text) { state.textContent = text; }
    function stereo(sdp) {
      const m = sdp.match(/a=rtpmap:(\d+) opus\/48000/i);
      if (!m) return sdp;
      return sdp.replace(new RegExp(`a=fmtp:${m[1]} ([^\r\n]*)`), (all, p) => `a=fmtp:${m[1]} ${p};stereo=1;sprop-stereo=1`);
    }
    async function startPc() {
      try { localStorage.setItem("cortexPcAddress", address.value.trim()); } catch (e) {}
      connectButton.textContent = "Desconectar do PC";
      say("Conectando à ponte do PC… (no PC, toque em Transmitir ao iPhone)");
      let pc;
      try { pc = new RTCPeerConnection({ iceServers: [] }); }
      catch (error) { stopPc(); say("WebRTC indisponível: " + error.message); return; }
      pcLink = pc;
      const tracks = new MediaStream();
      const update = () => {
        if (pcLink !== pc || pc.connectionState !== "connected" ||
            !tracks.getVideoTracks().length || !tracks.getAudioTracks().length) return;
        if (pcStream !== tracks) {
          stopBluetooth();
          releaseVideo();
          if (recorder) stopRec();
          pcStream = tracks;
          video.srcObject = tracks;
          video.hidden = false;
          $("out").style.display = "none";
          video.play().catch(() => {});
        }
        recButton.disabled = listen.disabled = false;
        say("Recebendo vídeo e som do PC.");
      };
      pc.ontrack = event => {
        if (pcLink !== pc) return;
        tracks.addTrack(event.track);
        update();
      };
      pc.onconnectionstatechange = () => {
        if (pcLink !== pc) return;
        if (pc.connectionState === "connected") update();
        else if (["failed", "disconnected"].includes(pc.connectionState)) {
          stopPc();
          say("Transmissão encerrada. Reinicie no PC e conecte de novo.");
        }
      };
      try {
        const offer = await native("espelhoConnect", { address: address.value.trim(), code: code.value.trim().toLowerCase() });
        if (pcLink !== pc) return;
        await pc.setRemoteDescription(JSON.parse(offer));
        const answer = await pc.createAnswer();
        answer.sdp = stereo(answer.sdp);
        await pc.setLocalDescription(answer);
        await new Promise(done => {
          if (pc.iceGatheringState === "complete") return done();
          const timer = setTimeout(done, 3000);
          pc.addEventListener("icegatheringstatechange", () => {
            if (pc.iceGatheringState === "complete") { clearTimeout(timer); done(); }
          });
        });
        if (pcLink !== pc) return;
        await native("espelhoAnswer", { answer: JSON.stringify(pc.localDescription) });
        if (pcLink === pc && pc.connectionState !== "connected") say("Conectando ao vídeo do PC…");
      } catch (error) {
        if (pcLink === pc) { stopPc(); say(error.message || "Não foi possível conectar ao PC."); }
      }
    }
    function stopPc() {
      stopPcRec();
      if (pcLink) pcLink.close();
      pcLink = null; pcStream = null;
      native("espelhoDisconnect").catch(() => {});
      video.srcObject = null; video.hidden = true; video.muted = true;
      $("out").style.display = "";
      recButton.disabled = listen.disabled = true;
      listen.textContent = "Ouvir no iPhone";
      connectButton.textContent = "Conectar ao PC";
      say("desconectado");
    }
    function startPcRec() {
      if (!pcStream || pcRec || recorder) return;
      if (!window.MediaRecorder) { say("Gravação WebRTC indisponível neste iOS."); return; }
      const mime = ["video/mp4;codecs=avc1,mp4a.40.2", "video/mp4"].find(m => MediaRecorder.isTypeSupported(m)) || "";
      const chunks = [];
      try {
        pcRec = new MediaRecorder(pcStream, mime ? { mimeType: mime, videoBitsPerSecond: 12000000, audioBitsPerSecond: 256000 } : {});
      } catch (e) { say("Gravação indisponível: " + e.message); return; }
      const recording = pcRec;
      recording.ondataavailable = event => { if (event.data.size) chunks.push(event.data); };
      recording.onstop = () => {
        if (chunks.length) saveTake(recording.mimeType || mime || "video/webm", chunks);
        else say("A gravação não recebeu quadros.");
        native("recording", { active: false }).catch(() => {});
      };
      try { recording.start(1000); }
      catch (error) { pcRec = null; say("Gravação indisponível: " + error.message); return; }
      native("recording", { active: true }).catch(() => {});
      recButton.textContent = "■ Parar";
      say("Gravando o que vem do PC.");
    }
    function stopPcRec() {
      if (!pcRec) return;
      const rec = pcRec;
      if (rec.state !== "inactive") rec.stop();
      pcRec = null;
      recButton.textContent = "● Gravar";
    }
    window.cortexStopPc = () => { if (pcLink || pcRec) stopPc(); };
  })();

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
    window.cortexStopPc?.();
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
