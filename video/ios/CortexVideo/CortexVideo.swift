// SPDX-License-Identifier: MPL-2.0
import AVFoundation
import AVKit
import CoreImage
import CoreTransferable
import Photos
import PhotosUI
import SwiftUI
import UIKit
import UniformTypeIdentifiers

struct ImportedMovie: Transferable {
    let url: URL

    static var transferRepresentation: some TransferRepresentation {
        FileRepresentation(contentType: .movie) { SentTransferredFile($0.url) } importing: {
            ImportedMovie(url: try copyMovie($0.file))
        }
    }

    static func copyMovie(_ url: URL) throws -> URL {
        let ext = url.pathExtension.isEmpty ? "mov" : url.pathExtension
        let target = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
            .appendingPathExtension(ext)
        try FileManager.default.copyItem(at: url, to: target)
        return target
    }
}

@MainActor
final class Studio: ObservableObject {
    @Published private(set) var source: VideoSource?
    @Published private(set) var player: AVPlayer?
    @Published private(set) var status = "Conecte o FM-1 por USB."
    @Published private(set) var message = ""
    @Published private(set) var recording = false
    @Published private(set) var busy = false
    @Published private(set) var lastRecording: URL?

    let camera = CameraCapture()
    private let midi = FM1Output()
    private let bluetooth = BluetoothVideoOutput()
    @Published var useBluetooth = true {
        didSet {
            if useBluetooth { midi.stop() } else { bluetooth.stop() }
        }
    }
    private let sampler = FrameSampler()
    private var videoOutput: AVPlayerItemVideoOutput?
    private var movieURL: URL?
    private var movieTransform = CGAffineTransform.identity
    private var endObserver: NSObjectProtocol?
    private var pixels: [UInt8]?
    private var nextSample: TimeInterval = 0
    private var active = true
    private var microphoneAllowed = false
    private var clipBuffer: CVPixelBuffer?
    private let photoContext = CIContext()
    private let fm1Photo = FM1Photo()

    init() {
        midi.command = { [weak self] cc in self?.remote(cc) }
        midi.received = { [weak self] bytes in self?.fm1Photo.feed(bytes) }
        bluetooth.receivedBLE = { [weak self] bytes in self?.fm1Photo.feedBLE(bytes) }
        fm1Photo.done = { [weak self] image in self?.savePicture(image, what: "Foto do FM-1") }
        bluetooth.command = { [weak self] cc in self?.remote(cc) }
        camera.frame = { [weak self] frame in
            guard let self, self.source == .camera, !frame.isEmpty else { return }
            self.pixels = frame
        }
        camera.finished = { [weak self] url, error in
            guard let self else { return }
            self.recording = false
            self.busy = false
            if let error {
                try? FileManager.default.removeItem(at: url)
                self.message = "A gravação falhou. " + error.localizedDescription
            } else {
                self.lastRecording = url
                self.message = "Gravação salva. Você pode compartilhar ou salvar em Fotos."
            }
        }
    }

    func openCamera() async {
        guard !busy, !recording else { return }
        busy = true
        guard await AVCaptureDevice.requestAccess(for: .video) else {
            message = "Permita o acesso à câmera nos Ajustes do iPhone."
            busy = false
            return
        }
        microphoneAllowed = await AVCaptureDevice.requestAccess(for: .audio)
        guard active else { busy = false; return }
        clearMovie()
        changeSource(.camera)
        message = microphoneAllowed ? "" : "As gravações serão feitas sem microfone."
        camera.start(microphone: microphoneAllowed) { [weak self] started in
            guard let self else { return }
            self.busy = false
            if !started {
                self.changeSource(nil)
                self.message = "A câmera está indisponível. Feche outros apps que usam a câmera."
            }
        }
    }

    func importPhotos(_ item: PhotosPickerItem) async {
        guard !busy, !recording else { return }
        busy = true
        defer { busy = false }
        do {
            guard let movie = try await item.loadTransferable(type: ImportedMovie.self) else {
                message = "O vídeo escolhido não pôde ser carregado."
                return
            }
            try await openMovie(movie.url)
        } catch { message = "Não foi possível importar o vídeo. " + error.localizedDescription }
    }

    func importFile(_ result: Result<[URL], Error>) async {
        guard !busy, !recording else { return }
        busy = true
        defer { busy = false }
        do {
            guard let url = try result.get().first else { return }
            let scoped = url.startAccessingSecurityScopedResource()
            defer { if scoped { url.stopAccessingSecurityScopedResource() } }
            let local = try await Task.detached(priority: .userInitiated) { try ImportedMovie.copyMovie(url) }.value
            try await openMovie(local)
        } catch { message = "Não foi possível abrir o arquivo. " + error.localizedDescription }
    }

    private func openMovie(_ url: URL) async throws {
        let asset = AVURLAsset(url: url)
        do {
            guard let track = try await asset.loadTracks(withMediaType: .video).first else {
                throw CocoaError(.fileReadCorruptFile)
            }
            movieTransform = try await track.load(.preferredTransform)
        } catch {
            try? FileManager.default.removeItem(at: url)
            throw error
        }
        camera.stop()
        clearMovie()
        movieURL = url
        let item = AVPlayerItem(asset: asset)
        let output = AVPlayerItemVideoOutput(pixelBufferAttributes: [
            kCVPixelBufferPixelFormatTypeKey as String: kCVPixelFormatType_32BGRA
        ])
        item.add(output)
        videoOutput = output
        let newPlayer = AVPlayer(playerItem: item)
        player = newPlayer
        endObserver = NotificationCenter.default.addObserver(
            forName: .AVPlayerItemDidPlayToEndTime, object: item, queue: .main
        ) { [weak newPlayer] _ in
            newPlayer?.seek(to: .zero)
            newPlayer?.play()
        }
        changeSource(.clip)
        if active { player?.play() }
        message = ""
    }

    private func clearMovie() {
        player?.pause()
        player?.replaceCurrentItem(with: nil)
        if let endObserver { NotificationCenter.default.removeObserver(endObserver) }
        self.endObserver = nil
        player = nil
        videoOutput = nil
        if let movieURL { try? FileManager.default.removeItem(at: movieURL) }
        movieURL = nil
    }

    private func changeSource(_ newSource: VideoSource?) {
        source = newSource
        pixels = nil
        nextSample = 0
        midi.changeSource(newSource)
        bluetooth.changeSource(newSource)
    }

    func toggleRecording() {
        guard source == .camera, !busy else { return }
        if recording {
            busy = true
            camera.finishRecording()
            return
        }
        do {
            let documents = try FileManager.default.url(for: .documentDirectory, in: .userDomainMask,
                                                        appropriateFor: nil, create: true)
            let url = documents.appendingPathComponent("Cortex-" + String(UUID().uuidString.prefix(8)) + ".mov")
            busy = true
            camera.record(url) { [weak self] started in
                guard let self else { return }
                self.busy = false
                self.recording = started
                self.message = started ? "" : "A gravação não pôde começar."
            }
        } catch { message = error.localizedDescription }
    }

    // the FM-1's buttons (FM1Remote): REC records, EDIT takes a photo, PLAY starts or stops the camera / the clip
    func remote(_ cc: UInt8) {
        switch cc {
        case FM1Remote.record:
            if source == .camera { toggleRecording() } else { message = "FM-1: para gravar, abra a câmera." }
        case FM1Remote.photo: break           // (the FM-1 sends its own picture: FM1Photo)
        case FM1Remote.run: toggleRunning()
        default: break
        }
    }

    func toggleRunning() {
        if source == .camera {
            camera.stop()
            recording = false
            changeSource(nil)
            message = "Câmera parada pelo FM-1."
        } else if source == .clip, let player {
            if player.rate == 0 { player.play() } else { player.pause() }
        } else {
            Task { await openCamera() }
        }
    }

    func takePhoto() {
        if source == .camera {
            camera.snapshot { [weak self] buffer in self?.savePhoto(buffer, transform: .identity) }
        } else if source == .clip {
            savePhoto(clipBuffer, transform: movieTransform)
        } else { message = "Abra a câmera ou um vídeo para tirar foto." }
    }

    private func savePhoto(_ buffer: CVPixelBuffer?, transform: CGAffineTransform) {
        guard let buffer else { message = "Ainda sem imagem para a foto."; return }
        let picture = CIImage(cvPixelBuffer: buffer).transformed(by: transform)
        guard let cgImage = photoContext.createCGImage(picture, from: picture.extent) else {
            message = "A foto não pôde ser feita."
            return
        }
        savePicture(UIImage(cgImage: cgImage), what: "Foto do iPhone")
    }

    func savePicture(_ image: UIImage, what: String) {
        Task {
            let permission = await PHPhotoLibrary.requestAuthorization(for: .addOnly)
            guard permission == .authorized || permission == .limited else {
                message = "Permita adicionar a Fotos nos Ajustes do iPhone."
                return
            }
            do {
                try await PHPhotoLibrary.shared().performChanges {
                    PHAssetChangeRequest.creationRequestForAsset(from: image)
                }
                message = what + " salva em Fotos."
            } catch { message = "Fotos não conseguiu salvar a foto. " + error.localizedDescription }
        }
    }

    func saveToPhotos() async {
        guard let url = lastRecording else { return }
        let permission = await PHPhotoLibrary.requestAuthorization(for: .addOnly)
        guard permission == .authorized || permission == .limited else {
            message = "Fotos indisponível. Use Compartilhar para salvar em Arquivos."
            return
        }
        do {
            try await PHPhotoLibrary.shared().performChanges {
                PHAssetChangeRequest.creationRequestForAssetFromVideo(atFileURL: url)
            }
            message = "Vídeo salvo em Fotos."
        } catch { message = "Fotos não conseguiu salvar o vídeo. " + error.localizedDescription }
    }

    func tick() {
        guard active else { return }
        let now = CACurrentMediaTime()
        if source == .clip, now >= nextSample, let videoOutput {
            nextSample = now + 0.18
            let itemTime = videoOutput.itemTime(forHostTime: now)
            if videoOutput.hasNewPixelBuffer(forItemTime: itemTime),
               let buffer = videoOutput.copyPixelBuffer(forItemTime: itemTime, itemTimeForDisplay: nil) {
                clipBuffer = buffer
                let sample = sampler.pixels(buffer, transform: movieTransform)
                if !sample.isEmpty { pixels = sample }
            }
        }
        let current = useBluetooth ? bluetooth.step(pixels) : midi.step(pixels, now: now)
        if current != status { status = current }
    }

    func setActive(_ active: Bool) {
        guard self.active != active else { return }
        self.active = active
        if active {
            if source == .camera { camera.start(microphone: microphoneAllowed) { _ in } }
            if source == .clip { player?.play() }
        } else {
            player?.pause()
            camera.stop()
            midi.stop()
            bluetooth.stop()
            pixels = nil
        }
    }
}

@main
struct CortexVideoApp: App {
    var body: some Scene {
        WindowGroup { StudioView() }
    }
}

@MainActor
struct StudioView: View {
    @StateObject private var studio = Studio()
    @State private var selection: PhotosPickerItem?
    @State private var filePicker = false
    @Environment(\.scenePhase) private var scenePhase
    private let clock = Timer.publish(every: 0.024, on: .main, in: .common).autoconnect()

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 16) {
                Text("CORTEX VIDEO").font(.title2.bold())
                Picker("Conexão com FM-1", selection: $studio.useBluetooth) {
                    Text("Bluetooth").tag(true)
                    Text("USB").tag(false)
                }
                .pickerStyle(.segmented)
                Text(studio.status).font(.subheadline).foregroundStyle(.secondary)
                ZStack {
                    Rectangle().fill(.black)
                    if studio.source == .camera {
                        CameraPreview(session: studio.camera.session)
                    } else if let player = studio.player {
                        VideoPlayer(player: player)
                    }
                }
                .aspectRatio(1, contentMode: .fit)
                .clipShape(RoundedRectangle(cornerRadius: 12))
                HStack {
                    Button("Câmera") { Task { await studio.openCamera() } }
                    PhotosPicker(selection: $selection, matching: .videos) { Text("Fotos") }
                    Button("Arquivos") { filePicker = true }
                }
                .buttonStyle(.bordered)
                .disabled(studio.busy || studio.recording)
                if studio.source != nil {
                    Button("Foto do iPhone") { studio.takePhoto() }.buttonStyle(.bordered).disabled(studio.busy)
                }
                if studio.source == .camera {
                    Button(studio.recording ? "Parar gravação" : "Gravar vídeo") { studio.toggleRecording() }
                        .buttonStyle(.borderedProminent).tint(.red).disabled(studio.busy)
                }
                if let url = studio.lastRecording {
                    HStack {
                        ShareLink(item: url) { Label("Compartilhar", systemImage: "square.and.arrow.up") }
                        Button("Salvar em Fotos") { Task { await studio.saveToPhotos() } }
                    }
                    .buttonStyle(.bordered)
                }
                Text("No FM-1 (Live Video / Video import): REC grava · EDIT foto do FM-1 (com efeitos, salva em Fotos) · PLAY inicia ou para · FX mostra as fotos guardadas no FM-1.")
                    .font(.caption).foregroundStyle(.secondary)
                if !studio.message.isEmpty {
                    Text(studio.message).font(.footnote).foregroundStyle(.secondary)
                }
            }
            .padding()
        }
        .onReceive(clock) { _ in studio.tick() }
        .onChange(of: scenePhase) { studio.setActive($0 != .background) }
        .onChange(of: selection) { item in
            guard let item else { return }
            Task { await studio.importPhotos(item); selection = nil }
        }
        .fileImporter(isPresented: $filePicker, allowedContentTypes: [.movie, .mpeg4Movie, .quickTimeMovie],
                      allowsMultipleSelection: false) { result in
            Task { await studio.importFile(result) }
        }
    }
}


