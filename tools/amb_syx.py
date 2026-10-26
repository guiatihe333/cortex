#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Send DX7 voices to AMB-1 FM6 on an FM-1 over USB-MIDI (app/bank.c).

  amb_syx.py BANK.syx --bank N        a 32-voice bank (a bulk dump, or 4096 bytes of packed voices, or up to 32 single
                                      voices, packed here) into bank N (1..16): the voices' Patch knob reaches them
                                      past each voice's own six (B01-01 .. B16-32)
  amb_syx.py VOICE.syx                a single voice (or a bank's first) straight into the voice whose pages the FM-1
                                      shows (the pad on the others), edited
  amb_syx.py --info                   how many voices each bank holds
  amb_syx.py --erase N                bank N emptied

The FM-1 takes SysEx frames of 640 bytes at most: a bank goes in 16 chunks of 256 (manufacturer 7D, "AM"), each
answered. Needs mido with python-rtmidi. Close other programs using the FM-1's MIDI port first.
Exit codes: 0 done, 1 an error, 2 bad arguments or file, 3 FM-1 not found, 4 no answer.
"""
import argparse
import sys
import time

TAG = [0x7D, 0x41, 0x4D]
PORT_HINTS = ("amb-1", "fm-1", "fm1")


def frames(raw):
    """the SysEx messages in a file (the bytes between F0 and F7)"""
    out, i = [], 0
    while True:
        a = raw.find(b"\xF0", i)
        if a < 0:
            return out
        b = raw.find(b"\xF7", a)
        if b < 0:
            return out
        out.append(raw[a + 1:b])
        i = b + 1


def read_voices(path):
    """-> (vmem 4096 bytes or None, a single voice 155 bytes or None)"""
    raw = open(path, "rb").read()
    fs = frames(raw)
    singles = []
    for f in fs:
        if len(f) >= 4 + 4096 and f[0] == 0x43 and f[2] == 0x09:      # a 32-voice bulk dump
            return bytes(f[5:5 + 4096]), None
        if len(f) >= 5 + 155 and f[0] == 0x43 and f[2] == 0x00:       # a single voice (VCED)
            singles.append(bytes(f[5:5 + 155]))
    if not fs and len(raw) == 4096:                                    # packed voices, no SysEx
        return raw, None
    if len(singles) == 1:
        return None, singles[0]
    if singles:                                                        # up to 32, packed here; the rest empty
        vm = b"".join(pack_single(v) for v in singles[:32])
        return vm + bytes(128 * (32 - min(len(singles), 32))), None
    raise SystemExit(f"{path}: no DX7 voice found (a 32-voice bulk dump, single voices or 4096 packed bytes)")


def pack_single(s):
    """VCED order: op6..op1 (21 each), PR1-4, PL1-4, ALG, FB, OKS, LFS, LFD, LPMD, LAMD, LKS, LFW, LPMS, TRNSP, NAME"""
    m = bytearray(128)
    for o in range(6):
        p = s[o * 21:o * 21 + 21]
        d = o * 17
        m[d:d + 11] = p[0:11]                       # R1-4, L1-4, BP, LD, RD
        m[d + 11] = (p[12] << 2 | p[11]) & 0x0F     # RC << 2 | LC
        m[d + 12] = (p[20] << 3 | p[13]) & 0x7F     # DET << 3 | RS
        m[d + 13] = (p[15] << 2 | p[14]) & 0x1F     # KVS << 2 | AMS
        m[d + 14] = p[16]                           # OL
        m[d + 15] = (p[18] << 1 | p[17]) & 0x3F     # FC << 1 | MODE
        m[d + 16] = p[19]                           # FF
    m[102:110] = s[126:134]
    m[110] = s[134] & 0x1F
    m[111] = (s[136] << 3 | s[135]) & 0x0F
    m[112:116] = s[137:141]
    m[116] = (s[143] << 4 | s[142] << 1 | s[141]) & 0x7F
    m[117] = s[144]
    m[118:128] = s[145:155]
    return bytes(m)


class Link:
    def __init__(self, port):
        import mido
        self.mido = mido
        ins, outs = mido.get_input_names(), mido.get_output_names()
        pick = lambda names: next((n for n in names if (port and port.lower() in n.lower()) or
                                   (not port and any(h in n.lower() for h in PORT_HINTS))), None)
        i, o = pick(ins), pick(outs)
        if not i or not o:
            print("FM-1 not found. MIDI ports:", ins, outs, file=sys.stderr)
            raise SystemExit(3)
        self.name = o
        self.q = []
        self.inp = mido.open_input(i, callback=lambda m: self.q.append(m))
        self.out = mido.open_output(o)
        time.sleep(0.3)

    def send(self, data):
        self.out.send(self.mido.Message("sysex", data=list(data)))

    def answer(self, cmd, secs=3.0):
        t = time.time() + secs
        while time.time() < t:
            while self.q:
                m = self.q.pop(0)
                if m.type == "sysex" and list(m.data[:4]) == TAG + [cmd]:
                    return list(m.data[4:])
            time.sleep(0.005)
        print("no answer from the FM-1 (is AMB-1 FM6 1.3.92 or later on it?)", file=sys.stderr)
        raise SystemExit(4)

    def close(self):
        self.inp.close()
        self.out.close()


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("file", nargs="?")
    ap.add_argument("--bank", type=int, help="1..16")
    ap.add_argument("--info", action="store_true")
    ap.add_argument("--erase", type=int, metavar="N")
    ap.add_argument("--port")
    a = ap.parse_args(argv)
    if not a.info and a.erase is None and not a.file:
        ap.error("give a .syx, --info or --erase N")
    for b in (a.bank, a.erase):
        if b is not None and not 1 <= b <= 16:
            ap.error("banks are 1..16")
    vmem = single = None
    if a.file:
        vmem, single = read_voices(a.file)
        if a.bank is None and vmem is not None:            # a bank's first voice, into the voice on the screen
            single = None
            raise SystemExit("a bank: give --bank N (1..16) to keep all 32")
    link = Link(a.port)
    try:
        if a.info:
            link.send(TAG + [0x05])
            counts = link.answer(0x06)
            for b, n in enumerate(counts):
                print(f"bank {b + 1:2}: {n:2} voices")
        if a.erase is not None:
            link.send(TAG + [0x03, a.erase - 1])
            r = link.answer(0x04)
            print(f"bank {a.erase}: " + ("erased" if r[1] == 0 else "NOT erased"))
        if vmem is not None:
            vmem = bytes(x & 0x7F for x in vmem[:4096]).ljust(4096, b"\x00")
            for c in range(16):
                link.send(TAG + [0x01, a.bank - 1, c] + list(vmem[c * 256:(c + 1) * 256]))
                r = link.answer(0x02)
                if r[2]:
                    raise SystemExit(f"bank {a.bank}: chunk {c} not written")
                print(f"\rbank {a.bank}: {(c + 1) * 100 // 16:3}%", end="", flush=True)
            print(f"\rbank {a.bank}: sent. On the FM-1, a voice's VOICE page: Patch past its own six")
        if single is not None:
            s = [x & 0x7F for x in single[:155]]
            cs = (128 - sum(s) % 128) % 128
            link.send([0x43, 0x00, 0x00, 0x01, 0x1B] + s + [cs])
            name = bytes(single[145:155]).decode("latin-1").strip()
            print(f"{name}: sent to the voice on the FM-1's screen")
            time.sleep(0.3)
    finally:
        link.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
