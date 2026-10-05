//==============================================================================
// TempoGate - PerformanceModel
// MIDI Workspace model.
//
// Stores the recorded performance in SOURCE beats (plus channel, velocity,
// CC, pitch-bend, program-change). Converted positions are identical in
// beat space (see TempoEngine); the export layer stamps the target tempo.
// Supports bar-range selection and note selection for partial export.
// Thread-safe for audio-thread recording + message-thread UI/export.
//==============================================================================

#pragma once

#include <JuceHeader.h>

namespace tempogate
{

struct PerfEvent
{
    juce::MidiMessage message;  // timeless MIDI bytes
    double sourceBeat = 0.0;    // position in source beats
    double sourceSeconds = 0.0; // wall clock at source tempo
    int barIndex = 1;           // 1-based performance bar (§11 mapping)
    int eventId = 0;            // stable id for note selection
    bool selected = false;
};

class PerformanceModel
{
public:
    PerformanceModel() = default;

    void clear()
    {
        const juce::ScopedLock sl (lock);
        events.clear();
        totalBeats = 0.0;
        nextId = 1;
    }

    void addEvent (const juce::MidiMessage& m, double sourceBeat,
                   double sourceSeconds, int barIndex)
    {
        const juce::ScopedLock sl (lock);
        PerfEvent e;
        e.message = m;
        e.sourceBeat = sourceBeat;
        e.sourceSeconds = sourceSeconds;
        e.barIndex = juce::jmax (1, barIndex);
        e.eventId = nextId++;
        events.push_back (e);
        totalBeats = juce::jmax (totalBeats, sourceBeat);
    }

    int size() const
    {
        const juce::ScopedLock sl (lock);
        return (int) events.size();
    }

    double getTotalBeats() const
    {
        const juce::ScopedLock sl (lock);
        return totalBeats;
    }

    int getNumBars (double barLengthBeats) const
    {
        const juce::ScopedLock sl (lock);
        if (barLengthBeats <= 0.0) return 0;
        int bars = 0;
        for (auto& e : events)
            bars = juce::jmax (bars, e.barIndex);
        return bars;
    }

    juce::Array<PerfEvent> snapshot() const
    {
        const juce::ScopedLock sl (lock);
        juce::Array<PerfEvent> out;
        out.ensureStorageAllocated ((int) events.size());
        for (auto& e : events) out.add (e);
        return out;
    }

    //---- selection ------------------------------------------------------------
    // Bar-range selection. Inclusive, 1-based.
    void setSelectedBarRange (int first, int last)
    {
        const juce::ScopedLock sl (lock);
        selBarFirst = juce::jmax (1, first);
        selBarLast  = juce::jmax (selBarFirst, last);
        hasBarSelection = true;
    }

    void clearBarSelection()
    {
        const juce::ScopedLock sl (lock);
        hasBarSelection = false;
    }

    bool getHasBarSelection() const
    {
        const juce::ScopedLock sl (lock);
        return hasBarSelection;
    }

    void getBarSelection (int& first, int& last) const
    {
        const juce::ScopedLock sl (lock);
        first = selBarFirst; last = selBarLast;
    }

    void selectAllBars (double barLengthBeats)
    {
        int bars = getNumBars (barLengthBeats);
        if (bars < 1) bars = 4;
        setSelectedBarRange (1, bars);
    }

    // Note selection (Phase 2 - stored but bar export is MVP).
    void setNoteSelected (int eventId, bool sel)
    {
        const juce::ScopedLock sl (lock);
        for (auto& e : events)
            if (e.eventId == eventId) { e.selected = sel; break; }
    }

    void clearNoteSelection()
    {
        const juce::ScopedLock sl (lock);
        for (auto& e : events) e.selected = false;
    }

    // Erase every event whose bar lies in [first, last] (1-based, inclusive).
    // Used by punch-in recording: the selected bars are wiped, all other
    // bars (with their absolute bar numbers) are preserved.
    void removeBars (int first, int last)
    {
        const juce::ScopedLock sl (lock);
        first = juce::jmax (1, first);
        last = juce::jmax (first, last);
        std::vector<PerfEvent> kept;
        kept.reserve (events.size());
        for (auto& e : events)
            if (e.barIndex < first || e.barIndex > last)
                kept.push_back (e);
        events.swap (kept);
        totalBeats = 0.0;
        for (auto& e : events)
            totalBeats = juce::jmax (totalBeats, e.sourceBeat);
    }

    // Filtered views -----------------------------------------------------------
    // Full performance in beat order.
    juce::Array<PerfEvent> getAll() const { return snapshot(); }

    // Events inside the selected bar range (or everything when no selection).
    juce::Array<PerfEvent> getBarSelectionEvents() const
    {
        const juce::ScopedLock sl (lock);
        juce::Array<PerfEvent> out;
        for (auto& e : events)
        {
            if (! hasBarSelection
                || (e.barIndex >= selBarFirst && e.barIndex <= selBarLast))
                out.add (e);
        }
        return out;
    }

    juce::Array<PerfEvent> getSelectedNotes() const
    {
        const juce::ScopedLock sl (lock);
        juce::Array<PerfEvent> out;
        for (auto& e : events)
            if (e.selected) out.add (e);
        return out;
    }

    double getMinBeat() const
    {
        const juce::ScopedLock sl (lock);
        double m = std::numeric_limits<double>::max();
        for (auto& e : events) m = juce::jmin (m, e.sourceBeat);
        return events.empty() ? 0.0 : m;
    }

    double getMaxBeat() const
    {
        const juce::ScopedLock sl (lock);
        double m = 0.0;
        for (auto& e : events)
        {
            double end = e.sourceBeat;
            if (e.message.isNoteOn())
                end += 1.0; // nominal length for display when off unknown
            m = juce::jmax (m, end);
        }
        return m;
    }

private:
    mutable juce::CriticalSection lock;
    std::vector<PerfEvent> events;
    double totalBeats = 0.0;
    int nextId = 1;

    bool hasBarSelection = false;
    int selBarFirst = 1, selBarLast = 4;
};

} // namespace tempogate
