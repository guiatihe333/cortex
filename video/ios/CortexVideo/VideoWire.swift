// SPDX-License-Identifier: MPL-2.0
import Foundation

enum VideoSource: UInt8 {
    case camera = 1
    case clip = 2

    var fm1Visual: UInt8 { self == .camera ? 9 : 10 }
}

enum VideoWire {
    // Keep in sync with video/vlink.h in the FM-1 project.
    static let side = 60
    static let partSize = 180
    static let partCount = side * side / partSize

    static func command(_ body: [UInt8]) -> [UInt8] {
        message(type: 0x10, body: body)
    }

    static func chunk(_ pixels: [UInt8], source: VideoSource, frame: UInt8, part: Int) -> [UInt8] {
        let offset = part * partSize
        var body = [source.rawValue, frame, UInt8(part)]
        body.append(contentsOf: pixels[offset..<(offset + partSize)])
        return message(type: 0x11, body: body)
    }

    private static func message(type: UInt8, body: [UInt8]) -> [UInt8] {
        var result: [UInt8] = [0xf0, 0x7d, 0x43, 0x56, type]
        for offset in stride(from: 0, to: body.count, by: 7) {
            let header = result.count
            result.append(0)
            for (index, value) in body[offset..<min(offset + 7, body.count)].enumerated() {
                result[header] |= (value >> 7) << index
                result.append(value & 0x7f)
            }
        }
        result.append(0xf7)
        return result
    }
}

