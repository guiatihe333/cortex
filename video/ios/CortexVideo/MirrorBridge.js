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
      const output = $("out");
      previewContext.fillStyle = "#000";
      previewContext.fillRect(0, 0, 60, 60);
      const scale = 60 / Math.max(output.width, output.height);
      const width = output.width * scale, height = output.height * scale;
      previewContext.drawImage(output, (60 - width) / 2, (60 - height) / 2, width, height);
      const data = previewContext.getImageData(0, 0, 60, 60).data;
      let encoded = "";
      for (let i = 0; i < data.length; i += 4) {
        encoded += String.fromCharCode((data[i] & 224) | ((data[i + 1] & 224) >> 3) | (data[i + 2] >> 6));
      }
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
  async function openZoom() {
    audioButton.disabled = true;
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
    } finally { audioButton.disabled = false; }
  }
  $("audioIn").onchange = () => {
    if ($("audioIn").value === "__zoom") openZoom();
    else openAudio().catch(error => { audioState.textContent = error.message; });
  };
  new MutationObserver(() => {
    const active = $("recBtn").classList.contains("on");
    audioButton.disabled = active;
    $("audioIn").disabled = active;
    native("recording", { active }).catch(() => {});
  }).observe($("recBtn"), { attributes: true, attributeFilter: ["class"] });
  window.cortexPause = () => {
    if (recorder) stopRec();
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
