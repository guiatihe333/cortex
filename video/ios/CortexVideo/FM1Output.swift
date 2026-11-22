// SPDX-License-Identifier: MPL-2.0
import CoreMIDI
import CoreBluetooth
import Foundation
import CoreImage
import UIKit

// Owned by Studio on the main queue. Each timer tick writes at most one packet.
final class FM1Output {
    private var client: MIDIClientRef = 0
    private var port: MIDIPortRef = 0
    private var inPort: MIDIPortRef = 0
    private var source: MIDIEndpointRef = 0
    var command: ((UInt8) -> Void)?
    var received: (([UInt8]) -> Void)?
    private var destination: MIDIEndpointRef = 0
    private var selected: VideoSource?
    private var pending: [UInt8] = []
    private var part = 0
    private var frame: UInt8 = 0
    private var nextKeep: TimeInterval = 0
    private var needsSetup = true

    init() {
        if MIDIClientCreate("Cortex Video" as CFString, nil, nil, &client) == noErr {
            MIDIOutputPortCreate(client, "FM-1" as CFString, &port)
            MIDIInputPortCreateWithBlock(client, "FM-1 in" as CFString, &inPort) { [weak self] list, _ in
                var bytes: [UInt8] = []
                let offset = MemoryLayout<MIDIPacket>.offset(of: \MIDIPacket.data) ?? 10
                for packet in list.unsafeSequence() {      // (a SysEx packet may run past the 256 bytes declared)
                    let data = UnsafeRawPointer(packet).advanced(by: offset)
                    bytes.append(contentsOf: UnsafeRawBufferPointer(start: data, count: Int(packet.pointee.length)))
                }
                guard !bytes.isEmpty else { return }
                let commands = FM1Remote.commands(bytes)
                DispatchQueue.main.async {
                    self?.received?(bytes)
                    commands.forEach { self?.command?($0) }
                }
            }
        }
    }

    deinit {
        if port != 0 { MIDIPortDispose(port) }
        if inPort != 0 { MIDIPortDispose(inPort) }
        if client != 0 { MIDIClientDispose(client) }
    }

    func changeSource(_ source: VideoSource?) {
        if selected != source {
            selected = source
            pending = []
            needsSetup = true
        }
    }

    func stop() {
        if destination != 0 { _ = send(VideoWire.command([0])) }
        if source != 0 { MIDIPortDisconnectSource(inPort, source); source = 0 }
        pending = []
        destination = 0
        needsSetup = true
    }

    func step(_ pixels: [UInt8]?, now: TimeInterval) -> String {
        guard port != 0 else { return "Core MIDI indisponível." }
        guard let selected else { return "Escolha câmera ou vídeo." }
        if destination == 0 || now >= nextKeep {
            _ = attach()
        }
        guard destination != 0 else { return "Conecte o CORTEX VIDEO FM-1 ao iPhone por USB." }
        guard let pixels else { return "FM-1 conectado · aguardando imagem." }
        if needsSetup {
            guard send(VideoWire.command([2])), send(VideoWire.command([3, 0, selected.fm1Visual, 0])) else {
                return disconnect()
            }
            nextKeep = now + 1
            needsSetup = false
        } else if now >= nextKeep {
            guard send(VideoWire.command([1])) else { return disconnect() }
            nextKeep = now + 1
            return "FM-1 conectado · prévia 60 × 60."
        }
        if pending.isEmpty {
            pending = pixels
            frame &+= 1
            part = 0
        }
        guard send(VideoWire.chunk(pending, source: selected, frame: frame, part: part)) else {
            return disconnect()
        }
        part += 1
        if part == VideoWire.partCount { pending = [] }
        return "FM-1 conectado · prévia 60 × 60."
    }

    func attach() -> Bool {
        guard port != 0, inPort != 0 else { return false }
        let found = findFM1()
        let input = findFM1Source()
        if input != source {
            if source != 0 { MIDIPortDisconnectSource(inPort, source) }
            source = 0
            if input != 0 && MIDIPortConnectSource(inPort, input, nil) == noErr { source = input }
        }
        if destination != found {
            destination = found
            needsSetup = true
            pending = []
        }
        return destination != 0 && source != 0
    }

    func writeMirror(_ bytes: [UInt8]) -> Bool {
        guard destination != 0, (6...256).contains(bytes.count),
              Array(bytes.prefix(4)) == [0xF0, 0x7D, 0x43, 0x56], bytes.last == 0xF7,
              [0x10, 0x11].contains(bytes[4]),
              !bytes.dropFirst().dropLast().contains(where: { $0 >= 0x80 }) else { return false }
        return send(bytes)
    }

    private func disconnect() -> String {
        destination = 0
        pending = []
        needsSetup = true
        return "Conexão MIDI interrompida. Confira o cabo."
    }

    private func findFM1() -> MIDIEndpointRef {
        for index in 0..<MIDIGetNumberOfDestinations() {
            let endpoint = MIDIGetDestination(index)
            var name: Unmanaged<CFString>?
            if MIDIObjectGetStringProperty(endpoint, kMIDIPropertyDisplayName, &name) == noErr,
               let value = name?.takeRetainedValue(), (value as String).localizedCaseInsensitiveContains("CORTEX VIDEO") {
                return endpoint
            }
        }
        return 0
    }

    private func findFM1Source() -> MIDIEndpointRef {
        for index in 0..<MIDIGetNumberOfSources() {
            let endpoint = MIDIGetSource(index)
            var name: Unmanaged<CFString>?
            if MIDIObjectGetStringProperty(endpoint, kMIDIPropertyDisplayName, &name) == noErr,
               let value = name?.takeRetainedValue(), (value as String).localizedCaseInsensitiveContains("CORTEX VIDEO") {
                return endpoint
            }
        }
        return 0
    }

    private func send(_ bytes: [UInt8]) -> Bool {
        // Each message fits in one classic MIDI packet (256 bytes).
        var packets = MIDIPacketList()
        return withUnsafeMutablePointer(to: &packets) { list in
            let current = MIDIPacketListInit(list)
            return bytes.withUnsafeBufferPointer { data in
                guard let first = data.baseAddress,
                      MIDIPacketListAdd(list, MemoryLayout<MIDIPacketList>.size, current, 0, data.count, first) != nil
                else { return false }
                return MIDISend(port, destination, list) == noErr
            }
        }
    }
}


// The FM-1's remote: Control Change 102 (record), 103 (photo), 104 (start / stop) on channel 16, value 127. Found in
// plain MIDI bytes (USB) and in Bluetooth MIDI packets (their timestamps never pass as two data bytes).
enum FM1Remote {
    static let record: UInt8 = 102, photo: UInt8 = 103, run: UInt8 = 104

    static func commands(_ bytes: [UInt8]) -> [UInt8] {
        var found: [UInt8] = []
        var index = 0
        while index + 2 < bytes.count {
            if bytes[index] == 0xBF, bytes[index + 1] >= record, bytes[index + 1] <= run, bytes[index + 2] == 127 {
                found.append(bytes[index + 1])
                index += 3
            } else { index += 1 }
        }
        return found
    }
}

// The FM-1's photo (EDIT): its picture with the effects, 120 x 120 RGB565, a SysEx a row (F0 7D 43 56 12, 7-in-8:
// photo number, row, 240 bytes high first, F7), over USB and Bluetooth MIDI alike; each photo saved once.
final class FM1Photo {
    static let side = 120
    var done: ((UIImage) -> Void)?
    private var sysex: [UInt8]?
    private var pixels = [UInt8](repeating: 0, count: 120 * 120 * 2)
    private var rows = Set<Int>()
    private var current = -1
    private var finished = -1
    private let context = CIContext()

    func feed(_ bytes: [UInt8]) {                  // plain MIDI bytes (USB)
        for byte in bytes { take(byte) }
    }

    func feedBLE(_ packet: [UInt8]) {              // a Bluetooth MIDI packet: its header, timestamps before statuses
        var index = 1
        while index < packet.count {
            let byte = packet[index]
            if byte < 0x80 {
                take(byte)
                index += 1
            } else if index + 1 < packet.count, packet[index + 1] >= 0x80 {
                take(packet[index + 1])           // (a timestamp, then a status)
                index += 2
            } else { index += 1 }                 // (a timestamp before running status)
        }
    }

    private func take(_ byte: UInt8) {
        if byte == 0xF0 { sysex = [] }
        else if byte == 0xF7 { if let body = sysex { frame(body) }; sysex = nil }
        else if byte < 0x80 { sysex?.append(byte) }
        else if byte < 0xF8 { sysex = nil }
    }

    private func frame(_ body: [UInt8]) {
        guard body.count > 4, body[0] == 0x7D, body[1] == 0x43, body[2] == 0x56, body[3] == 0x12 else { return }
        var raw: [UInt8] = []
        raw.reserveCapacity(242)
        var index = 4
        while index < body.count {
            let high = body[index]
            index += 1
            var bit = 0
            while bit < 7 && index < body.count {
                raw.append(body[index] | ((high >> UInt8(bit)) & 1) << 7)
                index += 1
                bit += 1
            }
        }
        guard raw.count >= 2 + FM1Photo.side * 2 else { return }
        let id = Int(raw[0]), row = Int(raw[1])
        guard id != finished, row < FM1Photo.side else { return }
        if id != current { current = id; rows = [] }
        pixels.replaceSubrange(row * 240..<(row + 1) * 240, with: raw[2..<242])
        rows.insert(row)
        if rows.count == FM1Photo.side || (row == FM1Photo.side - 1 && rows.count >= FM1Photo.side - 12) {
            for missing in 1..<FM1Photo.side where !rows.contains(missing) {   // (a row lost: the one above)
                pixels.replaceSubrange(missing * 240..<(missing + 1) * 240,
                                       with: Array(pixels[(missing - 1) * 240..<missing * 240]))
            }
            finished = id
            current = -1
            if let image = makeImage() { done?(image) }
        }
    }

    private func makeImage() -> UIImage? {
        var rgba = [UInt8](repeating: 255, count: FM1Photo.side * FM1Photo.side * 4)
        for index in 0..<(FM1Photo.side * FM1Photo.side) {
            let c = Int(pixels[index * 2]) << 8 | Int(pixels[index * 2 + 1])
            rgba[index * 4] = UInt8((c >> 11) << 3)
            rgba[index * 4 + 1] = UInt8((c >> 5 & 63) << 2)
            rgba[index * 4 + 2] = UInt8((c & 31) << 3)
        }
        guard let provider = CGDataProvider(data: Data(rgba) as CFData),
              let small = CGImage(width: FM1Photo.side, height: FM1Photo.side, bitsPerComponent: 8, bitsPerPixel: 32,
                                  bytesPerRow: FM1Photo.side * 4, space: CGColorSpaceCreateDeviceRGB(),
                                  bitmapInfo: CGBitmapInfo(rawValue: CGImageAlphaInfo.noneSkipLast.rawValue),
                                  provider: provider, decode: nil, shouldInterpolate: true, intent: .defaultIntent)
        else { return nil }
        let big = CIImage(cgImage: small).applyingFilter("CILanczosScaleTransform",
                                                         parameters: [kCIInputScaleKey: 8.0, kCIInputAspectRatioKey: 1.0])
        guard let output = context.createCGImage(big, from: CGRect(x: 0, y: 0, width: 960, height: 960)) else { return nil }
        return UIImage(cgImage: output)
    }
}

// Bluetooth preview service (UUIDs match video/app.h). This does not use Core MIDI.

final class BluetoothVideoOutput: NSObject, CBCentralManagerDelegate, CBPeripheralDelegate {
    private let serviceID = CBUUID(string: "C05F152E-A864-47E1-9C06-78F7223B4E01")
    private let dataID = CBUUID(string: "C05F152E-A864-47E1-9C06-78F7223B4E02")
    private let midiServiceID = CBUUID(string: "03B80E5A-EDE8-4B33-A751-6CE34EC4C700")
    private let midiDataID = CBUUID(string: "7772E5DB-3868-4112-A1A9-F2669D106BF3")
    var command: ((UInt8) -> Void)?
    var receivedBLE: (([UInt8]) -> Void)?
    // flow control: after a whole frame, wait for the FM-1's CC 105 (that frame's number) before the next, so no
    // queue piles up between the camera and the picture; an old FM-1 (none) after 0.4 s
    // two frames on their way at most (the radio busy while the FM-1 answers the one before); the next goes out as
    // soon as an answer comes (ready)
    private var inFlight: [(id: UInt8, at: TimeInterval)] = []
    private var blocked = false
    private var lastPixels: [UInt8] = []             // (the picture last coded: a frame goes only for a new one)
    private var lastFrameAt: TimeInterval = 0
    var ready: (() -> Void)?
    private var acks: [TimeInterval] = []
    private let codec = VideoCodec()
    private var sharpFrame = false
    private var central: CBCentralManager?
    private var peripheral: CBPeripheral?
    private var characteristic: CBCharacteristic?
    private var source: VideoSource?
    private var pending: [UInt8] = []
    private var offset = 0
    private var frameID: UInt8 = 0
    private var active = false

    func changeSource(_ newSource: VideoSource?) {
        guard source != newSource else { return }
        source = newSource
        pending = []
    }

    func stop() {
        active = false
        central?.stopScan()
        if let peripheral { central?.cancelPeripheralConnection(peripheral) }
        peripheral = nil
        characteristic = nil
        pending = []
    }

    func step(_ pixels: [UInt8]?) -> String {
        guard pixels != nil, let source else { return "Abra a câmera ou importe um vídeo." }
        if !active {
            active = true
            if central == nil { central = CBCentralManager(delegate: self, queue: .main) }
            else { scan() }
        }
        switch central?.state {
        case .some(.poweredOff): return "Ative o Bluetooth do iPhone."
        case .some(.unauthorized): return "Permita Bluetooth nos Ajustes do iPhone."
        case .some(.unsupported): return "Bluetooth LE indisponível neste iPhone."
        default: break
        }
        guard let peripheral, let characteristic else {
            return "Buscando FM-1 · selecione Live Video ou Video import. no aparelho."
        }
        guard let pixels else { return "FM-1 conectado por Bluetooth." }
        let now = CACurrentMediaTime()
        acks.removeAll { now - $0 > 2 }
        let rate = String(format: " · %.1f q/s", Double(acks.count) / 2)
        let before = inFlight.count
        inFlight.removeAll { now - $0.at > 0.5 }
        if inFlight.count < before { codec.forceKey = true }   // (one not answered: every block again)
        // a new frame only for a new picture (or each half second, to keep the FM-1's copy fresh): the same picture
        // again would fill the link with frames of nothing and read as a congestion
        let fresh = pixels != lastPixels || now - lastFrameAt > 0.5
        if pending.isEmpty && (!fresh || inFlight.count >= 4) {
            if fresh { blocked = true }               // (a new picture waits: the link is the limit)
            return (sharpFrame ? (pixels.count == VideoWire.cleanRGBA ? "Bluetooth · limpa 240 × 240" : "Bluetooth · nítido 120 × 120") : "Bluetooth · 60 × 60") + rate
        }
        let limit = min(VideoWire.blePayload, peripheral.maximumWriteValueLength(for: .withoutResponse) - 4)
        guard limit > 0 else { return "O FM-1 não aceitou pacotes de vídeo Bluetooth." }
        var sent = 0
        while sent < 16 && peripheral.canSendWriteWithoutResponse {   // (as much as the link takes this tick)
            if pending.isEmpty {
                sharpFrame = pixels.count == VideoWire.sharpRGBA || pixels.count == VideoWire.cleanRGBA
                codec.congested = blocked         // (it waited for answers since the last frame)
                blocked = false
                pending = sharpFrame ? codec.encode(pixels) : pixels
                lastPixels = pixels
                lastFrameAt = now
                offset = 0
                frameID &+= 1
            }
            let count = min(limit, pending.count - offset)
            let kind = source.rawValue | (sharpFrame ? 0x80 : 0)
            var payload = Data([kind, frameID, UInt8(offset & 255), UInt8(offset >> 8)])
            payload.append(contentsOf: pending[offset..<(offset + count)])
            peripheral.writeValue(payload, for: characteristic, type: .withoutResponse)
            offset += count
            sent += 1
            if offset == pending.count {
                pending = []
                inFlight.append((id: frameID & 127, at: now))
                break                                 // (one frame a call: the next waits for a new picture)
            }
        }
        return (sharpFrame ? (pixels.count == VideoWire.cleanRGBA ? "Bluetooth · limpa 240 × 240" : "Bluetooth · nítido 120 × 120") : "Bluetooth · 60 × 60") + rate
    }

    private func scan() {
        guard active, peripheral == nil, let central, central.state == .poweredOn, !central.isScanning else { return }
        central.scanForPeripherals(withServices: [serviceID])
    }

    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        if central.state != .poweredOn {
            peripheral = nil
            characteristic = nil
            pending = []
        }
        scan()
    }

    func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral,
                        advertisementData: [String: Any], rssi RSSI: NSNumber) {
        guard active, self.peripheral == nil else { return }
        self.peripheral = peripheral
        central.stopScan()
        central.connect(peripheral)
    }

    func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        guard self.peripheral == peripheral else { return }
        peripheral.delegate = self
        peripheral.discoverServices([serviceID, midiServiceID])
    }

    func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral, error: Error?) {
        guard self.peripheral == peripheral else { return }
        self.peripheral = nil
        characteristic = nil
        pending = []
        scan()
    }

    func centralManager(_ central: CBCentralManager, didFailToConnect peripheral: CBPeripheral, error: Error?) {
        centralManager(central, didDisconnectPeripheral: peripheral, error: error)
    }

    func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        guard self.peripheral == peripheral else { return }
        if error == nil, let service = peripheral.services?.first(where: { $0.uuid == serviceID }) {
            peripheral.discoverCharacteristics([dataID], for: service)
            if let midi = peripheral.services?.first(where: { $0.uuid == midiServiceID }) {
                peripheral.discoverCharacteristics([midiDataID], for: midi)
            }
        } else { central?.cancelPeripheralConnection(peripheral) }
    }

    func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
        guard self.peripheral == peripheral else { return }
        if service.uuid == midiServiceID {             // the FM-1's remote: its MIDI notifications
            if error == nil, let midi = service.characteristics?.first(where: { $0.uuid == midiDataID }) {
                peripheral.setNotifyValue(true, for: midi)
            }
            return
        }
        if error == nil, let value = service.characteristics?.first(where: {
            $0.uuid == dataID && $0.properties.contains(.writeWithoutResponse)
        }) {
            characteristic = value
            codec.forceKey = true
        } else { central?.cancelPeripheralConnection(peripheral) }
    }

    // the radio has room again: the next packets now, not at the next tick
    func peripheralIsReady(toSendWriteWithoutResponse peripheral: CBPeripheral) { ready?() }

    func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
        guard error == nil, characteristic.uuid == midiDataID, let value = characteristic.value else { return }
        let bytes = [UInt8](value)
        if bytes.count >= 3 {
            for index in 0..<(bytes.count - 2) where bytes[index] == 0xBF && bytes[index + 1] == 105 {
                if let found = inFlight.firstIndex(where: { $0.id == bytes[index + 2] }) {
                    inFlight.removeSubrange(0...found)          // (and any older: answered in order)
                    acks.append(CACurrentMediaTime())
                    ready?()
                }
            }
        }
        receivedBLE?(bytes)
        FM1Remote.commands(bytes).forEach { command?($0) }
    }
}
