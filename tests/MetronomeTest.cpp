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
#include <thread>

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

// Fake host transport for host-synced recording tests.
struct FakePlayHead : juce::AudioPlayHead
{
    double bpm = 120.0;
    bool playing = true;
    double ppq = 0.0; // position at current block start

    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        p.setBpm (bpm);
        p.setIsPlaying (playing);
        p.setPpqPosition (ppq);
        return p;
    }
};

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

    //---- Test 7: replace-all wipes the take -----------------------------------------
    {
        TempoGateAudioProcessor proc;
        proc.prepareToPlay (sr, block);
        setParam (proc, "sourceTempo", (float) tempo);
        setParam (proc, "followHost", 0.0f);
        setParam (proc, "metroOn", 0.0f);
        setParam (proc, "countIn", 0.0f);
        setParam (proc, "waitForTrigger", 0.0f);
        setParam (proc, "recordMode", 0.0f); // Replace All
        proc.startRecording();

        int64_t absS = 0;
        auto sparse = [] (int b, juce::MidiBuffer& mb)
        {
            if (b % 5 == 0)
                mb.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
        };
        runBlocks (proc, 50, sr, block, tempo, absS, sparse);
        proc.stopRecording();
        check (proc.getPerformance().size() == 10, "take 1 captured 10 notes");

        proc.startRecording(); // replace
        check (proc.getPerformance().size() == 0, "replace-all wipes on re-record");
        auto single = [] (int b, juce::MidiBuffer& mb)
        {
            if (b == 0)
                mb.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
        };
        runBlocks (proc, 10, sr, block, tempo, absS, single);
        proc.stopRecording();
        check (proc.getPerformance().size() == 1, "fresh take records from scratch");
    }

    //---- Test 8: overdub layers onto the existing take ---------------------------
    {
        TempoGateAudioProcessor proc;
        proc.prepareToPlay (sr, block);
        setParam (proc, "sourceTempo", (float) tempo);
        setParam (proc, "followHost", 0.0f);
        setParam (proc, "metroOn", 0.0f);
        setParam (proc, "countIn", 0.0f);
        setParam (proc, "waitForTrigger", 0.0f);
        setParam (proc, "recordMode", 0.0f);
        proc.startRecording();

        int64_t absS = 0;
        auto sparse = [] (int b, juce::MidiBuffer& mb)
        {
            if (b % 5 == 0)
                mb.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
        };
        runBlocks (proc, 50, sr, block, tempo, absS, sparse);
        proc.stopRecording();
        check (proc.getPerformance().size() == 10, "take 1 captured 10 notes");

        setParam (proc, "recordMode", 1.0f); // Overdub
        proc.startRecording();
        check (proc.getPerformance().size() == 10, "overdub preserves existing data");
        runBlocks (proc, 50, sr, block, tempo, absS, sparse);
        proc.stopRecording();
        check (proc.getPerformance().size() == 20, "overdub pass layered 10 more notes");
    }

    //---- Test 9: punch bars wipes + re-records only the range --------------------
    {
        TempoGateAudioProcessor proc;
        proc.prepareToPlay (sr, block);
        setParam (proc, "sourceTempo", (float) tempo);
        setParam (proc, "followHost", 0.0f);
        setParam (proc, "metroOn", 0.0f);
        setParam (proc, "countIn", 0.0f);
        setParam (proc, "waitForTrigger", 0.0f);
        proc.startRecording();

        int64_t absS = 0;
        auto take1 = [] (int b, juce::MidiBuffer& mb)
        {
            if (b % 10 == 0)
                mb.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
        };
        runBlocks (proc, 200, sr, block, tempo, absS, take1); // beats 0..4.6
        proc.stopRecording();

        int bar1 = 0, bar2 = 0;
        {
            auto snap = proc.getPerformance().snapshot();
            for (auto& e : snap)
                if (e.message.isNoteOn() && e.message.getNoteNumber() == 60)
                    (e.barIndex == 1 ? bar1 : bar2)++;
        }
        check (bar1 == 18 && bar2 == 2, "take 1 spans bars 1-2 (18 + 2 notes)");

        setParam (proc, "recordMode", 2.0f); // Punch Bars
        proc.getPerformance().setSelectedBarRange (2, 2);
        proc.startRecording();
        check (proc.getPerformance().size() == 18, "punch wiped only bar 2");
        {
            auto snap = proc.getPerformance().snapshot();
            bool allBar1 = true;
            for (auto& e : snap)
                if (e.barIndex != 1) allBar1 = false;
            check (allBar1, "bar 1 untouched by punch wipe");
        }

        auto pass2 = [] (int b, juce::MidiBuffer& mb)
        {
            if (b == 5) // beat ~0.12, outside punch range [4, 8): skipped
                mb.addEvent (juce::MidiMessage::noteOn (1, 62, (juce::uint8) 100), 0);
            if (b == 200) // beat ~4.64, inside: captured
                mb.addEvent (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 100), 0);
        };
        runBlocks (proc, 250, sr, block, tempo, absS, pass2);
        proc.stopRecording();

        check (proc.getPerformance().size() == 19, "punch pass added exactly 1 in-range note");
        {
            auto snap = proc.getPerformance().snapshot();
            bool saw62 = false, saw64inRange = false;
            for (auto& e : snap)
            {
                if (e.message.isNoteOn() && e.message.getNoteNumber() == 62) saw62 = true;
                if (e.message.isNoteOn() && e.message.getNoteNumber() == 64
                    && std::abs (e.sourceBeat - 200.0 * beatsPerBlock) < 0.05
                    && e.barIndex == 2)
                    saw64inRange = true;
            }
            check (! saw62, "out-of-range punch note ignored");
            check (saw64inRange, "in-range punch note captured in bar 2");
        }
    }

    //---- Test 10: record from the selected bar --------------------------------------
    {
        TempoGateAudioProcessor proc;
        proc.prepareToPlay (sr, block);
        setParam (proc, "sourceTempo", (float) tempo);
        setParam (proc, "followHost", 0.0f);
        setParam (proc, "metroOn", 0.0f);
        setParam (proc, "countIn", 0.0f);
        setParam (proc, "waitForTrigger", 0.0f);
        setParam (proc, "recordMode", 0.0f); // Replace All
        setParam (proc, "startFromBar", 1.0f);
        proc.getPerformance().setSelectedBarRange (3, 4);
        proc.startRecording();

        check (std::abs (proc.getRecordBeat() - 8.0) < 1e-9, "clock starts at bar 3 (beat 8.0)");
        check (proc.getBarGate().getCurrentBar() == 3, "gate starts at bar 3");

        int64_t absS = 0;
        auto single = [] (int b, juce::MidiBuffer& mb)
        {
            if (b == 0)
                mb.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
        };
        runBlocks (proc, 10, sr, block, tempo, absS, single);
        proc.stopRecording();

        check (proc.getPerformance().size() == 1, "note captured from bar 3");
        {
            auto snap = proc.getPerformance().snapshot();
            const auto& e = snap.getReference (0);
            check (std::abs (e.sourceBeat - 8.0) < 1e-9 && e.barIndex == 3,
                   "note recorded at 8.0 with bar index 3");
        }
    }

    //---- Test 11: from-selection + punch bars combined ------------------------------
    {
        TempoGateAudioProcessor proc;
        proc.prepareToPlay (sr, block);
        setParam (proc, "sourceTempo", (float) tempo);
        setParam (proc, "followHost", 0.0f);
        setParam (proc, "metroOn", 0.0f);
        setParam (proc, "countIn", 0.0f);
        setParam (proc, "waitForTrigger", 0.0f);
        proc.startRecording();

        int64_t absS = 0;
        auto take1 = [] (int b, juce::MidiBuffer& mb)
        {
            if (b % 10 == 0)
                mb.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
        };
        runBlocks (proc, 200, sr, block, tempo, absS, take1); // bars 1-2
        proc.stopRecording();
        check (proc.getPerformance().size() == 20, "take 1 has 20 notes");

        setParam (proc, "recordMode", 2.0f); // Punch Bars
        setParam (proc, "startFromBar", 1.0f);
        proc.getPerformance().setSelectedBarRange (2, 2);
        proc.startRecording();

        check (proc.getPerformance().size() == 18, "punch wiped bar 2");
        check (std::abs (proc.getRecordBeat() - 4.0) < 1e-9, "take starts at bar 2");
        check (proc.getBarGate().getCurrentBar() == 2, "gate starts at bar 2");

        auto repass = [] (int b, juce::MidiBuffer& mb)
        {
            if (b == 0)
                mb.addEvent (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 100), 0);
        };
        runBlocks (proc, 10, sr, block, tempo, absS, repass);
        proc.stopRecording();

        check (proc.getPerformance().size() == 19, "re-recorded bar 2 on top of bar 1");
        {
            auto snap = proc.getPerformance().snapshot();
            bool found = false;
            for (auto& e : snap)
                if (e.message.isNoteOn() && e.message.getNoteNumber() == 64
                    && std::abs (e.sourceBeat - 4.0) < 1e-9 && e.barIndex == 2)
                    found = true;
            check (found, "new note captured at 4.0 in bar 2");
        }
    }

    //---- Test 12: from-bar without selection falls back to the top ------------------
    {
        TempoGateAudioProcessor proc;
        proc.prepareToPlay (sr, block);
        setParam (proc, "sourceTempo", (float) tempo);
        setParam (proc, "startFromBar", 1.0f); // on, but no selection exists
        proc.startRecording();
        check (std::abs (proc.getRecordBeat() - 0.0) < 1e-9, "no selection -> start at top");
        check (proc.getBarGate().getCurrentBar() == 1, "gate at bar 1");
        proc.stopRecording();
    }

    //---- Test H1: host-synced recording is sample-exact at constant tempo --------
    // Replicates an 8th-note groove delivered with exact sample offsets while
    // the host runs at a steady 120 BPM.
    {
        TempoGateAudioProcessor proc;
        proc.prepareToPlay (sr, block);
        setParam (proc, "sourceTempo", (float) tempo);
        setParam (proc, "followHost", 1.0f);
        setParam (proc, "projectTempo", (float) tempo);
        setParam (proc, "metroOn", 0.0f);
        setParam (proc, "countIn", 0.0f);
        setParam (proc, "waitForTrigger", 0.0f);
        setParam (proc, "recordMode", 0.0f);

        FakePlayHead ph;
        ph.bpm = tempo;
        ph.playing = true;
        proc.setPlayHead (&ph);
        proc.startRecording();

        struct Sched { int64_t absSample = 0; juce::MidiMessage msg; double trueBeat = 0.0; };
        std::vector<Sched> sched;
        for (int i = 0; i < 8; ++i)
        {
            const double onBeat = (double) i * 0.5;
            const double offBeat = onBeat + 0.375;
            sched.push_back ({ (int64_t) llround (onBeat * 0.5 * sr),
                               juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), onBeat });
            sched.push_back ({ (int64_t) llround (offBeat * 0.5 * sr),
                               juce::MidiMessage::noteOff (1, 60, (juce::uint8) 0), offBeat });
        }

        juce::AudioBuffer<float> audio (2, block);
        size_t si = 0;
        for (int b = 0; b < 200; ++b)
        {
            ph.ppq = (double) b * (double) block / sr * (tempo / 60.0);
            juce::MidiBuffer mb;
            while (si < sched.size() && sched[si].absSample < (int64_t) (b + 1) * block)
            {
                const int off = (int) (sched[si].absSample - (int64_t) b * block);
                mb.addEvent (sched[si].msg, juce::jlimit (0, block - 1, off));
                ++si;
            }
            proc.processBlock (audio, mb);
        }
        proc.stopRecording();
        proc.setPlayHead (nullptr);

        auto snap = proc.getPerformance().snapshot();
        check ((int) snap.size() == (int) sched.size(), "host-synced take captured all events");
        double maxErrTicks = 0.0;
        for (int i = 0; i < juce::jmin (snap.size(), (int) sched.size()); ++i)
            maxErrTicks = juce::jmax (maxErrTicks,
                std::abs (snap[i].sourceBeat - sched[i].trueBeat)
                * (double) tempogate::ExportRenderer::ticksPerQuarterNote);
        std::printf ("  [debug] host-sync max err=%.2f ticks\n", maxErrTicks);
        check (maxErrTicks < 2.0, "host-synced beats are sample-exact (no distortion)");
    }

    //---- Test W1: transport loop wrap never piles events onto one beat -----------
    // Simulates a host looping a 1-bar region (ppq wraps 4.0 -> 0.0) with a
    // dense stream: every block's events must keep distinct beats and the
    // take clock must keep following wall time through the wrap.
    {
        TempoGateAudioProcessor proc;
        proc.prepareToPlay (sr, block);
        setParam (proc, "sourceTempo", (float) tempo);
        setParam (proc, "followHost", 1.0f);
        setParam (proc, "projectTempo", (float) tempo);
        setParam (proc, "metroOn", 0.0f);
        setParam (proc, "countIn", 0.0f);
        setParam (proc, "waitForTrigger", 0.0f);
        setParam (proc, "recordMode", 0.0f);

        FakePlayHead ph;
        ph.bpm = tempo;
        ph.playing = true;
        proc.setPlayHead (&ph);
        proc.startRecording();

        juce::AudioBuffer<float> audio (2, block);
        const int nBlocks = 250;
        for (int b = 0; b < nBlocks; ++b)
        {
            // 1-bar loop in host ppq, wrapping mid-take
            ph.ppq = std::fmod ((double) b * (double) block / sr * (tempo / 60.0), 4.0);
            juce::MidiBuffer mb;
            for (int k = 0; k < 3; ++k)
                mb.addEvent (juce::MidiMessage::noteOn (1, 60 + k, (juce::uint8) 100),
                             juce::jlimit (0, block - 1, k * 170));
            proc.processBlock (audio, mb);
        }
        proc.stopRecording();
        proc.setPlayHead (nullptr);

        auto snap = proc.getPerformance().snapshot();
        check ((int) snap.size() == nBlocks * 3, "looped stream captured fully");
        bool strictlyIncreasing = true;
        for (int i = 1; i < snap.size(); ++i)
            if (! (snap[i].sourceBeat > snap[i - 1].sourceBeat))
                { strictlyIncreasing = false; break; }
        check (strictlyIncreasing, "no wrap-block pile-ups (beats strictly increase)");
        check (std::abs (proc.getRecordBeat() - (double) nBlocks * beatsPerBlock) < 0.05,
               "take clock follows wall time through ppq wraps");
    }

    //---- Test 13: full export trims leading silence (starts at first event) ----
    // A pickup starting at beat ~2.0 lands at tick 0 with intervals intact,
    // so a DAW drop at the cursor starts with content (never dead space).
    {
        TempoGateAudioProcessor proc;
        proc.prepareToPlay (sr, block);
        setParam (proc, "sourceTempo", (float) tempo);
        setParam (proc, "followHost", 0.0f);
        setParam (proc, "metroOn", 0.0f);
        setParam (proc, "countIn", 0.0f);
        setParam (proc, "waitForTrigger", 0.0f);
        setParam (proc, "recordMode", 0.0f);
        proc.startRecording();

        int64_t absS = 0;
        auto pickup = [] (int b, juce::MidiBuffer& mb)
        {
            if (b == 86)
                mb.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
            if (b == 95)
                mb.addEvent (juce::MidiMessage::noteOff (1, 60, (juce::uint8) 0), 0);
        };
        runBlocks (proc, 120, sr, block, tempo, absS, pickup);
        proc.stopRecording();

        auto obj = proc.buildExportObject (TempoGateAudioProcessor::ExportScope::Full, 1);
        auto midiFile = tempogate::ExportRenderer::renderToMidiFile (obj);
        const juce::MidiMessageSequence* track = midiFile->getTrack (0);
        double onTick = -1.0, offTick = -1.0;
        for (int i = 0; i < track->getNumEvents(); ++i)
        {
            const auto& m = track->getEventPointer (i)->message;
            if (m.getNoteNumber() != 60)
                continue;
            if (m.isNoteOn() && onTick < 0.0)
                onTick = track->getEventTime (i);
            if (m.isNoteOff() && offTick < 0.0)
                offTick = track->getEventTime (i);
        }
        std::printf ("  [debug] pickup on=%.1f off=%.1f (want 0 and ~200)\n", onTick, offTick);
        check (std::abs (onTick - 0.0) < 3.0 && std::abs ((offTick - onTick) - 200.0) < 6.0,
               "exported pickup starts at tick 0 with intervals intact");
    }

    //---- Test 14: bar-selection export starts at the first selected event ------
    // Selected bars 2-2: the first selected note lands at tick 0 even though
    // its take beat is ~4.18 - no dead space in partial exports either.
    {
        TempoGateAudioProcessor proc;
        proc.prepareToPlay (sr, block);
        setParam (proc, "sourceTempo", (float) tempo);
        setParam (proc, "followHost", 0.0f);
        setParam (proc, "metroOn", 0.0f);
        setParam (proc, "countIn", 0.0f);
        setParam (proc, "waitForTrigger", 0.0f);
        setParam (proc, "recordMode", 0.0f);
        proc.startRecording();

        int64_t absS = 0;
        auto take1 = [] (int b, juce::MidiBuffer& mb)
        {
            if (b % 10 == 0)
                mb.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
        };
        runBlocks (proc, 200, sr, block, tempo, absS, take1); // beats 0..4.6
        proc.stopRecording();

        proc.getPerformance().setSelectedBarRange (2, 2);
        auto obj = proc.buildExportObject (TempoGateAudioProcessor::ExportScope::BarSelection, 1);
        auto midiFile = tempogate::ExportRenderer::renderToMidiFile (obj);
        const juce::MidiMessageSequence* track = midiFile->getTrack (0);
        double firstOnTick = -1.0;
        for (int i = 0; i < track->getNumEvents(); ++i)
        {
            const auto& m = track->getEventPointer (i)->message;
            if (m.isNoteOn() && firstOnTick < 0.0)
                firstOnTick = track->getEventTime (i);
        }
        std::printf ("  [debug] bar-sel first tick=%.1f (want 0)\n", firstOnTick);
        check (firstOnTick >= 0.0 && firstOnTick < 3.0,
               "bar selection starts at its first event, no dead space");
    }

    //---- Test 15: from-bar take export starts at its first event -----------------
    // Recording from bar 3 must export starting at tick 0 (trimmed), not with
    // 8 beats of leading silence.
    {
        TempoGateAudioProcessor proc;
        proc.prepareToPlay (sr, block);
        setParam (proc, "sourceTempo", (float) tempo);
        setParam (proc, "followHost", 0.0f);
        setParam (proc, "metroOn", 0.0f);
        setParam (proc, "countIn", 0.0f);
        setParam (proc, "waitForTrigger", 0.0f);
        setParam (proc, "recordMode", 0.0f);
        setParam (proc, "startFromBar", 1.0f);
        proc.getPerformance().setSelectedBarRange (3, 4);
        proc.startRecording();

        int64_t absS = 0;
        auto late = [] (int b, juce::MidiBuffer& mb)
        {
            if (b == 43) // ~1 beat after the 8.0 start
                mb.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
        };
        runBlocks (proc, 60, sr, block, tempo, absS, late);
        proc.stopRecording();

        auto obj = proc.buildExportObject (TempoGateAudioProcessor::ExportScope::Full, 1);
        auto midiFile = tempogate::ExportRenderer::renderToMidiFile (obj);
        const juce::MidiMessageSequence* track = midiFile->getTrack (0);
        double onTick = -1.0;
        for (int i = 0; i < track->getNumEvents(); ++i)
        {
            const auto& m = track->getEventPointer (i)->message;
            if (m.isNoteOn() && m.getNoteNumber() == 60)
                onTick = track->getEventTime (i);
        }
        std::printf ("  [debug] from-bar note tick=%.1f (want 0)\n", onTick);
        check (onTick >= 0.0 && onTick < 3.0,
               "from-bar export trimmed to its first event (no dead space)");
    }

    //---- Test ES: export ignores incoming sample-offset timestamps ------------------
    // Incoming MidiBuffer messages carry their sample offset as a timestamp.
    // Export must use only the recorded beat; otherwise MidiFile::writeTo
    // receives beat-ticks plus that sample offset.
    {
        tempogate::MidiExportObject obj;
        obj.source = tempogate::MidiExportObject::Source::Converted;
        obj.sourceTempo = tempo;
        obj.targetTempo = tempo;

        tempogate::PerfEvent on, off;
        on.message = juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100).withTimeStamp (707.0);
        on.sourceBeat = 4.0;
        off.message = juce::MidiMessage::noteOff (1, 60).withTimeStamp (679.0);
        off.sourceBeat = 4.5;
        obj.selectedEvents.add (on);
        obj.selectedEvents.add (off);

        auto midiFile = tempogate::ExportRenderer::renderToMidiFile (obj);
        const auto* track = midiFile->getTrack (0);
        double onTick = -1.0, offTick = -1.0;
        for (int i = 0; i < track->getNumEvents(); ++i)
        {
            const auto& m = track->getEventPointer (i)->message;
            if (m.isNoteOn() && m.getNoteNumber() == 60) onTick = track->getEventTime (i);
            if (m.isNoteOff() && m.getNoteNumber() == 60) offTick = track->getEventTime (i);
        }
        check (onTick == 0.0 && offTick == 480.0,
               "export ignores MidiBuffer sample-offset timestamps");
    }

    //---- Test HC: clean take reports input health OK --------------------------------
    {
        TempoGateAudioProcessor proc;
        proc.prepareToPlay (sr, block);
        setParam (proc, "sourceTempo", (float) tempo);
        setParam (proc, "followHost", 1.0f);
        setParam (proc, "projectTempo", (float) tempo);
        setParam (proc, "metroOn", 0.0f);
        setParam (proc, "countIn", 0.0f);
        setParam (proc, "waitForTrigger", 0.0f);
        setParam (proc, "recordMode", 0.0f);

        FakePlayHead ph;
        ph.bpm = tempo;
        ph.playing = true;
        proc.setPlayHead (&ph);
        proc.startRecording();

        juce::AudioBuffer<float> audio (2, block);
        for (int b = 0; b < 60; ++b)
        {
            ph.ppq = (double) b * (double) block / sr * (tempo / 60.0);
            juce::MidiBuffer mb;
            if (b % 10 == 0)
                mb.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
            if (b % 10 == 5)
                mb.addEvent (juce::MidiMessage::noteOff (1, 60, (juce::uint8) 0), 0);
            proc.processBlock (audio, mb);
        }
        proc.stopRecording();
        proc.setPlayHead (nullptr);

        check (proc.getHealthInversions() == 0, "clean take: no order inversions");
        check (proc.getHealthZeroLen() == 0, "clean take: no zero-length notes");
        check (proc.getHealthStalls() == 0, "clean take: no callback stalls");
        check (proc.getInputHealth() == 0, "clean take: health OK");
        check (proc.getInputHealthText() == "OK", "clean take: health text OK");
    }

    //---- Test HZ: zero-length notes flag the take ----------------------------------
    {
        TempoGateAudioProcessor proc;
        proc.prepareToPlay (sr, block);
        setParam (proc, "sourceTempo", (float) tempo);
        setParam (proc, "metroOn", 0.0f);
        setParam (proc, "countIn", 0.0f);
        setParam (proc, "waitForTrigger", 0.0f);
        setParam (proc, "recordMode", 0.0f);
        proc.startRecording();

        juce::AudioBuffer<float> audio (2, block);
        juce::MidiBuffer mb;
        mb.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 100);
        mb.addEvent (juce::MidiMessage::noteOff (1, 60, (juce::uint8) 0), 100);
        proc.processBlock (audio, mb);
        proc.stopRecording();

        check (proc.getHealthZeroLen() >= 1, "same-block on/off flags zero-length");
        check (proc.getInputHealth() == 1, "zero-length take: health UNSTABLE");
        check (proc.getInputHealthText().startsWith ("UNSTABLE"),
               "health text reports UNSTABLE with counts");
    }

    //---- Test HS: callback stall flags the take -------------------------------------
    {
        TempoGateAudioProcessor proc;
        proc.prepareToPlay (sr, block);
        setParam (proc, "sourceTempo", (float) tempo);
        setParam (proc, "metroOn", 0.0f);
        setParam (proc, "countIn", 0.0f);
        setParam (proc, "waitForTrigger", 0.0f);
        setParam (proc, "recordMode", 0.0f);
        proc.startRecording();

        juce::AudioBuffer<float> audio (2, block);
        juce::MidiBuffer mb;
        proc.processBlock (audio, mb);
        std::this_thread::sleep_for (std::chrono::milliseconds (250));
        proc.processBlock (audio, mb);
        proc.stopRecording();

        check (proc.getHealthStalls() >= 1, "250 ms gap flags a callback stall");
        check (proc.getInputHealth() == 1, "stalled take: health UNSTABLE");
    }

    //---- Test E1: export ignores message timestamps (nonzero offsets) --------------
    // MidiMessageSequence::addEvent(msg, t) ADDS t to the message's existing
    // timestamp. Recorded messages carry their original block sample offset
    // as timestamp (MidiBuffer round-trip), so the renderer must strip it
    // with withTimeStamp(0.0) - otherwise every event shifts by its offset
    // (non-uniformly, up to a full block). Regression test: nonzero offsets.
    {
        TempoGateAudioProcessor proc;
        proc.prepareToPlay (sr, block);
        setParam (proc, "sourceTempo", (float) tempo);
        setParam (proc, "followHost", 0.0f);
        setParam (proc, "metroOn", 0.0f);
        setParam (proc, "countIn", 0.0f);
        setParam (proc, "waitForTrigger", 0.0f);
        setParam (proc, "recordMode", 0.0f);
        proc.startRecording();

        juce::AudioBuffer<float> audio (2, block);
        for (int b = 0; b < 30; ++b)
        {
            juce::MidiBuffer mb;
            if (b == 10)
                mb.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 300);
            if (b == 20)
                mb.addEvent (juce::MidiMessage::noteOff (1, 60, (juce::uint8) 0), 100);
            proc.processBlock (audio, mb);
        }
        proc.stopRecording();

        auto obj = proc.buildExportObject (TempoGateAudioProcessor::ExportScope::Full, 1);
        auto midiFile = tempogate::ExportRenderer::renderToMidiFile (obj);
        const juce::MidiMessageSequence* track = midiFile->getTrack (0);
        double onTick = -1.0, offTick = -1.0;
        for (int i = 0; i < track->getNumEvents(); ++i)
        {
            const auto& m = track->getEventPointer (i)->message;
            if (m.getNoteNumber() != 60)
                continue;
            if (m.isNoteOn() && onTick < 0.0)
                onTick = track->getEventTime (i);
            if (m.isNoteOff() && offTick < 0.0)
                offTick = track->getEventTime (i);
        }
        // Expected interval from block/sample mapping (NOT plus offsets).
        // Export trims to the first event, so assert on==0 and the interval
        // intact: with the bug, on shifts by ITS offset (300) and off by ITS
        // offset (100), collapsing the interval 214.2 -> 14.2.
        const double wantLen = ((20.0 + 100.0 / 512.0) - (10.0 + 300.0 / 512.0))
                               * beatsPerBlock
                               * (double) tempogate::ExportRenderer::ticksPerQuarterNote;
        std::printf ("  [debug] offset-export on=%.1f (want 0) len=%.1f (want %.1f)\n",
                     onTick, offTick - onTick, wantLen);
        check (std::abs (onTick - 0.0) < 2.0
               && std::abs ((offTick - onTick) - wantLen) < 3.0,
               "file ticks ignore message timestamps (no offset contamination)");
    }

    if (failures == 0) std::printf ("\nALL METRONOME TESTS PASSED\n");
    else               std::printf ("\n%d METRONOME TEST(S) FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
