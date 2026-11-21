// SPDX-License-Identifier: MPL-2.0
import AVFoundation
import CoreImage
import UIKit
import SwiftUI

final class FrameSampler {
    private let context = CIContext()
    private let color = CGColorSpace(name: CGColorSpace.sRGB)!
    private let small = CGRect(x: 0, y: 0, width: VideoWire.side, height: VideoWire.side)
    private let large = CGRect(x: 0, y: 0, width: VideoWire.sharpSide, height: VideoWire.sharpSide)
    var sharp = false

    func pixels(_ buffer: CVPixelBuffer, transform: CGAffineTransform = .identity) -> [UInt8] {
        let sharp = self.sharp
        let bounds = sharp ? large : small
        let side = Int(bounds.width)
        let image = CIImage(cvPixelBuffer: buffer).transformed(by: transform)
        let extent = image.extent
        guard extent.width > 0, extent.height > 0 else { return [] }
        // fill the square, cut at the long sides (the middle kept): the whole screen, all 60 x 60 points used
        let scale = max(bounds.width / extent.width, bounds.height / extent.height)
        let centered = image.transformed(by: CGAffineTransform(translationX: -extent.minX, y: -extent.minY))
            .applyingFilter("CILanczosScaleTransform", parameters: [kCIInputScaleKey: scale, kCIInputAspectRatioKey: 1.0])
            .transformed(by: CGAffineTransform(
                translationX: (bounds.width - extent.width * scale) / 2,
                y: (bounds.height - extent.height * scale) / 2))
        let square = centered.composited(over: CIImage(color: .black).cropped(to: bounds))
        var rgba = [UInt8](repeating: 0, count: side * side * 4)
        rgba.withUnsafeMutableBytes { bytes in
            if let address = bytes.baseAddress {
                context.render(square, toBitmap: address, rowBytes: side * 4,
                               bounds: bounds, format: .RGBA8, colorSpace: color)
            }
        }
        if sharp { return rgba }                   // (the sharp picture: RGBA 120 x 120, coded when it is sent)
        var pixels = [UInt8](repeating: 0, count: VideoWire.side * VideoWire.side)
        let bayer: [Int] = [0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5]
        for index in 0..<pixels.count {          // RGBA to RGB332, dithered (4 x 4 Bayer): no bands
            let d: Int = bayer[(index / VideoWire.side % 4) * 4 + index % 4]
            let r: Int = min(255, Int(rgba[index * 4]) + d * 2)
            let g: Int = min(255, Int(rgba[index * 4 + 1]) + d * 2)
            let b: Int = min(255, Int(rgba[index * 4 + 2]) + d * 4)
            let red: UInt8 = UInt8(r) & 0xE0
            let green: UInt8 = (UInt8(g) & 0xE0) >> 3
            let blue: UInt8 = UInt8(b) >> 6
            pixels[index] = red | green | blue
        }
        return pixels
    }
}

enum RecordingInput: String, CaseIterable, Identifiable {
    case automatic = "Automático"
    case zoom = "Zoom R20 (USB)"
    var id: Self { self }

    func activate() throws {
        let audio = AVAudioSession.sharedInstance()
        try audio.setCategory(.playAndRecord, mode: .videoRecording)
        try audio.setActive(true)
        if self == .automatic {
            try audio.setPreferredInput(nil)
            return
        }
        guard let zoom = audio.availableInputs?.first(where: Self.isZoom) else {
            throw NSError(domain: "CortexAudio", code: 1, userInfo: [
                NSLocalizedDescriptionKey: "Zoom R20 não detectado. Ative Audio Interface no R20 e confira o cabo USB."
            ])
        }
        try audio.setPreferredInput(zoom)
    }

    static func isZoom(_ port: AVAudioSessionPortDescription) -> Bool {
        port.portType == .usbAudio &&
            (port.portName.localizedCaseInsensitiveContains("R20") ||
             port.portName.localizedCaseInsensitiveContains("Zoom"))
    }

    var isCurrent: Bool {
        self == .automatic || AVAudioSession.sharedInstance().currentRoute.inputs.contains(where: Self.isZoom)
    }
}

final class CameraCapture: NSObject, AVCaptureVideoDataOutputSampleBufferDelegate, AVCaptureFileOutputRecordingDelegate {
    let session = AVCaptureSession()
    private let queue = DispatchQueue(label: "cortex.camera", qos: .userInitiated)
    private let samples = AVCaptureVideoDataOutput()
    private let movies = AVCaptureMovieFileOutput()
    private let sampler = FrameSampler()
    var sharp: Bool {
        get { sampler.sharp }
        set { sampler.sharp = newValue }
    }
    private var configured = false
    private var lastFrame: TimeInterval = -.infinity
    private var latest: CVPixelBuffer?
    var frame: (([UInt8]) -> Void)?
    var finished: ((URL, Error?) -> Void)?

    func start(microphone: Bool, input: RecordingInput, completion: @escaping (Bool) -> Void) {
        queue.async {
            if microphone { try? input.activate() }
            let ready = self.configured || self.configure(microphone: microphone)
            if ready && !self.session.isRunning { self.session.startRunning() }
            let running = ready && self.session.isRunning
            DispatchQueue.main.async { completion(running) }
        }
    }

    func stop() {
        queue.async {
            if self.movies.isRecording { self.movies.stopRecording() }
            if self.session.isRunning { self.session.stopRunning() }
        }
    }

    func record(_ url: URL, input: RecordingInput, completion: @escaping (String?) -> Void) {
        queue.async {
            if input == .zoom {
                do {
                    try input.activate()
                    guard input.isCurrent, self.movies.connection(with: .audio)?.isActive == true else {
                        DispatchQueue.main.async { completion("A entrada do R20 ainda não está ativa. Confira a permissão de microfone e tente novamente.") }
                        return
                    }
                } catch {
                    DispatchQueue.main.async { completion(error.localizedDescription) }
                    return
                }
            }
            let ready = self.session.isRunning && self.movies.connection(with: .video)?.isActive == true
                && !self.movies.isRecording
            if ready { self.movies.startRecording(to: url, recordingDelegate: self) }
            DispatchQueue.main.async { completion(ready ? nil : "A gravação não pôde começar.") }
        }
    }

    // the newest camera frame (full size, for a photo), handed over on the main queue
    func snapshot(_ done: @escaping (CVPixelBuffer?) -> Void) {
        queue.async {
            let buffer = self.latest
            DispatchQueue.main.async { done(buffer) }
        }
    }

    func finishRecording() {
        queue.async { if self.movies.isRecording { self.movies.stopRecording() } }
    }

    private func configure(microphone: Bool) -> Bool {
        guard let camera = AVCaptureDevice.default(.builtInWideAngleCamera, for: .video, position: .back),
              let device = try? AVCaptureDeviceInput(device: camera) else { return false }
        session.automaticallyConfiguresApplicationAudioSession = false
        session.beginConfiguration()
        defer {
            if !configured {
                session.outputs.forEach { session.removeOutput($0) }
                session.inputs.forEach { session.removeInput($0) }
            }
            session.commitConfiguration()
        }
        if session.canSetSessionPreset(.high) { session.sessionPreset = .high }
        guard session.canAddInput(device) else { return false }
        session.addInput(device)
        if microphone, let mic = AVCaptureDevice.default(for: .audio),
           let input = try? AVCaptureDeviceInput(device: mic), session.canAddInput(input) {
            session.addInput(input)
        }
        samples.alwaysDiscardsLateVideoFrames = true
        samples.videoSettings = [kCVPixelBufferPixelFormatTypeKey as String: kCVPixelFormatType_32BGRA]
        guard session.canAddOutput(samples) else { return false }
        session.addOutput(samples)
        samples.setSampleBufferDelegate(self, queue: queue)
        guard session.canAddOutput(movies) else { return false }
        session.addOutput(movies)
        for output in [samples as AVCaptureOutput, movies as AVCaptureOutput] {
            if let connection = output.connection(with: .video), connection.isVideoOrientationSupported {
                connection.videoOrientation = .portrait
            }
        }
        configured = true
        return true
    }

    func captureOutput(_ output: AVCaptureOutput, didOutput sampleBuffer: CMSampleBuffer,
                       from connection: AVCaptureConnection) {
        let now = CACurrentMediaTime()
        guard now - lastFrame >= 0.03, let buffer = CMSampleBufferGetImageBuffer(sampleBuffer) else { return }
        lastFrame = now
        latest = buffer
        let pixels = sampler.pixels(buffer)
        DispatchQueue.main.async { self.frame?(pixels) }
    }

    func fileOutput(_ output: AVCaptureFileOutput, didFinishRecordingTo url: URL,
                    from connections: [AVCaptureConnection], error: Error?) {
        let markedSuccessful = (error as NSError?)?.userInfo[AVErrorRecordingSuccessfullyFinishedKey] as? Bool == true
        DispatchQueue.main.async { self.finished?(url, markedSuccessful ? nil : error) }
    }
}

final class CameraPreviewView: UIView {
    override class var layerClass: AnyClass { AVCaptureVideoPreviewLayer.self }
    var preview: AVCaptureVideoPreviewLayer { layer as! AVCaptureVideoPreviewLayer }
}

struct CameraPreview: UIViewRepresentable {
    let session: AVCaptureSession
    func makeUIView(context: Context) -> CameraPreviewView {
        let view = CameraPreviewView()
        view.preview.session = session
        view.preview.videoGravity = .resizeAspect
        if let connection = view.preview.connection, connection.isVideoOrientationSupported {
            connection.videoOrientation = .portrait
        }
        return view
    }
    func updateUIView(_ view: CameraPreviewView, context: Context) {
        if let connection = view.preview.connection, connection.isVideoOrientationSupported {
            connection.videoOrientation = .portrait
        }
    }
}

// The sharp picture's code (video/app.c codec_decode on the FM-1): the light 120 x 120 in 8 x 8 blocks as JPEG codes
// them, only the blocks that changed since the FM-1's copy (every block each 30 frames, and after a frame not
// answered), each with its colour when it moved (its 4 x 4 of the 60 x 60, Cb and Cr each a 4 x 4 DCT). The scale
// follows the radio: coarser while frames wait for answers, finer while the link is free.
private struct BitWriter {                         // the first bit the highest; Exp-Golomb codes
    var bytes: [UInt8] = []
    private var acc: UInt32 = 0
    private var count = 0

    mutating func put(_ value: Int, _ n: Int) {
        if n <= 0 { return }
        for i in stride(from: n - 1, through: 0, by: -1) {
            acc = acc << 1 | UInt32((value >> i) & 1)
            count += 1
            if count == 8 {
                bytes.append(UInt8(acc & 255))
                acc = 0
                count = 0
            }
        }
    }

    mutating func eg(_ v: Int) {
        let m = v + 1
        let z = Int.bitWidth - m.leadingZeroBitCount - 1
        put(0, z)
        put(m, z + 1)
    }

    mutating func seg(_ v: Int) { eg(v > 0 ? 2 * v - 1 : -2 * v) }

    mutating func finish() -> [UInt8] {
        if count > 0 {
            bytes.append(UInt8((acc << UInt32(8 - count)) & 255))
            acc = 0
            count = 0
        }
        return bytes
    }
}

final class VideoCodec {
    static let zigzag: [Int] = [0, 1, 8, 16, 9, 2, 3, 10, 17, 24, 32, 25, 18, 11, 4, 5, 12, 19, 26, 33, 40, 48,
                                41, 34, 27, 20, 13, 6, 7, 14, 21, 28, 35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23,
                                30, 37, 44, 51, 58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63]
    static let qbase: [Int] = [16, 11, 10, 16, 24, 40, 51, 61, 12, 12, 14, 19, 26, 58, 60, 55,
                               14, 13, 16, 24, 40, 57, 69, 56, 14, 17, 22, 29, 51, 87, 80, 62,
                               18, 22, 37, 56, 68, 109, 103, 77, 24, 35, 55, 64, 81, 104, 113, 92,
                               49, 64, 78, 87, 103, 121, 120, 101, 72, 92, 95, 98, 112, 100, 103, 99]
    static let zigzag4: [Int] = [0, 1, 4, 8, 5, 2, 3, 6, 9, 12, 13, 10, 7, 11, 14, 15]
    static let qchroma: [Int] = [17, 18, 24, 47, 18, 21, 26, 66, 24, 26, 56, 99, 47, 66, 99, 99]
    private let table: [Float]                       // [x * 8 + u]: C(u) / 2 cos((2x + 1) u pi / 16)
    private let table4: [Float]                      // [x * 4 + u]: the 4 x 4's (the colour's)
    private var refLuma = [Float](repeating: -1000, count: 14400)
    private var refChroma = [Float](repeating: -1000, count: 7200)   // (Cb 60 x 60, then Cr: as the FM-1 has them)
    private var scale = 10
    private var busy = 0, calm = 0
    var congested = false                            // (set by the sender: the frames before not answered yet)
    private var count = 0
    var forceKey = true

    init() {
        var t = [Float](repeating: 0, count: 64)
        for x in 0..<8 {
            for u in 0..<8 {
                let angle: Double = Double((2 * x + 1) * u) * Double.pi / 16.0
                t[x * 8 + u] = u == 0 ? 0.35355339 : Float(0.5 * cos(angle))
            }
        }
        table = t
        var t4 = [Float](repeating: 0, count: 16)
        for x in 0..<4 {
            for u in 0..<4 {
                let angle: Double = Double((2 * x + 1) * u) * Double.pi / 8.0
                t4[x * 4 + u] = u == 0 ? 0.5 : Float(0.70710678 * cos(angle))
            }
        }
        table4 = t4
    }

    func encode(_ rgba: [UInt8]) -> [UInt8] {
        let key = forceKey || count % 30 == 0
        forceKey = false
        count += 1
        var result = encodeFrame(rgba, key: key, scale: scale)
        while result.bytes.count > 8000 && scale < 120 {
            scale = scale * 3 / 2
            result = encodeFrame(rgba, key: key, scale: scale)
        }
        refLuma = result.luma
        refChroma = result.chroma
        // the quality follows the radio: frames piling up (the sender waits for answers) code coarser; a free link
        // finer, down to scale 4 (JPEG's quality ~87)
        if congested { busy += 1 } else { calm += 1 }
        if busy + calm >= 15 {
            if busy > 6 { scale = min(32, scale + scale / 8 + 1) }   // (never coarser than 32: half JPEG's 50)
            else if busy < 2 { scale = max(4, scale - scale / 10 - 1) }
            busy = 0
            calm = 0
        }
        return result.bytes
    }

    private func encodeFrame(_ rgba: [UInt8], key: Bool, scale: Int) -> (bytes: [UInt8], luma: [Float], chroma: [Float], coded: Int) {
        var luma = [Float](repeating: 0, count: 14400)
        var chroma = [Float](repeating: 0, count: 7200)   // (Cb 60 x 60 then Cr, each the mean of 2 x 2)
        for i in 0..<14400 {
            let r: Int = Int(rgba[i * 4]), g: Int = Int(rgba[i * 4 + 1]), b: Int = Int(rgba[i * 4 + 2])
            let y: Int = (77 * r + 150 * g + 29 * b) >> 8
            luma[i] = Float(y)
            let cell: Int = (i / 240) * 60 + (i % 120) / 2
            chroma[cell] += Float((-43 * r - 85 * g + 128 * b) >> 8) * 0.25
            chroma[3600 + cell] += Float((128 * r - 107 * g - 21 * b) >> 8) * 0.25
        }
        var step = [Int](repeating: 1, count: 64)
        for k in 0..<64 { step[k] = max(1, (VideoCodec.qbase[k] * scale + 8) / 16) }
        var cstep = [Int](repeating: 1, count: 16)
        for k in 0..<16 { cstep[k] = max(1, (VideoCodec.qchroma[k] * scale + 8) / 16) }
        var prevC = [0, 0]
        var f4 = [Float](repeating: 0, count: 16)
        var t4 = [Float](repeating: 0, count: 16)
        var q4 = [Int](repeating: 0, count: 16)
        var out: [UInt8] = [0, 0, key ? 1 : 0, UInt8(scale)]
        out.append(contentsOf: [UInt8](repeating: 0, count: 29))
        var newLuma = refLuma
        var newChroma = refChroma
        var coded = 0
        var writer = BitWriter()
        var prevDC = 0
        let threshold: Float = Float(5 + scale / 4) * 64
        var block = [Float](repeating: 0, count: 64)
        var tmp = [Float](repeating: 0, count: 64)
        var coef = [Int](repeating: 0, count: 64)
        for b in 0..<225 {
            let by: Int = b / 15, bx: Int = b % 15
            var sad: Float = 0
            for y in 0..<8 {
                for x in 0..<8 {
                    let index: Int = (by * 8 + y) * 120 + bx * 8 + x
                    block[y * 8 + x] = luma[index]
                    sad += abs(luma[index] - refLuma[index])
                }
            }
            var colourDiff: Float = 0                // (its 4 x 4 colour cells against the FM-1's)
            for comp in 0..<2 {
                for y in 0..<4 {
                    for x in 0..<4 {
                        let c: Int = comp * 3600 + (by * 4 + y) * 60 + bx * 4 + x
                        colourDiff += abs(chroma[c] - refChroma[c])
                    }
                }
            }
            let colourMoved = colourDiff > Float(4 + scale / 4) * 32
            if !key && sad < threshold && !colourMoved { continue }
            coded += 1
            out[4 + (b >> 3)] |= UInt8(1 << (b & 7))
            let colour = key || colourMoved || refChroma[(by * 4) * 60 + bx * 4] < -500
            writer.put(colour ? 1 : 0, 1)
            if colour {                              // (each plane's 4 x 4 as the light: DCT, quantizer, runs)
                for comp in 0..<2 {
                    for y in 0..<4 {
                        for x in 0..<4 { f4[y * 4 + x] = chroma[comp * 3600 + (by * 4 + y) * 60 + bx * 4 + x] }
                    }
                    for y in 0..<4 {
                        for u in 0..<4 {
                            var s: Float = 0
                            for x in 0..<4 { s += table4[x * 4 + u] * f4[y * 4 + x] }
                            t4[y * 4 + u] = s
                        }
                    }
                    for v in 0..<4 {
                        for u in 0..<4 {
                            var s: Float = 0
                            for y in 0..<4 { s += table4[y * 4 + v] * t4[y * 4 + u] }
                            let k = v * 4 + u
                            if k == 0 {
                                q4[k] = Int((s / Float(cstep[k])).rounded())
                            } else {
                                let a = min(1023, Int(abs(s) / Float(cstep[k]) + 0.35))
                                q4[k] = s >= 0 ? a : -a
                            }
                        }
                    }
                    writer.seg(q4[0] - prevC[comp])
                    prevC[comp] = q4[0]
                    var nz4: [Int] = []
                    for p in 1..<16 where q4[VideoCodec.zigzag4[p]] != 0 { nz4.append(p) }
                    writer.eg(nz4.count)
                    var last4 = 0
                    for p in nz4 {
                        let v = q4[VideoCodec.zigzag4[p]]
                        writer.eg(p - last4 - 1)
                        writer.eg(abs(v) - 1)
                        writer.put(v < 0 ? 1 : 0, 1)
                        last4 = p
                    }
                    for v in 0..<4 {                 // (the FM-1's copy: back, as it decodes)
                        for x in 0..<4 {
                            var s: Float = 0
                            for u in 0..<4 { s += table4[x * 4 + u] * Float(q4[v * 4 + u] * cstep[v * 4 + u]) }
                            t4[v * 4 + x] = s
                        }
                    }
                    for y in 0..<4 {
                        for x in 0..<4 {
                            var s: Float = 0
                            for v in 0..<4 { s += table4[y * 4 + v] * t4[v * 4 + x] }
                            newChroma[comp * 3600 + (by * 4 + y) * 60 + bx * 4 + x] = max(-128, min(127, s.rounded()))
                        }
                    }
                }
            }
            for y in 0..<8 {                         // forward: across, then down
                for u in 0..<8 {
                    var s: Float = 0
                    for x in 0..<8 { s += table[x * 8 + u] * (block[y * 8 + x] - 128) }
                    tmp[y * 8 + u] = s
                }
            }
            for v in 0..<8 {
                for u in 0..<8 {
                    var s: Float = 0
                    for y in 0..<8 { s += table[y * 8 + v] * tmp[y * 8 + u] }
                    let k = v * 8 + u
                    if k == 0 {
                        coef[k] = Int((s / Float(step[k])).rounded())
                    } else {                         // (a dead zone: the small ones to zero, fewer bits)
                        let a = min(1023, Int(abs(s) / Float(step[k]) + 0.35))
                        coef[k] = s >= 0 ? a : -a
                    }
                }
            }
            writer.seg(coef[0] - prevDC)
            prevDC = coef[0]
            var nonzero: [Int] = []
            for p in 1..<64 where coef[VideoCodec.zigzag[p]] != 0 { nonzero.append(p) }
            writer.eg(nonzero.count)
            var last = 0
            for p in nonzero {
                let v = coef[VideoCodec.zigzag[p]]
                writer.eg(p - last - 1)
                writer.eg(abs(v) - 1)
                writer.put(v < 0 ? 1 : 0, 1)
                last = p
            }
            for v in 0..<8 {                         // the FM-1's copy: back, as it decodes
                for x in 0..<8 {
                    var s: Float = 0
                    for u in 0..<8 { s += table[x * 8 + u] * Float(coef[v * 8 + u] * step[v * 8 + u]) }
                    tmp[v * 8 + x] = s
                }
            }
            for y in 0..<8 {
                for x in 0..<8 {
                    var s: Float = 128
                    for v in 0..<8 { s += table[y * 8 + v] * tmp[v * 8 + x] }
                    newLuma[(by * 8 + y) * 120 + bx * 8 + x] = max(0, min(255, s.rounded()))
                }
            }
        }
        out.append(contentsOf: writer.finish())
        let n = out.count
        out[0] = UInt8(n & 255)
        out[1] = UInt8(n >> 8)
        return (out, newLuma, newChroma, coded)
    }
}
