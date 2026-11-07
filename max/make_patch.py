#!/usr/bin/env python3
"""Writes amb1-fm6-breaks.maxpat: AMB-1 FM6's MIDI (clock, notes) and audio (USB) in, a fast breakbeat in step with
it out (breaks.js decides, MSP's own objects make the sound: no samples), the FM-1's pads washed and stuttered."""
import json
from pathlib import Path

boxes, lines = [], []
_n = [0]


def box(maxclass, x, y, w=None, text=None, ins=1, outs=1, types=None, **kw):
    _n[0] += 1
    oid = f"obj-{_n[0]}"
    b = {"id": oid, "maxclass": maxclass, "numinlets": ins, "numoutlets": outs,
         "patching_rect": [x, y, w or max(40, 7 * len(text or "") + 16), 22]}
    if outs:
        b["outlettype"] = types if types is not None else [""] * outs
    if text is not None:
        b["text"] = text
    b.update(kw)
    boxes.append({"box": b})
    return oid


def obj(text, x, y, ins=1, outs=1, types=None, w=None):
    return box("newobj", x, y, w, text, ins, outs, types)


def msg(text, x, y):
    return box("message", x, y, None, text, 2, 1, [""])


def comment(text, x, y, w=300):
    return box("comment", x, y, w, text, 1, 0)


def flonum(x, y):
    return box("flonum", x, y, 50, None, 1, 2, ["", "bang"], format=6, parameter_enable=0)


def number(x, y):
    return box("number", x, y, 50, None, 1, 2, ["", "bang"], parameter_enable=0)


def wire(a, ao, b, bi=0):
    lines.append({"patchline": {"source": [a, ao], "destination": [b, bi]}})


def param(name, value, x, y, isint=False):
    """a value for breaks.js: loadmess, a number box, prepend NAME"""
    lm = obj(f"loadmess {value}", x, y)
    nb = number(x, y + 30) if isint else flonum(x, y + 30)
    pp = obj(f"prepend {name}", x, y + 60)
    comment(name, x + 55, y + 30, 70)
    wire(lm, 0, nb)
    wire(nb, 0, pp)
    return pp


# ---------------------------------------------------------------- the title
comment("AMB-1 FM6 breaks: a fast breakbeat in step with the FM-1 (its MIDI clock), its pads washed and stuttered. "
        "1. Options > Audio Status: input = the FM-1 (USB audio 'AMB-1 FM-1'), output = your speakers. "
        "2. Choose the FM-1's MIDI port below. 3. On the FM-1: Setup page, MIDI OUT On; press PLAY. "
        "4. Turn the audio on (the speaker).", 20, 10, 900)

# ---------------------------------------------------------------- MIDI in
comment("MIDI from the FM-1 (choose its port)", 20, 70, 260)
lb = obj("loadbang", 20, 95)
mi = obj("midiinfo", 20, 125, 2, 2, ["", ""])
menu = box("umenu", 20, 155, 200, None, 1, 3, ["int", "", ""], items=[], parameter_enable=0)
midiin = obj("midiin", 20, 195, 1, 1, ["int"])
rtin = obj("rtin", 120, 195, 1, 1, ["int"])
pbyte = obj("prepend byte", 20, 225)
prt = obj("prepend rt", 120, 225)
wire(lb, 0, mi)
wire(mi, 0, menu)
wire(menu, 1, midiin)
wire(menu, 1, rtin)
wire(midiin, 0, pbyte)
wire(rtin, 0, prt)

# ---------------------------------------------------------------- the internal clock (without the FM-1)
comment("or an internal clock (BPM, the FM-1's tempo)", 300, 70, 300)
tog = box("toggle", 300, 95, 24, None, 1, 1, ["int"], parameter_enable=0)
bpm_lm = obj("loadmess 72", 340, 95)
bpm = flonum(340, 125)
ex = obj("expr 60000./($f1*24.)", 340, 155)
metro = obj("metro 34.7", 300, 190, 2, 1, ["bang"])
clk = msg("rt 248", 300, 220)
sel = obj("sel 1 0", 400, 190, 1, 3, ["bang", "bang", ""])
st = msg("rt 250", 400, 220)
sp = msg("rt 252", 460, 220)
wire(tog, 0, metro)
wire(tog, 0, sel)
wire(bpm_lm, 0, bpm)
wire(bpm, 0, ex)
wire(ex, 0, metro, 1)
wire(metro, 0, clk)
wire(sel, 0, st)
wire(sel, 1, sp)

# ---------------------------------------------------------------- the beat's settings
comment("the beat: density, ratchets (rolls), stutters of the FM-1's sound, swing; speed 2 = twice the FM-1's tempo, "
        "3 = three times; the sub follows the notes on BASSCH (voice 3: 3)", 620, 70, 560)
pars = [param("density", 0.6, 620, 110), param("ratchet", 0.35, 720, 110), param("stutter", 0.25, 820, 110),
        param("swing", 0.0, 920, 110), param("speed", 2, 1020, 110, True), param("bassch", 3, 1120, 110, True)]

# ---------------------------------------------------------------- breaks.js
js = obj("js breaks.js", 20, 300, 1, 6, ["", "", "", "", "", ""], w=600)
for src in (pbyte, prt, clk, st, sp, *pars):
    wire(src, 0, js)
state = msg("", 640, 300)
comment("state", 640, 280, 60)
wire(js, 5, state)

# ---------------------------------------------------------------- the drums (MSP's own: no samples)
comment("kick, sub", 20, 350, 120)
kt = obj("t f f f", 20, 370, 1, 3, ["float", "float", "float"])
kamp = msg("$1, 0 260", 20, 400)
kfrq = msg("170, 42 60", 110, 400)
kampl = obj("line~", 20, 430, 2, 2, ["signal", "bang"])
kfrql = obj("line~", 110, 430, 2, 2, ["signal", "bang"])
kosc = obj("cycle~", 110, 460, 2, 1, ["signal"])
kvca = obj("*~", 20, 490, 2, 1, ["signal"])
wire(js, 0, kt)
wire(kt, 2, kfrq)
wire(kt, 0, kamp)
wire(kamp, 0, kampl)
wire(kfrq, 0, kfrql)
wire(kfrql, 0, kosc)
wire(kampl, 0, kvca, 1)
wire(kosc, 0, kvca)

bfrq = msg("$1 30", 220, 400)
bline = obj("line~", 220, 430, 2, 2, ["signal", "bang"])
bosc = obj("cycle~", 220, 460, 2, 1, ["signal"])
benv = msg("$1, 0 420", 300, 400)
benvl = obj("line~", 300, 430, 2, 2, ["signal", "bang"])
bvca = obj("*~", 220, 490, 2, 1, ["signal"])
blvl = obj("*~ 0.5", 220, 520, 2, 1, ["signal"])
wire(js, 4, bfrq)
wire(bfrq, 0, bline)
wire(bline, 0, bosc)
wire(kt, 1, benv)
wire(benv, 0, benvl)
wire(bosc, 0, bvca)
wire(benvl, 0, bvca, 1)
wire(bvca, 0, blvl)
init_bass = msg("55", 220, 370)
wire(lb, 0, init_bass)
wire(init_bass, 0, bfrq)

comment("snare", 400, 350, 80)
stt = obj("t f f", 400, 370, 1, 2, ["float", "float"])
senv = msg("$1, 0 150", 400, 400)
senvl = obj("line~", 400, 430, 2, 2, ["signal", "bang"])
snoise = obj("noise~", 500, 400, 1, 1, ["signal"])
sbp = obj("svf~ 1900 0.5", 500, 430, 3, 4, ["signal", "signal", "signal", "signal"])
svca = obj("*~", 400, 490, 2, 1, ["signal"])
tenv = msg("$1, 0 70", 600, 400)
tenvl = obj("line~", 600, 430, 2, 2, ["signal", "bang"])
tosc = obj("cycle~ 185", 680, 430, 2, 1, ["signal"])
tvca = obj("*~", 600, 490, 2, 1, ["signal"])
wire(js, 1, stt)
wire(stt, 0, senv)
wire(stt, 1, tenv)
wire(senv, 0, senvl)
wire(snoise, 0, sbp)
wire(sbp, 2, svca)
wire(senvl, 0, svca, 1)
wire(tenv, 0, tenvl)
wire(tosc, 0, tvca)
wire(tenvl, 0, tvca, 1)

comment("hats", 780, 350, 80)
henv = msg("$1, 0 38", 780, 400)
henvl = obj("line~", 780, 430, 2, 2, ["signal", "bang"])
hnoise = obj("noise~", 880, 400, 1, 1, ["signal"])
hhp = obj("svf~ 8500 0.3", 880, 430, 3, 4, ["signal", "signal", "signal", "signal"])
hvca = obj("*~", 780, 490, 2, 1, ["signal"])
hlvl = obj("*~ 0.35", 780, 520, 2, 1, ["signal"])
wire(js, 2, henv)
wire(henv, 0, henvl)
wire(hnoise, 0, hhp)
wire(hhp, 1, hvca)
wire(henvl, 0, hvca, 1)
wire(hvca, 0, hlvl)

comment("the drums: grit (bits), drive, level", 20, 560, 300)
dbus = obj("*~ 0.8", 20, 580, 2, 1, ["signal"])
for s in (kvca, blvl, svca, tvca, hlvl):
    wire(s, 0, dbus)
crush = obj("degrade~ 1 12", 20, 610, 3, 1, ["signal"])
bits_lm = obj("loadmess 12", 140, 580)
bits = number(140, 610)
comment("bits", 195, 610, 40)
drive = obj("overdrive~ 1.6", 20, 640, 2, 1, ["signal"])
dlvl = obj("*~ 0.8", 20, 670, 2, 1, ["signal"])
dl_lm = obj("loadmess 0.8", 140, 650)
dl = flonum(140, 680)
comment("drums", 195, 680, 50)
wire(dbus, 0, crush)
wire(bits_lm, 0, bits)
wire(bits, 0, crush, 2)
wire(crush, 0, drive)
wire(drive, 0, dlvl)
wire(dl_lm, 0, dl)
wire(dl, 0, dlvl, 1)

# ---------------------------------------------------------------- the FM-1's sound: the pads, washed; stuttered
comment("the FM-1's sound (USB audio in): its pads, a dark wash", 300, 560, 400)
adc = obj("adc~ 1 2", 300, 580, 1, 2, ["signal", "signal"])
pl = obj("*~ 0.9", 300, 610, 2, 1, ["signal"])
pr = obj("*~ 0.9", 370, 610, 2, 1, ["signal"])
pad_lm = obj("loadmess 0.9", 450, 580)
padl = flonum(450, 610)
comment("pads", 505, 610, 40)
wire(adc, 0, pl)
wire(adc, 1, pr)
wire(pad_lm, 0, padl)
wire(padl, 0, pl, 1)
wire(padl, 0, pr, 1)
mono = obj("*~ 0.5", 300, 640, 2, 1, ["signal"])
wire(pl, 0, mono)
wire(pr, 0, mono)
tapin = obj("tapin~ 4000", 300, 670, 1, 1, ["tapconnect"])
tapout = obj("tapout~ 680 1130", 300, 700, 2, 2, ["signal", "signal"])
fb = obj("*~ 0.45", 420, 700, 2, 1, ["signal"])
dark_l = obj("lores~ 1400 0.2", 300, 730, 3, 1, ["signal"])
dark_r = obj("lores~ 1400 0.2", 420, 730, 3, 1, ["signal"])
wash = obj("*~ 0.5", 300, 760, 2, 1, ["signal"])
wash_r = obj("*~ 0.5", 420, 760, 2, 1, ["signal"])
wash_lm = obj("loadmess 0.5", 520, 730)
washn = flonum(520, 760)
comment("wash", 575, 760, 40)
wire(mono, 0, tapin)
wire(tapin, 0, tapout)
wire(tapout, 1, fb)
wire(fb, 0, tapin)
wire(tapout, 0, dark_l)
wire(tapout, 1, dark_r)
wire(dark_l, 0, wash)
wire(dark_r, 0, wash_r)
wire(wash_lm, 0, washn)
wire(washn, 0, wash, 1)
wire(washn, 0, wash_r, 1)

comment("stutter: the last 4 s of the FM-1 kept; breaks.js loops a slice of it in time", 620, 560, 520)
buf = obj("buffer~ fm1rec 4000 1", 620, 580, 1, 2, ["float", "bang"])
rec = obj("record~ fm1rec 1 @loop 1", 620, 640, 3, 1, ["signal"])
rec_on = msg("1", 620, 610)
snap = obj("snapshot~ 20", 620, 670, 2, 1, ["float"])
prec = obj("prepend recpos", 620, 700)
wire(lb, 0, rec_on)
wire(rec_on, 0, rec)
wire(mono, 0, rec)
wire(rec, 0, snap)
wire(snap, 0, prec)
wire(prec, 0, js)
rt_ = obj("route stut stutoff", 760, 610, 1, 3, ["", "", ""])
wire(js, 3, rt_)
setloop = obj("prepend setloop", 760, 640)
trig = obj("t b l", 900, 640, 1, 2, ["bang", ""])
startloop = msg("startloop", 900, 670)
gate_on = msg("1 5", 980, 670)
gate_off = msg("0 15", 1040, 670)
speed1 = obj("sig~ 1", 760, 670, 1, 1, ["signal"])
loopon = msg("loop 1", 840, 670)
groove = obj("groove~ fm1rec 1", 760, 710, 3, 2, ["signal", "signal"])
gline = obj("line~", 980, 710, 2, 2, ["signal", "bang"])
gvca = obj("*~", 760, 740, 2, 1, ["signal"])
glvl = obj("*~ 0.9", 760, 770, 2, 1, ["signal"])
wire(rt_, 0, trig)
wire(trig, 1, setloop)
wire(setloop, 0, groove)
wire(trig, 0, startloop)
wire(trig, 0, gate_on)
wire(startloop, 0, groove)
wire(rt_, 1, gate_off)
wire(gate_on, 0, gline)
wire(gate_off, 0, gline)
wire(lb, 0, loopon)
wire(loopon, 0, groove)
wire(speed1, 0, groove)
wire(groove, 0, gvca)
wire(gline, 0, gvca, 1)
wire(gvca, 0, glvl)

# ---------------------------------------------------------------- out
comment("out (the speaker: audio on)", 20, 820, 300)
outl = obj("clip~ -1 1", 20, 850, 3, 1, ["signal"])
outr = obj("clip~ -1 1", 160, 850, 3, 1, ["signal"])
for s in (dlvl, pl, wash, glvl):
    wire(s, 0, outl)
for s in (dlvl, pr, wash_r, glvl):
    wire(s, 0, outr)
dac = box("ezdac~", 20, 890, 45, None, 2, 0)
boxes[-1]["box"]["patching_rect"] = [20, 890, 45, 45]
wire(outl, 0, dac, 0)
wire(outr, 0, dac, 1)

patch = {"patcher": {
    "fileversion": 1,
    "appversion": {"major": 8, "minor": 5, "revision": 0, "architecture": "x64", "modernui": 1},
    "classnamespace": "box",
    "rect": [40.0, 80.0, 1240.0, 960.0],
    "openinpresentation": 0,
    "default_fontsize": 12.0,
    "boxes": boxes,
    "lines": lines,
}}
ids = {b["box"]["id"]: b["box"] for b in boxes}
for l in lines:                                  # every wire between boxes that have that outlet and inlet
    (a, ao), (b, bi) = l["patchline"]["source"], l["patchline"]["destination"]
    assert ao < ids[a]["numoutlets"], (ids[a].get("text"), ao)
    assert bi < ids[b]["numinlets"], (ids[b].get("text"), bi)
out = Path(__file__).with_name("amb1-fm6-breaks.maxpat")
out.write_text(json.dumps(patch, indent=1))
print(f"{out.name}: {len(boxes)} boxes, {len(lines)} wires")
