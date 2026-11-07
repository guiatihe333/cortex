#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""AMB-1 FM6's own bank of pads and ambient voices, designed here (none taken from elsewhere): 32 DX7 voices as a
32-voice bulk dump (tools/AMB-PADS.syx, for any DX7 or Dexed) and as app/bank_pads.h (the bank built in after the
sixteen, B17: on the Patch knob right after AMB-1's own eighteen).

  tools/gen_pads.py OUT.h OUT.syx

How they are made. Each voice is a few operator pairs or stacks held long:
  - carriers with a slow attack (rate 30 .. 55: 0.5 to 4 s), a high sustain and a long release (rate 28 .. 45);
  - modulators a little slower to open than their carriers and quicker to close, so a note blooms brighter and
    fades darker; their level falls from C3 up (right depth, -lin), so the high notes stay soft;
  - carriers detuned against each other (detune 4 .. 10, 7 none) for a slow chorus;
  - the LFO slow (speed 4 .. 20) on the modulators' amplitude (AMS 1 .. 2): the tone breathes; a vibrato only where a
    voice asks for one (the strings, the choirs), after a delay;
  - rate scaling 1 .. 2: high notes a little quicker, as a real one.
Operator 1 .. 6 as on a DX7 (the dump has the sixth first)."""
import sys
from pathlib import Path

# envelopes: (R1, R2, R3, R4, L1, L2, L3, L4)


def car(a, r, s=96, l2=98):                       # a carrier: a slow rise, held, a long release (rate a - 10: the
    return (a - 10, 24, 20, r, 99, l2, s, 0)      # DX7's attack is quick past 40; 34 rises in ~0.7 s)


def held(s, k):                                   # a sustain level, its drop from 99 eased (levels are 0.75 dB a step)
    return int(round(99 - (99 - s) * k))


def mod(a, r, s=88, l1=99, l2=94):                # a modulator: opens a little later, closes earlier
    return (a - 10, 22, 18, r, l1, l2, held(s, 0.6), 0)


def swell(a, r, peak=99, s=70):                   # a modulator that rises past its sustain and settles (a sweep)
    return (a - 8, 12, 10, r, peak, peak - 6, held(s, 0.6), 0)


def bell(a, r, s=55):                              # a carrier struck softly that fades to a held glow
    return (a, 30, 14, r, 99, 92, held(s, 0.45), 0)


def op(fc, ol, env, ff=0, det=7, kvs=1, rs=1, ams=0, scale=0, fixed=0):
    """an operator: coarse (0: x0.5), level, envelope, fine, detune (7 none), velocity, rate scaling, amp mod
    sensitivity, scale (right depth from C3 up, -lin: a modulator's brightness tamed up high)"""
    return dict(fc=fc, ol=ol, env=env, ff=ff, det=det, kvs=kvs, rs=rs, ams=ams, rd=scale, fixed=fixed)


def voice(name, alg, fb, ops, lfs=8, lfd=0, pmd=0, amd=0, wave=4, pms=0, sync=0, peg=None):
    assert len(ops) == 6 and len(name) <= 10
    return dict(name=name, alg=alg, fb=fb, ops=ops, lfs=lfs, lfd=lfd, pmd=pmd, amd=amd, wave=wave, pms=pms,
                sync=sync, peg=peg)


V = []
# ---------------------------------------------------------------- warm pads (algorithm 5: three pairs, 6 fed back)
V.append(voice("WARM PAD", 5, 4, [
    op(1, 97, car(44, 36), det=5), op(1, 66, mod(40, 42, 84), scale=30, ams=1),
    op(1, 92, car(44, 36), det=9), op(1, 60, mod(38, 42, 80), scale=30, ams=1),
    op(0, 84, car(42, 36)), op(1, 58, mod(40, 42, 82), scale=25, ams=1)],
    lfs=7, amd=14))
V.append(voice("SOFT SAW", 5, 7, [
    op(1, 96, car(46, 36), det=4), op(1, 72, mod(42, 40, 86), scale=30),
    op(1, 94, car(46, 36), det=10), op(1, 70, mod(42, 40, 86), scale=30),
    op(1, 94, car(46, 36), det=7), op(1, 74, mod(40, 40, 88), scale=30)],
    lfs=33, lfd=65, pmd=4, pms=2))
V.append(voice("DARK PAD", 5, 2, [
    op(1, 97, car(40, 34), det=6), op(1, 54, mod(34, 40, 78), scale=40, ams=1),
    op(0, 92, car(40, 34), det=8), op(1, 50, mod(34, 40, 76), scale=40, ams=1),
    op(1, 86, car(40, 34), det=9), op(2, 44, mod(30, 42, 70), scale=40, ams=2)],
    lfs=5, amd=18))
V.append(voice("VELVET", 22, 3, [
    op(1, 90, car(42, 34), det=6), op(3, 46, mod(36, 42, 74), scale=35, ams=1),
    op(1, 96, car(44, 34), det=8), op(2, 84, car(44, 34), det=5), op(0, 88, car(44, 34), det=7),
    op(1, 60, mod(38, 40, 84), scale=30, ams=1)],
    lfs=6, amd=12))
V.append(voice("ANALOG 5TH", 5, 5, [
    op(1, 97, car(42, 34), det=5), op(1, 66, mod(38, 40, 84), scale=30),
    op(1, 84, car(42, 34), ff=50, det=9), op(1, 58, mod(38, 40, 80), scale=30),
    op(0, 88, car(42, 34), det=7), op(1, 60, mod(38, 40, 82), scale=30)],
    lfs=30, lfd=70, pmd=3, pms=2))
V.append(voice("HOLLOW", 5, 1, [
    op(1, 97, car(46, 36), det=5), op(2, 64, mod(42, 42, 84), scale=35, ams=1),
    op(1, 93, car(46, 36), det=9), op(2, 60, mod(42, 42, 80), scale=35, ams=1),
    op(3, 76, car(48, 36), det=7), op(2, 50, mod(40, 42, 78), scale=35)],
    lfs=9, amd=16))
V.append(voice("GLOW", 5, 3, [
    op(1, 97, car(40, 38), det=6), op(2, 58, swell(32, 40, 99, 74), scale=30, ams=1),
    op(2, 86, car(40, 38), det=8), op(1, 54, swell(30, 40, 99, 70), scale=30, ams=1),
    op(0, 84, car(40, 38), det=7), op(1, 52, mod(36, 42, 80), scale=30)],
    lfs=6, amd=20, wave=0))
V.append(voice("EMBER", 2, 5, [
    op(1, 97, car(44, 36), det=5), op(1, 62, mod(40, 40, 86), scale=30),
    op(1, 94, car(44, 36), det=9), op(1, 56, mod(40, 40, 84), scale=30, ams=1),
    op(2, 48, mod(36, 42, 80), scale=35, ams=1), op(1, 40, mod(34, 42, 76), scale=35)],
    lfs=7, amd=14))
# ---------------------------------------------------------------- glass and bell pads
V.append(voice("GLASS PAD", 5, 1, [
    op(1, 97, car(46, 38), det=6), op(1, 64, mod(42, 42, 84), scale=30),
    op(1, 88, car(46, 38), det=8), op(3, 52, mod(40, 44, 64), ff=50, scale=40, ams=2),
    op(2, 80, car(48, 38), det=7), op(7, 42, mod(40, 46, 56), scale=45, ams=1)],
    lfs=8, amd=18))
V.append(voice("CRYSTAL", 5, 0, [
    op(1, 97, car(50, 38)), op(1, 60, (60, 28, 16, 44, 99, 84, 72, 0), ff=41, scale=35),
    op(2, 86, car(50, 38), det=9), op(5, 52, (62, 30, 16, 46, 99, 82, 68, 0), scale=45),
    op(4, 72, car(52, 36), det=5), op(3, 48, (60, 30, 16, 46, 99, 80, 66, 0), scale=45)],
    lfs=10, amd=12))
V.append(voice("SLOW BELLS", 5, 0, [
    op(1, 97, bell(52, 36, 62)), op(3, 62, (64, 34, 16, 46, 99, 84, 70, 0), ff=50, scale=35),
    op(1, 88, bell(50, 36, 60), det=9), op(2, 58, (62, 32, 16, 46, 99, 82, 68, 0), ff=76, scale=40),
    op(4, 76, bell(54, 34, 50), det=5), op(5, 52, (64, 34, 16, 48, 99, 80, 64, 0), scale=45)],
    lfs=12, amd=10))
V.append(voice("FROST", 22, 2, [
    op(1, 94, car(44, 36), det=6), op(1, 66, mod(40, 42, 84), scale=30),
    op(1, 96, car(46, 36), det=8), op(2, 84, car(46, 36), ff=1, det=5), op(3, 76, car(48, 36), det=9),
    op(7, 50, mod(42, 44, 76), scale=40, ams=2)],
    lfs=11, amd=22))
V.append(voice("PRISM", 31, 2, [
    op(1, 97, car(46, 36)), op(2, 86, car(42, 36), det=8), op(3, 80, car(38, 36), det=6),
    op(4, 74, car(34, 34), det=9), op(6, 72, car(30, 34), det=5),
    op(1, 56, mod(40, 42, 80), scale=30, ams=1)],
    lfs=6, amd=14))
V.append(voice("SHIMMER", 5, 0, [
    op(1, 97, car(44, 38), det=6), op(1, 62, mod(40, 42, 84), scale=30),
    op(4, 74, car(44, 36), det=10), op(1, 56, mod(40, 42, 80), ff=1, scale=40, ams=2),
    op(8, 64, car(46, 34), det=4), op(1, 48, mod(40, 44, 76), scale=45, ams=2)],
    lfs=14, amd=26))
V.append(voice("BOWED GLAS", 5, 4, [
    op(1, 97, car(40, 36), det=5), op(1, 70, mod(36, 40, 86), scale=30),
    op(2, 88, car(42, 36), det=9), op(1, 60, mod(36, 40, 82), scale=30),
    op(1, 84, car(38, 36), det=7), op(3, 52, swell(28, 42, 92, 66), ff=50, scale=40, ams=1)],
    lfs=32, lfd=60, pmd=3, pms=2))
V.append(voice("DUST", 5, 0, [
    op(1, 97, car(44, 38), det=6), op(1, 58, mod(40, 42, 80), scale=30),
    op(1, 90, car(44, 38), det=8), op(11, 40, mod(42, 44, 66), scale=50, ams=3),
    op(2, 78, car(46, 36), det=7), op(13, 38, mod(42, 44, 62), ff=1, scale=50, ams=3)],
    lfs=26, amd=34, wave=5))
# ---------------------------------------------------------------- air, choirs, breath
V.append(voice("AIR CHOIR", 5, 3, [
    op(1, 97, car(42, 36), det=5), op(1, 62, mod(38, 40, 84), scale=30),
    op(3, 78, car(44, 36), det=8), op(1, 50, mod(40, 40, 78), scale=35),
    op(5, 66, car(46, 36), det=7), op(1, 44, mod(40, 40, 74), scale=40)],
    lfs=31, lfd=70, pmd=4, pms=3))
V.append(voice("BREATH", 22, 7, [
    op(1, 96, car(40, 36), det=6), op(1, 60, mod(36, 40, 82), scale=30),
    op(1, 90, car(42, 36), det=8), op(2, 78, car(42, 36), det=5), op(0, 82, car(42, 36), det=7),
    op(14, 38, mod(36, 40, 74), ff=27, scale=40, ams=2)],
    lfs=5, amd=24))
V.append(voice("VOX HUSH", 1, 3, [
    op(1, 97, car(42, 36), det=6), op(1, 62, mod(38, 40, 82), scale=30),
    op(1, 92, car(42, 36), det=8), op(2, 56, mod(38, 40, 80), scale=30),
    op(3, 50, mod(36, 42, 76), scale=35), op(1, 42, mod(34, 42, 72), scale=35)],
    lfs=30, lfd=75, pmd=4, pms=3))
V.append(voice("MIST", 22, 2, [
    op(1, 92, car(38, 34), det=6), op(0, 50, mod(34, 40, 76), scale=30),
    op(1, 96, car(40, 34), det=4), op(1, 92, car(40, 34), det=10), op(1, 94, car(40, 34), det=7),
    op(2, 54, mod(36, 40, 78), scale=35, ams=1)],
    lfs=5, amd=16))
V.append(voice("AURORA", 5, 2, [
    op(1, 97, car(40, 38), det=5), op(1, 62, swell(24, 40, 99, 70), scale=30, ams=1),
    op(2, 86, car(42, 38), det=9), op(1, 58, swell(22, 40, 98, 66), scale=30, ams=1),
    op(1, 90, car(40, 38), det=7), op(3, 46, swell(20, 42, 96, 60), scale=40, ams=2)],
    lfs=4, amd=22, wave=0))
V.append(voice("AIR STRING", 5, 6, [
    op(2, 80, car(40, 34), det=4), op(2, 60, mod(36, 40, 84), scale=30),
    op(1, 96, car(40, 34), det=10), op(1, 70, mod(36, 40, 86), scale=30),
    op(1, 96, car(40, 34), det=6), op(1, 74, mod(36, 40, 88), scale=30)],
    lfs=34, lfd=70, pmd=4, pms=2))
V.append(voice("CELLO HAZE", 5, 5, [
    op(1, 96, car(38, 34), det=5), op(1, 70, mod(34, 40, 86), scale=25),
    op(0, 92, car(38, 34), det=9), op(1, 64, mod(34, 40, 84), scale=25),
    op(1, 90, car(38, 34), det=7), op(1, 72, mod(34, 40, 86), scale=25)],
    lfs=32, lfd=60, pmd=5, pms=2))
# ---------------------------------------------------------------- drones, textures, slow swells
V.append(voice("DRONE 5TH", 32, 3, [
    op(0, 88, car(34, 32), det=6), op(1, 97, car(36, 32)), op(1, 84, car(38, 32), ff=50, det=8),
    op(2, 82, car(36, 32), det=5), op(3, 70, car(32, 32), det=9), op(1, 70, car(36, 32), det=7)],
    lfs=4, amd=14, wave=0))
V.append(voice("SUB DRONE", 5, 1, [
    op(0, 97, car(36, 32), det=6), op(1, 52, mod(32, 40, 80), scale=30, ams=1),
    op(1, 86, car(36, 32), det=8), op(1, 48, mod(32, 40, 78), scale=30, ams=1),
    op(0, 80, car(36, 32), det=9), op(2, 40, mod(30, 42, 72), scale=35, ams=2)],
    lfs=3, amd=20))
V.append(voice("CATHEDRAL", 32, 2, [
    op(0, 86, car(36, 32), det=6), op(1, 97, car(38, 32)), op(2, 88, car(36, 32), det=8),
    op(3, 76, car(34, 32), det=5), op(4, 72, car(32, 32), det=9), op(8, 62, car(30, 30), det=7)],
    lfs=5, amd=12, wave=0))
V.append(voice("SPECTRAL", 31, 1, [
    op(1, 97, car(46, 36)), op(3, 82, car(40, 36), det=8), op(5, 74, car(34, 34), det=6),
    op(7, 68, car(30, 32), det=9), op(9, 62, car(26, 30), det=5),
    op(1, 50, mod(36, 42, 76), scale=30, ams=1)],
    lfs=5, amd=16))
V.append(voice("OCEAN", 22, 7, [
    op(1, 90, car(32, 32), det=6), op(1, 56, mod(30, 40, 80), scale=30, ams=2),
    op(1, 94, car(34, 32), det=8, ams=2), op(2, 80, car(34, 32), det=5, ams=2), op(0, 86, car(34, 32), det=7, ams=2),
    op(9, 40, mod(30, 40, 76), ff=37, scale=40, ams=3)],
    lfs=4, amd=52, wave=4))
V.append(voice("TAPE HAZE", 5, 3, [
    op(1, 97, car(42, 36), det=5), op(1, 62, mod(38, 40, 84), scale=30),
    op(1, 92, car(42, 36), det=9), op(2, 54, mod(38, 40, 80), scale=30),
    op(0, 84, car(42, 36), det=7), op(1, 56, mod(38, 40, 80), scale=30)],
    lfs=6, pmd=8, pms=4, wave=0))
V.append(voice("TIDE", 5, 2, [
    op(1, 97, car(40, 36), det=5, ams=1), op(1, 60, mod(36, 40, 82), scale=30),
    op(2, 86, car(40, 36), det=9, ams=1), op(1, 56, mod(36, 40, 80), scale=30),
    op(1, 90, car(40, 36), det=7, ams=1), op(3, 46, mod(36, 42, 76), scale=40, ams=2)],
    lfs=16, amd=34))
V.append(voice("NIGHT SWEL", 5, 3, [
    op(1, 97, car(28, 30), det=5), op(1, 60, swell(22, 34, 96, 74), scale=30, ams=1),
    op(1, 92, car(28, 30), det=9), op(2, 52, swell(20, 34, 94, 70), scale=35, ams=1),
    op(0, 84, car(28, 30), det=7), op(1, 54, mod(24, 36, 80), scale=30)],
    lfs=4, amd=16))
V.append(voice("STILL", 5, 0, [
    op(1, 97, car(40, 34), det=6), op(1, 44, mod(36, 40, 76), scale=30),
    op(2, 80, car(42, 34), det=8), op(1, 36, mod(36, 40, 70), scale=35),
    op(1, 90, car(40, 34), det=8), op(1, 38, mod(36, 40, 72), scale=30)],
    lfs=4, amd=8))
assert len(V) == 32, len(V)


MOD_LIFT = 12
CARRIERS = {1: {1, 3}, 2: {1, 3}, 5: {1, 3, 5}, 19: {1, 4, 5}, 22: {1, 3, 4, 5}, 31: {1, 2, 3, 4, 5},
            32: {1, 2, 3, 4, 5, 6}}


def unbeat(v):
    """carriers at one ratio (a chorus) kept 10 levels apart (-7.5 dB) from the loudest of them: two equal ones, a
    little apart in pitch, beat to silence and back"""
    seen = {}
    for i in sorted(CARRIERS[v["alg"]], key=lambda i: -v["ops"][i - 1]["ol"]):
        o = v["ops"][i - 1]
        r = (o["fc"], o["ff"])
        if r in seen:                             # (the second 10 under, a third 16, a fourth 20)
            seen[r][1] += 1
            o["ol"] = min(o["ol"], seen[r][0] - (10, 16, 20)[min(seen[r][1], 3) - 1])
        else:
            seen[r] = [o["ol"], 0]
    for i in range(1, 7):                         # the modulators: their index (an index halves each 8 levels) as
        if i not in CARRIERS[v["alg"]]:           # designed, from the levels written (heard: 12 too dark)
            v["ops"][i - 1]["ol"] = min(99, v["ops"][i - 1]["ol"] + MOD_LIFT)


def pack(v):
    """a voice as a 32-voice dump packs it (VMEM, 128 bytes)"""
    unbeat(v)
    b = []
    for o in reversed(v["ops"]):                  # (the sixth operator first)
        r1, r2, r3, r4, l1, l2, l3, l4 = o["env"]
        b += [r1, r2, r3, r4, l1, l2, l3, l4,
              39, 0, o["rd"],                     # break point C3, left depth 0, right depth
              0 << 2 | 0,                          # curves: left -lin, right -lin
              o["det"] << 3 | o["rs"],
              o["kvs"] << 2 | o["ams"],
              o["ol"],
              o["fc"] << 1 | o["fixed"],
              o["ff"]]
    peg = v["peg"] or (99, 99, 99, 99, 50, 50, 50, 50)
    b += list(peg)
    b += [v["alg"] - 1, 1 << 3 | v["fb"], v["lfs"], v["lfd"], v["pmd"], v["amd"],
          v["pms"] << 4 | v["wave"] << 1 | v["sync"], 24]
    b += list(v["name"].ljust(10).encode())
    assert len(b) == 128 and max(b) < 128, (v["name"], len(b), max(b))
    return b


def main(out_h, out_syx):
    data = []
    for v in V:
        data += pack(v)
    cs = (-sum(data)) & 0x7F
    Path(out_syx).write_bytes(bytes([0xF0, 0x43, 0x00, 0x09, 0x20, 0x00] + data + [cs, 0xF7]))
    lines = ["/* generated by tools/gen_pads.py: AMB-1 FM6's own pads and ambient voices (B17, built in; designed in that",
             " * script), 32 voices as a DX7 packs them */",
             "/* " + ", ".join(v["name"] for v in V) + " */",
             "static const uint8_t BANK_PADS[4096] = {"]
    for i in range(0, 4096, 32):
        lines.append("    " + ",".join(str(x) for x in data[i:i + 32]) + ",")
    lines.append("};")
    Path(out_h).write_text("\n".join(lines) + "\n")
    print(f"pads: {out_h}, {out_syx} (32 voices)")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    main(*sys.argv[1:])
