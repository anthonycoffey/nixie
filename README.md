# nixie

A software drum machine modeled on the **Linn LM-1**, built with JUCE 8 and
targeting **Audio Units (AU)** so it runs natively in **Universal Audio LUNA** on
macOS — plus **VST3** and a **Standalone** app from the same codebase. Universal
binary (Apple Silicon + Intel).

It's a complete **step-sequencer + sampler instrument**: 12 authentic LM-1 voices,
a programmable step grid with real time signatures and triplet/shuffle subdivisions,
a 100-pattern genre groove library, a per-voice mixer with per-channel multi-out,
the signature lo-fi character, and MIDI drag-out into your DAW.

<img width="1010" height="763" alt="image" src="https://github.com/user-attachments/assets/683ad823-2117-40ca-90ef-0cc6840549f3" />


## Features

- **Formats:** AU + VST3 + Standalone from one CMake/JUCE project; passes `auval`.
- **12 LM-1 voices** — Bass, Snare, Hi-Hat (+ choked Open Hat), Cabasa, Tambourine,
  Tom Lo/Hi, Conga Lo/Hi, Cowbell, Clave, Clap (famously **no crash cymbal**).
  Play them from MIDI (GM drum map) or the on-screen pads.
- **Per-voice sampling** — load your own WAV/AIFF per voice, or restore the factory
  sound. Reference-counted kit with a lock-free audio-thread swap.
- **Step sequencer** — host-synced + internal clock, sample-accurate. **Meter**
  selector (2/4, 3/4, 4/4, 5/8, 6/8, 7/8, 9/8, 12/8) and **step rate** (1/4, 1/8,
  1/16, 1/8T, 1/16T); the grid auto-sizes from meter × rate and draws beat groups.
  Triplet rates give real shuffle / compound feel.
- **Step grid** — click/drag to program, mouse-wheel for per-step velocity, sweeping
  playhead, meter-aware beat dividers.
- **Real-time record** — arm REC and play MIDI/pads to capture onto the grid
  (nearest-step quantize).
- **Shuffle** — global + per-track musical swing (Straight / Light / Medium /
  Triplet / Hard, with per-track Follow).
- **Groove library** — 12 banks × 10 slots. Banks 1–10 ship **100 genre grooves**
  (80s Pop, 80s Dance/New Wave, Funk, R&B/Gospel, Hip-Hop, Neo-Soul, Reggae/Latin,
  Rock, Blues, Folk); banks 11–12 are user-saveable (persisted to disk). The LED
  reads "GENRE - pattern name".
- **12-channel mixer** — level, pan, tune, mute, solo per voice, plus a per-channel
  **output** selector for multi-out.
- **Multi-out** — a stereo Main bus plus 12 direct outs, so you can process a single
  voice (or a group) on its own track in LUNA. Several channels can share an out.
- **MIDI export** — "Export MIDI…" to a file, **and** drag the pattern straight onto
  the DAW timeline.
- **Character** — global Master / Lo-Fi (bit-crush + sample-rate reduction) / Tune;
  vintage wood-cheek faceplate, LED readouts, custom knobs/faders/buttons.
- State (params, kit, patterns, banks) saves/restores with the host project.

## Prerequisites (macOS)

1. **Xcode** (full install, from the App Store) + command-line tools:
   ```bash
   xcode-select --install
   ```
2. **CMake 3.22+**:
   ```bash
   brew install cmake
   ```
   (Install Homebrew first from https://brew.sh if you don't have it.)

JUCE is pulled automatically by CMake (`FetchContent`) — no separate download. The
first configure clones JUCE, so it needs internet and takes a few minutes once.

## Build

```bash
# Configure (generates an Xcode-backed build in ./build). Re-run after CMakeLists
# changes (e.g. a version bump) or adding a source file.
cmake -B build -G Xcode

# Build the plugin (Release)
cmake --build build --config Release
```

`COPY_PLUGIN_AFTER_BUILD TRUE` installs the plugin into your user folders on a
successful build:

- AU: `~/Library/Audio/Plug-Ins/Components/LM-1.component`
- VST3: `~/Library/Audio/Plug-Ins/VST3/LM-1.vst3`

## Validate the AU (do this before opening LUNA)

```bash
auval -v aumu Lm01 Ynme
```

`aumu` = music device (instrument); `Lm01` / `Ynme` are the `PLUGIN_CODE` and
`PLUGIN_MANUFACTURER_CODE` from `CMakeLists.txt`. A clean pass means hosts will load it.

## Run it

- **Fastest iteration:** the **Standalone** app
  (`build/LM_One_artefacts/Release/Standalone/`). Fully quit + relaunch it to pick
  up a rebuild (it caches).
- **In LUNA:** add an Instrument track and insert **LM-1**. For multi-out, set
  channels to **Out 1 / Out 2 / …** in the mixer, then add those outputs in LUNA's
  multi-out mixer (or right-click the track → Add Multi-Output) and process them.

## Project layout

```
LM-1/
├── CMakeLists.txt              # build config: pulls JUCE, defines the plugin + buses
├── README.md                   # this file
├── ROADMAP.md                  # build history + possible future work
├── LM-1_Clone_Design_and_Plan.md  # original architecture/design reference
├── CHANGELOG.md                # release notes per version
├── assets/factory_kit/         # drop 12 WAVs here to bundle a factory kit
└── src/
    ├── PluginProcessor.*       # engine: MIDI, params, sequencer host-sync, lo-fi,
    │                           #   banks, multi-out routing
    ├── PluginEditor.*          # full UI: mixer strips, step grid, transport, banks
    ├── DrumKit.*               # reference-counted 12-voice kit + sample loading
    ├── DrumVoice.h             # one variable-rate sample voice
    ├── DrumSynth.h             # procedural fallback sounds (when no WAV is present)
    ├── Pattern.h               # step pattern + time-grid (meter / rate / steps)
    ├── Sequencer.h             # sample-accurate step clock
    ├── FactoryGrooves.h        # the 100 genre grooves
    ├── VoiceStripComponent.*   # one mixer strip (pad, sample slot, fader, knobs, out)
    ├── StepGridComponent.*     # the step grid
    ├── RetroWidgets.h          # LED text, step arrows, X button
    ├── LMOneLookAndFeel.h      # vintage knobs/faders/buttons
    ├── LedDisplay.h            # 7-segment LED readouts
    ├── TransportButton.h       # flat Play / Rec buttons with LEDs
    ├── MidiDragSource.h        # drag-pattern-to-DAW handle
    ├── MidiExport.h            # pattern → .mid builder
    └── PresetManager.h         # full-state .lm1preset save/load
```

## Before sharing the plugin

To load on other Macs without Gatekeeper warnings the plugin must be **code-signed
(Developer ID) and notarized** (requires an Apple Developer account, $99/yr). For
your own machine during development you can skip this.
