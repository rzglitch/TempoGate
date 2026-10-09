# TempoGate - MIDI Performance Workspace

Record at your tempo. Shape your bars. Drag your performance into your project.

TempoGate is a MIDI performance environment (AU MIDI FX / VST3 / Standalone,
built with JUCE 9) that unifies four workflows:

1. **Independent Tempo MIDI Recording** - play at the source tempo while the
   project runs at its own tempo ( host tempo optionally followed).
2. **Tempo Conversion** - musical positions are kept in beats; the exported
   MIDI stamps the project tempo so notes land where the performer intended.
3. **MIDI-Triggered Bar Gate** - the next bar starts on your configured MIDI
   trigger (Any Note-On / C4 / velocity / sustain pedal). Waiting time is
   excluded from the take, so exported bars concatenate without gaps.
4. **MIDI Workspace + Drag & Drop Export** - Original / Converted / Compare
   piano-roll views, bar-range selection, OS drag & drop of a freshly rendered
   `.mid` onto a DAW track, one-click Commit (full converted performance as
   MIDI output), and Save `.mid` - all sharing one export renderer.

Plus: a **MIDI click metronome** at the source tempo (GM wood-block,
accent/beat notes + channel configurable) with optional **count-in**
(Off / 1 Bar / 2 Bars).

## Formats & host notes

| Format | Classification | Host usage |
|---|---|---|
| AU (`aumi`, MIDI Processor) | MIDI FX | Logic Pro MIDI FX slot |
| VST3 (`Instrument \| Tools`) | VSTi | Cubase/REAPER instrument track (Cubase requires the Instrument tag to route MIDI input - hence the VSTi classification) |
| Standalone | App | Live playing / testing without a DAW |

- **Logic Pro**: load as a MIDI FX. MIDI passes through (thru), recording and
  the workspace run inside the plug-in window.
- **Cubase**: create an instrument track with TempoGate. To hear its MIDI
  output (thru / Commit / click) on a real instrument, set the destination
  track's MIDI Input to the TempoGate track's MIDI output.
- Some DAWs, such as Ableton Live, have an issue where they cannot load VST
  plugins if the `MIDI Effect` option is enabled in the plugin's properties.
  Therefore, when building VST3 plugins, set the value of `IS_MIDI_EFFECT`
  to `FALSE`.
- The plug-in itself produces no audio; it is a pure MIDI processor.

## Building (macOS)

Requirements: Xcode, CMake >= 3.22, JUCE 9 sources at `~/JUCE`
(override with `-DJUCE_DIR=<path>`).

```sh
cmake -S . -B ./Builds/CMake -DCMAKE_BUILD_TYPE=Release
cmake --build ./Builds/CMake --config Release -j
```

`COPY_PLUGIN_AFTER_BUILD` installs AU -> `~/Library/Audio/Plug-Ins/Components`,
VST3 -> `~/Library/Audio/Plug-Ins/VST3`. The legacy Projucer project
(`TempoGate.jucer`, `Builds/MacOSX`) is kept in sync for reference; the CMake
build above is canonical.

Validate the AU build with:

```sh
auval -v aumi Gzlb Rzgl
```

## Building (Linux)

Requirements: C++ Build Tools, CMake >= 3.22, JUCE 9 sources at `~/JUCE`
(override with `-DJUCE_DIR=<path>`).

### Install packages in Ubuntu >= 22.04

```sh
sudo apt-get update && sudo apt-get install -y \
    build-essential \
    cmake \
    pkg-config \
    libasound2-dev \
    libfreetype-dev \
    libfontconfig1-dev \
    libgl1-mesa-dev \
    libegl1-mesa-dev \
    libcurl4-openssl-dev \
    libwebkit2gtk-4.1-dev \
    libgtk-3-dev \
    unzip \
    git
```

### Install packages in Fedora >= 43

```sh
sudo dnf group install development-tools c-development && \
sudo dnf -y install \
  cmake \
  alsa-lib-devel \
  freetype-devel \
  fontconfig-devel \
  mesa-libEGL-devel \
  mesa-libGL-devel \
  libcurl-devel \
  webkit2gtk4.1-devel \
  gtk3-devel \
  pkgconf-pkg-config
```

### Build from source

```sh
cmake -S . -B ./Builds/CMake -DCMAKE_BUILD_TYPE=Release
cmake --build ./Builds/CMake --config Release -j$(nproc)
```

## Building (Windows - Visual Studio 2026)

Requirements: Visual Studio 2026 (x64 workload), CMake 4.x,
JUCE 9 sources (override with `-DJUCE_DIR=<path>`).
AU is Apple-only, so Windows builds produce **VST3 + Standalone**.

```bat
cmake -S . -B .\Builds\MSVC -G "Visual Studio 18 2026" -A x64
cmake --build .\Builds\MSVC --config Release
```

Notes:

- The VST3 is classified `Instrument | Tools`, so Cubase lists it as a VSTi.
- Installing into `C:\Program Files\Common Files\VST3` needs elevation: run
  the build from an elevated prompt, or configure with
  `-DTEMPOGATE_COPY_PLUGIN_AFTER_BUILD=OFF` and copy
  `Builds\MSVC\TempoGate_artefacts\Release\VST3\TempoGate.vst3` manually.
- Sources are compiled as UTF-8 (`/utf-8` is forced for MSVC) and use only
  JUCE + standard C++17 - no macOS-only code paths.
- The headless tests build as `TempoGateTest.exe` in the same tree; run it
  on Windows to verify DSP/export behavior there.
- A Projucer `VS2022` exporter entry (`Builds/VisualStudio2022`) is also kept
  in `TempoGate.jucer`; VS2026 opens it directly.

## Tests

Headless regression tests drive `processBlock` directly (no GUI/audio HW):

```sh
./Builds/CMake/TempoGateTest_artefacts/Release/TempoGateTest
```

Covered: click grid/accents at the source tempo, count-in (clicks but no
capture, pre-roll accounted in recorded beats), metro on/off, continuous play
across a barline (nothing swallowed, no skipped bar), silent-barline freeze
with trigger resume at the exact barline, and specific-note trigger
accept/reject. The VST3 `moduleinfo.json` must list the `Instrument`
sub-category; the AU must stay type `aumi`.

## Source layout

```
Source/
  TempoEngine.h       source/project tempo, ratio, beats<->seconds
  BarGate.h           MIDI-triggered bar state machine (Idle/Recording/Waiting/...)
  PerformanceModel.h  recorded take in source beats + bar/note selection
  MidiExport.h        MidiExportObject + shared ExportRenderer (-> .mid / drag)
  PluginProcessor.*   APVTS params, record/convert/commit pipeline, MIDI click
  PluginEditor.*      workspace UI, piano rolls, OS drag & drop, Commit
tests/
  MetronomeTest.cpp   headless processBlock tests (TempoGateTest binary)
```

## Key runtime behaviors

- Recording clock follows the host PPQ when the transport runs, else an
  internal source-tempo clock. The Bar Gate freezes the clock at the barline
  (including barlines crossed in silence) until the trigger arrives.
- A note played across a barline instantly becomes the next bar's downbeat -
  continuous playing is never chopped.
- While waiting, non-trigger note-ons sound live but are not captured;
  note-offs/CCs are captured at the frozen beat so takes can't stick.
- Take modes on RECORD (`Take:` selector): **Replace All** (fresh take),
  **Overdub** (layer onto the existing take from the top), **Punch Bars**
  (wipe only the selected bars and capture solely inside that range).
- `From Bar` checkbox: start the take at the selected bar instead of the top
  (absolute bar numbers preserved; combines with all take modes and count-in).
- Every drag re-exports a fresh temp `.mid`; drag and file-save share the
  same `ExportRenderer`, so both paths can never diverge.
- Exports always start at the first exported event (leading silence trimmed,
  intervals preserved): a dropped region starts with content, matching what
  the workspace shows and what playback plays.
- Export strips message timestamps before rendering (`withTimeStamp(0.0)`):
  `MidiMessageSequence::addEvent(msg, t)` *adds* `t` to the message's
  existing timestamp, and recorded messages carry their block sample offset
  there (MidiBuffer round-trip). Without stripping, every event shifts by its
  own offset - non-uniformly, up to a full block, order-changing included.
  Larger buffers meant larger shifts, which is why small buffers looked
  "cleaner". Guarded by a nonzero-offset regression test.
- Take-health indicator (`In:` in the selection row): `OK`, or `UNSTABLE`
  with counts of order inversions / zero-length notes / audio-callback
  stalls seen while recording. It tells input-timing trouble apart from
  take-engine trouble at a glance (details in the tooltip and, for logging
  builds, in `~/tempogate_take.log`).

## Diagnostic logging build

When a take's timing is suspect and the cause must be attributed to either
the input stream or the take engine, build with take logging:

```sh
cmake -S . -B ./Builds/Log -DCMAKE_BUILD_TYPE=Release \
      -DTEMPOGATE_TAKE_LOG=ON
cmake --build ./Builds/Log --config Release -j
```

> **Do not use this build in production**: file I/O on the audio thread can perturb timing under load.

The log holds one `B` line per audio block (transport state, host ppq/bpm,
clock delta, record-beat window, gate state) and one `E` line per MIDI event
(`norm` = captured, `trig-*` = gate release, `closer` = frozen-beat closer,
`skip-*` = not captured with reason). Comparing `samp` (arrival block/offset)
against `beat` (assigned musical time) separates delivery jitter (arrival
already late) from clock error (arrival fine, beats wrong) on the spot.

## Recording troubleshooting

TempoGate records exactly what arrives at its MIDI input (verified
sample-accurate in headless tests covering host-sync, looping transport,
count-in, bar gate, punch-in and export timelines). If a take's timing looks
off, check in order:

1. Re-export with the current build first: takes exported before the
   timestamp-stripping fix carry per-event sample-offset shifts described
   above (non-uniform, order-changing, magnitude up to one audio block).
2. No groove/quantize/humanize or MIDI FX on the source track or item.
3. No transport looping, seeking, or tempo changes during the take.
4. One audio-device owner: close competing DAWs/apps, or lower the buffer.
   Virtual MIDI cables (IAC Driver etc.) under CPU load can burst/delay
   events by hundreds of ms - prefer a direct connection when possible.
   (Note: with the timestamp fix in place, buffer size no longer affects
   export timing; it only affects live scheduling robustness.)
5. Compare against a grid-straight reference file tick-for-tick. Use the
   `In:` health indicator (UNSTABLE = inversions/zero-length/stalls seen)
   and, if needed, a logging build (`~/tempogate_take.log` holds per-block
   clock state and per-event arrival-vs-assigned beats) to attribute any
   remainder to delivery vs engine on the spot.
