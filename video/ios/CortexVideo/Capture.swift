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

    func start(microphone: Bool, completion: @escaping (Bool) -> Void) {
        queue.async {
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

    func record(_ url: URL, completion: @escaping (Bool) -> Void) {
        queue.async {
            let ready = self.session.isRunning && self.movies.connection(with: .video)?.isActive == true
                && !self.movies.isRecording
            if ready { self.movies.startRecording(to: url, recordingDelegate: self) }
            DispatchQueue.main.async { completion(ready) }
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
        guard now - lastFrame >= 0.06, let buffer = CMSampleBufferGetImageBuffer(sampleBuffer) else { return }
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
// answered), each with its colour (four cells of the 30 x 30, a byte each: Cb, Cr nibbles, steps of 8). The scale
// follows the size: about 11 bytes a block.
final class VideoCodec {
    static let zigzag: [Int] = [0, 1, 8, 16, 9, 2, 3, 10, 17, 24, 32, 25, 18, 11, 4, 5, 12, 19, 26, 33, 40, 48,
                                41, 34, 27, 20, 13, 6, 7, 14, 21, 28, 35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23,
                                30, 37, 44, 51, 58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63]
    static let qbase: [Int] = [16, 11, 10, 16, 24, 40, 51, 61, 12, 12, 14, 19, 26, 58, 60, 55,
                               14, 13, 16, 24, 40, 57, 69, 56, 14, 17, 22, 29, 51, 87, 80, 62,
                               18, 22, 37, 56, 68, 109, 103, 77, 24, 35, 55, 64, 81, 104, 113, 92,
                               49, 64, 78, 87, 103, 121, 120, 101, 72, 92, 95, 98, 112, 100, 103, 99]
    private let table: [Float]                       // [x * 8 + u]: C(u) / 2 cos((2x + 1) u pi / 16)
    private var refLuma = [Float](repeating: -1000, count: 14400)
    private var refChroma = [UInt8](repeating: 0, count: 900)
    private var scale = 16
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
        if result.coded > 40 {
            let perBlock: Int = (result.bytes.count - 33) / result.coded
            if perBlock > 14 { scale = min(80, scale + scale / 8 + 1) }
            else if perBlock < 8 { scale = max(6, scale - scale / 10 - 1) }
        }
        return result.bytes
    }

    private func encodeFrame(_ rgba: [UInt8], key: Bool, scale: Int) -> (bytes: [UInt8], luma: [Float], chroma: [UInt8], coded: Int) {
        var luma = [Float](repeating: 0, count: 14400)
        var cbSum = [Int](repeating: 0, count: 900)
        var crSum = [Int](repeating: 0, count: 900)
        for i in 0..<14400 {
            let r: Int = Int(rgba[i * 4]), g: Int = Int(rgba[i * 4 + 1]), b: Int = Int(rgba[i * 4 + 2])
            let y: Int = (77 * r + 150 * g + 29 * b) >> 8
            luma[i] = Float(y)
            let cell: Int = (i / 480) * 30 + (i % 120) / 4
            cbSum[cell] += (-43 * r - 85 * g + 128 * b) >> 8
            crSum[cell] += (128 * r - 107 * g - 21 * b) >> 8
        }
        var chroma = [UInt8](repeating: 0x88, count: 900)
        for c in 0..<900 {
            let cbN: Int = max(0, min(15, Int((Double(cbSum[c]) / 128.0).rounded()) + 8))
            let crN: Int = max(0, min(15, Int((Double(crSum[c]) / 128.0).rounded()) + 8))
            chroma[c] = UInt8(cbN << 4 | crN)
        }
        var step = [Int](repeating: 1, count: 64)
        for k in 0..<64 { step[k] = max(1, (VideoCodec.qbase[k] * scale + 8) / 16) }
        var out: [UInt8] = [0, 0, key ? 1 : 0, UInt8(scale)]
        out.append(contentsOf: [UInt8](repeating: 0, count: 29))
        var newLuma = refLuma
        var newChroma = refChroma
        var coded = 0
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
            var cells = [Int](repeating: 0, count: 4)
            var colourMoved = false
            for k in 0..<4 {
                let c: Int = (2 * by + k / 2) * 30 + 2 * bx + k % 2
                cells[k] = c
                let a = Int(chroma[c]), o = Int(refChroma[c])
                if abs((a >> 4) - (o >> 4)) > 1 || abs((a & 15) - (o & 15)) > 1 { colourMoved = true }
            }
            if !key && sad < threshold && !colourMoved { continue }
            coded += 1
            out[4 + (b >> 3)] |= UInt8(1 << (b & 7))
            for c in cells {
                out.append(chroma[c])
                newChroma[c] = chroma[c]
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
                    var q = Int((s / Float(step[k])).rounded())
                    if k != 0 { q = max(-127, min(127, q)) }
                    coef[k] = q
                }
            }
            let dc = max(-32768, min(32767, coef[0]))
            out.append(UInt8(truncatingIfNeeded: dc))
            out.append(UInt8(truncatingIfNeeded: dc >> 8))
            var last = 0
            for p in 1..<64 where coef[VideoCodec.zigzag[p]] != 0 { last = p }
            var run = 0
            if last > 0 {
                for p in 1...last {
                    let v = coef[VideoCodec.zigzag[p]]
                    if v == 0 { run += 1; continue }
                    while run > 15 { out.append(0xF0); run -= 16 }
                    if v >= -7 && v <= 7 {
                        out.append(UInt8(run << 4 | (v > 0 ? v : v + 16)))
                    } else {
                        out.append(UInt8(run << 4 | 8))
                        out.append(UInt8(truncatingIfNeeded: v))
                    }
                    run = 0
                }
            }
            out.append(0)
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
        let n = out.count
        out[0] = UInt8(n & 255)
        out[1] = UInt8(n >> 8)
        return (out, newLuma, newChroma, coded)
    }
}
