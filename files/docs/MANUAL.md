# AMB-1 FM6

AMB-1 FM6 turns the M-VAVE FM-1 into a generative ambient machine, inspired by Brian Eno's *Music for Airports*. It
plays itself from the moment it's switched on: three FM voices (each breathing chords, ringing loops or gliding a
bass, as you choose), quiet Euclidean percussion, a short echo, a tape loop and a plate reverb, and a reel of tape
that records it all and plays it back as grains. You don't perform it, you tend it: pick a piece and a scale, take
notes out, put them back, and let it grow.

It is an unofficial fork of Charles Vestal's AMB-1: the same garden, with DX7-style FM voices (their patches edited
operator by operator, your own DX7 banks), three voices alike, the dice and the reel.

![A drawing of the FM-1 running AMB-1 FM6, each control labelled with what it does: the Controls section below says the same in words](img/fm1-panel.svg "The FM-1 (a drawing), each control labelled")

---

## Contents

1. Install
2. Getting started
3. Controls
4. The screen
5. The garden
6. The pieces
7. The pages
8. The reel
9. MIDI
10. Saving
11. Troubleshooting
12. Parameters
13. Credits

---

## 1. Install

This is **AMB-1 FM6**, its own package (identity `FM-1_76…`), not the one on AMB-1's site. The FM-1 must run
M-VAVE's firmware **V15** first: a V16 goes back to V15 with M-VAVE's updater, M-UPGRADE, and the V15 file
(`firmware-original-M-VAVE/FM-1_V15.fwsc` in the package's folder).

**Install:** with Python 3 and its MIDI modules (`pip install mido python-rtmidi`; on Windows the Visual C++
runtime too), connect the FM-1 with a USB data cable and run, from the package's folder:

```
python fm1_install.py amb-1.3.99-fm6.fwsc
```

Don't unplug it while it's writing; when it finishes, the FM-1 restarts into AMB-1 FM6 and the script checks the
identity it reports (`FM-1_76010399`). `python fm1_install.py --info` shows what an FM-1 runs.

**Go back:** AMB-1's [web installer](https://charlesvestal.github.io/fm1-amb/install/) has "Back to the stock
firmware" at the bottom, for any FM-1 whatever it runs: choose the V15 file and press the button. M-VAVE's
M-UPGRADE works too.

> Installing other firmware is at your own risk. If the FM-1 ever won't start, see
> [Recovering an FM-1 that won't start](#recovering-an-fm-1-that-won-t-start).

---

## 2. Getting started

AMB-1 FM6 starts playing as soon as it's on. A few minutes with it:

1. **Listen.** The first piece, Garden, is playing: D Dorian, a chord breathing (Voice 1), bells on loops
   (Voice 2), a bass (Voice 3) and a soft pulse. Turn MASTER for the volume.
2. **Try another piece.** Turn PRESETS. Each of the eight pieces has its own key, tempo, sounds and movement.
3. **Change the scale.** Press a key whose note is lit: it leaves the scale. Press it again to bring it back.
   Any key that isn't lit adds its note.
4. **Move on.** Press EDIT: the chord moves on at the start of the next bar.
5. **Freeze.** Press REC: the chord holds and the reverb rings on, while the rest goes on above it. REC again lets
   it go.
6. **Shape a voice.** ENV, ARP and LFO are Voices 1, 2 and 3: Level, Role, Octave, Density. Press the button again
   for the voice's patch, its operators and its modulators.
7. **Roll the dice.** Press SEL: the page shown gets new values, in ranges that keep it music. Hold SEL a second to
   undo.
8. **Record.** SEQ twice opens the Tape page; EDIT records, REC stops, and the reel plays it back.
9. **Rest.** Press PLAY: the garden stops growing and its sounds fade out slowly. PLAY again and it grows back.

What you change is kept with the piece. It saves by itself while resting, or press SAVE.

Not sure what a button does? Hold it for a second: a card says. Let go and nothing happens.

---

## 3. Controls

| Control | What it does |
|---|---|
| Keys | The scale. Each key adds its note to the scale or takes it out; the keys light up for the notes in it. Every octave of a note is the same key. The root stays, and the scale keeps at least three notes. |
| PLAY | Grow / rest. At rest, the sounds fade out slowly and the reverb rings away. Lit green while growing. |
| REC | **Freeze**: the chord holds and the reverb rings on. Again: thaw. On the reel's pages: record (see [The reel](#s8)). |
| EDIT | The next chord, at the start of the next bar. On the reel's pages: a new splice. |
| HOME | The Garden page. |
| ENV · ARP · LFO | Voices 1, 2 and 3. Again: the voice's PATCH, OPERATOR and MOD pages, then back. |
| SEQ | The Pulse. Again: the reel's Tape, Gene and Deck pages, then back. |
| FX · GLO | The Space and Setup pages. |
| SEL | **The dice.** Tapped: the page shown rolled. Held with ENV, ARP, LFO, HOME, SEQ, FX or GLO: that whole layer (with HOME and SEQ, the loops and patterns grown anew too). With EDIT: new loops and patterns, the settings kept. Held alone a second: the last roll undone. |
| SELECT | The previous or next page. Hold HOME and turn it for the tempo. |
| ALGORITHM | Mode: Ionian, Dorian, Phrygian, Lydian, Mixolydian, Aeolian, pentatonic major and minor. Editing the scale with the keys makes it Custom. |
| PRESETS | The piece (see [The pieces](#s6)). |
| KNOB 1–4 | The four values on the screen. A few clicks are single steps; a quick turn speeds up. |
| OCT− / OCT+ | The octave of the voice shown (Voice 1 on the other pages), two down to two up. |
| MASTER | Volume. |
| SAVE | Saves all eight pieces (on the reel's pages, the reel too). Held 1.5 s: the piece back to its recipe. |

**Help:** hold any button but PLAY for a second and a card says what it does. A key, a knob or another button
puts it away; letting go after it does nothing else.

![SAVE's card: SAVES ALL EIGHT PIECES. KEEP HOLDING: THE PIECE BACK TO ITS RECIPE](img/screen-help.png "SAVE's card")

---

## 4. The screen

![The Garden page: Garden, D Dorian, tempo 72; the ring with the chord Dm7, the four level bars on the left, the pulse rings on the right](img/screen-garden.png "The Garden page (HOME)")

- **Top line:** the piece, its root and mode. On the right, a slow beat light, the tempo (EXT when it follows MIDI
  clock) and a dot when there are unsaved changes. Messages ("ADDED F#", "VOICE 2 / PATCH") show here for a moment.
- **Top right of the garden:** a thin bar fills up on the way to the next chord; while frozen it says "held".
- **The ring:** the twelve notes, C at the top. Notes in the scale are green, the chord's deeper, the root ringed.
  The chord's name is in the middle.
- **Around it:** the four loops of the first Loops voice, 13, 17, 23 and 29 steps, from the inside out. The blue
  dots hold a note; a playhead goes round each loop and glows when it rings.
- **Left:** the levels as they sound: the chord (green), the loops (blue), the bass (olive), the pulse (dark).
- **Right:** the three pulse rings: thump (16 steps), closed hat (12), open hat (7).
- **Bottom:** the page's name and a dot for each of the ten pages, then its four values, one for each knob: a dial
  (or, for a switch, its positions) and the value. The one you're turning lights up.

On the reel's pages the garden gives way to the reel (see [The reel](#s8)).

![The Space page with ECHO TIME lit as it turns](img/screen-turning.png "Turning Echo time on the Space page")

---

## 5. The garden

AMB-1 FM6 grows its voices from one scale and one chord. Each of the three voices plays its own FM patch (six
operators, as a DX7) in one of three **roles**, and any voice can take any role:

- **Chord:** up to four breathing notes. Each swells in, hangs and fades on its own cycle (unequal lengths, as in
  the tape loops of "2/1"), taking its note of the chord as it starts, so the chord forms and dissolves instead of
  sitting there. Every so often (**Drift**) the chord moves to another one in the scale; notes the new chord shares
  hold to the end of their breath, the others fade and breathe in again on their new notes.
- **Loops:** four tape loops 13, 17, 23 and 29 steps long. Each holds a few notes, so the loops slide against each
  other and the melody never quite repeats. Each loop is its own shade of the patch, and now and then a note rings
  with its octave.
- **Bass:** the chord's root, low and soft, gliding; sometimes it leans on the fifth.

By default Voice 1 is a chord, Voice 2 the loops and Voice 3 the bass, as AMB-1's pad, bells and bass were.

- **Pulse:** a soft thump tuned to the chord's root, a closed hi-hat and an open one, each on its own Euclidean
  pattern over 16, 12 and 7 sixteenths. The patterns slowly turn and gain or lose a hit.

Then the space, after Eno's chain for *Discreet Music*: a short tape echo, a tape loop (up to 2.8 seconds, two
heads, each repeat darker and thinner) and a plate reverb. With **Motion** up, the echo's heads wander across the
field and now and then the second slides to another part of the loop.

Over it all:

- **The arc:** a slow tide, a few minutes from end to end, between sparse (fewer notes, darker, quieter pulse, more
  reverb) and full. **Motion** sets how far it swings, and how far pitch, pan and the echo's heads wander.
- **Rests:** with **Evolve** up, the voices take breaths: the pulse sits out two to four bars, a bass skips a chord,
  a loop goes quiet for a pass.

---

## 6. The pieces

PRESETS chooses one of eight pieces, each a recipe for the whole garden: the voices and their sounds, tempo, key and
mode, how busy and how changeable. Each remembers how you left it. Hold SAVE 1.5 s to put the piece back to its
recipe. Tune, MIDI out and Lights are the same for all.

| Piece | What it is |
|---|---|
| **Garden** | D Dorian, 72: everything, gently. Rhodes. |
| **Departures** | C# Ionian, 60: soft bells an octave down over a quiet Rhodes, as "1/1". |
| **Voices** | F Aeolian, 56: only sung voices and a low bass, as "2/1". |
| **Afternoon** | G Lydian, 84: a bright Rhodes, busier bells, a light pulse. |
| **Night Bus** | A Aeolian, 96: the pulse forward, metallic bells, short echoes. |
| **Shoreline** | E pentatonic major, 64: dark, round bells lost in long echoes, as "2/2". |
| **Glass** | Eb pentatonic minor, 76: clangy bells an octave up, dense. |
| **Tides** | Bb Mixolydian, 70: sung voices, the most changeable. |

---

## 7. The pages

A page on each button, four values on KNOB 1–4; SELECT steps through them: Garden, Voice 1, 2, 3, Pulse, Space,
Setup, Tape, Gene, Deck. Each voice has more pages of its own (ENV, ARP or LFO again).

### HOME: Garden

- **Tempo:** 30 to 120 (also HOME + SELECT, from any page).
- **Evolve:** how restless the garden is. At 0 the loops repeat exactly; turned up, notes change or move at the end
  of a pass, the pulse patterns turn, and the voices take rests.
- **Motion:** how far the arc swings, and how much pitch, pan and the echo wander.
- **Drift:** how long a chord lasts before it moves on, 20 seconds to 5 minutes. EDIT moves on now.

### ENV · ARP · LFO: Voices 1, 2, 3

![Voice 2's page: LEVEL 65, ROLE Loops, OCTAVE 0, DENSITY 40](img/screen-voice.png "A voice's page")

The three alike:

- **Level.**
- **Role:** Chord, Loops or Bass (see [The garden](#s5)). Changing it fades the voice out and in.
- **Octave:** two down to two up (OCT− and OCT+ too).
- **Density:** a chord's notes (1 to 4); the loops' notes (none to five a loop); how often the bass leans on the
  fifth.

### SEQ: Pulse

- **Level.**
- **Hits:** how many beats the three rings play. 0: no pulse.
- **Swing:** the off sixteenths later, from straight (50%) to a heavy lilt (75%).
- **Hats:** from dark and long to bright and crisp.

### FX: Space

- **Echo:** how much goes to the echoes.
- **Echo time:** the tape loop's length: a dotted quarter, a half, a dotted half, 1, 2 or 4 bars, shortened to fit
  its 2.8 seconds at slow tempos. The second head repeats at three quarters of it (with Motion, at other parts).
- **Reverb:** how much goes to the plate.
- **Size:** how long the plate rings, from a room to a long hall, and how many times the echo comes back.

### GLO: Setup

![The Setup page: ROOT D, TUNE 0, MIDI OUT On, LIGHTS On](img/screen-setup.png "The Setup page")

- **Root:** the key's root note.
- **Tune:** fine tuning, ±50 cents.
- **MIDI out:** on or off.
- **Lights:** On: the keys light the scale and the unlit buttons glow dimly. Keys: the key lights only. Off.

The top right of this page shows how much of the FM-1's time the sound takes (CPU).

### ENV, ARP or LFO again: a voice's own pages

Each voice's patch is a DX7 voice of six operators, edited as a DX7 edits it, with five generative modulators. Its
pages come in three groups: press the voice's button again for **PATCH**, again for **OPERATOR**, again for
**MOD**, and once more to come back. SELECT steps through the group's pages; each group comes back on the page it
was left on. The top of the screen says which voice and which page. Each piece keeps all of it.

**PATCH**

| Page | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
| --- | --- | --- | --- | --- |
| Patch | Patch | Algorithm (1–32) | Feedback (0–7) | Index (±24 dB on the modulators) |
| Play | Envelope (AMB, Patch, ADSR) | Gate (5–100 %) | Pace | Glide |
| ADSR | Attack | Decay | Sustain | Release |
| LFO | Speed | Delay | Pitch mod | Amp mod |
| LFO 2 | Wave | Pitch sens | Key sync | Transpose (±24 semitones, as a DX7 plays it) |
| Pitch EG | Stage (1–4) | Rate | Level (±50) | Osc sync |

![A voice's Play page: ENVELOPE Patch, GATE 25%, PACE 1/4, GLIDE 260ms](img/screen-play.png "The Play page")

- **Patch:** AMB-1 FM6's own eighteen first (pads: Rhodes, Voices, Glass, Strings, Warm, Halo; bells: Bell, shaped
  by the piece's tone, Vibes, Marimba, Tubular, Kalimba, Celesta; basses: Sine, Round, Pick, Sub, Reed, Fretless),
  any voice any of them, then the voices of the DX7 banks you sent (see [MIDI](#s9)), by name. Turning it loads the
  patch as it is; any edit makes it the piece's own, shown with a `*` ("E.PIANO 1*").
- **Envelope:** how the notes are shaped. **AMB:** AMB-1's own (a chord breathes, the loops let a held note go after
  a beat, the bass is one note held and faded in). **Patch** (the default): each note struck and let go after
  **Gate**, so the patch's own envelopes are heard, and editing them changes the sound. **ADSR:** the same, the
  carriers' envelope from the four ADSR knobs (the modulators keep the patch's).
- **Gate:** how long a note is held down: of a chord's breath, of a bar for the loops, of a chord (or a step) for the
  bass.
- **Pace:** a chord's breath (4 to 16 s); the loops' step (1/16, 1/8, 1/4, 1/2); the bass struck each chord, or
  each bar, 1/2, 1/4, 1/8 (with Patch or ADSR).
- **Glide:** how slowly the bass slides to its next note, 20 ms to 1.2 s.

![A voice's ADSR page: ATTACK 5, DECAY 40, SUSTAIN 70, RELEASE 40](img/screen-adsr.png "The ADSR page")

**OPERATOR** (KNOB 1 picks the operator, OP1–OP6, on every page of the group)

| Page | KNOB 2 | KNOB 3 | KNOB 4 |
| --- | --- | --- | --- |
| OP | Ratio (×0.50 .. ×31; fixed mode: Hz) | Fine | Level |
| OP Env | Stage (1–4) | Rate | Level |
| OP 2 | Mode (Ratio, Fixed) | Detune (±7) | Velocity |
| OP 3 | Break (a note) | L depth | R depth |
| OP 4 | L curve | R curve | Rate scale |
| OP 5 | Amp sens | | |

Ratio and Fine show the operator's frequency together ("x1.50"); in fixed mode it is in Hz. Edits are heard at once
on the notes sounding.

**MOD** (KNOB 1 picks the modulator, 1–5, on every page of the group)

| Page | KNOB 2 | KNOB 3 | KNOB 4 |
| --- | --- | --- | --- |
| Mod | Target | Depth | Rate |
| Mod 2 | Shape | Sync | Smooth |
| Mod 3 | Spread | Fade | |

Each of a voice's four notes has a generator of its own in each modulator, each at its own pace around Rate; the five
are summed; the arc deepens them when the garden is full and calms them when sparse.

- **Depth:** how far it goes. 0: off.
- **Rate:** a cycle from 60 seconds to an eighth of a second. With Sync at Tempo: a note value, 8 bars to 1/16.
- **Shape:** Sine, Triangle, Saw up, Saw down, Square; S&H (a random value a cycle, held); Walk (gliding between
  random values); Chaos (the logistic map, jumping); Env (from each note's start, falling to nothing over a cycle).
- **Target:** the voice's: **Timbre** (the modulators' level, ±12 dB), **Ratio** (harmonic steps, ×1 to ×5),
  **Metal** (the modulators out of tune with each other), **Feedback**, **Decay** (each new note's envelopes),
  **Pitch** (±30 cents), **Space** (the stereo field), **All**, **Level** (a tremolo, to −10 dB), **LFO PM** (the
  patch's vibrato, deeper). And any operator's own, **OP1**..**OP6**: **Level**, **Ratio**, **Fine**, **Atk**,
  **Dec**, **Sus**, **Rel**.
- **Spread:** 100: each note its own way. 0: the voice's notes move as one.
- **Fade:** the modulation coming in after each note, at once (Off) to 8 seconds.
- **Sync:** Free; Note (each note starts the shape again); Tempo (Rate a note value).
- **Smooth:** a glide on the shape, 2 ms to half a second.

---

## 8. The reel

A track of tape, 11.8 seconds, mono, after a TASCAM portastudio's tape and Make Noise's Morphagene's genes (the idea,
not their code). SEQ on the Pulse page opens **Tape**, again **Gene**, again **Deck**, again back to the Pulse.

![The Tape page: a cassette with its spools, the reel as a strip with two splices and the cursor; 4.2 / 11.8 s, FX BEFORE, 2 SPLICES; SOURCE Mix, DRY/WET 50, SOS 70, SPEED +1.00x](img/screen-tape.png "The Tape page")

On these pages the screen shows the reel: a cassette whose spools wind and turn at Speed, the reel as a strip (its
splices, the one playing bright, the cursor, each gene where it reads, the record head), the seconds recorded and
the splices.

| Page | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
| --- | --- | --- | --- | --- |
| Tape | Source (Mix, Voice 1, 2, 3, Pulse) | Dry/Wet | SOS | Speed (−2x .. Stop .. +2x) |
| Gene | Gene (the whole splice .. 1/256) | Slide | Morph | Organize (splice 1–8) |
| Deck | FX (Before, After) | Wow | Drive | Tone |

**Recording** (on these three pages):

- **EDIT** records a new **splice** at the end of the reel (up to 8). EDIT again while recording ends it and starts
  the next at once; **REC** stops.
- **REC** overdubs the splice Organize picks, round and round, sound on sound (**SOS**: how much of what was there
  stays; 0 replaces it). REC again stops.
- **SAVE** keeps the pieces and the reel; the screen shows how far it has gone, and the reel comes back after a
  restart. **REC held** 1.5 s erases the reel (save after, to erase the saved one too).
- A splice can't be erased alone: overdub it at SOS 0, or erase the reel.

**What it records** is **Source**'s: the whole mix (the reel itself left out), one voice alone, or the pulse, a
little saturated as onto tape (**Drive**).

**How it plays:** the splice Organize picks, as **genes**: each a window of the splice (**Gene**), starting where
the tape is, past it by **Slide**, at **Speed** (pitch and time together, as tape; 1x and −1x stick; at Stop it
falls silent), with **Wow** and flutter. **Morph**: gaps between the genes (0–40), one after another (40–55), two to
four overlapping (55–100), spread across the field (past 75), an octave up or down now and then (past 85). **Tone**
darkens it (1.5 kHz to open).

![The Gene page: GENE 100, SLIDE 0, MORPH 45, ORGANIZE 3](img/screen-gene.png "The Gene page")

**Dry/Wet:** the garden against the reel: Dry, the garden alone; 50, both full; Wet, the reel alone (the garden is
still recorded, unheard, as a send).

**FX** sets where the space (echo and reverb) is: **Before** the reel, the garden goes through it and the reel
records it with its space and plays dry beside it; **After** the reel, it records the garden dry and plays through
the space, as the garden does.

![The Deck page: FX Before, WOW 25, DRIVE 20, TONE 85](img/screen-deck.png "The Deck page")

---

## 9. MIDI

MIDI comes in over USB and the TRS jack, and goes out over USB.

- **In, channel 1:** a note adds or removes its note from the scale, like a key.
- **In, clock:** the beat follows MIDI clock, and start / stop start and stop the garden. The screen shows EXT for
  the tempo. What hangs on the tempo follows it too, measured a beat at a time: the echo, the modulators at Sync
  Tempo, the loops' Gate, the swing.
- **Out** (on by default): Voices 1, 2 and 3 on channels 1, 2 and 3, the pulse on 10, and clock with start / stop
  when it runs on its own tempo. Turning MIDI out off sends all notes off.

### CC and NRPN, in and out

What the knobs set comes and goes as MIDI CC (out with MIDI out on; what comes in is not sent back):

| Channel | CC | What |
| --- | --- | --- |
| 16 | 20–49 | the parameters 0–29 (the layers, the space, setup; a patch source as its number) |
| 16 | 50–61 | the reel: Source, Dry/Wet, SOS, Speed, Gene, Slide, Morph, Organize, FX, Wow, Drive, Tone |
| 1, 2, 3 (the voices) | 20 | the voice's Index |
| 1, 2, 3 | 21–60 | its five modulators, eight each: Depth, Rate, Shape, Target, Spread, Fade, Sync, Smooth |
| 1, 2, 3 | 102–112 | how it plays: Envelope, Gate, Role, Octave, Density, Pace, Glide, Attack, Decay, Sustain, Release |
| 1, 2, 3 | NRPN: 99 = 0/1, 98 = 0..127, 6 = value | its patch, byte by byte (the DX7's VCED order, 0..154) |

A value is scaled to its range. The patch's bytes go as they are.

### DX7 voices and banks from a computer

The firmware carries no Yamaha voices: you send your own. `amb_syx.py` sends them over USB (Python with mido and
python-rtmidi; close other programs using the FM-1's MIDI port first):

```sh
python amb_syx.py ROM1A.syx --bank 1      # a 32-voice bank into bank 1 (of 16)
python amb_syx.py EPIANO.syx              # a single voice into the voice whose pages are shown
python amb_syx.py --info                  # how many voices each bank holds
python amb_syx.py --erase 1
```

A bank can be a DX7 bulk dump (4104 bytes), 4096 bytes of packed voices, or up to 32 single voices in one file;
values past the DX7's ranges (common in old banks) are held to them. The screen says BANK n and how many voices came.
A single voice from a DX7 editor (Dexed's Send) goes straight into the voice whose pages are shown. The banks (16 of
32 voices) stay in the FM-1's memory apart from the pieces; a piece keeps a copy of each voice it plays, so sending
another bank later never changes it.

### Screen reader

AMB-1 can tell a screen reader what its screen shows: connect the FM-1 by USB, open the
[FM-1 Screen Reader](https://charlesvestal.github.io/fm1-reader/) in Chrome or Edge, and click Connect. It reads out
the page, its knobs, a knob's value as you turn it, the piece and its key, help cards, messages and each chord. Press
W to hear the whole screen again.

---

## 10. Saving

AMB-1 FM6 keeps all eight pieces in the FM-1's memory, each as you left it: the knobs, the scale, the tempo, each
voice's patch and settings, plus Tune, MIDI out and Lights.

- **By itself:** a few seconds after a change, but only while resting (writing to the memory interrupts the sound
  for a moment). A dot at the top of the screen means there are unsaved changes.
- **SAVE:** saves now. On the reel's pages, the reel too.
- **Hold SAVE** 1.5 s: the piece goes back to its recipe (not saved until the next save).
- The pieces of earlier versions load as they were, each voice in the role its layer had.

---

## 11. Troubleshooting

**Clicks with everything going:** the sound takes most of the FM-1's time with three voices busy, the modulators on
and the reel playing many genes (CPU on the Setup page, top right). Fewer overlapping genes (Morph lower), fewer
modulators or a lower Density help.

**Update mode:** hold OCT- and OCT+ together for five seconds. A countdown appears; let go before it ends to cancel.

**Calibration:** if a button or knob does the wrong thing, hold OCT- and OCT+ while you switch the FM-1 on. The
screen then asks you to press each button and turn each knob in turn.

**After a crash:** a red CRASH screen, and it restarts by itself a few seconds later. The next start shows RESTARTED
AFTER A CRASH and an address (PC ...) for a moment: note it, and what you were doing.

**Safe mode:** if it crashes twice in a row while starting, it starts in SAFE MODE instead: no sound, but USB works,
so the installer can reach it: install again, or put the stock firmware back. Press PLAY to try starting normally.
If even safe mode fails twice, the FM-1 drops into the chip's own update mode, and only the rescue below reaches it.

### Recovering an FM-1 that won't start

An FM-1 in the chip's own update mode shows a blank screen, and neither the installers nor M-VAVE's updater can see
it. A script can put M-VAVE's firmware back, on a Mac, with the USB cable you already have. Open Terminal, paste this
line and press Return:

```
bash -c "$(curl -fsSL https://raw.githubusercontent.com/hugelton/felucca-bsp/main/tools/fm1_rescue.sh)"
```

1. If a window asks to install the command line developer tools, click Install, then paste the line again.
2. If it asks for a password, type your Mac password (nothing shows) and press Return.
3. When it says it's waiting, plug the FM-1 in and switch it on. The script catches it, checks the chip, saves a
   backup of the FM-1's memory in the `fm1-rescue` folder in your home folder, and asks before it writes anything.
4. Type **yes** and press Return. Don't unplug anything until it says it's restarting the FM-1. It then starts into
   its original firmware, and you can install AMB-1 FM6 again.

The script downloads M-VAVE's FM-1 V15 firmware and JieLi's flash loader and checks both against known
fingerprints. It writes only the firmware area, never the bootloader or the FM-1's own settings. If the FM-1 never
shows up at all, Felucca's [FM-1-transporter](https://github.com/kurogedelic/FM-1-transporter) can still reach it.

---

## 12. Parameters

The defaults are Garden's; each piece has its own.

| Page | Value | Range | Garden |
|---|---|---|---|
| Garden | Tempo | 30–120 | 72 |
| Garden | Evolve | 0–100 | 35 |
| Garden | Motion | 0–100 | 50 |
| Garden | Drift | 20 s – 5:00 | 25 (about 40 s) |
| Voice 1, 2, 3 | Level | 0–100 | 60, 65, 45 |
| Voice 1, 2, 3 | Role | Chord, Loops, Bass | Chord, Loops, Bass |
| Voice 1, 2, 3 | Octave | −2 – +2 | 0 |
| Voice 1, 2, 3 | Density | 0–100 | 100, 40, 35 |
| Pulse | Level | 0–100 | 40 |
| Pulse | Hits | 0–100 | 35 |
| Pulse | Swing | 50%–75% | 50% |
| Pulse | Hats | 0–100 | 50 |
| Space | Echo | 0–100 | 45 |
| Space | Echo time | 1/4., 1/2, 1/2., 1 bar, 2 bars, 4 bars | 1 bar |
| Space | Reverb | 0–100 | 65 |
| Space | Size | 0–100 | 80 |
| Setup | Root | C – B | D |
| Setup | Tune | −50 – +50 cents | 0 |
| Setup | MIDI out | Off, On | On |
| Setup | Lights | Off, On, Keys | On |
| Tape | Source | Mix, Voice 1, 2, 3, Pulse | Mix |
| Tape | Dry/Wet | 0–100 | 50 |
| Tape | SOS | 0–100 | 70 |
| Tape | Speed | −2x .. Stop .. +2x | +1x |
| Gene | Gene, Slide, Morph | 0–100 | 100, 0, 45 |
| Gene | Organize | 1–8 | 1 |
| Deck | FX | Before, After | Before |
| Deck | Wow, Drive, Tone | 0–100 | 25, 20, 85 |
| A voice's PATCH | Patch | the eighteen, then the banks' voices | Rhodes, Bell, Sine |
| A voice's PATCH | Algorithm, Feedback, LFO, Pitch EG, Transpose | as a DX7 | as the patch |
| A voice's PATCH | Index | −24 – +24 dB | 0 dB |
| A voice's Play | Envelope | AMB, Patch, ADSR | Patch |
| A voice's Play | Gate | 5–100 % | 70 %, 25 %, 80 % |
| A voice's Play | Pace | 0–100 | 60, 60 (1/4), 0 (each chord) |
| A voice's Play | Glide | 20 ms – 1.2 s | 260 ms |
| A voice's ADSR | Attack, Decay, Sustain, Release | 0–100 | 5, 40, 70, 40 |
| A voice's OPERATOR | Ratio, Fine, Level, Envelope, Mode, Detune, Velocity, Break, Depths, Curves, Rate scale, Amp sens | as a DX7 | as the patch |
| A voice's MOD (×5) | Depth | 0–100 | 20 (the first), 0 |
| A voice's MOD (×5) | Rate | 0–100 (60 s – 0.12 s; Tempo: 8 bars – 1/16) | 27 (11 s) |
| A voice's MOD (×5) | Shape | Sine, Triangle, Saw up, Saw down, Square, S&H, Walk, Chaos, Env | Sine |
| A voice's MOD (×5) | Target | the voice's ten, then each operator's seven | Timbre |
| A voice's MOD (×5) | Spread, Fade, Sync, Smooth | 0–100, Off – 8 s, Free/Note/Tempo, 2–500 ms | 100, Off, Free, 2 ms |
| ALGORITHM | Mode | 8 modes, or Custom | Dorian |

---

## 13. Credits

AMB-1 FM6 is free software under the Mozilla Public License 2.0: if you share it changed, share the changed files'
source too. Its source comes with it (`codigo-fonte/`), with `LICENSING.md`.

- **AMB-1**, the garden, its pieces and its screen: Charles Vestal. AMB-1 FM6 is an unofficial fork, not connected
  with him.
- The idea comes from Brian Eno's generative music: *Music for Airports*' tape loops of unequal length, and the echo
  and tape-delay chain of *Discreet Music*.
- The FM voices: **msfa**, the DX7 core of Dexed (Google Inc., Pascal Gauthier), Apache-2.0, as ported to integer C
  by Leo Kuroshita and carried in Juan Ignacio Antonucci's JIANT FM.
- The reel's genes follow the idea of Make Noise's Morphagene, its tape a TASCAM portastudio's; no code of theirs.
  Its coding is IMA ADPCM.
- The reverb is Jon Dattorro's plate ("Effect Design, Part 1", JAES 1997), by way of FoMni-1. The Euclidean rhythms
  are Godfried Toussaint's reading of Bjorklund's algorithm.
- The platform is [fm1-base](https://github.com/charlesvestal/fm1-base) and
  [felucca-bsp](https://github.com/hugelton/felucca-bsp), from **Felucca** by Leo Kuroshita, Hügelton Instruments.
- The fonts are Barlow Semi Condensed and Terminus (SIL Open Font License).

M-VAVE, FM-1, Yamaha, DX7, Make Noise, Morphagene and TASCAM are trademarks of their owners. AMB-1 FM6 is not
connected with any of them, with Brian Eno or with Hügelton Instruments.
