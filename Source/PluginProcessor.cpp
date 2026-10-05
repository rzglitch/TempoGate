/*
  ==============================================================================
   TempoGate - Processor implementation
  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout TempoGateAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID ("sourceTempo", 1), "Source Tempo",
        juce::NormalisableRange<float> (40.0f, 240.0f, 0.1f), 90.0f));

    layout.add (std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID ("followHost", 1), "Follow Host Tempo", true));

    layout.add (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID ("projectTempo", 1), "Project Tempo",
        juce::NormalisableRange<float> (40.0f, 240.0f, 0.1f), 120.0f));

    layout.add (std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID ("timeSigNum", 1), "Time Sig Numerator", 1, 12, 4));

    layout.add (std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID ("timeSigDen", 1), "Time Sig Denominator",
        juce::StringArray ({ "2", "4", "8", "16" }), 1));

    layout.add (std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID ("triggerMode", 1), "Bar Trigger",
        juce::StringArray ({ "Any Note-On", "Note C4 (60)", "Velocity >= 100", "Sustain Pedal (CC64)" }), 0));

    layout.add (std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID ("waitForTrigger", 1), "Wait For Trigger (Bar Gate)", true));

    // 0 = Converted (default), 1 = Original, 2 = Selection
    layout.add (std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID ("exportSource", 1), "Export Source",
        juce::StringArray ({ "Converted", "Original", "Selection" }), 0));

    // 0 = Original, 1 = Converted, 2 = Compare
    layout.add (std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID ("viewMode", 1), "Workspace View",
        juce::StringArray ({ "Original", "Converted", "Compare" }), 1));

    layout.add (std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID ("midiThru", 1), "MIDI Thru", true));

    // Metronome: MIDI click at the configured (source) tempo while recording.
    layout.add (std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID ("metroOn", 1), "Click", true));

    layout.add (std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID ("countIn", 1), "Count-In",
        juce::StringArray ({ "Off", "1 Bar", "2 Bars" }), 1));

    layout.add (std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID ("metroChannel", 1), "Click Channel", 1, 16, 10));

    layout.add (std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID ("metroAccent", 1), "Click Accent Note", 0, 127, 76));

    layout.add (std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID ("metroNote", 1), "Click Beat Note", 0, 127, 77));

    return layout;
}

//==============================================================================
TempoGateAudioProcessor::TempoGateAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
    syncEngineFromParams();
}

TempoGateAudioProcessor::~TempoGateAudioProcessor()
{
}

//==============================================================================
double TempoGateAudioProcessor::getSourceTempoParam() const
{
    if (auto* p = apvts.getRawParameterValue ("sourceTempo")) return (double) *p;
    return 90.0;
}

double TempoGateAudioProcessor::getProjectTempoParam() const
{
    if (auto* p = apvts.getRawParameterValue ("projectTempo")) return (double) *p;
    return 120.0;
}

bool TempoGateAudioProcessor::getFollowHost() const
{
    if (auto* p = apvts.getRawParameterValue ("followHost")) return *p > 0.5f;
    return true;
}

int TempoGateAudioProcessor::getExportSource() const
{
    if (auto* p = apvts.getRawParameterValue ("exportSource")) return (int) *p;
    return 0;
}

int TempoGateAudioProcessor::getViewMode() const
{
    if (auto* p = apvts.getRawParameterValue ("viewMode")) return (int) *p;
    return 1;
}

bool TempoGateAudioProcessor::getMidiThru() const
{
    if (auto* p = apvts.getRawParameterValue ("midiThru")) return *p > 0.5f;
    return true;
}

bool TempoGateAudioProcessor::getMetroOn() const
{
    if (auto* p = apvts.getRawParameterValue ("metroOn")) return *p > 0.5f;
    return true;
}

int TempoGateAudioProcessor::getCountInBars() const
{
    if (auto* p = apvts.getRawParameterValue ("countIn")) return (int) *p;
    return 1;
}

int TempoGateAudioProcessor::getMetroChannel() const
{
    if (auto* p = apvts.getRawParameterValue ("metroChannel")) return (int) *p;
    return 10;
}

int TempoGateAudioProcessor::getMetroAccentNote() const
{
    if (auto* p = apvts.getRawParameterValue ("metroAccent")) return (int) *p;
    return 76;
}

int TempoGateAudioProcessor::getMetroBeatNote() const
{
    if (auto* p = apvts.getRawParameterValue ("metroNote")) return (int) *p;
    return 77;
}

double TempoGateAudioProcessor::getEffectiveProjectTempo() const
{
    if (getFollowHost() && lastHostTempo > 0.0)
        return lastHostTempo;
    return getProjectTempoParam();
}

void TempoGateAudioProcessor::syncEngineFromParams()
{
    tempoEngine.setSourceTempo (getSourceTempoParam());
    tempoEngine.setProjectTempo (getEffectiveProjectTempo());

    int num = 4;
    if (auto* p = apvts.getRawParameterValue ("timeSigNum")) num = (int) *p;
    int denChoice = 1;
    if (auto* p = apvts.getRawParameterValue ("timeSigDen")) denChoice = (int) *p;
    int den = (denChoice == 0 ? 2 : denChoice == 1 ? 4 : denChoice == 2 ? 8 : 16);
    barGate.setTimeSignature (num, den);

    if (auto* p = apvts.getRawParameterValue ("triggerMode")) barGate.setTriggerMode ((int) *p);
    barGate.setWaitForTrigger (getFollowHost() ? true : true); // gate armed independently below
    if (auto* p = apvts.getRawParameterValue ("waitForTrigger")) barGate.setWaitForTrigger (*p > 0.5f);
}

//==============================================================================
const juce::String TempoGateAudioProcessor::getName() const { return JucePlugin_Name; }
bool TempoGateAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}
bool TempoGateAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}
bool TempoGateAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}
double TempoGateAudioProcessor::getTailLengthSeconds() const { return 0.0; }
int TempoGateAudioProcessor::getNumPrograms() { return 1; }
int TempoGateAudioProcessor::getCurrentProgram() { return 0; }
void TempoGateAudioProcessor::setCurrentProgram (int) {}
const juce::String TempoGateAudioProcessor::getProgramName (int) { return {}; }
void TempoGateAudioProcessor::changeProgramName (int, const juce::String&) {}

//==============================================================================
void TempoGateAudioProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = (sampleRate > 0.0 ? sampleRate : 44100.0);
    lastHostPpq = -1.0;
    lastPlaybackHostPpq = -1.0;
    pendingClickOffs.clear();
}

void TempoGateAudioProcessor::releaseResources() {}

//==============================================================================
void TempoGateAudioProcessor::startRecording()
{
    syncEngineFromParams();
    performance.clear();
    performance.selectAllBars (barGate.getBarLengthBeats());
    barGate.reset();
    barGate.startRecording();
    barGate.clearPausedBeats();
    recordBeat = 0.0;
    lastHostPpq = -1.0;
    gateWasWaiting = false;
    pendingClickOffs.clear();

    // Count-in pre-roll: the click cursor runs negative and recording of
    // incoming MIDI starts once it reaches beat 0.
    const double preRoll = (double) getCountInBars() * barGate.getBarLengthBeats();
    inCountIn = (preRoll > 0.0);
    clickCursor = inCountIn ? -preRoll : 0.0;
    lastEmittedClickBeat = (long long) std::ceil (clickCursor - 1e-9) - 1;

    recording = true;
    stopPlayback();
    sendChangeMessage();
}

void TempoGateAudioProcessor::stopRecording()
{
    recording = false;
    inCountIn = false;
    barGate.stop();
    performance.selectAllBars (barGate.getBarLengthBeats());
    sendChangeMessage();
}

void TempoGateAudioProcessor::schedulePlaybackQueue (const juce::Array<tempogate::PerfEvent>& evts)
{
    const juce::ScopedLock sl (playbackLock);
    playbackQueue.clear();
    playbackIndex = 0;

    double base = 0.0;
    if (! evts.isEmpty())
    {
        base = evts.getReference (0).sourceBeat;
        for (auto& e : evts) base = juce::jmin (base, e.sourceBeat);
    }
    for (auto& e : evts)
    {
        ScheduledEvent s;
        s.beat = juce::jmax (0.0, e.sourceBeat - base);
        s.msg = e.message;
        playbackQueue.push_back (s);
    }
    std::sort (playbackQueue.begin(), playbackQueue.end(),
               [] (auto& a, auto& b) { return a.beat < b.beat; });

    playbackBeat = 0.0;
    playbackActive = ! playbackQueue.empty();
    lastPlaybackHostPpq = -1.0;
}

void TempoGateAudioProcessor::startPlayback()
{
    syncEngineFromParams();
    schedulePlaybackQueue (performance.getBarSelectionEvents());
    sendChangeMessage();
}

void TempoGateAudioProcessor::stopPlayback()
{
    const juce::ScopedLock sl (playbackLock);
    playbackActive = false;
    playbackQueue.clear();
    playbackIndex = 0;
    sendChangeMessage();
}

// full converted performance out to the DAW.
void TempoGateAudioProcessor::commitToDaw()
{
    syncEngineFromParams();
    schedulePlaybackQueue (performance.getAll());
    sendChangeMessage();
}

void TempoGateAudioProcessor::clearPerformance()
{
    performance.clear();
    stopPlayback();
    sendChangeMessage();
}

//==============================================================================
void TempoGateAudioProcessor::renderPlayback (juce::MidiBuffer& out, int numSamples,
                                              double projectTempo,
                                              double blockStartProjectBeat,
                                              double projectBeatsThisBlock)
{
    const juce::ScopedLock sl (playbackLock);
    if (! playbackActive) return;

    const double blockEnd = blockStartProjectBeat + projectBeatsThisBlock;
    while (playbackIndex < playbackQueue.size())
    {
        auto& ev = playbackQueue[playbackIndex];
        if (ev.beat < blockStartProjectBeat)
        {
            // Late event (e.g. tempo jumped) - emit immediately.
            out.addEvent (ev.msg, 0);
            ++playbackIndex;
            continue;
        }
        if (ev.beat > blockEnd)
            break;

        double frac = (projectBeatsThisBlock > 0.0)
                        ? ((ev.beat - blockStartProjectBeat) / projectBeatsThisBlock) : 0.0;
        int samplePos = juce::jlimit (0, juce::jmax (0, numSamples - 1),
                                      (int) std::round (frac * numSamples));
        out.addEvent (ev.msg, samplePos);
        ++playbackIndex;
    }

    if (playbackIndex >= playbackQueue.size())
    {
        playbackActive = false;
        playbackQueue.clear();
        playbackIndex = 0;
        juce::MessageManager::callAsync ([this] { sendChangeMessage(); });
    }
    (void) projectTempo;
}

//==============================================================================
// Metronome: MIDI clicks on the source-tempo grid. Accent on bar downbeats,
// GM wood-block defaults (76/77) on the chosen channel. Note-offs are 45 ms
// later; offs landing past the block end are carried to the next block so no
// click note can get stuck.
void TempoGateAudioProcessor::flushPendingClickOffs (juce::MidiBuffer& out, int numSamples,
                                                     int64_t blockStartAbsSample)
{
    if (pendingClickOffs.empty())
        return;

    const int64_t blockEnd = blockStartAbsSample + numSamples;
    std::vector<PendingOff> keep;
    keep.reserve (pendingClickOffs.size());

    for (auto& p : pendingClickOffs)
    {
        if (p.dueAbsSample < blockEnd)
            out.addEvent (p.msg, juce::jlimit (0, numSamples - 1,
                                                (int) (p.dueAbsSample - blockStartAbsSample)));
        else
            keep.push_back (p);
    }
    pendingClickOffs.swap (keep);
}

void TempoGateAudioProcessor::emitClicksForRange (double prevBeat, double newBeat,
                                                  juce::MidiBuffer& out, int numSamples,
                                                  int64_t blockStartAbsSample)
{
    if (! getMetroOn() || numSamples <= 0)
    {
        // Keep the cursor moving so enabling mid-take doesn't burst old beats.
        if (numSamples > 0 && newBeat > prevBeat)
            lastEmittedClickBeat = (long long) std::floor (newBeat + 1e-9);
        return;
    }
    if (! (newBeat > prevBeat))
        return;

    const double barLen = barGate.getBarLengthBeats();
    const int channel = juce::jlimit (1, 16, getMetroChannel());
    const int accentNote = juce::jlimit (0, 127, getMetroAccentNote());
    const int beatNote = juce::jlimit (0, 127, getMetroBeatNote());
    const double span = newBeat - prevBeat;
    const int offSamples = (int) std::round (0.045 * currentSampleRate);

    // lastEmittedClickBeat already deduplicates across blocks (including the
    // frozen-clock case, where newBeat == prevBeat emits nothing), so every
    // integer beat up to newBeat is emitted exactly once - beat 0 included.
    for (long long k = lastEmittedClickBeat + 1; (double) k <= newBeat + 1e-9; ++k)
    {
        double frac = ((double) k - prevBeat) / span;
        frac = juce::jlimit (0.0, 1.0, frac);
        const int onSample = juce::jlimit (0, numSamples - 1,
                                           (int) std::round (frac * (numSamples - 1)));

        // Downbeat (works for negative count-in beats and fractional bars).
        double rem = std::fmod ((double) k, barLen);
        const bool downbeat = (std::abs (rem) < 1e-9) || (std::abs (rem - barLen) < 1e-9);
        const int note = downbeat ? accentNote : beatNote;
        const juce::uint8 vel = (juce::uint8) (downbeat ? 120 : 96);

        out.addEvent (juce::MidiMessage::noteOn (channel, note, vel), onSample);

        juce::MidiMessage off = juce::MidiMessage::noteOff (channel, note, (juce::uint8) 0);
        const int64_t dueAbs = blockStartAbsSample + onSample + juce::jmax (1, offSamples);
        if (dueAbs < blockStartAbsSample + numSamples)
            out.addEvent (off, (int) (dueAbs - blockStartAbsSample));
        else
            pendingClickOffs.push_back ({ dueAbs, off });

        lastEmittedClickBeat = k;
    }
}

//==============================================================================
void TempoGateAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    for (auto i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    syncEngineFromParams();

    //---- host position ---------------------------------------------------------
    double hostBpm = -1.0, hostPpq = -1.0;
    bool playing = false;
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) hostBpm = *b;
            playing = pos->getIsPlaying();
            if (auto q = pos->getPpqPosition()) hostPpq = *q;
        }
    }
    if (hostBpm > 0.0) lastHostTempo = hostBpm;
    hostIsPlaying = playing;

    const double sourceTempo = tempoEngine.getSourceTempo();
    const double projectTempo = getEffectiveProjectTempo();
    const int numSamples = buffer.getNumSamples();
    const double blockSeconds = (currentSampleRate > 0.0) ? ((double) numSamples / currentSampleRate) : 0.0;

    // Project-beat window of this block (for the commit/playback scheduler).
    double blockStartProjectBeat = playbackBeat;
    double projectBeatsThisBlock = 0.0;
    bool useHostPpqForPlayback = (playing && hostPpq >= 0.0 && lastPlaybackHostPpq >= 0.0);
    if (useHostPpqForPlayback)
    {
        blockStartProjectBeat = playbackBeat;
        projectBeatsThisBlock = juce::jmax (0.0, hostPpq - lastPlaybackHostPpq);
    }
    else
    {
        projectBeatsThisBlock = blockSeconds * projectTempo / 60.0;
    }
    if (playbackActive && playing && hostPpq >= 0.0 && lastPlaybackHostPpq < 0.0)
    {
        // First block synced to host: anchor without jumping.
        lastPlaybackHostPpq = hostPpq;
        projectBeatsThisBlock = blockSeconds * projectTempo / 60.0;
        useHostPpqForPlayback = false;
    }

    //---- collect incoming -------------------------------------------------------
    struct Incoming { juce::MidiMessage msg; int sample = 0; };
    std::vector<Incoming> incoming;
    incoming.reserve (64);
    for (const auto meta : midiMessages)
        incoming.push_back ({ meta.getMessage(), meta.samplePosition });

    //---- recording + click clock ---------------------------------------------------
    // liveDelta: wall-clock advance in SOURCE beats for this block. It is
    // computed even while the gate is waiting so the clock can resume
    // mid-block once a trigger arrives (the freeze is applied below).
    const int64_t blockStartAbs = absSampleCounter;
    const double blockStartBeat = recordBeat;
    double liveDelta = 0.0;
    if (recording)
    {
        if (playing && hostPpq >= 0.0 && lastHostPpq >= 0.0)
        {
            double projectDelta = juce::jmax (0.0, hostPpq - lastHostPpq);
            liveDelta = projectDelta * sourceTempo / juce::jmax (1.0, projectTempo);
        }
        else
        {
            liveDelta = blockSeconds * sourceTempo / 60.0;
        }
    }
    if (playing && hostPpq >= 0.0) lastHostPpq = hostPpq;

    // No MIDI is captured during the count-in pre-roll; the click runs alone
    // and the gate is untouched until the pre-roll ends.
    const bool captureThisBlock = recording && ! inCountIn;
    const double prevClickCursor = clickCursor;

    const double barLen = barGate.getBarLengthBeats();
    const double rate = (numSamples > 0) ? liveDelta / (double) numSamples : 0.0;
    auto wallSecsFor = [&] (double beats)
    { return beats * 60.0 / juce::jmax (1.0, sourceTempo); };
    auto barIndexFor = [&] (double beats)
    { return juce::jmax (1, (int) (beats / barLen) + 1); };

    // Sweep state: the clock may freeze at a barline mid-block and resume on a
    // trigger later in the same block. frozenBeat tracks the freeze point.
    double frozenBeat = blockStartBeat;
    bool resumedThisBlock = false;
    int resumeSample = 0;
    double resumeBeat = 0.0;

    //---- rebuild output ----------------------------------------------------------
    midiMessages.clear();
    flushPendingClickOffs (midiMessages, numSamples, blockStartAbs);
    const bool thru = getMidiThru();

    for (auto& in : incoming)
    {
        auto m = in.msg;

        if (thru)
            midiMessages.addEvent (m, in.sample);

        if (! captureThisBlock)
            continue;

        //--- gate waiting: this event can only release the gate -----------------
        // The clock is frozen at frozenBeat (== recordBeat, the barline).
        if (barGate.isWaiting())
        {
            if (barGate.handleMidiTrigger (m))
            {
                // Resume exactly at the frozen barline: the trigger becomes
                // the downbeat of the pending bar - no bar is skipped and no
                // waiting time leaks into the take (§11).
                const double B = (double) (barGate.getCurrentBar() - 1) * barLen;
                performance.addEvent (m, B, wallSecsFor (B), barGate.getCurrentBar());
                resumedThisBlock = true;
                resumeSample = in.sample;
                resumeBeat = B;
                frozenBeat = B;
                juce::MessageManager::callAsync ([this] { sendChangeMessage(); });
            }
            else if (m.isNoteOn())
            {
                // Noodling while waiting: heard live (thru above), not captured.
                // Only the trigger starts the bar.
            }
            else
            {
                // Closers (note-offs, CC, ...) must still be captured at the
                // frozen beat so notes held across the barline can't stick.
                performance.addEvent (m, frozenBeat, wallSecsFor (frozenBeat),
                                      juce::jmax (1, barGate.getCurrentBar() - 1));
            }
            continue;
        }

        //--- clock running: map the event onto the source-beat grid -------------
        const double baseBeat = resumedThisBlock ? resumeBeat : blockStartBeat;
        const double baseSample = resumedThisBlock ? (double) resumeSample : 0.0;
        const double eb = baseBeat + ((double) in.sample - baseSample) * rate;

        // Keep the bar tracker fresh; latch when this event crosses a barline.
        auto adv = barGate.advance (eb + 1e-9);
        if (adv.boundaryCrossed && barGate.isWaiting())
        {
            // The barline fell inside this block: freeze there and treat this
            // very event as a trigger candidate, so playing across the barline
            // continues into the next bar instead of losing the note.
            frozenBeat = adv.boundaryBeat;
            if (barGate.handleMidiTrigger (m))
            {
                const double B = adv.boundaryBeat;
                performance.addEvent (m, B, wallSecsFor (B), barGate.getCurrentBar());
                resumedThisBlock = true;
                resumeSample = in.sample;
                resumeBeat = B;
                juce::MessageManager::callAsync ([this] { sendChangeMessage(); });
            }
            else if (m.isNoteOn())
            {
                // Swallowed (still heard via thru); the next trigger starts the bar.
                juce::MessageManager::callAsync ([this] { sendChangeMessage(); });
            }
            else
            {
                performance.addEvent (m, frozenBeat, wallSecsFor (frozenBeat),
                                      juce::jmax (1, barGate.getCurrentBar() - 1));
            }
            continue;
        }

        performance.addEvent (m, eb, wallSecsFor (eb), barIndexFor (eb));
    }

    //--- finalize the recording clock ----------------------------------------------
    if (captureThisBlock)
    {
        if (resumedThisBlock)
            recordBeat = resumeBeat + ((double) numSamples - (double) resumeSample) * rate;
        else if (barGate.isWaiting())
            recordBeat = frozenBeat;
        else
        {
            // Clock ran the whole block: catch a barline crossed in silence
            // (no events to trip the per-event scan), so waiting time stays
            // out of the take (§11).
            const double newBeat = blockStartBeat + liveDelta;
            auto adv = barGate.advance (newBeat);
            if (adv.boundaryCrossed && barGate.isWaiting())
            {
                recordBeat = adv.boundaryBeat;
                juce::MessageManager::callAsync ([this] { sendChangeMessage(); });
            }
            else
                recordBeat = newBeat;
        }
    }

    //---- metronome + count-in transition (same source-tempo clock) -----------------
    if (recording)
    {
        if (inCountIn)
            clickCursor += liveDelta;
        else
            clickCursor = recordBeat;

        emitClicksForRange (prevClickCursor, clickCursor, midiMessages, numSamples, blockStartAbs);

        if (inCountIn && clickCursor >= 0.0)
        {
            inCountIn = false;
            recordBeat = clickCursor; // keep click grid aligned with recorded beats
            clickCursor = recordBeat;
            juce::MessageManager::callAsync ([this] { sendChangeMessage(); });
        }
    }

    absSampleCounter = blockStartAbs + numSamples;

    //---- playback / commit scheduler ----------------------------------------------
    if (playbackActive)
    {
        renderPlayback (midiMessages, numSamples, projectTempo,
                        blockStartProjectBeat, projectBeatsThisBlock);
        if (useHostPpqForPlayback && hostPpq >= 0.0)
            lastPlaybackHostPpq = hostPpq;
        else
            playbackBeat = blockStartProjectBeat + projectBeatsThisBlock;
    }
    else if (playing && hostPpq >= 0.0)
    {
        lastPlaybackHostPpq = hostPpq;
        playbackBeat = hostPpq; // keep anchored for the next commit
    }

    (void) gateWasWaiting;
}

//==============================================================================
bool TempoGateAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* TempoGateAudioProcessor::createEditor()
{
    return new TempoGateAudioProcessorEditor (*this);
}

//==============================================================================
tempogate::MidiExportObject TempoGateAudioProcessor::buildExportObject (ExportScope scope, int sourceOverride)
{
    syncEngineFromParams();
    int srcChoice = (sourceOverride >= 0 ? sourceOverride : getExportSource());
    // exportSource: 0 = Converted, 1 = Original, 2 = Selection(->Converted of selection)
    auto src = (srcChoice == 1) ? tempogate::MidiExportObject::Source::Original
                                : tempogate::MidiExportObject::Source::Converted;

    const double srcTempo = tempoEngine.getSourceTempo();
    const double tgtTempo = tempoEngine.getProjectTempo();
    const int num = barGate.getNumerator(), den = barGate.getDenominator();

    switch (scope)
    {
        case ExportScope::BarSelection:
            return tempogate::ExportRenderer::makeBarSelectionExport (
                performance, src, srcTempo, tgtTempo, num, den);
        case ExportScope::NoteSelection:
            return tempogate::ExportRenderer::makeNoteSelectionExport (
                performance, src, srcTempo, tgtTempo, num, den);
        case ExportScope::Full:
        default:
            return tempogate::ExportRenderer::makeFullExport (
                performance, src, srcTempo, tgtTempo, num, den);
    }
}

juce::File TempoGateAudioProcessor::exportTempFileForDrag (ExportScope scope, int sourceOverride)
{
    auto obj = buildExportObject (scope, sourceOverride);
    juce::String prefix = (obj.source == tempogate::MidiExportObject::Source::Converted)
                              ? "TempoGate-Converted" : "TempoGate-Original";
    return tempogate::ExportRenderer::writeToTempFile (obj, prefix);
}

bool TempoGateAudioProcessor::writeExportToFile (ExportScope scope, const juce::File& dest, int sourceOverride)
{
    auto obj = buildExportObject (scope, sourceOverride);
    tempogate::ExportRenderer::writeToFile (obj, dest.withFileExtension (".mid"));
    return dest.withFileExtension (".mid").existsAsFile();
}

//==============================================================================
void TempoGateAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); true)
    {
        if (auto xml = state.createXml())
            copyXmlToBinary (*xml, destData);
    }
}

void TempoGateAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));

    syncEngineFromParams();
    sendChangeMessage();
}

//==============================================================================
bool TempoGateAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif
    return true;
  #endif
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TempoGateAudioProcessor();
}
