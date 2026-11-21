// SPDX-License-Identifier: MPL-2.0
import CoreMIDI
import CoreBluetooth
import Foundation

// Owned by Studio on the main queue. Each timer tick writes at most one packet.
final class FM1Output {
    private var client: MIDIClientRef = 0
    private var port: MIDIPortRef = 0
    private var inPort: MIDIPortRef = 0
    private var source: MIDIEndpointRef = 0
    var command: ((UInt8) -> Void)?
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
                for packet in list.unsafeSequence() {
                    let count = min(Int(packet.pointee.length), 256)
                    withUnsafeBytes(of: packet.pointee.data) { raw in bytes.append(contentsOf: raw.prefix(count)) }
                }
                let commands = FM1Remote.commands(bytes)
                guard !commands.isEmpty else { return }
                DispatchQueue.main.async { commands.forEach { self?.command?($0) } }
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
        pending = []
        destination = 0
        needsSetup = true
    }

    func step(_ pixels: [UInt8]?, now: TimeInterval) -> String {
        guard port != 0 else { return "Core MIDI indisponível." }
        guard let selected else { return "Escolha câmera ou vídeo." }
        if destination == 0 || now >= nextKeep {
            let found = findFM1()
            let input = findFM1Source()
            if input != source, inPort != 0 {
                if source != 0 { MIDIPortDisconnectSource(inPort, source) }
                source = input
                if input != 0 { MIDIPortConnectSource(inPort, input, nil) }
            }
            if destination != found {
                destination = found
                needsSetup = true
                pending = []
            }
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

// Bluetooth preview service (UUIDs match video/app.h). This does not use Core MIDI.

final class BluetoothVideoOutput: NSObject, CBCentralManagerDelegate, CBPeripheralDelegate {
    private let serviceID = CBUUID(string: "C05F152E-A864-47E1-9C06-78F7223B4E01")
    private let dataID = CBUUID(string: "C05F152E-A864-47E1-9C06-78F7223B4E02")
    private let midiServiceID = CBUUID(string: "03B80E5A-EDE8-4B33-A751-6CE34EC4C700")
    private let midiDataID = CBUUID(string: "7772E5DB-3868-4112-A1A9-F2669D106BF3")
    var command: ((UInt8) -> Void)?
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
        guard peripheral.canSendWriteWithoutResponse, let pixels else { return "FM-1 conectado por Bluetooth." }
        let limit = min(VideoWire.partSize, peripheral.maximumWriteValueLength(for: .withoutResponse) - 4)
        guard limit > 0 else { return "O FM-1 não aceitou pacotes de vídeo Bluetooth." }
        if pending.isEmpty {
            pending = pixels
            offset = 0
            frameID &+= 1
        }
        let count = min(limit, pending.count - offset)
        var payload = Data([source.rawValue, frameID, UInt8(offset & 255), UInt8(offset >> 8)])
        payload.append(contentsOf: pending[offset..<(offset + count)])
        peripheral.writeValue(payload, for: characteristic, type: .withoutResponse)
        offset += count
        if offset == pending.count { pending = [] }
        return "Enviando prévia por Bluetooth · 60 × 60."
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
        } else { central?.cancelPeripheralConnection(peripheral) }
    }

    func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
        guard error == nil, characteristic.uuid == midiDataID, let value = characteristic.value else { return }
        FM1Remote.commands([UInt8](value)).forEach { command?($0) }
    }
}
