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
