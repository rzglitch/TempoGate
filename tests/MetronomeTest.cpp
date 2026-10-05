//==============================================================================
// TempoGate - Metronome smoke test.
//
// Drives TempoGateAudioProcessor::processBlock directly (no GUI, no audio
// hardware) and verifies:
//  1. MIDI clicks land on the source-tempo grid with downbeat accents.
//  2. Count-in pre-roll emits clicks but records nothing.
//  3. metroOn=false emits no clicks.
//==============================================================================

#include <JuceHeader.h>
#include "PluginProcessor.h"

namespace
{
int failures = 0;

void check (bool cond, const char* msg)
{
    if (cond) std::printf ("ok: %s\n", msg);
    else      { std::printf ("FAIL: %s\n", msg); ++failures; }
}

void setParam (TempoGateAudioProcessor& p, const char* id, float v)
{
    if (auto* par = p.apvts.getRawParameterValue (id))
        *par = v;
    else { std::printf ("FAIL: missing param %s\n", id); ++failures; }
}

struct ClickHit
{
    double beat = 0.0; // in source beats since startRecording
    int note = 0, vel = 0, ch = 0;
};

// Run n blocks; input(blockIndex, midiIn) may inject MIDI. Returns source-tempo
// click note-ons detected on the click channel. absSample tracks time across
// calls so multi-phase runs stay comparable.
std::vector<ClickHit> runBlocks (TempoGateAudioProcessor& proc, int n, double sampleRate,
                                 int blockSize, double sourceTempo, int64_t& absSample,
                                 std::function<void (int, juce::MidiBuffer&)> input = {})
{
    std::vector<ClickHit> clicks;
    juce::AudioBuffer<float> audio (2, blockSize);
    const int ch = proc.getMetroChannel();
    const int acc = proc.getMetroAccentNote();
    const int beat = proc.getMetroBeatNote();

    for (int b = 0; b < n; ++b)
    {
        juce::MidiBuffer mb;
        if (input) input (b, mb);
        proc.processBlock (audio, mb);

        for (const auto meta : mb)
        {
            const auto m = meta.getMessage();
            if (m.isNoteOn() && m.getChannel() == ch
                && (m.getNoteNumber() == acc || m.getNoteNumber() == beat))
            {
                const double secs = (double) (absSample + meta.samplePosition) / sampleRate;
                clicks.push_back ({ secs * sourceTempo / 60.0,
                                    m.getNoteNumber(), m.getVelocity(), m.getChannel() });
            }
        }
        absSample += blockSize;
    }
    return clicks;
}
} // namespace

int main()
{
    constexpr double sr = 44100.0;
    constexpr int block = 512;
    constexpr double tempo = 120.0; // 0.5 s per beat
    const double beatsPerBlock = ((double) block / sr) * tempo / 60.0;

    //---- Test 1: grid + accents, no count-in, gate off ---------------------------
    {
        TempoGateAudioProcessor proc;
        proc.prepareToPlay (sr, block);
        setParam (proc, "sourceTempo", (float) tempo);
        setParam (proc, "followHost", 0.0f);
        setParam (proc, "metroOn", 1.0f);
        setParam (proc, "countIn", 0.0f); // Off
        setParam (proc, "waitForTrigger", 0.0f);
        proc.startRecording();

        int64_t absS = 0;
        auto clicks = runBlocks (proc, 300, sr, block, tempo, absS);
        proc.stopRecording();

        check (clicks.size() == 7, "7 clicks in 300 blocks @120bpm (beats 0..6)");
        bool gridOk = (clicks.size() == 7);
        for (size_t i = 0; i < clicks.size() && i < 7; ++i)
            if (std::abs (clicks[i].beat - (double) i) > 0.03) gridOk = false;
        check (gridOk, "clicks land on integer beats (+-0.03)");

        if (clicks.size() >= 5)
        {
            check (clicks[0].note == proc.getMetroAccentNote(), "beat 0 is accented");
            check (clicks[4].note == proc.getMetroAccentNote(), "beat 4 (bar 2) is accented");
            check (clicks[1].note == proc.getMetroBeatNote(), "beat 1 is a plain beat");
            check (clicks[2].note == proc.getMetroBeatNote(), "beat 2 is a plain beat");
        }
        else { check (false, "accent pattern (not enough clicks)"); }
    }

    //---- Test 2: count-in clicks but doesn't record -------------------------------
    {
        TempoGateAudioProcessor proc;
        proc.prepareToPlay (sr, block);
        setParam (proc, "sourceTempo", (float) tempo);
        setParam (proc, "followHost", 0.0f);
        setParam (proc, "metroOn", 1.0f);
        setParam (proc, "countIn", 1.0f); // 1 Bar = 4 beats = 2.0 s pre-roll
        setParam (proc, "waitForTrigger", 0.0f);
        proc.startRecording();
        check (proc.isCountingIn(), "count-in active right after start");

        int64_t absS = 0;
        auto feedEarly = [] (int b, juce::MidiBuffer& mb)
        {
            if (b == 10) // inside the 2.0 s pre-roll: must be ignored
                mb.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
        };
        auto pre = runBlocks (proc, 100, sr, block, tempo, absS, feedEarly);
        check (proc.isCountingIn(), "still counting in after 100 blocks");
        check (proc.getPerformance().size() == 0, "nothing recorded during pre-roll");
        check (pre.size() >= 2, "clicks emitted during count-in pre-roll");

        auto feedLate = [] (int b, juce::MidiBuffer& mb)
        {
            if (b == 100) // absolute block 200: after the pre-roll
                mb.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
        };
        auto post = runBlocks (proc, 200, sr, block, tempo, absS, feedLate);
        check (! proc.isCountingIn(), "count-in finished after pre-roll");
        (void) post;
        proc.stopRecording();

        // Only the post-roll note was captured.
        const int nEvents = proc.getPerformance().size();
        check (nEvents == 1, "pre-roll note ignored, post-roll note recorded");
        if (nEvents == 1)
        {
            auto snap = proc.getPerformance().snapshot();
            const auto& e = snap.getReference (0);
            const double got = e.sourceBeat;
            // block-200 note lands at (200*beatsPerBlock - 4.0 pre-roll) beats:
            const double wantNote = 200.0 * beatsPerBlock - 4.0;
            check (std::abs (got - wantNote) < 0.15, "recorded beat accounts for pre-roll");
        }
    }

    //---- Test 3: metro off = silence -----------------------------------------------
    {
        TempoGateAudioProcessor proc;
        proc.prepareToPlay (sr, block);
        setParam (proc, "sourceTempo", (float) tempo);
        setParam (proc, "metroOn", 0.0f);
        setParam (proc, "countIn", 0.0f);
        setParam (proc, "waitForTrigger", 0.0f);
        proc.startRecording();
        int64_t absS = 0;
        auto clicks = runBlocks (proc, 100, sr, block, tempo, absS);
        proc.stopRecording();
        check (clicks.empty(), "no clicks when metro is off");
    }

    //---- Test 4: continuous play across the barline is never swallowed ------------
    // Every note-on is a trigger (Any mode), so the crossing note must become
    // the downbeat of bar 2 and the clock must run straight through.
    {
        TempoGateAudioProcessor proc;
        proc.prepareToPlay (sr, block);
        setParam (proc, "sourceTempo", (float) tempo);
        setParam (proc, "followHost", 0.0f);
        setParam (proc, "metroOn", 0.0f);
        setParam (proc, "countIn", 0.0f);
        setParam (proc, "waitForTrigger", 1.0f);
        setParam (proc, "triggerMode", 0.0f); // Any Note-On
        proc.startRecording();

        int64_t absS = 0;
        auto everyBlock = [] (int, juce::MidiBuffer& mb)
        { mb.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0); };
        runBlocks (proc, 250, sr, block, tempo, absS, everyBlock);
        proc.stopRecording();

        check (proc.getPerformance().size() == 250, "all 250 crossing notes recorded, none swallowed");
        check (std::abs (proc.getRecordBeat() - 250.0 * beatsPerBlock) < 0.05,
               "clock ran through the barline with continuous input");
        check (proc.getBarGate().getCurrentBar() == 2, "now in bar 2 (not skipped to 3)");
        check (! proc.getBarGate().isWaiting(), "not waiting after continuous play");

        // The crossing note sits exactly on the barline as the new downbeat.
        bool foundDownbeat = false;
        for (auto& e : proc.getPerformance().snapshot())
            if (e.message.isNoteOn() && std::abs (e.sourceBeat - 4.0) < 1e-9 && e.barIndex == 2)
                foundDownbeat = true;
        check (foundDownbeat, "crossing note became the bar-2 downbeat at 4.0");
    }

    //---- Test 5: silence freezes at the barline; trigger resumes there ------------
    {
        TempoGateAudioProcessor proc;
        proc.prepareToPlay (sr, block);
        setParam (proc, "sourceTempo", (float) tempo);
        setParam (proc, "followHost", 0.0f);
        setParam (proc, "metroOn", 0.0f);
        setParam (proc, "countIn", 0.0f);
        setParam (proc, "waitForTrigger", 1.0f);
        setParam (proc, "triggerMode", 0.0f);
        proc.startRecording();

        int64_t absS = 0;
        auto sparse = [] (int b, juce::MidiBuffer& mb)
        {
            if (b % 10 == 0)
                mb.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
        };
        runBlocks (proc, 100, sr, block, tempo, absS, sparse); // beats 0..2.3
        runBlocks (proc, 150, sr, block, tempo, absS);         // silence past 4.0

        check (proc.getBarGate().isWaiting(), "gate waiting after silent barline cross");
        check (std::abs (proc.getRecordBeat() - 4.0) < 0.02, "clock frozen exactly at barline 4.0");

        auto triggerOnce = [] (int b, juce::MidiBuffer& mb)
        {
            if (b == 0)
                mb.addEvent (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 100), 0);
        };
        runBlocks (proc, 1, sr, block, tempo, absS, triggerOnce);
        check (! proc.getBarGate().isWaiting(), "trigger released the gate");
        check (proc.getBarGate().getCurrentBar() == 2, "resumed into bar 2 (not bar 3)");

        runBlocks (proc, 20, sr, block, tempo, absS);
        proc.stopRecording();

        check (std::abs (proc.getRecordBeat() - (4.0 + 21.0 * beatsPerBlock)) < 0.05,
               "clock resumed from the barline, waiting time excluded");
        check (proc.getPerformance().size() == 11, "10 pre notes + trigger note recorded");

        bool triggerAtBarline = false;
        for (auto& e : proc.getPerformance().snapshot())
            if (e.message.isNoteOn() && e.message.getNoteNumber() == 64
                && std::abs (e.sourceBeat - 4.0) < 1e-9 && e.barIndex == 2)
                triggerAtBarline = true;
        check (triggerAtBarline, "trigger recorded exactly at 4.0 as bar-2 downbeat");
    }

    //---- Test 6: specific-note trigger rejects wrong notes -------------------------
    {
        TempoGateAudioProcessor proc;
        proc.prepareToPlay (sr, block);
        setParam (proc, "sourceTempo", (float) tempo);
        setParam (proc, "followHost", 0.0f);
        setParam (proc, "metroOn", 0.0f);
        setParam (proc, "countIn", 0.0f);
        setParam (proc, "waitForTrigger", 1.0f);
        setParam (proc, "triggerMode", 1.0f); // Note C4 (60)
        proc.startRecording();

        int64_t absS = 0;
        runBlocks (proc, 200, sr, block, tempo, absS); // silence past 4.0
        check (proc.getBarGate().isWaiting(), "specific-note gate waiting after silence");

        auto playNote = [] (int pitch)
        {
            return [pitch] (int b, juce::MidiBuffer& mb)
            {
                if (b == 0)
                    mb.addEvent (juce::MidiMessage::noteOn (1, pitch, (juce::uint8) 100), 0);
            };
        };
        runBlocks (proc, 1, sr, block, tempo, absS, playNote (62)); // D4: wrong
        check (proc.getBarGate().isWaiting(), "wrong note does not release the gate");
        check (proc.getPerformance().size() == 0, "wrong note not captured");

        runBlocks (proc, 1, sr, block, tempo, absS, playNote (60)); // C4: trigger
        check (! proc.getBarGate().isWaiting(), "C4 releases the gate");
        check (proc.getBarGate().getCurrentBar() == 2, "C4 resumes into bar 2");
        check (proc.getPerformance().size() == 1, "only the trigger captured");
        if (proc.getPerformance().size() == 1)
        {
            auto snap = proc.getPerformance().snapshot();
            const auto& e = snap.getReference (0);
            check (std::abs (e.sourceBeat - 4.0) < 1e-9 && e.barIndex == 2,
                   "C4 recorded at 4.0 as bar-2 downbeat");
        }
        proc.stopRecording();
    }

    if (failures == 0) std::printf ("\nALL METRONOME TESTS PASSED\n");
    else               std::printf ("\n%d METRONOME TEST(S) FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
