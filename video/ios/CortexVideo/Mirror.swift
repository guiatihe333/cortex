// SPDX-License-Identifier: MPL-2.0
import AVFoundation
import Network
import Photos
import SwiftUI
import WebKit

struct MirrorScreen: View {
    @Environment(\.dismiss) private var dismiss
    @Environment(\.scenePhase) private var scenePhase
    @State private var file: URL?
    @State private var status = ""
    @State private var recording = false

    var body: some View {
        NavigationStack {
            MirrorWebView(file: $file, status: $status, recording: $recording, foreground: scenePhase != .background)
                .navigationTitle("Espelho")
                .navigationBarTitleDisplayMode(.inline)
                .toolbar {
                    ToolbarItem(placement: .cancellationAction) {
                        Button("Fechar") { dismiss() }.disabled(recording)
                    }
                    ToolbarItem(placement: .primaryAction) {
                        if let file {
                            Menu("Gravação") {
                                ShareLink(item: file) { Label("Compartilhar", systemImage: "square.and.arrow.up") }
                                if ["mov", "mp4"].contains(file.pathExtension.lowercased()) {
                                    Button("Salvar em Fotos") { Task { await save(file) } }
                                }
                            }
                        }
                    }
                }
                .safeAreaInset(edge: .bottom) {
                    if !status.isEmpty {
                        Text(status).font(.caption).padding(8).frame(maxWidth: .infinity).background(.thinMaterial)
                    }
                }
        }
    }

    @MainActor private func save(_ file: URL) async {
        let permission = await PHPhotoLibrary.requestAuthorization(for: .addOnly)
        guard permission == .authorized || permission == .limited else {
            status = "Permita acesso a Fotos ou compartilhe por Arquivos."
            return
        }
        do {
            try await PHPhotoLibrary.shared().performChanges {
                _ = PHAssetChangeRequest.creationRequestForAssetFromVideo(atFileURL: file)
            }
            status = "Vídeo salvo em Fotos."
        } catch { status = error.localizedDescription }
    }
}

struct MirrorWebView: UIViewRepresentable {
    @Binding var file: URL?
    @Binding var status: String
    @Binding var recording: Bool
    let foreground: Bool

    func makeCoordinator() -> Coordinator { Coordinator(self) }

    func makeUIView(context: Context) -> WKWebView {
        let config = WKWebViewConfiguration()
        config.allowsInlineMediaPlayback = true
        config.mediaTypesRequiringUserActionForPlayback = []
        let controller = config.userContentController
        controller.addScriptMessageHandler(context.coordinator, contentWorld: .page, name: "cortex")
        controller.addUserScript(WKUserScript(source: Self.usbBridge, injectionTime: .atDocumentStart, forMainFrameOnly: true))
        if let script = Bundle.main.url(forResource: "MirrorBridge", withExtension: "js"),
           let source = try? String(contentsOf: script, encoding: .utf8) {
            controller.addUserScript(WKUserScript(source: source, injectionTime: .atDocumentEnd, forMainFrameOnly: true))
        }
        let view = WKWebView(frame: .zero, configuration: config)
        view.navigationDelegate = context.coordinator
        view.uiDelegate = context.coordinator
        context.coordinator.view = view
        if let html = Bundle.main.url(forResource: "Mirror", withExtension: "html") {
            context.coordinator.html = html
            view.loadFileURL(html, allowingReadAccessTo: html)
        }
        return view
    }

    func updateUIView(_ view: WKWebView, context: Context) {
        context.coordinator.parent = self
        if !foreground { context.coordinator.background() }
    }

    static func dismantleUIView(_ view: WKWebView, coordinator: Coordinator) {
        coordinator.background()
        view.configuration.userContentController.removeScriptMessageHandler(forName: "cortex", contentWorld: .page)
        view.navigationDelegate = nil
        view.uiDelegate = nil
        view.stopLoading()
    }

    @MainActor final class Coordinator: NSObject, WKNavigationDelegate, WKUIDelegate, WKScriptMessageHandlerWithReply, WKDownloadDelegate {
        var parent: MirrorWebView
        weak var view: WKWebView?
        var html: URL?
        private let midi = FM1Output()
        private let bluetooth = BluetoothVideoOutput()
        private let r20Network = R20NetworkInput()
        private let pcLink = PCMirrorLink()
        private var midiBuffer: [UInt8] = []
        private var downloads: [ObjectIdentifier: URL] = [:]
        private var timer: Timer?
        private var bluetoothPixels: [UInt8]?
        private var bluetoothSource: VideoSource?

        init(_ parent: MirrorWebView) {
            self.parent = parent
            super.init()
            midi.received = { [weak self] bytes in self?.receive(bytes) }
            bluetooth.ready = { [weak self] in self?.sendBluetooth() }
            r20Network.audio = { [weak self] pcm, rate, channels in
                self?.view?.callAsyncJavaScript("window.cortexNetworkAudio?.(pcm, rate, channels)",
                    arguments: ["pcm": pcm.base64EncodedString(), "rate": rate, "channels": channels],
                    in: nil, in: .page, completionHandler: nil)
            }
            r20Network.problem = { [weak self] error in
                self?.view?.callAsyncJavaScript("window.cortexNetworkError?.(message)",
                    arguments: ["message": error], in: nil, in: .page, completionHandler: nil)
            }
        }

        func background() {
            midi.stop()
            stopBluetooth()
            r20Network.stop()
            pcLink.stop()
            view?.evaluateJavaScript("window.cortexPause?.()")
        }

        private func receive(_ bytes: [UInt8]) {
            var messages: [[UInt8]] = []
            for byte in bytes {
                if byte == 0xf0 { midiBuffer = [byte] }
                else if byte == 0xf7, !midiBuffer.isEmpty {
                    midiBuffer.append(byte)
                    messages.append(midiBuffer)
                    midiBuffer = []
                } else if byte < 0x80, !midiBuffer.isEmpty, midiBuffer.count < 1024 {
                    midiBuffer.append(byte)
                } else if byte < 0xf8 { midiBuffer = [] }
            }
            guard !messages.isEmpty, let data = try? JSONSerialization.data(withJSONObject: messages),
                  let json = String(data: data, encoding: .utf8) else { return }
            view?.evaluateJavaScript("window.cortexDeliverMIDI(\(json))")
        }

        private func sendBluetooth() {
            let state = bluetooth.step(bluetoothPixels)
            if parent.status != state { parent.status = state }
        }

        private func stopBluetooth() {
            timer?.invalidate()
            timer = nil
            bluetooth.stop()
            bluetooth.changeSource(nil)
            bluetoothPixels = nil
            bluetoothSource = nil
        }

        func userContentController(_ userContentController: WKUserContentController,
                                   didReceive message: WKScriptMessage,
                                   replyHandler: @escaping (Any?, String?) -> Void) {
            guard message.frameInfo.isMainFrame, let html, view?.url == html,
                  let request = message.body as? [String: Any], let action = request["action"] as? String
            else { replyHandler(nil, "Origem inválida."); return }
            switch action {
            case "connect":
                stopBluetooth()
                replyHandler(midi.attach(), nil)
            case "midi":
                guard let bytes = request["bytes"] as? [UInt8] else {
                    replyHandler(false, nil); return
                }
                replyHandler(midi.writeMirror(bytes), nil)
            case "ble":
                guard let type = request["source"] as? UInt8, let source = VideoSource(rawValue: type),
                      let encoded = request["pixels"] as? String,
                      let data = Data(base64Encoded: encoded),
                      data.count == VideoWire.sharpRGBA || data.count == VideoWire.cleanRGBA   // (RGBA 120 or 240)
                else { replyHandler(false, nil); return }
                if timer == nil {
                    midi.stop()
                    timer = Timer.scheduledTimer(withTimeInterval: 0.024, repeats: true) { [weak self] _ in
                        Task { @MainActor in self?.sendBluetooth() }
                    }
                }
                if bluetoothSource != source { bluetooth.changeSource(source); bluetoothSource = source }
                bluetoothPixels = [UInt8](data)
                replyHandler(true, nil)
            case "stopBLE":
                stopBluetooth()
                parent.status = ""
                replyHandler(true, nil)
            case "r20Connect":
                guard let address = request["address"] as? String, let code = request["code"] as? String else {
                    replyHandler(nil, "Informe o endereço e o código do PC."); return
                }
                do { try r20Network.connect(address: address, code: code); replyHandler(true, nil) }
                catch { replyHandler(nil, error.localizedDescription) }
            case "espelhoConnect":
                guard let address = request["address"] as? String, let code = request["code"] as? String else {
                    replyHandler(nil, "Informe o endereço e o código do PC."); return
                }
                pcLink.connect(address: address, code: code) { offer, error in replyHandler(offer, error) }
            case "espelhoDisconnect":
                pcLink.stop()
                replyHandler(true, nil)
            case "espelhoAnswer":
                guard let answer = request["answer"] as? String else { replyHandler(nil, "Resposta vazia."); return }
                pcLink.answer(answer) { error in replyHandler(error == nil, error) }
            case "r20Disconnect":
                r20Network.stop()
                replyHandler(true, nil)
            case "zoom":
                Task { @MainActor in
                    guard await AVCaptureDevice.requestAccess(for: .audio) else {
                        replyHandler(nil, "Permita acesso ao microfone para usar o R20.")
                        return
                    }
                    do { try RecordingInput.zoom.activate(); replyHandler(true, nil) }
                    catch { replyHandler(nil, error.localizedDescription) }
                }
            case "zoomActive":
                replyHandler(RecordingInput.zoom.isCurrent, nil)
            case "recording":
                parent.recording = request["active"] as? Bool ?? false
                replyHandler(true, nil)
            default: replyHandler(nil, "Comando desconhecido.")
            }
        }

        func webView(_ webView: WKWebView, decidePolicyFor action: WKNavigationAction,
                     decisionHandler: @escaping (WKNavigationActionPolicy) -> Void) {
            let url = action.request.url
            if url == html { decisionHandler(.allow) }
            else if action.shouldPerformDownload && url?.scheme == "blob" { decisionHandler(.download) }
            else { decisionHandler(.cancel) }
        }

        func webView(_ webView: WKWebView, navigationAction: WKNavigationAction, didBecome download: WKDownload) {
            download.delegate = self
        }

        func download(_ download: WKDownload, decideDestinationUsing response: URLResponse,
                      suggestedFilename: String, completionHandler: @escaping (URL?) -> Void) {
            let ext = URL(fileURLWithPath: suggestedFilename).pathExtension.lowercased()
            guard ["mp4", "mov", "webm", "png"].contains(ext),
                  let folder = try? FileManager.default.url(for: .documentDirectory, in: .userDomainMask,
                                                           appropriateFor: nil, create: true) else {
                completionHandler(nil); return
            }
            let url = folder.appendingPathComponent("Cortex-Espelho-" + String(UUID().uuidString.prefix(8))).appendingPathExtension(ext)
            downloads[ObjectIdentifier(download)] = url
            completionHandler(url)
        }

        func downloadDidFinish(_ download: WKDownload) {
            parent.file = downloads.removeValue(forKey: ObjectIdentifier(download))
            parent.status = "Arquivo salvo. Use Gravação para compartilhar."
        }

        func download(_ download: WKDownload, didFailWithError error: Error, resumeData: Data?) {
            if let url = downloads.removeValue(forKey: ObjectIdentifier(download)) {
                try? FileManager.default.removeItem(at: url)
            }
            parent.status = "Falha ao salvar: " + error.localizedDescription
        }
    }

    private static let usbBridge = #"""
    (() => {
      const call = (action, payload = {}) => window.webkit.messageHandlers.cortex.postMessage({action, ...payload});
      window.cortexNative = call;
      const input = {id:"fm1",name:"CORTEX VIDEO FM-1",state:"connected",onmidimessage:null};
      const output = {id:"fm1",name:input.name,state:"connected",send(bytes) {
        call("midi", {bytes:Array.from(bytes)}).catch(() => {});
      }};
      window.cortexDeliverMIDI = messages => {
        for (const message of messages) input.onmidimessage?.({data:Uint8Array.from(message)});
      };
      Object.defineProperty(navigator, "requestMIDIAccess", {value:async () => {
        if (!await call("connect")) throw Error("Conecte o FM-1 por USB, ou use Bluetooth abaixo.");
        return {inputs:new Map([[input.id,input]]),outputs:new Map([[output.id,output]])};
      }});
    })();
    """#
}

@MainActor
private final class R20NetworkInput {
    private var connection: NWConnection?
    private var authenticated = false
    var audio: ((Data, Int, Int) -> Void)?
    var problem: ((String) -> Void)?

    func connect(address: String, code: String) throws {
        let parts = address.trimmingCharacters(in: .whitespacesAndNewlines).split(separator: ":", omittingEmptySubsequences: false)
        let port = parts.count == 1 ? UInt16(8771) : UInt16(parts.last ?? "")
        guard (1...2).contains(parts.count), let host = parts.first, let ipv4 = IPv4Address(String(host)),
              Self.isPrivate([UInt8](ipv4.rawValue)), let number = port, number > 0,
              let endpointPort = NWEndpoint.Port(rawValue: number),
              code.utf8.count == 16, code.utf8.allSatisfy({ (48...57).contains($0) || (97...102).contains($0) }) else {
            throw NSError(domain: "CortexR20", code: 1, userInfo: [
                NSLocalizedDescriptionKey: "Informe o IPv4:porta do PC e o código de 16 caracteres da ponte."
            ])
        }

        stop()
        let stream = NWConnection(host: .ipv4(ipv4), port: endpointPort, using: .tcp)
        connection = stream
        stream.stateUpdateHandler = { [weak self, weak stream] state in
            Task { @MainActor in
                guard let self, let stream, self.connection === stream else { return }
                switch state {
                case .ready:
                    stream.send(content: Data((code + "\n").utf8), completion: .contentProcessed { error in
                        Task { @MainActor in
                            if error == nil { self.readHeader(stream) }
                            else { self.fail(stream, "Falha no envio do código. Verifique a ponte no PC.") }
                        }
                    })
                case .failed:
                    self.fail(stream, "Sem conexão com o PC. Confira o endereço, a rede local e a ponte.")
                default: break
                }
            }
        }
        stream.start(queue: .main)
        DispatchQueue.main.asyncAfter(deadline: .now() + 12) { [weak self, weak stream] in
            guard let self, let stream, self.connection === stream, !self.authenticated else { return }
            self.fail(stream, "Tempo esgotado. Confira a rede local, o firewall do PC e o código.")
        }
    }

    func stop() {
        let previous = connection
        connection = nil
        authenticated = false
        previous?.stateUpdateHandler = nil
        previous?.cancel()
    }

    private static func isPrivate(_ bytes: [UInt8]) -> Bool {
        bytes.count == 4 && (bytes[0] == 10
            || (bytes[0] == 172 && (16...31).contains(bytes[1]))
            || (bytes[0] == 192 && bytes[1] == 168))
    }

    private func fail(_ stream: NWConnection, _ message: String) {
        guard connection === stream else { return }
        stop()
        problem?(message)
    }

    private func read(_ stream: NWConnection, count: Int, reply: @escaping (Data) -> Void) {
        stream.receive(minimumIncompleteLength: count, maximumLength: count) { [weak self] data, _, _, _ in
            Task { @MainActor in
                guard let self, self.connection === stream else { return }
                guard let data, data.count == count else {
                    self.fail(stream, "O áudio do R20 pela rede foi interrompido.")
                    return
                }
                reply(data)
            }
        }
    }

    private func readHeader(_ stream: NWConnection) {
        read(stream, count: 4) { [weak self] marker in
            guard let self else { return }
            switch String(decoding: marker, as: UTF8.self) {
            case "CVR1": break
            case "NOPE": self.fail(stream, "Código de conexão incorreto."); return
            case "BUSY": self.fail(stream, "A ponte já está enviando áudio para outro dispositivo."); return
            default: self.fail(stream, "Resposta inválida da ponte R20."); return
            }
            self.read(stream, count: 8) { [weak self] metadata in
                guard let self else { return }
                let bytes = [UInt8](metadata)
                let rate = bytes.prefix(4).reduce(0) { ($0 << 8) | Int($1) }
                let channels = Int(bytes[4])
                guard [44_100, 48_000].contains(rate), (1...2).contains(channels) else {
                    self.fail(stream, "A ponte enviou um formato de áudio incompatível.")
                    return
                }
                self.authenticated = true
                self.readAudio(stream, rate: rate, channels: channels)
            }
        }
    }

    private func readAudio(_ stream: NWConnection, rate: Int, channels: Int) {
        read(stream, count: (rate / 25) * channels * 2) { [weak self] data in
            guard let self else { return }
            self.audio?(data, rate, channels)
            self.readAudio(stream, rate: rate, channels: channels)
        }
    }
}


// The PC mirror's bridge (video/tools/espelho-wifi.py): the code; its OFFER (the PC's WebRTC offer, one line of JSON);
// our ANSWER; OK. The picture and the sound then go straight from the PC's Chrome to the web view.
@MainActor
private final class PCMirrorLink {
    private var connection: NWConnection?
    private var buffer = Data()

    func connect(address: String, code: String, offer: @escaping (String?, String?) -> Void) {
        let parts = address.trimmingCharacters(in: .whitespacesAndNewlines).split(separator: ":", omittingEmptySubsequences: false)
        let port = parts.count == 1 ? UInt16(8783) : UInt16(parts.last ?? "")
        guard (1...2).contains(parts.count), let host = parts.first, let ipv4 = IPv4Address(String(host)),
              Self.isPrivate([UInt8](ipv4.rawValue)), let number = port, number > 0,
              let endpointPort = NWEndpoint.Port(rawValue: number),
              code.utf8.count == 16, code.utf8.allSatisfy({ (48...57).contains($0) || (97...102).contains($0) }) else {
            offer(nil, "Informe o IPv4:porta do PC e o código de 16 caracteres da ponte do espelho.")
            return
        }
        stop()
        let stream = NWConnection(host: .ipv4(ipv4), port: endpointPort, using: .tcp)
        connection = stream
        var replied = false
        let reply: (String?, String?) -> Void = { text, error in
            if replied { return }
            replied = true
            offer(text, error)
        }
        stream.stateUpdateHandler = { [weak self, weak stream] state in
            Task { @MainActor in
                guard let self, let stream, self.connection === stream else { return }
                switch state {
                case .ready:
                    stream.send(content: Data((code + "\n").utf8), completion: .contentProcessed { _ in })
                    self.readLine(stream) { line in
                        guard let line else { reply(nil, "A ponte do PC fechou a conexão."); return }
                        if line == "NOPE" { reply(nil, "Código incorreto."); return }
                        if line == "BUSY" { reply(nil, "A transmissão já está em uso."); return }
                        guard line.hasPrefix("OFFER ") else { reply(nil, "Resposta inválida da ponte do PC."); return }
                        reply(String(line.dropFirst(6)), nil)
                    }
                case .failed:
                    reply(nil, "Sem conexão com o PC. Confira o endereço, a rede local, o firewall e a ponte.")
                default: break
                }
            }
        }
        stream.start(queue: .main)
        DispatchQueue.main.asyncAfter(deadline: .now() + 95) { [weak self, weak stream] in
            guard let self, let stream, self.connection === stream else { return }
            reply(nil, "Tempo esgotado: no espelho do PC, toque em Transmitir ao iPhone.")
        }
    }

    func answer(_ text: String, done: @escaping (String?) -> Void) {
        guard let stream = connection else { done("Sem conexão com a ponte do PC."); return }
        stream.send(content: Data(("ANSWER " + text + "\n").utf8), completion: .contentProcessed { _ in })
        readLine(stream) { [weak self] line in
            done(line == "OK" ? nil : "A ponte do PC não aceitou a resposta.")
            self?.stop()
        }
    }

    func stop() {
        let previous = connection
        connection = nil
        buffer = Data()
        previous?.stateUpdateHandler = nil
        previous?.cancel()
    }

    private static func isPrivate(_ bytes: [UInt8]) -> Bool {
        bytes.count == 4 && (bytes[0] == 10
            || (bytes[0] == 172 && (16...31).contains(bytes[1]))
            || (bytes[0] == 192 && bytes[1] == 168))
    }

    private func readLine(_ stream: NWConnection, _ done: @escaping (String?) -> Void) {
        if let end = buffer.firstIndex(of: 10) {
            let line = String(decoding: buffer[buffer.startIndex..<end], as: UTF8.self)
            buffer.removeSubrange(buffer.startIndex...end)
            done(line)
            return
        }
        stream.receive(minimumIncompleteLength: 1, maximumLength: 65536) { [weak self] data, _, _, _ in
            Task { @MainActor in
                guard let self, self.connection === stream else { done(nil); return }
                guard let data, !data.isEmpty, self.buffer.count + data.count <= 65_552 else { done(nil); return }
                self.buffer.append(data)
                self.readLine(stream, done)
            }
        }
    }
}
