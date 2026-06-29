# Changelog

All notable changes to Nixie. Versions follow the `CMakeLists.txt` project version
and the git tags (`vX.Y.Z`).

## [0.5.0] - 2026-06-29

### Changed
- **Renamed LM-1 → Nixie** for the public release (still inspired by the classic Linn
  LM-1). New product name, faceplate, on-screen logo, and docs throughout. New AU/VST
  identity — `PLUGIN_CODE` `Nix1`, manufacturer code `Coff`, bundle id
  `com.anthonycoffey.nixie` — and the preset extension is now **`.nixiepreset`**.

### Breaking
- Hosts see Nixie as a **brand-new plugin**: existing LM-1 project/session recall does
  not carry over, and old `.lm1preset` files (and the `~/Music/LM-1/Presets` folder)
  are not read. Re-insert the plugin and re-save your presets.

## [0.4.2] - 2026-06-24

### Fixed
- **Mono multi-out tracks.** The direct outs are now declared **mono** (they carry
  mono drum voices) — that's what makes LUNA offer mono multi-out tracks; declaring
  them stereo made LUNA treat the plugin as stereo-only. Bus validation accepts mono,
  stereo, or disabled per direct out, and a voice routed to a mono out is written at
  unity (pan is meaningless on one channel, so no +3 dB center bump).

## [0.4.1] - 2026-06-23

### Added
- **Time grid:** per-pattern **time signature** (2/4, 3/4, 4/4, 5/8, 6/8, 7/8, 9/8,
  12/8) and **step rate** (1/4, 1/8, 1/16, 1/8T, 1/16T). Step count is derived from
  meter × rate; the grid auto-sizes and draws meter-aware beat-group dividers.
  Triplet rates give a real shuffle / compound-time feel.
- **Meter + Rate selectors** in the transport bar (replacing the fixed 8/16/32 Steps
  picker).
- **12 banks** (was 10): banks 1–10 are factory genre banks, **11–12 are user** slots.
- **100 factory grooves** across 10 genres (80s Pop, 80s Dance/New Wave, Funk,
  R&B/Gospel, Hip-Hop, Neo-Soul, Reggae/Latin, Rock, Blues, Folk), re-authored to
  honor each name (shuffles on 1/8T, slow blues/gospel/ballads in 12/8, Two-Beat in
  2/4, waltzy Songwriter in 3/4, etc.).
- Bank LED **"GENRE - pattern name"** readout.

### Changed
- Duple swing is bypassed on triplet grids (the grid already carries the feel).
- `Pattern` max steps 32 → 48 (with migration of legacy 32-step saves).

## [0.4.0] - 2026-06-23

### Added
- **Per-voice multi-output:** a stereo **Main** bus plus **12 direct outs**, with a
  per-channel **Output** selector (Main / Out 1–12). Route a voice — or a group of
  voices sharing an out — to its own track in LUNA for independent processing.

## [0.3.0] - 2026-06-23

### Added
- **Real-time record** (REC): arm and play MIDI/pads to capture onto the grid, with
  nearest-step quantize.
- **Musical shuffle**: global + per-track swing (Straight / Light / Medium / Triplet /
  Hard, per-track Follow).
- **Preset library**: 10 banks × 10 slots with bank navigation; factory grooves +
  user-saveable banks persisted to disk.
- Preset persistence across editor reopen / project reload; `.nixiepreset` save/load.

### Changed
- Major UI overhaul: vintage faceplate, LED readouts, custom flat knobs/faders/
  buttons, step-grid LEDs + step numbers, larger knobs, labels above controls.

## [0.2.0] - 2026-06-23

First tagged release — a playable AU/VST3/Standalone Nixie: 12 voices, per-voice
sampling, host-synced step sequencer + grid, per-voice mixer, MIDI export
(file + drag-out), and global Master / Lo-Fi / Tune.

[0.5.0]: https://github.com/anthonycoffey/nixie/releases/tag/v0.5.0
[0.4.2]: https://github.com/anthonycoffey/nixie/releases/tag/v0.4.2
[0.4.1]: https://github.com/anthonycoffey/nixie/releases/tag/v0.4.1
[0.4.0]: https://github.com/anthonycoffey/nixie/releases/tag/v0.4.0
[0.3.0]: https://github.com/anthonycoffey/nixie/releases/tag/v0.3.0
[0.2.0]: https://github.com/anthonycoffey/nixie/releases/tag/v0.2.0
