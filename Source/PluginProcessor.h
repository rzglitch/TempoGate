/*
  ==============================================================================
   TempoGate - MIDI Performance Workspace
   Independent Tempo Recording / Tempo Conversion / MIDI-Triggered Bar Gate
   Workspace + Drag & Drop Export.
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "TempoEngine.h"
#include "BarGate.h"
#include "PerformanceModel.h"
#include "MidiExport.h"

//==============================================================================
class TempoGateAudioProcessor  : public juce::AudioProcessor,
                                 public juce::ChangeBroadcaster
{
public:
    //==============================================================================
    TempoGateAudioProcessor();
    ~TempoGateAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    // Parameters (APVTS ids)
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorValueTreeState apvts { *this, nullptr, "TempoGate", createParameterLayout() };

    double getSourceTempoParam() const;
    double getProjectTempoParam() const;
    bool   getFollowHost() const;
    int    getExportSource() const;   // 0 = Converted (default), 1 = Original, 2 = Selection
    int    getViewMode() const;       // 0 = Original, 1 = Converted, 2 = Compare
    bool   getMidiThru() const;

    // Metronome (MIDI click at source tempo while recording)
    bool   getMetroOn() const;
    int    getCountInBars() const;    // 0 = off, 1, 2
    int    getMetroChannel() const;   // 1-16
    int    getMetroAccentNote() const;
    int    getMetroBeatNote() const;

    // Effective project tempo: host tempo wins when followHost && host valid.
    double getEffectiveProjectTempo() const;
    double getLastHostTempo() const { return lastHostTempo; }
    bool   getHostIsPlaying() const { return hostIsPlaying; }

    //==============================================================================
    // Transport / workspace API used by the editor
    void startRecording();
    void stopRecording();
    void startPlayback();   // preview current view (full performance)
    void stopPlayback();
    void commitToDaw();     // full converted via MIDI output
    void clearPerformance();

    bool isRecording() const { return recording; }
    bool isCountingIn() const { return recording && inCountIn; }
    bool isPlayingBack() const { return playbackActive; }

    tempogate::PerformanceModel& getPerformance() { return performance; }
    const tempogate::PerformanceModel& getPerformance() const { return performance; }
    tempogate::BarGate& getBarGate() { return barGate; }
    tempogate::TempoEngine& getTempoEngine() { return tempoEngine; }

    double getRecordBeat() const { return recordBeat; }

    //==============================================================================
    // Export pipeline - drag and file-save share this code.
    enum class ExportScope { Full, BarSelection, NoteSelection };

    tempogate::MidiExportObject buildExportObject (ExportScope scope, int sourceOverride = -1);
    juce::File exportTempFileForDrag (ExportScope scope, int sourceOverride = -1);
    bool writeExportToFile (ExportScope scope, const juce::File& dest, int sourceOverride = -1);

private:
    //==============================================================================
    void syncEngineFromParams();
    void schedulePlaybackQueue (const juce::Array<tempogate::PerfEvent>& evts);
    void renderPlayback (juce::MidiBuffer& out, int numSamples,
                         double projectTempo, double blockStartProjectBeat,
                         double projectBeatsThisBlock);
    // Metronome: emit MIDI clicks for integer source-beats crossed in
    // (prevBeat, newBeat]. Sample positions are spread across this block.
    void emitClicksForRange (double prevBeat, double newBeat,
                             juce::MidiBuffer& out, int numSamples,
                             int64_t blockStartAbsSample);
    void flushPendingClickOffs (juce::MidiBuffer& out, int numSamples,
                                int64_t blockStartAbsSample);

    tempogate::TempoEngine tempoEngine;
    tempogate::BarGate barGate;
    tempogate::PerformanceModel performance;

    double currentSampleRate = 44100.0;

    // recording clock
    bool recording = false;
    double recordBeat = 0.0;          // accumulated SOURCE beats
    double lastHostPpq = -1.0;
    bool gateWasWaiting = false;

    // metronome / count-in (SOURCE beats; cursor runs negative during pre-roll)
    bool inCountIn = false;
    double clickCursor = 0.0;
    long long lastEmittedClickBeat = -1;
    int64_t absSampleCounter = 0;
    struct PendingOff { int64_t dueAbsSample = 0; juce::MidiMessage msg; };
    std::vector<PendingOff> pendingClickOffs;

    // playback / commit scheduler (PROJECT beats)
    struct ScheduledEvent { double beat = 0.0; juce::MidiMessage msg; };
    std::vector<ScheduledEvent> playbackQueue;
    size_t playbackIndex = 0;
    bool playbackActive = false;
    double playbackBeat = 0.0;        // current position in project beats
    double lastPlaybackHostPpq = -1.0;
    juce::CriticalSection playbackLock;

    // host tracking (message-thread readable)
    double lastHostTempo = 120.0;
    bool hostIsPlaying = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TempoGateAudioProcessor)
};
