//==============================================================================
// TempoGate - MidiExport
// MIDI Export Object, MIDI File creation, Based on converted
// Export Renderer + Common Pipeline.
//
// Pipeline: MIDI Performance -> ExportRenderer -> { MIDI File | Drag Data }.
// Drag & Drop and "Save MIDI file" share this exact renderer, so both paths
// can never diverge.
//==============================================================================

#pragma once

#include <JuceHeader.h>
#include "TempoEngine.h"
#include "PerformanceModel.h"

namespace tempogate
{

// Object model.
struct MidiExportObject
{
    enum class Source { Original, Converted };

    Source source = Source::Converted;
    double startBeat = 0.0;   // eventRange.start (source beats)
    double endBeat = 0.0;     // eventRange.end   (source beats, excl.)
    juce::Array<PerfEvent> selectedEvents; // empty = range filter applies

    double sourceTempo = 90.0;
    double targetTempo = 120.0;
    int timeSigNum = 4;
    int timeSigDen = 4;
    int startBar = 1;
    double durationBeats = 0.0;

    bool includeTempoMeta = true;
    bool includeTimeSigMeta = true;
};

class ExportRenderer
{
public:
    static constexpr int ticksPerQuarterNote = 960;

    // Build the export object for the requested scope.
    //
    // Exports always start at the first exported event (leading silence is
    // trimmed, intervals preserved): a dropped region starts with content,
    // matching what the workspace shows and what playback plays. Timeline
    // preservation (leading silence kept) is intentionally NOT the default -
    // it strands gate-wait/from-bar takes behind bars of dead space.
    static MidiExportObject makeFullExport (const PerformanceModel& perf,
                                            MidiExportObject::Source src,
                                            double sourceTempo, double targetTempo,
                                            int tsNum, int tsDen)
    {
        MidiExportObject obj;
        obj.source = src;
        obj.selectedEvents = perf.getAll();
        obj.sourceTempo = sourceTempo;
        obj.targetTempo = targetTempo;
        obj.timeSigNum = tsNum;
        obj.timeSigDen = tsDen;
        obj.startBar = 1;
        if (! obj.selectedEvents.isEmpty())
        {
            obj.startBeat = obj.selectedEvents.getReference (0).sourceBeat;
            double mx = obj.startBeat;
            for (auto& e : obj.selectedEvents)
                mx = juce::jmax (mx, e.sourceBeat);
            obj.endBeat = mx;
            obj.durationBeats = mx - obj.startBeat;
        }
        return obj;
    }

    static MidiExportObject makeBarSelectionExport (const PerformanceModel& perf,
                                                   MidiExportObject::Source src,
                                                   double sourceTempo, double targetTempo,
                                                   int tsNum, int tsDen)
    {
        MidiExportObject obj;
        obj.source = src;
        obj.selectedEvents = perf.getBarSelectionEvents();
        obj.sourceTempo = sourceTempo;
        obj.targetTempo = targetTempo;
        obj.timeSigNum = tsNum;
        obj.timeSigDen = tsDen;
        int first = 1, last = 1;
        if (perf.getHasBarSelection()) perf.getBarSelection (first, last);
        obj.startBar = first;
        if (! obj.selectedEvents.isEmpty())
        {
            obj.startBeat = obj.selectedEvents.getReference (0).sourceBeat;
            double mx = obj.startBeat;
            for (auto& e : obj.selectedEvents)
                mx = juce::jmax (mx, e.sourceBeat);
            obj.endBeat = mx;
            obj.durationBeats = mx - obj.startBeat;
        }
        return obj;
    }

    static MidiExportObject makeNoteSelectionExport (const PerformanceModel& perf,
                                                    MidiExportObject::Source src,
                                                    double sourceTempo, double targetTempo,
                                                    int tsNum, int tsDen)
    {
        MidiExportObject obj = makeFullExport (perf, src, sourceTempo, targetTempo, tsNum, tsDen);
        auto sel = perf.getSelectedNotes();
        if (! sel.isEmpty())
            obj.selectedEvents = sel;
        return obj;
    }

    // Render to a juce::MidiFile. Converted keeps beat positions and stamps
    // the TARGET tempo (§10); Original stamps the source tempo.
    static std::unique_ptr<juce::MidiFile> renderToMidiFile (const MidiExportObject& obj)
    {
        auto file = std::make_unique<juce::MidiFile>();
        file->setTicksPerQuarterNote (ticksPerQuarterNote);

        juce::MidiMessageSequence track;
        const double refTempo = (obj.source == MidiExportObject::Source::Converted)
                                    ? obj.targetTempo : obj.sourceTempo;

        if (obj.includeTempoMeta)
            track.addEvent (juce::MidiMessage::tempoMetaEvent (
                                TempoEngine::microsecondsPerQuarterNote (refTempo)), 0.0);

        if (obj.includeTimeSigMeta)
            track.addEvent (juce::MidiMessage::timeSignatureMetaEvent (
                                obj.timeSigNum, obj.timeSigDen), 0.0);

        // Rebase so the export starts at tick 0: the first exported event
        // becomes the region start (§11 bar mapping for selections).
        double base = obj.selectedEvents.isEmpty() ? 0.0
                    : obj.selectedEvents.getReference (0).sourceBeat;
        if (obj.selectedEvents.size() > 1)
        {
            base = obj.selectedEvents.getReference (0).sourceBeat;
            for (auto& e : obj.selectedEvents)
                base = juce::jmin (base, e.sourceBeat);
        }

        for (auto& e : obj.selectedEvents)
        {
            // MidiBuffer messages retain their sample offset in the message
            // timestamp.  MidiMessageSequence::addEvent() *adds* its second
            // argument to that timestamp, so remove it before converting the
            // recorded beat to an SMF tick position.
            const auto m = e.message.withTimeStamp (0.0);
            if (m.isNoteOn() || m.isNoteOff() || m.isController()
                || m.isPitchWheel() || m.isProgramChange()
                || m.isChannelPressure() || m.isAftertouch())
            {
                double beats = juce::jmax (0.0, e.sourceBeat - base);
                track.addEvent (m, beats * ticksPerQuarterNote);
            }
        }

        track.sort();
        // End-of-track a touch after the last event.
        double lastTick = 0.0;
        if (track.getNumEvents() > 0)
            lastTick = track.getEventTime (track.getNumEvents() - 1);
        track.addEvent (juce::MidiMessage::endOfTrack(),
                        lastTick + ticksPerQuarterNote);

        track.updateMatchedPairs();
        file->addTrack (track);
        return file;
    }

    // Shared pipeline exit #1 - write a .mid file. Returns the file.
    static juce::File writeToFile (const MidiExportObject& obj, const juce::File& dest)
    {
        auto midiFile = renderToMidiFile (obj);
        if (auto out = dest.createOutputStream())
            midiFile->writeTo (*out);
        return dest;
    }

    // Shared pipeline exit #2 - temp file backing an OS drag source (§19).
    // Every drag re-exports fresh, so the DAW always receives the current
    // Workspace state.
    static juce::File writeToTempFile (const MidiExportObject& obj,
                                       const juce::String& prefix)
    {
        auto tmp = juce::File::getSpecialLocation (
            juce::File::SpecialLocationType::tempDirectory);
        auto safe = prefix.retainCharacters ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_");
        if (safe.isEmpty()) safe = "TempoGate";
        juce::File dest = tmp.getChildFile (safe + ".mid").getNonexistentSibling();
        return writeToFile (obj, dest);
    }

    static void writeToMemoryBlock (const MidiExportObject& obj, juce::MemoryBlock& mb)
    {
        auto midiFile = renderToMidiFile (obj);
        juce::MemoryOutputStream mos (mb, false);
        midiFile->writeTo (mos);
    }
};

} // namespace tempogate
