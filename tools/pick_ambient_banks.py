#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Pick the most ambient DX7 voices from Dave Benson's archive (dx7patch.zip, deckard.zip): every 32-voice bulk dump
read, duplicates dropped, each voice scored by its carriers' envelopes (a soft or slow start, held, a long tail),
how harsh its modulation is, and its name; the best grouped into four banks written as .syx (VMEM + checksum)."""
import glob
import os
import re
import sys

# the DX7's carriers per algorithm (operators 1..6), as Yamaha's chart
CAR = {1: [1, 3], 2: [1, 3], 3: [1, 4], 4: [1, 4], 5: [1, 3, 5], 6: [1, 3, 5], 7: [1, 3], 8: [1, 3], 9: [1, 3],
       10: [1, 4], 11: [1, 4], 12: [1, 3], 13: [1, 3], 14: [1, 3], 15: [1, 3], 16: [1], 17: [1], 18: [1],
       19: [1, 4, 5], 20: [1, 2, 4], 21: [1, 2, 4, 5], 22: [1, 3, 4, 5], 23: [1, 2, 4, 5], 24: [1, 2, 3, 4, 5],
       25: [1, 2, 3, 4, 5], 26: [1, 2, 4], 27: [1, 2, 4], 28: [1, 3, 6], 29: [1, 2, 3, 5], 30: [1, 2, 3, 6],
       31: [1, 2, 3, 4, 5], 32: [1, 2, 3, 4, 5, 6]}
GOOD = ['PAD', 'STRING', 'STRNG', 'CHOIR', 'VOX', 'VOICE', 'AIR', 'ATMOS', 'SPACE', 'GLASS', 'WARM', 'SOFT', 'DREAM',
        'ETHER', 'FLUTE', 'SWEEP', 'DRONE', 'MIST', 'CLOUD', 'WIND', 'HALO', 'ANGEL', 'CELEST', 'CHIME', 'BELL',
        'SHIMMER', 'ORGAN', 'BOW', 'ENSEM', 'HAZE', 'FOG', 'DUST', 'STAR', 'NIGHT', 'OCEAN', 'WAVE', 'FLOAT',
        'SLOW', 'ECHO', 'VIBES', 'MARIMB', 'KALIMB', 'SYNTH']
BAD = ['DRUM', 'SNARE', 'KICK', 'TOM ', 'HIHAT', 'HI-HAT', 'CYMB', 'CLAP', 'PERC', 'GUN', 'SHOT', 'CLAV', 'GUITAR',
       'GTR', 'BANJO', 'SLAP', 'PLUCK', 'HARPSI', 'TRUMP', 'TRMBN', 'SAX', 'HORN', 'BRASS', 'FUNK', 'ZAP', 'LASER',
       'NOISE', 'BASS', 'PIANO', 'E.P', 'EPIANO', 'CLAVI', 'SITAR', 'KOTO']


def voices(path):
    raw = open(path, 'rb').read()
    i = raw.find(bytes([0xF0, 0x43]))
    while i >= 0 and i + 4104 <= len(raw):
        if raw[i + 3] == 0x09 and raw[i + 4] == 0x20 and raw[i + 5] == 0x00:
            data = raw[i + 6:i + 6 + 4096]
            if (-sum(data)) & 0x7F == raw[i + 6 + 4096]:
                for v in range(32):
                    yield data[v * 128:(v + 1) * 128]
            i = raw.find(bytes([0xF0, 0x43]), i + 4104)
        else:
            i = raw.find(bytes([0xF0, 0x43]), i + 1)


def op(vm, n):                       # operator n (1..6): packed, the sixth first in VMEM
    b = vm[(6 - n) * 17:(6 - n) * 17 + 17]
    return dict(R=list(b[0:4]), L=list(b[4:8]), OL=b[14], FC=b[15] >> 1, MODE=b[15] & 1, FF=b[16])


def score(vm):
    alg = (vm[110] & 31) + 1
    fb = vm[111] & 7
    name = bytes(vm[118:128]).decode('ascii', 'replace').upper()
    car = CAR[alg]
    mods = [n for n in range(1, 7) if n not in car]
    cs = [op(vm, n) for n in car if op(vm, n)['OL'] > 40] or [op(vm, car[0])]
    s = 0.0
    for c in cs:
        atk, rel = c['R'][0], c['R'][3]
        held = max(c['L'][2], c['L'][1] if c['R'][1] < 40 else 0)   # held, or decaying very slowly
        s += (1.2 if atk < 60 else 0.6 if atk < 80 else 0.0)        # a soft or slow start
        s += held / 99.0 * 1.4                                        # sustained
        s += (1.4 if rel < 45 else 0.8 if rel < 65 else 0.0)         # a long tail
    s /= len(cs)
    harsh = sum(op(vm, n)['OL'] for n in mods) / max(1, len(mods))
    s -= max(0.0, harsh - 80) * 0.04 + max(0, fb - 5) * 0.4
    if any(op(vm, n)['MODE'] for n in car):
        s -= 1.0                                                      # a fixed-pitch carrier: a drum, an effect
    s += 1.0 * any(k in name for k in GOOD) - 2.5 * any(k in name for k in BAD)
    if not re.search('[A-Z]', name):
        s -= 3.0                                                      # no name: a scratch voice
    return s, name, alg


def category(name, vm):
    if any(k in name for k in ['BELL', 'CHIME', 'GLASS', 'VIBES', 'MARIMB', 'KALIMB', 'CELEST', 'CRYST']):
        return 2
    if any(k in name for k in ['CHOIR', 'VOX', 'VOICE', 'ANGEL', 'AIR', 'FLUTE', 'WIND', 'BREATH']):
        return 1
    if any(k in name for k in ['STRING', 'STRNG', 'ENSEM', 'BOW', 'ORGAN', 'DRONE']):
        return 3
    return 0                                                          # pads, atmospheres


seen, pool = set(), []
for f in sorted(glob.glob(sys.argv[1] + '/**/*.[sS][yY][xX]', recursive=True)):
    for vm in voices(f):
        key = bytes(vm[:118])
        if key in seen:
            continue
        seen.add(key)
        s, name, alg = score(vm)
        pool.append((s, name, alg, os.path.basename(f), bytes(vm)))
pool.sort(key=lambda x: -x[0])
print('voices read (unique):', len(pool))
banks, names = [[], [], [], []], set()
for s, name, alg, src, vm in pool:
    nm = name.strip()
    if nm in names:                   # (one voice a name: no near twins)
        continue
    c = category(name, vm)
    order = [c] + [k for k in (0, 1, 3, 2) if k != c]
    for k in order:
        if len(banks[k]) < 32:
            banks[k].append((s, name, alg, src, vm))
            names.add(nm)
            break
    if all(len(b) == 32 for b in banks):
        break
TITLE = ['pads & atmospheres', 'voices & air', 'bells & glass', 'strings & drones']
os.makedirs(sys.argv[2], exist_ok=True)
for k, b in enumerate(banks):
    data = b''.join(v[4] for v in b)
    syx = bytes([0xF0, 0x43, 0x00, 0x09, 0x20, 0x00]) + data + bytes([(-sum(data)) & 0x7F, 0xF7])
    fn = os.path.join(sys.argv[2], 'cortex_ambient_%d.syx' % (k + 1))
    open(fn, 'wb').write(syx)
    print('\n%s (%s): %s' % (fn, TITLE[k], ', '.join('%s' % v[1].strip() for v in b)))
    print('  from:', ', '.join(sorted(set(v[3] for v in b)))[:400])
