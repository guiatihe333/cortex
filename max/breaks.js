// AMB-1 FM6 breaks: a fast breakbeat (drill'n'bass, after Aphex Twin's way with breaks over slow pads) in step with
// the FM-1's MIDI clock. The FM-1 sends 24 clocks a beat (MIDI OUT on, its Setup page); this counts them and plays a
// grid of SPEED steps a sixteenth (2: the beat at twice the FM-1's tempo, 3: three times), each bar a pattern after
// the classic breaks, mutated: ghost snares, ratchets (a hit rolled 2 to 8 times inside its step), a snare rush at
// the end of every fourth bar, dropouts; and now and then a stutter of the FM-1's own sound (the patch's buffer).
// The sub follows the bass voice's notes (its MIDI channel: BASSCH, voice 3 by default).
//
// In (one inlet, messages): rt <byte> (realtime: 248 clock, 250 start, 251 continue, 252 stop), byte <byte> (the
// FM-1's MIDI, raw), recpos <0..1> (where the stutter buffer records), density, ratchet, stutter, swing <0..1>,
// speed <1|2|3>, bassch <1..16>, bufms <ms> (the stutter buffer's length).
// Out: 0 kick, 1 snare, 2 hat (velocity 0..1), 3 stut <start ms> <end ms> | stutoff, 4 the sub's frequency (Hz),
// 5 a line of state (set ...).

inlets = 1;
outlets = 6;

var P = {
    density: 0.6, ratchet: 0.35, stutter: 0.2, swing: 0.0, speed: 2, bassch: 3, bufms: 4000
};

// 16-step patterns: kick, snare, hat (1 hit, 0 rest), after the classic breaks
var BREAKS = [
    {k: [1,0,1,0,0,0,0,0,0,0,1,1,0,0,0,0], s: [0,0,0,0,1,0,0,1,0,1,0,0,1,0,0,1], h: [1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0]},
    {k: [1,0,0,0,0,0,1,0,0,1,0,0,0,0,0,0], s: [0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0], h: [1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1]},
    {k: [1,0,0,1,0,0,1,0,0,0,1,0,0,1,0,0], s: [0,0,0,0,1,0,0,1,0,0,0,0,1,0,1,1], h: [1,1,0,1,1,1,0,1,1,1,0,1,1,1,0,1]},
    {k: [1,0,0,0,0,0,0,0,1,0,1,0,0,0,0,0], s: [0,0,0,0,1,0,0,0,0,0,0,1,1,0,0,0], h: [1,0,1,1,1,0,1,1,1,0,1,1,1,0,1,1]},
    {k: [1,1,0,0,0,0,1,0,0,0,1,0,0,1,0,0], s: [0,0,0,0,1,0,1,0,0,1,0,0,1,0,0,0], h: [1,0,1,0,1,0,1,0,1,0,1,0,1,1,1,1]}
];

var running = 0, ticks = 0, step = 0, bar = 0, cur = null;
var lastTick = 0, tickMs = 60000 / (72 * 24);     // smoothed time between clocks
var recPos = 0, stutLeft = 0;                    // (recPos: where the buffer records, 0..1)
var bassNote = 36, running_status = 0, mbuf = [];
var pending = [];

function clamp(x, a, b) { return x < a ? a : x > b ? b : x; }
function chance(p) { return Math.random() < p; }

// ---------------------------------------------------------------- parameters
function density(v) { P.density = clamp(v, 0, 1); }
function ratchet(v) { P.ratchet = clamp(v, 0, 1); }
function stutter(v) { P.stutter = clamp(v, 0, 1); }
function swing(v) { P.swing = clamp(v, 0, 0.5); }
function speed(v) { P.speed = clamp(Math.round(v), 1, 3); }
function bassch(v) { P.bassch = clamp(Math.round(v), 1, 16); }
function bufms(v) { P.bufms = Math.max(100, v); }
function recpos(v) { recPos = clamp(v, 0, 1); }

// ---------------------------------------------------------------- the clock
function ticksPerStep() { return P.speed === 1 ? 6 : P.speed === 2 ? 3 : 2; }
function stepMs() { return tickMs * ticksPerStep(); }

function rt(b) {
    var now = new Date().getTime();
    if (b === 248) {                             // clock
        if (lastTick) {
            var d = now - lastTick;
            if (d > 2 && d < 200)
                tickMs += (d - tickMs) * 0.1;
        }
        lastTick = now;
        if (!running)
            return;
        if (ticks % ticksPerStep() === 0)
            onStep();
        ticks++;
    } else if (b === 250) {                      // start: from the top
        running = 1;
        ticks = 0;
        step = 0;
        bar = 0;
        cur = null;
        status();
    } else if (b === 251) {                      // continue
        running = 1;
        status();
    } else if (b === 252) {                      // stop
        running = 0;
        outlet(3, "stutoff");
        status();
    }
}

// ---------------------------------------------------------------- the FM-1's notes (the sub follows the bass)
function byte(b) {
    if (b >= 248) {                              // realtime inside the stream: the clock's own way
        rt(b);
        return;
    }
    if (b & 0x80) {
        running_status = b;
        mbuf = [];
        return;
    }
    mbuf.push(b);
    var st = running_status & 0xF0, ch = (running_status & 0x0F) + 1;
    if ((st === 0x90 || st === 0x80) && mbuf.length === 2) {
        var note = mbuf[0], vel = mbuf[1];
        mbuf = [];
        if (st === 0x90 && vel > 0 && ch === P.bassch) {
            bassNote = note;
            outlet(4, 440 * Math.pow(2, (bassNote - 12 - 69) / 12));   // an octave under the bass voice
        }
    } else if ((st === 0xB0 || st === 0xE0 || st === 0xA0) && mbuf.length === 2) {
        mbuf = [];
    } else if ((st === 0xC0 || st === 0xD0) && mbuf.length === 1) {
        mbuf = [];
    }
}

// ---------------------------------------------------------------- the beat
function newBar() {
    var i, base = BREAKS[Math.floor(Math.random() * BREAKS.length)], p = {k: [], s: [], h: []};
    for (i = 0; i < 16; i++) {                   // the break, mutated by density
        p.k[i] = base.k[i] && chance(0.75 + 0.25 * P.density) ? 1 : (!base.k[i] && chance(0.08 * P.density) ? 0.7 : 0);
        p.s[i] = base.s[i] ? 1 : (chance(0.22 * P.density) ? 0.3 : 0);   // ghost snares
        p.h[i] = base.h[i] && chance(0.55 + 0.45 * P.density) ? 1 : 0;
    }
    if (chance(0.15))                            // a beat dropped now and then
        for (i = 8; i < 12; i++)
            p.k[i] = p.s[i] = 0;
    return p;
}

function hit(which, vel, delay) {
    if (delay <= 0) {
        outlet(which, vel);
        return;
    }
    var t = new Task(function (w, v) { outlet(w, v); }, this, which, vel);
    t.schedule(delay);
    pending.push(t);
    if (pending.length > 96)
        pending.shift().freepeer();
}

function roll(which, vel, n, len, accel) {      // n hits inside len ms (accelerating when accel)
    var i, t = 0, d = len / n;
    for (i = 0; i < n; i++) {
        hit(which, vel * (0.55 + 0.45 * (accel ? i / (n - 1 || 1) : 1 - i / n)), t);
        t += accel ? d * (1.4 - 0.8 * i / n) : d;
    }
}

function onStep() {
    var s = step % 16, ms = stepMs(), sw = (s % 2) ? ms * P.swing : 0, n;
    if (s === 0) {
        if (!cur || chance(0.35 + 0.4 * P.density))
            cur = newBar();
        if (step === 0)
            bar++;
    }
    if (bar % 4 === 0 && s >= 12) {             // the fourth bar's end: a snare rush
        roll(1, 0.9, s === 15 ? 8 : 4, ms, true);
        if (s === 12)
            hit(0, 1, 0);
    } else {
        if (cur.k[s])
            hit(0, cur.k[s], sw);
        if (cur.s[s]) {
            if (cur.s[s] >= 1 && chance(P.ratchet * 0.6)) {
                var rn = [2, 3, 4, 6, 8][Math.floor(Math.random() * (P.ratchet > 0.6 ? 5 : 3))];
                roll(1, cur.s[s], rn, ms, chance(0.5));
            } else {
                hit(1, cur.s[s], sw);
            }
        }
        if (cur.h[s]) {
            if (chance(P.ratchet * 0.4))
                roll(2, 0.7, chance(0.5) ? 2 : 3, ms, false);
            else
                hit(2, (s % 4 === 0 ? 0.9 : 0.55) * (0.8 + 0.2 * Math.random()), sw);
        }
    }
    // the FM-1's own sound, stuttered: now and then a slice of what just played, looped for a few steps
    if (stutLeft > 0) {
        stutLeft--;
        if (!stutLeft)
            outlet(3, "stutoff");
    } else if (s % 4 === 0 && chance(P.stutter * 0.5)) {
        var lens = [0.5, 1, 1, 2], len = ms * lens[Math.floor(Math.random() * lens.length)],
            end = recPos * P.bufms, start = end - len;
        if (start >= 0) {                        /* (not across the buffer's wrap: the next time) */
            outlet(3, "stut", start, end);
            stutLeft = 2 + Math.floor(Math.random() * 3);
        }
    }
    step = (step + 1) % 16;
    if (s === 15)
        status();
}

function status() {
    outlet(5, "set", running ? "running" : "stopped", "bar", bar, "step", Math.round(stepMs()) + "ms",
           "fm1 bpm", Math.round(60000 / (tickMs * 24)));
}

function bang() { status(); }
