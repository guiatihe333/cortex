#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Send a MIDI file to AMB-1 FM6 on an FM-1 as a song (its SONG page: GLO, again): its chords, melody and bass for the
garden's three voices (app/dsp/amb.h has the record; app/bank.c takes it).

  amb_mid.py SONG.mid --song N        the file into song slot N (1..4): on the FM-1, Setup's GLO again (the SONG page),
                                      SONG to N; FOLLOW how closely the garden plays it, VARY how it varies it
  amb_mid.py SONG.mid --dry           how it is read (its key, tempo, chords, melody, bass), nothing sent
  amb_mid.py --info                   the songs on the FM-1
  amb_mid.py --erase N                song slot N emptied

The file is read as a sixteenth grid (its notes where they begin, to the nearest sixteenth). Its drums (channel 10)
are left out. The rest becomes three lanes:
  chords   a chord each half bar: the pitch classes sounding in it (their time held, the strongest four), its root the
           bass's note (else the strongest)
  melody   the highest note beginning on each sixteenth (from middle C up), with its length
  bass     the lowest beginning on each sixteenth (under G3), with its length
A file's tracks can be named instead: --melody 2 --bass 3 --chords 1,4 (track numbers as --dry lists them). Its key
(Krumhansl's profiles) gives the garden's root and scale, its tempo the garden's (halved past 120). Up to 128 bars:
--from BAR and --bars N take part of a longer one.

Needs mido (and python-rtmidi to send). Close other programs using the FM-1's MIDI port first.
Exit codes: 0 done, 1 an error, 2 bad arguments or file, 3 FM-1 not found, 4 no answer.
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from amb_syx import TAG, Link  # noqa: E402

HDR, MAXN, SIZE, NSONG = 32, 2048, 0x4000, 4
NAMES = ["C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"]
MAJOR = [6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88]   # Krumhansl-Kessler
MINOR = [6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17]


def read_notes(path):
    """-> (notes [(start_tick, end_tick, pitch, vel, track)], ticks a beat, tempo BPM, track names)"""
    import mido
    mf = mido.MidiFile(path)
    tempo, notes, names = None, [], []
    for ti, tr in enumerate(mf.tracks):
        t, held, name = 0, {}, ""
        for m in tr:
            t += m.time
            if m.type == "set_tempo" and tempo is None:
                tempo = mido.tempo2bpm(m.tempo)
            elif m.type == "track_name":
                name = m.name
            elif m.type == "note_on" and m.velocity > 0 and m.channel != 9:
                held.setdefault((m.channel, m.note), []).append((t, m.velocity))
            elif (m.type == "note_off" or (m.type == "note_on" and m.velocity == 0)) and m.channel != 9:
                q = held.get((m.channel, m.note))
                if q:
                    st, vel = q.pop(0)
                    notes.append((st, t, m.note, vel, ti))
        names.append(name)
    return notes, mf.ticks_per_beat, tempo or 120.0, names


def key_of(hist):
    """(root, minor) by Krumhansl's profiles"""
    best, key = -9e9, (0, False)
    for r in range(12):
        for minor, prof in ((False, MAJOR), (True, MINOR)):
            v = sum(hist[(r + i) % 12] * prof[i] for i in range(12))
            if v > best:
                best, key = v, (r, minor)
    return key


def build(path, a):
    notes, tpb, bpm, names = read_notes(path)
    if not notes:
        raise SystemExit(2)
    step = tpb / 4.0
    first = (a.from_bar - 1) * 16 if a.from_bar else 0
    ev = []                                       # (start step, length, pitch, vel, track)
    for st, en, p, vel, tr in notes:
        s = int(round(st / step)) - first
        ln = max(1, int(round((en - st) / step)))
        if s >= 0:
            ev.append((s, ln, p, vel, tr))
    if not ev:
        raise SystemExit(f"{path}: no notes from bar {a.from_bar}")
    n = max(s + 1 for s, *_ in ev)
    n = min(((n + 15) // 16) * 16, (a.bars * 16 if a.bars else MAXN), MAXN)
    ev = [e for e in ev if e[0] < n]
    pick = lambda arg: None if arg is None else {int(x) for x in str(arg).split(",")}
    mel_t, bas_t, chd_t = pick(a.melody), pick(a.bass), pick(a.chords)
    hist = [0.0] * 12
    for s, ln, p, vel, tr in ev:
        hist[p % 12] += ln
    root, minor = key_of(hist)
    tot = sum(hist) or 1
    order = sorted(range(12), key=lambda i: -hist[i])
    pool = {root} | {i for i in order[:7] if hist[i] >= 0.03 * tot}
    pool_rel = sum(1 << ((i - root) % 12) for i in pool)
    # the lanes
    mel, mell, bas, basl = [0] * n, [0] * n, [0] * n, [0] * n
    for s, ln, p, vel, tr in ev:
        if (mel_t is None and p >= 60 or mel_t is not None and tr in mel_t) and p > mel[s]:
            mel[s], mell[s] = p, min(ln, 127)
        if (bas_t is None and p < 55 or bas_t is not None and tr in bas_t) and (not bas[s] or p < bas[s]):
            bas[s], basl[s] = p, min(ln, 127)
    nch = (n + 7) // 8
    chords = []
    for c in range(nch):
        w = [0.0] * 12
        lo, hi = c * 8, c * 8 + 8
        for s, ln, p, vel, tr in ev:
            if chd_t is not None and tr not in chd_t:
                continue
            ov = min(hi, s + ln) - max(lo, s)
            if ov > 0:
                w[p % 12] += ov * (1.5 if p < 55 else 1.0)
        if max(w) <= 0:
            chords.append((0, 127))
            continue
        top = sorted(range(12), key=lambda i: -w[i])[:4]
        mask = [i for i in top if w[i] >= 0.25 * max(w)]
        b = [bas[s] for s in range(lo, min(hi, n)) if bas[s]]

        def as_root(r):                          # a third and a fifth above it: a root (the bass's, the strongest)
            return ((((r + 3) % 12 in mask) or ((r + 4) % 12 in mask)) * 2 + ((r + 7) % 12 in mask) * 2 +
                    (bool(b) and b[0] % 12 == r) + w[r] / (max(w) * 2))
        r = max(mask, key=as_root)
        chords.append((sum(1 << i for i in mask), r))
    # major or its relative minor (the same scale): the one its chords begin and end on, if they do
    roots = [r for m, r in chords if r < 12]
    rel = (root + 9) % 12 if not minor else (root + 3) % 12
    if roots and (roots[0] == rel or roots[-1] == rel) and roots[0] != root:
        root, minor = rel, not minor
        pool_rel = sum(1 << ((i - root) % 12) for i in pool | {root})
    name = (a.name or os.path.splitext(os.path.basename(path))[0])
    name = "".join(ch if 32 <= ord(ch) < 127 else "_" for ch in name)[:12].ljust(12)
    tempo = int(round(bpm))
    rec = bytearray(HDR)
    rec[0:4] = b"ASNG"
    rec[4] = 1
    rec[5], rec[6] = n >> 7, n & 0x7F
    rec[7], rec[8] = (tempo >> 7) & 0x7F, tempo & 0x7F
    rec[9] = root
    rec[10], rec[11] = pool_rel & 0x3F, pool_rel >> 6 & 0x3F
    rec[12:24] = name.encode()
    for m, r in chords:
        rec += bytes([m & 0x3F, m >> 6 & 0x3F, r])
    rec += bytes(mel) + bytes(mell) + bytes(bas) + bytes(basl)
    assert len(rec) <= SIZE and max(rec) < 0x80
    return bytes(rec), dict(n=n, tempo=tempo, root=root, minor=minor, pool=sorted(pool), chords=chords, names=names,
                            mel=sum(1 for x in mel if x), bas=sum(1 for x in bas if x), name=name.strip(),
                            tracks=sorted({e[4] for e in ev}))


def chord_name(mask, r):
    if r > 11:
        return "-"
    pcs = {(i - r) % 12 for i in range(12) if mask >> i & 1}
    q = "m" if 3 in pcs and 4 not in pcs else ""
    if 10 in pcs:
        q += "7"
    elif 11 in pcs:
        q += "M7"
    return NAMES[r] + q


def show(info):
    print(f"{info['name']}: {info['n'] // 16} bars, {info['tempo']} BPM, key {NAMES[info['root']]} "
          f"{'minor' if info['minor'] else 'major'}, scale {' '.join(NAMES[i] for i in info['pool'])}")
    print(f"  tracks: " + ", ".join(f"{t}: {info['names'][t] or '(no name)'}" for t in info['tracks']))
    print(f"  melody {info['mel']} notes, bass {info['bas']} notes")
    ch = [chord_name(m, r) for m, r in info["chords"]]
    for b in range(0, min(len(ch), 32), 8):
        print("  bars %3d-%3d: %s" % (b // 2 + 1, b // 2 + 4, "  ".join(f"{c:5}" for c in ch[b:b + 8])))


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("file", nargs="?")
    ap.add_argument("--song", type=int, help="1..4")
    ap.add_argument("--dry", action="store_true")
    ap.add_argument("--info", action="store_true")
    ap.add_argument("--erase", type=int, metavar="N")
    ap.add_argument("--melody")
    ap.add_argument("--bass")
    ap.add_argument("--chords")
    ap.add_argument("--from", dest="from_bar", type=int, metavar="BAR")
    ap.add_argument("--bars", type=int)
    ap.add_argument("--name")
    ap.add_argument("--port")
    a = ap.parse_args(argv)
    if not a.info and a.erase is None and not a.file:
        ap.error("give a .mid (with --song N or --dry), --info or --erase N")
    for s in (a.song, a.erase):
        if s is not None and not 1 <= s <= NSONG:
            ap.error("songs are 1..4")
    rec = None
    if a.file:
        if not a.dry and a.song is None:
            ap.error("give --song N (1..4), or --dry")
        rec, info = build(a.file, a)
        show(info)
        if a.dry:
            return 0
    link = Link(a.port)
    try:
        if a.erase is not None:
            link.send(TAG + [0x0D, a.erase - 1])
            r = link.answer(0x0E)
            print(f"song {a.erase}: " + ("erased" if r[1] == 0 else "NOT erased"))
        if rec is not None:
            nchunk = (len(rec) + 255) // 256
            for c in range(nchunk):
                data = list(rec[c * 256:(c + 1) * 256].ljust(256, b"\x00"))
                for tries in range(3):
                    link.send(TAG + [0x07, a.song - 1, c] + data)
                    try:
                        r = link.answer(0x08, quiet=tries < 2)
                        break
                    except SystemExit:
                        if tries == 2:
                            raise
                if r[2]:
                    raise SystemExit(f"song {a.song}: chunk {c} not written")
                print(f"\rsong {a.song}: {(c + 1) * 100 // nchunk:3}%", end="", flush=True)
            link.send(TAG + [0x0B, a.song - 1])
            r = link.answer(0x0C)
            print(f"\rsong {a.song}: " + ("sent. On the FM-1: GLO, GLO again (the SONG page), SONG to "
                                          f"{a.song}" if r[1] == 0 else "sent, but the FM-1 does not take it"))
        if a.info:
            for s in range(NSONG):
                link.send(TAG + [0x09, s])
                r = link.answer(0x0A)
                n = r[13] << 7 | r[14]
                nm = bytes(r[1:13]).decode("ascii", "replace").strip()
                print(f"song {s + 1}: " + (f"{nm} ({n // 16} bars)" if n else "empty"))
    finally:
        link.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
