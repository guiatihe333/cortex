// SPDX-License-Identifier: MPL-2.0
import AVFoundation
import CoreImage
import UIKit
import SwiftUI

final class FrameSampler {
    private let context = CIContext()
    private let color = CGColorSpace(name: CGColorSpace.sRGB)!
    private let bounds = CGRect(x: 0, y: 0, width: VideoWire.side, height: VideoWire.side)

    func pixels(_ buffer: CVPixelBuffer, transform: CGAffineTransform = .identity) -> [UInt8] {
        let image = CIImage(cvPixelBuffer: buffer).transformed(by: transform)
        let extent = image.extent
        guard extent.width > 0, extent.height > 0 else { return [] }
        let scale = min(bounds.width / extent.width, bounds.height / extent.height)
        let centered = image.transformed(by: CGAffineTransform(translationX: -extent.minX, y: -extent.minY))
            .transformed(by: CGAffineTransform(scaleX: scale, y: scale))
            .transformed(by: CGAffineTransform(
                translationX: (bounds.width - extent.width * scale) / 2,
                y: (bounds.height - extent.height * scale) / 2))
        let square = centered.composited(over: CIImage(color: .black).cropped(to: bounds))
        var rgba = [UInt8](repeating: 0, count: VideoWire.side * VideoWire.side * 4)
        rgba.withUnsafeMutableBytes { bytes in
            if let address = bytes.baseAddress {
                context.render(square, toBitmap: address, rowBytes: VideoWire.side * 4,
                               bounds: bounds, format: .RGBA8, colorSpace: color)
            }
        }
        var pixels = [UInt8](repeating: 0, count: VideoWire.side * VideoWire.side)
        for index in 0..<pixels.count {          // RGBA to RGB332, a plane at a time (typed: quick to compile)
            let red: UInt8 = rgba[index * 4] & 0xE0
            let green: UInt8 = (rgba[index * 4 + 1] & 0xE0) >> 3
            let blue: UInt8 = rgba[index * 4 + 2] >> 6
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
    private var configured = false
    private var lastFrame: TimeInterval = -.infinity
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
        guard now - lastFrame >= 0.18, let buffer = CMSampleBufferGetImageBuffer(sampleBuffer) else { return }
        lastFrame = now
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

