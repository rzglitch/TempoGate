//==============================================================================
// TempoGate - BarGate
// MIDI-Triggered Bar Gate, Preserving the Bar Boundary
//
// The gate tracks the current bar in SOURCE beats. When armed with
// waitForTrigger, the beat clock pauses at the bar boundary (state WAITING)
// and resumes on the configured MIDI trigger. Waiting wall-time is excluded
// from the performance, so exported bars are concatenated without gaps.
//==============================================================================

#pragma once

#include <JuceHeader.h>
#include "TempoEngine.h"

namespace tempogate
{

class BarGate
{
public:
    enum class State { Idle, Recording, Waiting, Playing, CountIn };
    enum TriggerMode
    {
        AnyNoteOn = 0,
        SpecificNote,   // default C4 (60)
        VelocityThreshold,
        SustainPedal    // CC64
    };

    BarGate() = default;

    void reset() noexcept
    {
        currentBar = 1;
        barStartBeat = 0.0;
        state = State::Idle;
        waiting = false;
    }

    void startRecording() noexcept
    {
        currentBar = 1;
        barStartBeat = 0.0;
        state = State::Recording;
        waiting = false;
    }

    // Start (or resume) the take at a given bar instead of the top - used by
    // "record from selected bar". Bar numbering stays absolute.
    void jumpToBar (int bar) noexcept
    {
        currentBar = juce::jmax (1, bar);
        barStartBeat = (double) (currentBar - 1) * getBarLengthBeats();
        state = State::Recording;
        waiting = false;
    }

    void stop() noexcept
    {
        state = State::Idle;
        waiting = false;
    }

    //---- configuration -------------------------------------------------------
    void setTimeSignature (int num, int den) noexcept
    {
        numerator = juce::jlimit (1, 16, num);
        denominator = (den <= 0 ? 4 : den);
    }

    void setTriggerMode (int m) noexcept
    {
        triggerMode = juce::jlimit (0, 3, m);
    }

    void setTriggerNote (int n) noexcept      { triggerNote = juce::jlimit (0, 127, n); }
    void setVelocityThreshold (int v) noexcept { velocityThreshold = juce::jlimit (1, 127, v); }
    void setWaitForTrigger (bool w) noexcept   { waitForTrigger = w; }

    int getNumerator() const noexcept    { return numerator; }
    int getDenominator() const noexcept  { return denominator; }
    int getTriggerMode() const noexcept  { return triggerMode; }
    int getCurrentBar() const noexcept   { return currentBar; }
    State getState() const noexcept      { return state; }
    bool isWaiting() const noexcept      { return waiting; }
    bool getWaitForTrigger() const noexcept { return waitForTrigger; }

    double getBarLengthBeats() const noexcept
    {
        return TempoEngine::barLengthBeats (numerator, denominator);
    }

    //---- runtime -------------------------------------------------------------
    // Advance the source-beat clock. Returns the bar index the given beat
    // belongs to. If a boundary is crossed while waitForTrigger is armed,
    // the gate latches into WAITING and reports the boundary beat so the
    // caller can freeze its clock there.
    struct AdvanceResult
    {
        int  barIndex = 1;
        bool boundaryCrossed = false;
        double boundaryBeat = 0.0;
    };

    AdvanceResult advance (double sourceBeat)
    {
        AdvanceResult r;
        const double barLen = getBarLengthBeats();

        int bar = (int) (sourceBeat / barLen) + 1;
        if (bar < 1) bar = 1;
        r.barIndex = bar;

        if (bar != currentBar)
        {
            r.boundaryCrossed = true;
            r.boundaryBeat = (double) (bar - 1) * barLen;
            currentBar = bar;

            if (waitForTrigger && state == State::Recording)
            {
                waiting = true;
                state = State::Waiting;
            }
        }
        return r;
    }

    // Feed every incoming MIDI message; returns true when a waiting gate
    // is released by this message.
    //
    // NOTE: advance() already moved currentBar to the pending bar when the
    // barline was crossed, so releasing must NOT increment again - the
    // trigger becomes the downbeat of the pending bar (no bar is skipped).
    bool handleMidiTrigger (const juce::MidiMessage& m)
    {
        if (! waiting)
            return false;

        bool fire = false;
        switch (triggerMode)
        {
            case AnyNoteOn:
                fire = m.isNoteOn();
                break;
            case SpecificNote:
                fire = m.isNoteOn() && m.getNoteNumber() == triggerNote;
                break;
            case VelocityThreshold:
                fire = m.isNoteOn() && m.getVelocity() >= (float) velocityThreshold / 127.0f;
                break;
            case SustainPedal:
                fire = m.isController() && m.getControllerNumber() == 64 && m.getControllerValue() >= 64;
                break;
            default:
                fire = m.isNoteOn();
                break;
        }

        if (fire)
        {
            waiting = false;
            state = State::Recording;
            barStartBeat = (double) (currentBar - 1) * getBarLengthBeats();
        }
        return fire;
    }

    // True while a barline crossing should latch into WAITING (armed, running,
    // not already waiting). Used by the processor's sample-accurate sweep.
    bool armed() const noexcept
    {
        return waitForTrigger && ! waiting && state == State::Recording;
    }

    // Source-beat offset that must be subtracted from incoming beats while
    // waiting time is excluded. The processor accumulates paused beats here.
    void addPausedBeats (double beats) noexcept { pausedBeats += beats; }
    double getPausedBeats() const noexcept     { return pausedBeats; }
    void clearPausedBeats() noexcept           { pausedBeats = 0.0; }

    static juce::String stateToString (State s)
    {
        switch (s)
        {
            case State::Idle:      return "IDLE";
            case State::Recording: return "RECORDING";
            case State::Waiting:   return "WAITING";
            case State::Playing:   return "PLAYING";
            case State::CountIn:    return "COUNT-IN";
        }
        return "IDLE";
    }

    static juce::String triggerToString (int m)
    {
        switch (m)
        {
            case AnyNoteOn:         return "Any Note-On";
            case SpecificNote:      return "Note C4 (60)";
            case VelocityThreshold: return "Velocity >= 100";
            case SustainPedal:      return "Sustain Pedal (CC64)";
        }
        return "Any Note-On";
    }

private:
    int numerator = 4, denominator = 4;
    int triggerMode = AnyNoteOn;
    int triggerNote = 60;
    int velocityThreshold = 100;
    bool waitForTrigger = true;

    int currentBar = 1;
    double barStartBeat = 0.0;
    double pausedBeats = 0.0;
    State state = State::Idle;
    bool waiting = false;
};

} // namespace tempogate
