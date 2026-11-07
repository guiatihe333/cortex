{
 "patcher": {
  "fileversion": 1,
  "appversion": {
   "major": 8,
   "minor": 5,
   "revision": 0,
   "architecture": "x64",
   "modernui": 1
  },
  "classnamespace": "box",
  "rect": [
   40.0,
   80.0,
   1240.0,
   960.0
  ],
  "openinpresentation": 0,
  "default_fontsize": 12.0,
  "boxes": [
   {
    "box": {
     "id": "obj-1",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      20,
      10,
      900,
      22
     ],
     "text": "AMB-1 FM6 breaks: a fast breakbeat in step with the FM-1 (its MIDI clock), its pads washed and stuttered. 1. Options > Audio Status: input = the FM-1 (USB audio 'AMB-1 FM-1'), output = your speakers. 2. Choose the FM-1's MIDI port below. 3. On the FM-1: Setup page, MIDI OUT On; press PLAY. 4. Turn the audio on (the speaker)."
    }
   },
   {
    "box": {
     "id": "obj-2",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      20,
      70,
      260,
      22
     ],
     "text": "MIDI from the FM-1 (choose its port)"
    }
   },
   {
    "box": {
     "id": "obj-3",
     "maxclass": "newobj",
     "numinlets": 1,
     "numoutlets": 1,
     "patching_rect": [
      20,
      95,
      72,
      22
     ],
     "outlettype": [
      ""
     ],
     "text": "loadbang"
    }
   },
   {
    "box": {
     "id": "obj-4",
     "maxclass": "newobj",
     "numinlets": 2,
     "numoutlets": 2,
     "patching_rect": [
      20,
      125,
      72,
      22
     ],
     "outlettype": [
      "",
      ""
     ],
     "text": "midiinfo"
    }
   },
   {
    "box": {
     "id": "obj-5",
     "maxclass": "umenu",
     "numinlets": 1,
     "numoutlets": 3,
     "patching_rect": [
      20,
      155,
      200,
      22
     ],
     "outlettype": [
      "int",
      "",
      ""
     ],
     "items": [],
     "parameter_enable": 0
    }
   },
   {
    "box": {
     "id": "obj-6",
     "maxclass": "newobj",
     "numinlets": 1,
     "numoutlets": 1,
     "patching_rect": [
      20,
      195,
      58,
      22
     ],
     "outlettype": [
      "int"
     ],
     "text": "midiin"
    }
   },
   {
    "box": {
     "id": "obj-7",
     "maxclass": "newobj",
     "numinlets": 1,
     "numoutlets": 1,
     "patching_rect": [
      120,
      195,
      44,
      22
     ],
     "outlettype": [
      "int"
     ],
     "text": "rtin"
    }
   },
   {
    "box": {
     "id": "obj-8",
     "maxclass": "newobj",
     "numinlets": 1,
     "numoutlets": 1,
     "patching_rect": [
      20,
      225,
      100,
      22
     ],
     "outlettype": [
      ""
     ],
     "text": "prepend byte"
    }
   },
   {
    "box": {
     "id": "obj-9",
     "maxclass": "newobj",
     "numinlets": 1,
     "numoutlets": 1,
     "patching_rect": [
      120,
      225,
      86,
      22
     ],
     "outlettype": [
      ""
     ],
     "text": "prepend rt"
    }
   },
   {
    "box": {
     "id": "obj-10",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      300,
      70,
      300,
      22
     ],
     "text": "or an internal clock (BPM, the FM-1's tempo)"
    }
   },
   {
    "box": {
     "id": "obj-11",
     "maxclass": "toggle",
     "numinlets": 1,
     "numoutlets": 1,
     "patching_rect": [
      300,
      95,
      24,
      22
     ],
     "outlettype": [
      "int"
     ],
     "parameter_enable": 0
    }
   },
   {
    "box": {
     "id": "obj-12",
     "maxclass": "newobj",
     "numinlets": 1,
     "numoutlets": 1,
     "patching_rect": [
      340,
      95,
      93,
      22
     ],
     "outlettype": [
      ""
     ],
     "text": "loadmess 72"
    }
   },
   {
    "box": {
     "id": "obj-13",
     "maxclass": "flonum",
     "numinlets": 1,
     "numoutlets": 2,
     "patching_rect": [
      340,
      125,
      50,
      22
     ],
     "outlettype": [
      "",
      "bang"
     ],
     "format": 6,
     "parameter_enable": 0
    }
   },
   {
    "box": {
     "id": "obj-14",
     "maxclass": "newobj",
     "numinlets": 1,
     "numoutlets": 1,
     "patching_rect": [
      340,
      155,
      163,
      22
     ],
     "outlettype": [
      ""
     ],
     "text": "expr 60000./($f1*24.)"
    }
   },
   {
    "box": {
     "id": "obj-15",
     "maxclass": "newobj",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      300,
      190,
      86,
      22
     ],
     "outlettype": [
      "bang"
     ],
     "text": "metro 34.7"
    }
   },
   {
    "box": {
     "id": "obj-16",
     "maxclass": "message",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      300,
      220,
      58,
      22
     ],
     "outlettype": [
      ""
     ],
     "text": "rt 248"
    }
   },
   {
    "box": {
     "id": "obj-17",
     "maxclass": "newobj",
     "numinlets": 1,
     "numoutlets": 3,
     "patching_rect": [
      400,
      190,
      65,
      22
     ],
     "outlettype": [
      "bang",
      "bang",
      ""
     ],
     "text": "sel 1 0"
    }
   },
   {
    "box": {
     "id": "obj-18",
     "maxclass": "message",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      400,
      220,
      58,
      22
     ],
     "outlettype": [
      ""
     ],
     "text": "rt 250"
    }
   },
   {
    "box": {
     "id": "obj-19",
     "maxclass": "message",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      460,
      220,
      58,
      22
     ],
     "outlettype": [
      ""
     ],
     "text": "rt 252"
    }
   },
   {
    "box": {
     "id": "obj-20",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      620,
      70,
      560,
      22
     ],
     "text": "the beat: density, ratchets (rolls), stutters of the FM-1's sound, swing; speed 2 = twice the FM-1's tempo, 3 = three times; the sub follows the notes on BASSCH (voice 3: 3)"
    }
   },
   {
    "box": {
     "id": "obj-21",
     "maxclass": "newobj",
     "numinlets": 1,
     "numoutlets": 1,
     "patching_rect": [
      620,
      110,
      100,
      22
     ],
     "outlettype": [
      ""
     ],
     "text": "loadmess 0.6"
    }
   },
   {
    "box": {
     "id": "obj-22",
     "maxclass": "flonum",
     "numinlets": 1,
     "numoutlets": 2,
     "patching_rect": [
      620,
      140,
      50,
      22
     ],
     "outlettype": [
      "",
      "bang"
     ],
     "format": 6,
     "parameter_enable": 0
    }
   },
   {
    "box": {
     "id": "obj-23",
     "maxclass": "newobj",
     "numinlets": 1,
     "numoutlets": 1,
     "patching_rect": [
      620,
      170,
      121,
      22
     ],
     "outlettype": [
      ""
     ],
     "text": "prepend density"
    }
   },
   {
    "box": {
     "id": "obj-24",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      675,
      140,
      70,
      22
     ],
     "text": "density"
    }
   },
   {
    "box": {
     "id": "obj-25",
     "maxclass": "newobj",
     "numinlets": 1,
     "numoutlets": 1,
     "patching_rect": [
      720,
      110,
      107,
      22
     ],
     "outlettype": [
      ""
     ],
     "text": "loadmess 0.35"
    }
   },
   {
    "box": {
     "id": "obj-26",
     "maxclass": "flonum",
     "numinlets": 1,
     "numoutlets": 2,
     "patching_rect": [
      720,
      140,
      50,
      22
     ],
     "outlettype": [
      "",
      "bang"
     ],
     "format": 6,
     "parameter_enable": 0
    }
   },
   {
    "box": {
     "id": "obj-27",
     "maxclass": "newobj",
     "numinlets": 1,
     "numoutlets": 1,
     "patching_rect": [
      720,
      170,
      121,
      22
     ],
     "outlettype": [
      ""
     ],
     "text": "prepend ratchet"
    }
   },
   {
    "box": {
     "id": "obj-28",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      775,
      140,
      70,
      22
     ],
     "text": "ratchet"
    }
   },
   {
    "box": {
     "id": "obj-29",
     "maxclass": "newobj",
     "numinlets": 1,
     "numoutlets": 1,
     "patching_rect": [
      820,
      110,
      107,
      22
     ],
     "outlettype": [
      ""
     ],
     "text": "loadmess 0.25"
    }
   },
   {
    "box": {
     "id": "obj-30",
     "maxclass": "flonum",
     "numinlets": 1,
     "numoutlets": 2,
     "patching_rect": [
      820,
      140,
      50,
      22
     ],
     "outlettype": [
      "",
      "bang"
     ],
     "format": 6,
     "parameter_enable": 0
    }
   },
   {
    "box": {
     "id": "obj-31",
     "maxclass": "newobj",
     "numinlets": 1,
     "numoutlets": 1,
     "patching_rect": [
      820,
      170,
      121,
      22
     ],
     "outlettype": [
      ""
     ],
     "text": "prepend stutter"
    }
   },
   {
    "box": {
     "id": "obj-32",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      875,
      140,
      70,
      22
     ],
     "text": "stutter"
    }
   },
   {
    "box": {
     "id": "obj-33",
     "maxclass": "newobj",
     "numinlets": 1,
     "numoutlets": 1,
     "patching_rect": [
      920,
      110,
      100,
      22
     ],
     "outlettype": [
      ""
     ],
     "text": "loadmess 0.0"
    }
   },
   {
    "box": {
     "id": "obj-34",
     "maxclass": "flonum",
     "numinlets": 1,
     "numoutlets": 2,
     "patching_rect": [
      920,
      140,
      50,
      22
     ],
     "outlettype": [
      "",
      "bang"
     ],
     "format": 6,
     "parameter_enable": 0
    }
   },
   {
    "box": {
     "id": "obj-35",
     "maxclass": "newobj",
     "numinlets": 1,
     "numoutlets": 1,
     "patching_rect": [
      920,
      170,
      107,
      22
     ],
     "outlettype": [
      ""
     ],
     "text": "prepend swing"
    }
   },
   {
    "box": {
     "id": "obj-36",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      975,
      140,
      70,
      22
     ],
     "text": "swing"
    }
   },
   {
    "box": {
     "id": "obj-37",
     "maxclass": "newobj",
     "numinlets": 1,
     "numoutlets": 1,
     "patching_rect": [
      1020,
      110,
      86,
      22
     ],
     "outlettype": [
      ""
     ],
     "text": "loadmess 2"
    }
   },
   {
    "box": {
     "id": "obj-38",
     "maxclass": "number",
     "numinlets": 1,
     "numoutlets": 2,
     "patching_rect": [
      1020,
      140,
      50,
      22
     ],
     "outlettype": [
      "",
      "bang"
     ],
     "parameter_enable": 0
    }
   },
   {
    "box": {
     "id": "obj-39",
     "maxclass": "newobj",
     "numinlets": 1,
     "numoutlets": 1,
     "patching_rect": [
      1020,
      170,
      107,
      22
     ],
     "outlettype": [
      ""
     ],
     "text": "prepend speed"
    }
   },
   {
    "box": {
     "id": "obj-40",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      1075,
      140,
      70,
      22
     ],
     "text": "speed"
    }
   },
   {
    "box": {
     "id": "obj-41",
     "maxclass": "newobj",
     "numinlets": 1,
     "numoutlets": 1,
     "patching_rect": [
      1120,
      110,
      86,
      22
     ],
     "outlettype": [
      ""
     ],
     "text": "loadmess 3"
    }
   },
   {
    "box": {
     "id": "obj-42",
     "maxclass": "number",
     "numinlets": 1,
     "numoutlets": 2,
     "patching_rect": [
      1120,
      140,
      50,
      22
     ],
     "outlettype": [
      "",
      "bang"
     ],
     "parameter_enable": 0
    }
   },
   {
    "box": {
     "id": "obj-43",
     "maxclass": "newobj",
     "numinlets": 1,
     "numoutlets": 1,
     "patching_rect": [
      1120,
      170,
      114,
      22
     ],
     "outlettype": [
      ""
     ],
     "text": "prepend bassch"
    }
   },
   {
    "box": {
     "id": "obj-44",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      1175,
      140,
      70,
      22
     ],
     "text": "bassch"
    }
   },
   {
    "box": {
     "id": "obj-45",
     "maxclass": "newobj",
     "numinlets": 1,
     "numoutlets": 6,
     "patching_rect": [
      20,
      300,
      600,
      22
     ],
     "outlettype": [
      "",
      "",
      "",
      "",
      "",
      ""
     ],
     "text": "js breaks.js"
    }
   },
   {
    "box": {
     "id": "obj-46",
     "maxclass": "message",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      640,
      300,
      40,
      22
     ],
     "outlettype": [
      ""
     ],
     "text": ""
    }
   },
   {
    "box": {
     "id": "obj-47",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      640,
      280,
      60,
      22
     ],
     "text": "state"
    }
   },
   {
    "box": {
     "id": "obj-48",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      20,
      350,
      120,
      22
     ],
     "text": "kick, sub"
    }
   },
   {
    "box": {
     "id": "obj-49",
     "maxclass": "newobj",
     "numinlets": 1,
     "numoutlets": 3,
     "patching_rect": [
      20,
      370,
      65,
      22
     ],
     "outlettype": [
      "float",
      "float",
      "float"
     ],
     "text": "t f f f"
    }
   },
   {
    "box": {
     "id": "obj-50",
     "maxclass": "message",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      20,
      400,
      79,
      22
     ],
     "outlettype": [
      ""
     ],
     "text": "$1, 0 260"
    }
   },
   {
    "box": {
     "id": "obj-51",
     "maxclass": "message",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      110,
      400,
      86,
      22
     ],
     "outlettype": [
      ""
     ],
     "text": "170, 42 60"
    }
   },
   {
    "box": {
     "id": "obj-52",
     "maxclass": "newobj",
     "numinlets": 2,
     "numoutlets": 2,
     "patching_rect": [
      20,
      430,
      51,
      22
     ],
     "outlettype": [
      "signal",
      "bang"
     ],
     "text": "line~"
    }
   },
   {
    "box": {
     "id": "obj-53",
     "maxclass": "newobj",
     "numinlets": 2,
     "numoutlets": 2,
     "patching_rect": [
      110,
      430,
      51,
      22
     ],
     "outlettype": [
      "signal",
      "bang"
     ],
     "text": "line~"
    }
   },
   {
    "box": {
     "id": "obj-54",
     "maxclass": "newobj",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      110,
      460,
      58,
      22
     ],
     "outlettype": [
      "signal"
     ],
     "text": "cycle~"
    }
   },
   {
    "box": {
     "id": "obj-55",
     "maxclass": "newobj",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      20,
      490,
      40,
      22
     ],
     "outlettype": [
      "signal"
     ],
     "text": "*~"
    }
   },
   {
    "box": {
     "id": "obj-56",
     "maxclass": "message",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      220,
      400,
      51,
      22
     ],
     "outlettype": [
      ""
     ],
     "text": "$1 30"
    }
   },
   {
    "box": {
     "id": "obj-57",
     "maxclass": "newobj",
     "numinlets": 2,
     "numoutlets": 2,
     "patching_rect": [
      220,
      430,
      51,
      22
     ],
     "outlettype": [
      "signal",
      "bang"
     ],
     "text": "line~"
    }
   },
   {
    "box": {
     "id": "obj-58",
     "maxclass": "newobj",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      220,
      460,
      58,
      22
     ],
     "outlettype": [
      "signal"
     ],
     "text": "cycle~"
    }
   },
   {
    "box": {
     "id": "obj-59",
     "maxclass": "message",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      300,
      400,
      79,
      22
     ],
     "outlettype": [
      ""
     ],
     "text": "$1, 0 420"
    }
   },
   {
    "box": {
     "id": "obj-60",
     "maxclass": "newobj",
     "numinlets": 2,
     "numoutlets": 2,
     "patching_rect": [
      300,
      430,
      51,
      22
     ],
     "outlettype": [
      "signal",
      "bang"
     ],
     "text": "line~"
    }
   },
   {
    "box": {
     "id": "obj-61",
     "maxclass": "newobj",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      220,
      490,
      40,
      22
     ],
     "outlettype": [
      "signal"
     ],
     "text": "*~"
    }
   },
   {
    "box": {
     "id": "obj-62",
     "maxclass": "newobj",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      220,
      520,
      58,
      22
     ],
     "outlettype": [
      "signal"
     ],
     "text": "*~ 0.5"
    }
   },
   {
    "box": {
     "id": "obj-63",
     "maxclass": "message",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      220,
      370,
      40,
      22
     ],
     "outlettype": [
      ""
     ],
     "text": "55"
    }
   },
   {
    "box": {
     "id": "obj-64",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      400,
      350,
      80,
      22
     ],
     "text": "snare"
    }
   },
   {
    "box": {
     "id": "obj-65",
     "maxclass": "newobj",
     "numinlets": 1,
     "numoutlets": 2,
     "patching_rect": [
      400,
      370,
      51,
      22
     ],
     "outlettype": [
      "float",
      "float"
     ],
     "text": "t f f"
    }
   },
   {
    "box": {
     "id": "obj-66",
     "maxclass": "message",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      400,
      400,
      79,
      22
     ],
     "outlettype": [
      ""
     ],
     "text": "$1, 0 150"
    }
   },
   {
    "box": {
     "id": "obj-67",
     "maxclass": "newobj",
     "numinlets": 2,
     "numoutlets": 2,
     "patching_rect": [
      400,
      430,
      51,
      22
     ],
     "outlettype": [
      "signal",
      "bang"
     ],
     "text": "line~"
    }
   },
   {
    "box": {
     "id": "obj-68",
     "maxclass": "newobj",
     "numinlets": 1,
     "numoutlets": 1,
     "patching_rect": [
      500,
      400,
      58,
      22
     ],
     "outlettype": [
      "signal"
     ],
     "text": "noise~"
    }
   },
   {
    "box": {
     "id": "obj-69",
     "maxclass": "newobj",
     "numinlets": 3,
     "numoutlets": 4,
     "patching_rect": [
      500,
      430,
      107,
      22
     ],
     "outlettype": [
      "signal",
      "signal",
      "signal",
      "signal"
     ],
     "text": "svf~ 1900 0.5"
    }
   },
   {
    "box": {
     "id": "obj-70",
     "maxclass": "newobj",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      400,
      490,
      40,
      22
     ],
     "outlettype": [
      "signal"
     ],
     "text": "*~"
    }
   },
   {
    "box": {
     "id": "obj-71",
     "maxclass": "message",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      600,
      400,
      72,
      22
     ],
     "outlettype": [
      ""
     ],
     "text": "$1, 0 70"
    }
   },
   {
    "box": {
     "id": "obj-72",
     "maxclass": "newobj",
     "numinlets": 2,
     "numoutlets": 2,
     "patching_rect": [
      600,
      430,
      51,
      22
     ],
     "outlettype": [
      "signal",
      "bang"
     ],
     "text": "line~"
    }
   },
   {
    "box": {
     "id": "obj-73",
     "maxclass": "newobj",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      680,
      430,
      86,
      22
     ],
     "outlettype": [
      "signal"
     ],
     "text": "cycle~ 185"
    }
   },
   {
    "box": {
     "id": "obj-74",
     "maxclass": "newobj",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      600,
      490,
      40,
      22
     ],
     "outlettype": [
      "signal"
     ],
     "text": "*~"
    }
   },
   {
    "box": {
     "id": "obj-75",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      780,
      350,
      80,
      22
     ],
     "text": "hats"
    }
   },
   {
    "box": {
     "id": "obj-76",
     "maxclass": "message",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      780,
      400,
      72,
      22
     ],
     "outlettype": [
      ""
     ],
     "text": "$1, 0 38"
    }
   },
   {
    "box": {
     "id": "obj-77",
     "maxclass": "newobj",
     "numinlets": 2,
     "numoutlets": 2,
     "patching_rect": [
      780,
      430,
      51,
      22
     ],
     "outlettype": [
      "signal",
      "bang"
     ],
     "text": "line~"
    }
   },
   {
    "box": {
     "id": "obj-78",
     "maxclass": "newobj",
     "numinlets": 1,
     "numoutlets": 1,
     "patching_rect": [
      880,
      400,
      58,
      22
     ],
     "outlettype": [
      "signal"
     ],
     "text": "noise~"
    }
   },
   {
    "box": {
     "id": "obj-79",
     "maxclass": "newobj",
     "numinlets": 3,
     "numoutlets": 4,
     "patching_rect": [
      880,
      430,
      107,
      22
     ],
     "outlettype": [
      "signal",
      "signal",
      "signal",
      "signal"
     ],
     "text": "svf~ 8500 0.3"
    }
   },
   {
    "box": {
     "id": "obj-80",
     "maxclass": "newobj",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      780,
      490,
      40,
      22
     ],
     "outlettype": [
      "signal"
     ],
     "text": "*~"
    }
   },
   {
    "box": {
     "id": "obj-81",
     "maxclass": "newobj",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      780,
      520,
      65,
      22
     ],
     "outlettype": [
      "signal"
     ],
     "text": "*~ 0.35"
    }
   },
   {
    "box": {
     "id": "obj-82",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      20,
      560,
      300,
      22
     ],
     "text": "the drums: grit (bits), drive, level"
    }
   },
   {
    "box": {
     "id": "obj-83",
     "maxclass": "newobj",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      20,
      580,
      58,
      22
     ],
     "outlettype": [
      "signal"
     ],
     "text": "*~ 0.8"
    }
   },
   {
    "box": {
     "id": "obj-84",
     "maxclass": "newobj",
     "numinlets": 3,
     "numoutlets": 1,
     "patching_rect": [
      20,
      610,
      107,
      22
     ],
     "outlettype": [
      "signal"
     ],
     "text": "degrade~ 1 12"
    }
   },
   {
    "box": {
     "id": "obj-85",
     "maxclass": "newobj",
     "numinlets": 1,
     "numoutlets": 1,
     "patching_rect": [
      140,
      580,
      93,
      22
     ],
     "outlettype": [
      ""
     ],
     "text": "loadmess 12"
    }
   },
   {
    "box": {
     "id": "obj-86",
     "maxclass": "number",
     "numinlets": 1,
     "numoutlets": 2,
     "patching_rect": [
      140,
      610,
      50,
      22
     ],
     "outlettype": [
      "",
      "bang"
     ],
     "parameter_enable": 0
    }
   },
   {
    "box": {
     "id": "obj-87",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      195,
      610,
      40,
      22
     ],
     "text": "bits"
    }
   },
   {
    "box": {
     "id": "obj-88",
     "maxclass": "newobj",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      20,
      640,
      114,
      22
     ],
     "outlettype": [
      "signal"
     ],
     "text": "overdrive~ 1.6"
    }
   },
   {
    "box": {
     "id": "obj-89",
     "maxclass": "newobj",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      20,
      670,
      58,
      22
     ],
     "outlettype": [
      "signal"
     ],
     "text": "*~ 0.8"
    }
   },
   {
    "box": {
     "id": "obj-90",
     "maxclass": "newobj",
     "numinlets": 1,
     "numoutlets": 1,
     "patching_rect": [
      140,
      650,
      100,
      22
     ],
     "outlettype": [
      ""
     ],
     "text": "loadmess 0.8"
    }
   },
   {
    "box": {
     "id": "obj-91",
     "maxclass": "flonum",
     "numinlets": 1,
     "numoutlets": 2,
     "patching_rect": [
      140,
      680,
      50,
      22
     ],
     "outlettype": [
      "",
      "bang"
     ],
     "format": 6,
     "parameter_enable": 0
    }
   },
   {
    "box": {
     "id": "obj-92",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      195,
      680,
      50,
      22
     ],
     "text": "drums"
    }
   },
   {
    "box": {
     "id": "obj-93",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      300,
      560,
      400,
      22
     ],
     "text": "the FM-1's sound (USB audio in): its pads, a dark wash"
    }
   },
   {
    "box": {
     "id": "obj-94",
     "maxclass": "newobj",
     "numinlets": 1,
     "numoutlets": 2,
     "patching_rect": [
      300,
      580,
      72,
      22
     ],
     "outlettype": [
      "signal",
      "signal"
     ],
     "text": "adc~ 1 2"
    }
   },
   {
    "box": {
     "id": "obj-95",
     "maxclass": "newobj",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      300,
      610,
      58,
      22
     ],
     "outlettype": [
      "signal"
     ],
     "text": "*~ 0.9"
    }
   },
   {
    "box": {
     "id": "obj-96",
     "maxclass": "newobj",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      370,
      610,
      58,
      22
     ],
     "outlettype": [
      "signal"
     ],
     "text": "*~ 0.9"
    }
   },
   {
    "box": {
     "id": "obj-97",
     "maxclass": "newobj",
     "numinlets": 1,
     "numoutlets": 1,
     "patching_rect": [
      450,
      580,
      100,
      22
     ],
     "outlettype": [
      ""
     ],
     "text": "loadmess 0.9"
    }
   },
   {
    "box": {
     "id": "obj-98",
     "maxclass": "flonum",
     "numinlets": 1,
     "numoutlets": 2,
     "patching_rect": [
      450,
      610,
      50,
      22
     ],
     "outlettype": [
      "",
      "bang"
     ],
     "format": 6,
     "parameter_enable": 0
    }
   },
   {
    "box": {
     "id": "obj-99",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      505,
      610,
      40,
      22
     ],
     "text": "pads"
    }
   },
   {
    "box": {
     "id": "obj-100",
     "maxclass": "newobj",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      300,
      640,
      58,
      22
     ],
     "outlettype": [
      "signal"
     ],
     "text": "*~ 0.5"
    }
   },
   {
    "box": {
     "id": "obj-101",
     "maxclass": "newobj",
     "numinlets": 1,
     "numoutlets": 1,
     "patching_rect": [
      300,
      670,
      93,
      22
     ],
     "outlettype": [
      "tapconnect"
     ],
     "text": "tapin~ 4000"
    }
   },
   {
    "box": {
     "id": "obj-102",
     "maxclass": "newobj",
     "numinlets": 2,
     "numoutlets": 2,
     "patching_rect": [
      300,
      700,
      128,
      22
     ],
     "outlettype": [
      "signal",
      "signal"
     ],
     "text": "tapout~ 680 1130"
    }
   },
   {
    "box": {
     "id": "obj-103",
     "maxclass": "newobj",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      420,
      700,
      65,
      22
     ],
     "outlettype": [
      "signal"
     ],
     "text": "*~ 0.45"
    }
   },
   {
    "box": {
     "id": "obj-104",
     "maxclass": "newobj",
     "numinlets": 3,
     "numoutlets": 1,
     "patching_rect": [
      300,
      730,
      121,
      22
     ],
     "outlettype": [
      "signal"
     ],
     "text": "lores~ 1400 0.2"
    }
   },
   {
    "box": {
     "id": "obj-105",
     "maxclass": "newobj",
     "numinlets": 3,
     "numoutlets": 1,
     "patching_rect": [
      420,
      730,
      121,
      22
     ],
     "outlettype": [
      "signal"
     ],
     "text": "lores~ 1400 0.2"
    }
   },
   {
    "box": {
     "id": "obj-106",
     "maxclass": "newobj",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      300,
      760,
      58,
      22
     ],
     "outlettype": [
      "signal"
     ],
     "text": "*~ 0.5"
    }
   },
   {
    "box": {
     "id": "obj-107",
     "maxclass": "newobj",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      420,
      760,
      58,
      22
     ],
     "outlettype": [
      "signal"
     ],
     "text": "*~ 0.5"
    }
   },
   {
    "box": {
     "id": "obj-108",
     "maxclass": "newobj",
     "numinlets": 1,
     "numoutlets": 1,
     "patching_rect": [
      520,
      730,
      100,
      22
     ],
     "outlettype": [
      ""
     ],
     "text": "loadmess 0.5"
    }
   },
   {
    "box": {
     "id": "obj-109",
     "maxclass": "flonum",
     "numinlets": 1,
     "numoutlets": 2,
     "patching_rect": [
      520,
      760,
      50,
      22
     ],
     "outlettype": [
      "",
      "bang"
     ],
     "format": 6,
     "parameter_enable": 0
    }
   },
   {
    "box": {
     "id": "obj-110",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      575,
      760,
      40,
      22
     ],
     "text": "wash"
    }
   },
   {
    "box": {
     "id": "obj-111",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      620,
      560,
      520,
      22
     ],
     "text": "stutter: the last 4 s of the FM-1 kept; breaks.js loops a slice of it in time"
    }
   },
   {
    "box": {
     "id": "obj-112",
     "maxclass": "newobj",
     "numinlets": 1,
     "numoutlets": 2,
     "patching_rect": [
      620,
      580,
      163,
      22
     ],
     "outlettype": [
      "float",
      "bang"
     ],
     "text": "buffer~ fm1rec 4000 1"
    }
   },
   {
    "box": {
     "id": "obj-113",
     "maxclass": "newobj",
     "numinlets": 3,
     "numoutlets": 1,
     "patching_rect": [
      620,
      640,
      184,
      22
     ],
     "outlettype": [
      "signal"
     ],
     "text": "record~ fm1rec 1 @loop 1"
    }
   },
   {
    "box": {
     "id": "obj-114",
     "maxclass": "message",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      620,
      610,
      40,
      22
     ],
     "outlettype": [
      ""
     ],
     "text": "1"
    }
   },
   {
    "box": {
     "id": "obj-115",
     "maxclass": "newobj",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      620,
      670,
      100,
      22
     ],
     "outlettype": [
      "float"
     ],
     "text": "snapshot~ 20"
    }
   },
   {
    "box": {
     "id": "obj-116",
     "maxclass": "newobj",
     "numinlets": 1,
     "numoutlets": 1,
     "patching_rect": [
      620,
      700,
      114,
      22
     ],
     "outlettype": [
      ""
     ],
     "text": "prepend recpos"
    }
   },
   {
    "box": {
     "id": "obj-117",
     "maxclass": "newobj",
     "numinlets": 1,
     "numoutlets": 3,
     "patching_rect": [
      760,
      610,
      142,
      22
     ],
     "outlettype": [
      "",
      "",
      ""
     ],
     "text": "route stut stutoff"
    }
   },
   {
    "box": {
     "id": "obj-118",
     "maxclass": "newobj",
     "numinlets": 1,
     "numoutlets": 1,
     "patching_rect": [
      760,
      640,
      121,
      22
     ],
     "outlettype": [
      ""
     ],
     "text": "prepend setloop"
    }
   },
   {
    "box": {
     "id": "obj-119",
     "maxclass": "newobj",
     "numinlets": 1,
     "numoutlets": 2,
     "patching_rect": [
      900,
      640,
      51,
      22
     ],
     "outlettype": [
      "bang",
      ""
     ],
     "text": "t b l"
    }
   },
   {
    "box": {
     "id": "obj-120",
     "maxclass": "message",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      900,
      670,
      79,
      22
     ],
     "outlettype": [
      ""
     ],
     "text": "startloop"
    }
   },
   {
    "box": {
     "id": "obj-121",
     "maxclass": "message",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      980,
      670,
      40,
      22
     ],
     "outlettype": [
      ""
     ],
     "text": "1 5"
    }
   },
   {
    "box": {
     "id": "obj-122",
     "maxclass": "message",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      1040,
      670,
      44,
      22
     ],
     "outlettype": [
      ""
     ],
     "text": "0 15"
    }
   },
   {
    "box": {
     "id": "obj-123",
     "maxclass": "newobj",
     "numinlets": 1,
     "numoutlets": 1,
     "patching_rect": [
      760,
      670,
      58,
      22
     ],
     "outlettype": [
      "signal"
     ],
     "text": "sig~ 1"
    }
   },
   {
    "box": {
     "id": "obj-124",
     "maxclass": "message",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      840,
      670,
      58,
      22
     ],
     "outlettype": [
      ""
     ],
     "text": "loop 1"
    }
   },
   {
    "box": {
     "id": "obj-125",
     "maxclass": "newobj",
     "numinlets": 3,
     "numoutlets": 2,
     "patching_rect": [
      760,
      710,
      128,
      22
     ],
     "outlettype": [
      "signal",
      "signal"
     ],
     "text": "groove~ fm1rec 1"
    }
   },
   {
    "box": {
     "id": "obj-126",
     "maxclass": "newobj",
     "numinlets": 2,
     "numoutlets": 2,
     "patching_rect": [
      980,
      710,
      51,
      22
     ],
     "outlettype": [
      "signal",
      "bang"
     ],
     "text": "line~"
    }
   },
   {
    "box": {
     "id": "obj-127",
     "maxclass": "newobj",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      760,
      740,
      40,
      22
     ],
     "outlettype": [
      "signal"
     ],
     "text": "*~"
    }
   },
   {
    "box": {
     "id": "obj-128",
     "maxclass": "newobj",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      760,
      770,
      58,
      22
     ],
     "outlettype": [
      "signal"
     ],
     "text": "*~ 0.9"
    }
   },
   {
    "box": {
     "id": "obj-129",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      20,
      820,
      300,
      22
     ],
     "text": "out (the speaker: audio on)"
    }
   },
   {
    "box": {
     "id": "obj-130",
     "maxclass": "newobj",
     "numinlets": 3,
     "numoutlets": 1,
     "patching_rect": [
      20,
      850,
      86,
      22
     ],
     "outlettype": [
      "signal"
     ],
     "text": "clip~ -1 1"
    }
   },
   {
    "box": {
     "id": "obj-131",
     "maxclass": "newobj",
     "numinlets": 3,
     "numoutlets": 1,
     "patching_rect": [
      160,
      850,
      86,
      22
     ],
     "outlettype": [
      "signal"
     ],
     "text": "clip~ -1 1"
    }
   },
   {
    "box": {
     "id": "obj-132",
     "maxclass": "ezdac~",
     "numinlets": 2,
     "numoutlets": 0,
     "patching_rect": [
      20,
      890,
      45,
      45
     ]
    }
   }
  ],
  "lines": [
   {
    "patchline": {
     "source": [
      "obj-3",
      0
     ],
     "destination": [
      "obj-4",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-4",
      0
     ],
     "destination": [
      "obj-5",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-5",
      1
     ],
     "destination": [
      "obj-6",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-5",
      1
     ],
     "destination": [
      "obj-7",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-6",
      0
     ],
     "destination": [
      "obj-8",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-7",
      0
     ],
     "destination": [
      "obj-9",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-11",
      0
     ],
     "destination": [
      "obj-15",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-11",
      0
     ],
     "destination": [
      "obj-17",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-12",
      0
     ],
     "destination": [
      "obj-13",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-13",
      0
     ],
     "destination": [
      "obj-14",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-14",
      0
     ],
     "destination": [
      "obj-15",
      1
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-15",
      0
     ],
     "destination": [
      "obj-16",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-17",
      0
     ],
     "destination": [
      "obj-18",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-17",
      1
     ],
     "destination": [
      "obj-19",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-21",
      0
     ],
     "destination": [
      "obj-22",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-22",
      0
     ],
     "destination": [
      "obj-23",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-25",
      0
     ],
     "destination": [
      "obj-26",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-26",
      0
     ],
     "destination": [
      "obj-27",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-29",
      0
     ],
     "destination": [
      "obj-30",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-30",
      0
     ],
     "destination": [
      "obj-31",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-33",
      0
     ],
     "destination": [
      "obj-34",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-34",
      0
     ],
     "destination": [
      "obj-35",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-37",
      0
     ],
     "destination": [
      "obj-38",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-38",
      0
     ],
     "destination": [
      "obj-39",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-41",
      0
     ],
     "destination": [
      "obj-42",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-42",
      0
     ],
     "destination": [
      "obj-43",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-8",
      0
     ],
     "destination": [
      "obj-45",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-9",
      0
     ],
     "destination": [
      "obj-45",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-16",
      0
     ],
     "destination": [
      "obj-45",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-18",
      0
     ],
     "destination": [
      "obj-45",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-19",
      0
     ],
     "destination": [
      "obj-45",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-23",
      0
     ],
     "destination": [
      "obj-45",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-27",
      0
     ],
     "destination": [
      "obj-45",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-31",
      0
     ],
     "destination": [
      "obj-45",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-35",
      0
     ],
     "destination": [
      "obj-45",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-39",
      0
     ],
     "destination": [
      "obj-45",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-43",
      0
     ],
     "destination": [
      "obj-45",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-45",
      5
     ],
     "destination": [
      "obj-46",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-45",
      0
     ],
     "destination": [
      "obj-49",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-49",
      2
     ],
     "destination": [
      "obj-51",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-49",
      0
     ],
     "destination": [
      "obj-50",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-50",
      0
     ],
     "destination": [
      "obj-52",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-51",
      0
     ],
     "destination": [
      "obj-53",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-53",
      0
     ],
     "destination": [
      "obj-54",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-52",
      0
     ],
     "destination": [
      "obj-55",
      1
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-54",
      0
     ],
     "destination": [
      "obj-55",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-45",
      4
     ],
     "destination": [
      "obj-56",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-56",
      0
     ],
     "destination": [
      "obj-57",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-57",
      0
     ],
     "destination": [
      "obj-58",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-49",
      1
     ],
     "destination": [
      "obj-59",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-59",
      0
     ],
     "destination": [
      "obj-60",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-58",
      0
     ],
     "destination": [
      "obj-61",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-60",
      0
     ],
     "destination": [
      "obj-61",
      1
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-61",
      0
     ],
     "destination": [
      "obj-62",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-3",
      0
     ],
     "destination": [
      "obj-63",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-63",
      0
     ],
     "destination": [
      "obj-56",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-45",
      1
     ],
     "destination": [
      "obj-65",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-65",
      0
     ],
     "destination": [
      "obj-66",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-65",
      1
     ],
     "destination": [
      "obj-71",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-66",
      0
     ],
     "destination": [
      "obj-67",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-68",
      0
     ],
     "destination": [
      "obj-69",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-69",
      2
     ],
     "destination": [
      "obj-70",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-67",
      0
     ],
     "destination": [
      "obj-70",
      1
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-71",
      0
     ],
     "destination": [
      "obj-72",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-73",
      0
     ],
     "destination": [
      "obj-74",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-72",
      0
     ],
     "destination": [
      "obj-74",
      1
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-45",
      2
     ],
     "destination": [
      "obj-76",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-76",
      0
     ],
     "destination": [
      "obj-77",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-78",
      0
     ],
     "destination": [
      "obj-79",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-79",
      1
     ],
     "destination": [
      "obj-80",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-77",
      0
     ],
     "destination": [
      "obj-80",
      1
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-80",
      0
     ],
     "destination": [
      "obj-81",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-55",
      0
     ],
     "destination": [
      "obj-83",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-62",
      0
     ],
     "destination": [
      "obj-83",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-70",
      0
     ],
     "destination": [
      "obj-83",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-74",
      0
     ],
     "destination": [
      "obj-83",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-81",
      0
     ],
     "destination": [
      "obj-83",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-83",
      0
     ],
     "destination": [
      "obj-84",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-85",
      0
     ],
     "destination": [
      "obj-86",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-86",
      0
     ],
     "destination": [
      "obj-84",
      2
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-84",
      0
     ],
     "destination": [
      "obj-88",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-88",
      0
     ],
     "destination": [
      "obj-89",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-90",
      0
     ],
     "destination": [
      "obj-91",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-91",
      0
     ],
     "destination": [
      "obj-89",
      1
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-94",
      0
     ],
     "destination": [
      "obj-95",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-94",
      1
     ],
     "destination": [
      "obj-96",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-97",
      0
     ],
     "destination": [
      "obj-98",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-98",
      0
     ],
     "destination": [
      "obj-95",
      1
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-98",
      0
     ],
     "destination": [
      "obj-96",
      1
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-95",
      0
     ],
     "destination": [
      "obj-100",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-96",
      0
     ],
     "destination": [
      "obj-100",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-100",
      0
     ],
     "destination": [
      "obj-101",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-101",
      0
     ],
     "destination": [
      "obj-102",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-102",
      1
     ],
     "destination": [
      "obj-103",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-103",
      0
     ],
     "destination": [
      "obj-101",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-102",
      0
     ],
     "destination": [
      "obj-104",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-102",
      1
     ],
     "destination": [
      "obj-105",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-104",
      0
     ],
     "destination": [
      "obj-106",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-105",
      0
     ],
     "destination": [
      "obj-107",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-108",
      0
     ],
     "destination": [
      "obj-109",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-109",
      0
     ],
     "destination": [
      "obj-106",
      1
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-109",
      0
     ],
     "destination": [
      "obj-107",
      1
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-3",
      0
     ],
     "destination": [
      "obj-114",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-114",
      0
     ],
     "destination": [
      "obj-113",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-100",
      0
     ],
     "destination": [
      "obj-113",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-113",
      0
     ],
     "destination": [
      "obj-115",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-115",
      0
     ],
     "destination": [
      "obj-116",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-116",
      0
     ],
     "destination": [
      "obj-45",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-45",
      3
     ],
     "destination": [
      "obj-117",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-117",
      0
     ],
     "destination": [
      "obj-119",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-119",
      1
     ],
     "destination": [
      "obj-118",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-118",
      0
     ],
     "destination": [
      "obj-125",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-119",
      0
     ],
     "destination": [
      "obj-120",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-119",
      0
     ],
     "destination": [
      "obj-121",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-120",
      0
     ],
     "destination": [
      "obj-125",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-117",
      1
     ],
     "destination": [
      "obj-122",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-121",
      0
     ],
     "destination": [
      "obj-126",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-122",
      0
     ],
     "destination": [
      "obj-126",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-3",
      0
     ],
     "destination": [
      "obj-124",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-124",
      0
     ],
     "destination": [
      "obj-125",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-123",
      0
     ],
     "destination": [
      "obj-125",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-125",
      0
     ],
     "destination": [
      "obj-127",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-126",
      0
     ],
     "destination": [
      "obj-127",
      1
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-127",
      0
     ],
     "destination": [
      "obj-128",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-89",
      0
     ],
     "destination": [
      "obj-130",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-95",
      0
     ],
     "destination": [
      "obj-130",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-106",
      0
     ],
     "destination": [
      "obj-130",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-128",
      0
     ],
     "destination": [
      "obj-130",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-89",
      0
     ],
     "destination": [
      "obj-131",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-96",
      0
     ],
     "destination": [
      "obj-131",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-107",
      0
     ],
     "destination": [
      "obj-131",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-128",
      0
     ],
     "destination": [
      "obj-131",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-130",
      0
     ],
     "destination": [
      "obj-132",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-131",
      0
     ],
     "destination": [
      "obj-132",
      1
     ]
    }
   }
  ]
 }
}