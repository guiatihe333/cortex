// SPDX-License-Identifier: MPL-2.0
import CoreMIDI
import Foundation

// Owned by Studio on the main queue. Each timer tick writes at most one packet.
final class FM1Output {
    private var client: MIDIClientRef = 0
    private var port: MIDIPortRef = 0
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
        }
    }

    deinit {
        if port != 0 { MIDIPortDispose(port) }
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

