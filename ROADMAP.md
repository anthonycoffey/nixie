# LM-1 — Roadmap

**What we're building:** a **modern drum-machine instrument plugin** (AU/VST3/Standalone)
that _sounds and looks like_ the Linn LM-1, but is built for a **modern DAW workflow** — not
a museum recreation of the 1980 hardware. The product is a **step sequencer + sampler**: 12
authentic LM-1 voices, per-voice sampling and mixing, a step grid you program with the
mouse, host-tempo sync, and — importantly — **MIDI export** so you can drag programmed beats
straight into your DAW.

Priorities, in order: **sequencer & drum programming → MIDI export → editable samples &
presets → the 12-voice mixer → vintage character/look (charm).** Where the real LM-1 had a
quirk that doesn't help a modern workflow, we adapt or omit it (see
[Adapted / omitted for a digital VST](#adapted--omitted-for-a-digital-vst)).

Legend: ✅ done · ⬜ to build. **Stages A–E are complete (a fully playable, host-synced
step sequencer with MIDI export); Stage F (patterns/presets) is the next milestone.**

---

## Where the prototype stands today

A playable, host-synced drum sequencer that loads in LUNA, with **Stages A–E complete**:

- ✅ AU + VST3 + Standalone from one CMake/JUCE 8 project; passes `auval` (11k–192k).
- ✅ **12 authentic LM-1 voices** on a reference-counted `DrumKit` (lock-free swap).
- ✅ **Per-voice sample loading** (WAV/AIFF) from disk; optional bundled factory kit via
  binary data; procedural sounds remain as a silent-fail fallback.
- ✅ **Per-voice mixer**: level / pan / tune / mute / solo, plus a 12-strip editor UI.
- ✅ **Step sequencer**: host-synced + internal clock, sample-accurate; **step-grid UI**
  (click/drag/wheel-velocity, sweeping playhead, 8/16/32 length).
- ✅ **MIDI export**: "Export MIDI" file save + drag-pattern-to-DAW handle.
- ✅ Global Master / Lo-Fi / Tune; per-voice tune + trim; kit + mixer + pattern persist.
- ❌ No multi-pattern slots / preset manager yet (Stage F). ❌ Minimal styling (Stage G).
  ❌ No preset manager (kit/params persist, but factory/user presets are Stage F).

The path below carries that foundation to the full product.

---

## The instrument we're emulating (spec reference)

The Linn LM-1 (Roger Linn, 1980) — the first drum machine to use digital samples. Front
panel, left→right:

- **Mixer:** 12 instrument channels (+ a metronome _click_ we omit), each with a **volume
  fader** and a **3-position pan switch** (L / C / R). The 12 instruments:
  **Bass (kick), Snare, Hi-Hat, Cabasa, Tambourine, Tom Lo, Tom Hi, Conga Lo, Conga Hi,
  Cowbell, Clave, Clap** — famously **no crash cymbal**.
- **Performance pads:** 12 momentary buttons (one per instrument; not velocity-sensitive on
  the hardware — we _do_ support velocity, a modern convenience).
- **Sequencer/transport:** 2-digit 7-segment LED (pattern # 00–99 / tempo), numeric keypad
  0–9, Play/Stop, Record, Tempo knob, Timing Correct (quantize), Shuffle (swing), Erase.
- **Tuning:** 12 per-voice tuning pots (on the _rear_ panel) — pitch by playback-rate change.
- **Sound:** 8-bit PCM, ~28 kHz, AM6070 companding DAC → the signature lo-fi grit.
- **Styling:** black faceplate, white engraved type, black fader caps.

**Voice → GM-note map we use** (collision-free, so MIDI export & a pad controller both work):

| #   | Voice       | Note | #   | Voice    | Note |
| --- | ----------- | ---- | --- | -------- | ---- |
| 0   | Bass (Kick) | 36   | 6   | Tom Hi   | 48   |
| 1   | Snare       | 38   | 7   | Conga Lo | 64   |
| 2   | Hi-Hat      | 42   | 8   | Conga Hi | 63   |
| 3   | Cabasa      | 69   | 9   | Cowbell  | 56   |
| 4   | Tambourine  | 54   | 10  | Clave    | 75   |
| 5   | Tom Lo      | 45   | 11  | Clap     | 39   |

> The deeper architecture rationale lives in
> [LM-1_Clone_Design_and_Plan.md](LM-1_Clone_Design_and_Plan.md). This file is the actionable,
> ordered plan.

---

## Stages

### ✅ Stage A — Authentic 12 voices + sample loading _(done)_

**Goal:** replace the generic placeholders with the 12 real instruments and make samples
loadable & editable — the "samples aren't usable" fix.

- Expand the voice table 8 → **12 authentic instruments** (map above). The 4 genuinely new
  ones (Cabasa, Tambourine, Conga Lo/Hi) and Clave use procedural placeholders until real
  WAVs are dropped in.
- New `DrumKit` (a `juce::ReferenceCountedObject`) holding 12 voice samples; a message-thread
  `KitLoader` decodes WAV/AIFF via `AudioFormatManager`.
- **Lock-free kit swap:** build a new kit off-thread, publish via an atomic pointer; the audio
  thread takes one snapshot per block. Each playing voice holds a `DrumKit::Ptr` so a note in
  flight can never be freed mid-sound.
- Per-voice **start/end trim** (kit data) so user samples with pre-roll still land on the grid.
- Bundle the user-supplied factory kit via `juce_add_binary_data` (self-contained; survives
  the AU sandbox). Procedural `DrumSynth` stays as a silent-fail fallback per voice.

**Done when:** the 12 pads play real samples, you can load your own WAV per voice, Tune still
pitches, and `auval` stays green.

### ✅ Stage B — Per-voice mixer _(done)_

**Goal:** the LM-1 mixer section — balance and shape each voice.

- Loop-generate APVTS params per voice: **level, pan, tune, mute, solo** (`v0_level` …).
- Read them in `processBlock`; compute a solo mask once per block; pass real level/pan into
  the render and fold per-voice + global tune at trigger.
- Continuous pan (modern) with a center detent (nod to the 3-position switch).

**Done when:** you can mute the hat, pan the toms, detune the kick, and solo a voice — and the
settings persist across reopen.

### ✅ Stage C — Sequencer engine _(done)_

**Goal:** an internal step pattern that plays in time with the host (the heart of the product).

- `Pattern` = 12 lanes × up to 32 steps, per-step velocity (0 = off), default 16 steps.
- RT-safe hand-off (editor → audio) via a **double-buffer + atomic index swap**.
- Read the host playhead (ppq/bpm/playing), convert ppq → step, fire **sample-accurately**
  inside the block (monotonic step counter; reset on transport jumps).
- **Internal clock** (Play/Stop + tempo) drives Standalone and a stopped host; **host
  transport wins when actively playing** so bounces lock to the grid.
- Live MIDI/pad play keeps working alongside the sequencer.

**Done when:** a programmed beat plays locked to the DAW tempo and survives a bounce.

### ✅ Stage D — Step-grid UI + transport _(done)_

**Goal:** the classic grid you actually program on.

- `StepGridComponent`: 12 lanes × N steps; click to toggle; drag for per-step velocity.
- Playhead highlight polled from an atomic via a `Timer`; transport, tempo readout, length.

**Done when:** you click out a beat in the window and hear it with the playhead sweeping.

### ✅ Stage E — MIDI export _(done — headline modern feature)_

**Goal:** get patterns out of the plugin and into the DAW.

- One `juce::MidiFile` builder (lane → GM note, per-step velocity, tempo/time-sig meta).
- **"Export MIDI…"** via async `FileChooser`, **and** **drag-to-DAW**: drag a handle out of
  the window to drop a `.mid` clip onto the timeline
  (`DragAndDropContainer::performExternalDragDropOfFiles`, unique temp file).

**Done when:** dragging the handle drops a correctly-timed, playable MIDI clip in the DAW.

### ✅ Stage F — Patterns, presets & persistence _(done)_

- 8 in-project pattern slots (being superseded by the bank library, Stage I).
- Kit (sample sources + trim) + mixer + patterns serialize into plugin state.
- `PresetManager`: save/load full state tree as `.lm1preset` via the gear menu.

### ✅ Stage G — Character & styling _(done)_

Wood side cheeks, black faceplate with `#fc5824` section frames + labels, custom
vintage knobs/faders/buttons, red 7-segment LEDs (Step/Tempo/Pattern), open/closed
hi-hat, and a **Shuffle** swing control.

### 🔨 Stage I — Pattern banks & preset library _(building next)_

**Goal:** a browsable groove library on the panel — the LM-1's pattern-select, scaled up.

- **Model:** 100 banks × 8 slots = 800 preset slots. A **Bank** LED (1–100) with
  prev/next; the 8 slot buttons load the selected bank's slot into the working
  sequence (and set its tempo). Replaces the in-project A/B pattern slots.
- **Preloaded:** banks 1–10 ship with **80 factory grooves** generated across styles
  + tempos (rock, funk, hip-hop, house, disco, latin, pop, electro, breakbeat, ballad).

**Deferred (per request — noted here):**
- **Full-state presets:** slots load pattern + tempo only; capturing mixer/kit/tuning
  per slot is deferred (the gear-menu `.lm1preset` files already save the whole setup).
- **User save-to-slot + persistence** for banks 11–100 (user-writable library).

**Done when:** sweep banks, tap a slot, and a styled groove drops into the grid in time.

### ⬜ Stage H — Robustness & distribution

- `pluginval --strictness-level 10` clean; real-time-safety audit of `processBlock`.
- Optional AU multi-out (individual voice buses) as an isolated, well-tested change.
- Code-sign (Developer ID) + notarize + staple so it loads on other Macs.

**Done when:** pluginval passes and a friend can install it without Gatekeeper workarounds.

---

## Adapted / omitted for a digital VST

The LM-1 features that are iconic but don't earn their keep in a modern plugin — deliberately
dropped or reinterpreted (per project direction):

- **Timing Correct (quantize):** the step grid is inherently quantized, so this is moot for
  grid input. (A real-time-record quantize could return with Stage G's optional record mode.)
- **Shuffle / swing:** the `Pattern` keeps a swing field, but we don't engineer the feel now.
- **Song chaining:** replaced by simple modern pattern slots (A/B); full song mode is optional.
- **Numeric keypad + 2-digit LED data entry:** replaced by direct mouse/grid editing. The LED
  may return as cosmetic charm in Stage G.
- **Rear-panel tuning pots:** just per-voice Tune knobs in the mixer.
- **Metronome click channel:** omitted — the DAW provides a click.
- **Non-velocity-sensitive pads:** we _add_ velocity (MIDI + per-step accent).

---

## Working tips

- **Iterate in the Standalone build** (`build/LM_One_artefacts/Release/Standalone/`) — far
  faster than reopening LUNA. Only open LUNA to confirm host integration at the end of a stage.
- **Keep `auval` green** after every stage (`auval -v aumu Lm01 Ynme`); add `pluginval` later.
- **Never touch the audio thread carelessly:** no `new`/`delete`, locks, file, or GUI calls in
  `processBlock`. Pass data in via atomics / the reference-counted kit swap.
- **Checkpoint per stage** so you can always return to a building state. _(This tree isn't a
  git repo yet — consider `git init` so stage checkpoints are real commits.)_

---

## Effort sketch (part-time)

| Stage | What you get                  | Rough effort       |
| ----: | ----------------------------- | ------------------ |
|     A | 12 voices + sample loading    | 2–4 days           |
|     B | Per-voice mixer               | 1–2 days           |
|     C | Sequencer engine              | 4–7 days ← biggest |
|     D | Step-grid UI                  | 3–6 days           |
|     E | MIDI export (file + drag-out) | 1–3 days           |
|     F | Patterns / presets / save     | 3–5 days           |
|     G | Character & styling           | 2–4 days           |
|     H | pluginval / multi-out / sign  | 2–4 days           |

**"Usable modern LM-1 sequencer" = Stages A–E.** F–H turn it into something you'd release.
