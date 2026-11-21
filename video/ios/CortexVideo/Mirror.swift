// SPDX-License-Identifier: MPL-2.0
import AVFoundation
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
        }

        func background() {
            midi.stop()
            stopBluetooth()
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
                      let encoded = request["pixels"] as? String, encoded.count == 4800,
                      let data = Data(base64Encoded: encoded), data.count == VideoWire.side * VideoWire.side
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
