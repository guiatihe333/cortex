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
        if sharp { return FrameSampler.packSharp(rgba) }
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

    // RGBA 120 x 120 to the sharp layout: the light dithered to 6 bits, the colour averaged over 4 x 4
    static func packSharp(_ rgba: [UInt8]) -> [UInt8] {
        let side = VideoWire.sharpSide, cside = VideoWire.sharpChroma
        var out = [UInt8](repeating: 128, count: VideoWire.sharpBytes)
        let bayer: [Int] = [0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5]
        var light = [Int](repeating: 0, count: side * side)
        var cb = [Int](repeating: 0, count: cside * cside)
        var cr = [Int](repeating: 0, count: cside * cside)
        for index in 0..<(side * side) {
            let r: Int = Int(rgba[index * 4]), g: Int = Int(rgba[index * 4 + 1]), b: Int = Int(rgba[index * 4 + 2])
            let y: Int = (77 * r + 150 * g + 29 * b) >> 8
            let d: Int = bayer[(index / side % 4) * 4 + index % 4] / 4
            light[index] = min(63, (y + d) >> 2)
            let cell: Int = (index / side / 4) * cside + (index % side) / 4
            cb[cell] += (-43 * r - 85 * g + 128 * b) >> 8
            cr[cell] += (128 * r - 107 * g - 21 * b) >> 8
        }
        var o = 0
        var index = 0
        while index < side * side {
            let a = light[index], b = light[index + 1], c = light[index + 2], e = light[index + 3]
            out[o] = UInt8(a << 2 | b >> 4)
            out[o + 1] = UInt8((b & 15) << 4 | c >> 2)
            out[o + 2] = UInt8((c & 3) << 6 | e)
            o += 3
            index += 4
        }
        for cell in 0..<(cside * cside) {
            out[o + cell] = UInt8(max(0, min(255, 128 + cb[cell] / 16)))
            out[o + cside * cside + cell] = UInt8(max(0, min(255, 128 + cr[cell] / 16)))
        }
        return out
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

