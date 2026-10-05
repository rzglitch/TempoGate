//==============================================================================
// TempoGate - TempoEngine
// Independent Tempo + Tempo Conversion
//
// Musical intent is preserved in BEATS. A note played at source-beat b stays
// at beat b in the converted performance; only the tempo reference changes
// (sourceTempo -> projectTempo). A DAW interpreting the exported SMF at the
// project tempo therefore places the note at the intended musical position.
// The wall-clock duration compresses by ratio = source / project.
//==============================================================================

#pragma once

#include <JuceHeader.h>

namespace tempogate
{

class TempoEngine
{
public:
    TempoEngine() = default;

    void setSourceTempo (double bpm) noexcept  { sourceTempo = juce::jlimit (30.0, 300.0, bpm); }
    void setProjectTempo (double bpm) noexcept { projectTempo = juce::jlimit (30.0, 300.0, bpm); }

    double getSourceTempo() const noexcept  { return sourceTempo; }
    double getProjectTempo() const noexcept { return projectTempo; }

    // RATIO display. 90/120 = 0.75
    double getRatio() const noexcept
    {
        return (projectTempo > 0.0) ? (sourceTempo / projectTempo) : 1.0;
    }

    // Beats <-> seconds helpers at either tempo reference.
    double beatsToSecondsSource (double beats) const noexcept  { return beats * 60.0 / sourceTempo; }
    double beatsToSecondsProject (double beats) const noexcept { return beats * 60.0 / projectTempo; }
    double secondsToBeatsSource (double secs) const noexcept   { return secs * sourceTempo / 60.0; }
    double secondsToBeatsProject (double secs) const noexcept  { return secs * projectTempo / 60.0; }

    // Conversion entry points.
    // Identity in beat space (musical position preserved); the tempo meta
    // of the exported object carries the target tempo instead.
    static double toProjectBeats (double sourceBeats) noexcept { return sourceBeats; }
    static double toSourceBeats (double projectBeats) noexcept { return projectBeats; }

    // Bar length in beats for a given signature. 4/4 -> 4 quarter-note beats.
    static double barLengthBeats (int numerator, int denominator) noexcept
    {
        numerator = juce::jlimit (1, 16, numerator);
        if (denominator <= 0) denominator = 4;
        return (double) numerator * (4.0 / (double) denominator);
    }

    static int microsecondsPerQuarterNote (double bpm) noexcept
    {
        return (int) std::round (60000000.0 / juce::jlimit (30.0, 300.0, bpm));
    }

private:
    double sourceTempo  = 90.0;
    double projectTempo = 120.0;
};

} // namespace tempogate
