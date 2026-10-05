/*
  ==============================================================================
   TempoGate - Editor (MIDI Performance Workspace)
   Workspace UI, drag & drop, commit.
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
// Piano-roll view shared by Source / Result panes.
class PianoRollView : public juce::Component
{
public:
    PianoRollView() = default;

    struct NoteRect { int pitch = 60; double startBeat = 0.0; double lengthBeats = 1.0; };

    void setData (const juce::Array<NoteRect>& notes, double barLenBeats,
                  int numBars, double viewBeats, const juce::String& title,
                  juce::Colour accent, bool ghost = false)
    {
        this->notes = notes;
        this->barLenBeats = juce::jmax (1.0, barLenBeats);
        this->numBars = juce::jmax (1, numBars);
        this->viewBeats = juce::jmax (4.0, viewBeats);
        this->title = title;
        this->accent = accent;
        this->ghost = ghost;
        repaint();
    }

    void setSelection (int first, int last, bool has)
    {
        selFirst = first; selLast = last; hasSel = has; repaint();
    }

    static juce::Array<NoteRect> pairNotes (const juce::Array<tempogate::PerfEvent>& evts)
    {
        juce::Array<NoteRect> out;
        std::map<int, double> open; // pitch -> on-beat
        auto sorted = evts;
        std::sort (sorted.begin(), sorted.end(),
                   [] (auto& a, auto& b) { return a.sourceBeat < b.sourceBeat; });
        for (auto& e : sorted)
        {
            const auto& m = e.message;
            if (m.isNoteOn())
                open[m.getNoteNumber()] = e.sourceBeat;
            else if (m.isNoteOff())
            {
                auto it = open.find (m.getNoteNumber());
                if (it != open.end())
                {
                    NoteRect n;
                    n.pitch = m.getNoteNumber();
                    n.startBeat = it->second;
                    n.lengthBeats = juce::jmax (0.12, e.sourceBeat - it->second);
                    out.add (n);
                    open.erase (it);
                }
            }
        }
        for (auto& kv : open)
        {
            NoteRect n;
            n.pitch = kv.first;
            n.startBeat = kv.second;
            n.lengthBeats = 0.5;
            out.add (n);
        }
        return out;
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff16181d));
        g.fillRoundedRectangle (bounds, 6.0f);

        const float padL = 46.0f, padT = 22.0f, padB = 18.0f, padR = 10.0f;
        auto grid = juce::Rectangle<float> (padL, padT,
                                            bounds.getWidth() - padL - padR,
                                            bounds.getHeight() - padT - padB);
        g.setColour (juce::Colour (0xff0c0d10));
        g.fillRect (grid);

        // title
        g.setColour (juce::Colours::lightgrey);
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText (title, bounds.getX() + 8.0f, bounds.getY() + 4.0f,
                    bounds.getWidth() - 16.0f, 16.0f, juce::Justification::left);

        const int lo = 21, hi = 108;
        auto yFor = [&] (int p)
        {
            double f = (double) (hi - p) / (double) (hi - lo);
            return grid.getY() + (float) f * grid.getHeight();
        };
        auto xFor = [&] (double b)
        { return grid.getX() + (float) (b / viewBeats) * grid.getWidth(); };

        // bar / beat grid
        for (int bar = 0; bar <= numBars; ++bar)
        {
            float x = xFor (bar * barLenBeats);
            g.setColour (bar % 1 == 0 ? juce::Colour (0xff3a3f47) : juce::Colour (0xff26292f));
            g.drawVerticalLine ((int) x, grid.getY(), grid.getBottom());
            g.setColour (juce::Colour (0xff8a8f98));
            g.setFont (juce::FontOptions (10.0f));
            if (bar < numBars)
                g.drawText ("Bar " + juce::String (bar + 1), x + 3.0f, grid.getBottom() + 2.0f,
                            60.0f, 14.0f, juce::Justification::left);
        }

        // selection shade
        if (hasSel)
        {
            float x0 = xFor ((selFirst - 1) * barLenBeats);
            float x1 = xFor (selLast * barLenBeats);
            g.setColour (juce::Colour (0x22ffffff));
            g.fillRect (juce::Rectangle<float> (x0, grid.getY(), x1 - x0, grid.getHeight()));
        }

        // pitch labels
        g.setColour (juce::Colour (0xff6a6f78));
        g.setFont (juce::FontOptions (9.0f));
        for (int p : { 36, 48, 60, 72, 84 })
            g.drawText ("C" + juce::String (p / 12 - 1), 4.0f, yFor (p) - 7.0f, 40.0f, 14.0f,
                        juce::Justification::left);

        // notes
        for (auto& n : notes)
        {
            float x = xFor (n.startBeat);
            float w = (float) (n.lengthBeats / viewBeats) * grid.getWidth();
            float y = yFor (n.pitch);
            float h = juce::jmax (3.0f, grid.getHeight() / (hi - lo) * 1.6f);
            juce::Colour c = ghost ? accent.withAlpha (0.45f) : accent;
            g.setColour (c);
            g.fillRoundedRectangle (juce::Rectangle<float> (x, y - h * 0.5f, juce::jmax (3.0f, w), h), 2.0f);
        }

        if (notes.isEmpty())
        {
            g.setColour (juce::Colour (0xff555a63));
            g.setFont (juce::FontOptions (12.0f, juce::Font::italic));
            g.drawText ("Press RECORD and play at your tempo.", grid,
                        juce::Justification::centred);
        }
    }

private:
    juce::Array<NoteRect> notes;
    double barLenBeats = 4.0, viewBeats = 16.0;
    int numBars = 4;
    juce::String title;
    juce::Colour accent { juce::Colours::skyblue };
    bool ghost = false;
    int selFirst = 1, selLast = 4;
    bool hasSel = false;
};

//==============================================================================
class TempoGateAudioProcessorEditor  : public juce::AudioProcessorEditor,
                                       public juce::Timer,
                                       public juce::ChangeListener,
                                       public juce::DragAndDropContainer
{
public:
    TempoGateAudioProcessorEditor (TempoGateAudioProcessor&);
    ~TempoGateAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;

private:
    //---- actions ---------------------------------------------------------------
    void updateViewButtons();
    void updateExportButtons();
    void refreshAll (bool force = false);
    void doDragExport (TempoGateAudioProcessor::ExportScope scope);
    void doSaveMidiFile (TempoGateAudioProcessor::ExportScope scope);
    void cleanupStaleTempFiles();

    TempoGateAudioProcessor& audioProcessor;

    // header
    juce::Label titleLabel, ratioLabel, hostLabel;

    // tempo section
    juce::Label sourceTempoLabel, projectTempoLabel;
    juce::Slider sourceTempoSlider, projectTempoSlider;
    juce::ToggleButton followHostButton { "Follow Host" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sourceTempoAtt, projectTempoAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> followHostAtt;

    // bar gate section
    juce::Label gateTitleLabel, barLabel, stateLabel;
    juce::ComboBox triggerBox, timeSigNumBox, timeSigDenBox;
    juce::ToggleButton waitButton { "Bar Gate (wait for trigger)" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> triggerAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> waitAtt;

    // metronome section (MIDI click at source tempo while recording)
    juce::Label clickTitleLabel, countInLabel, metroChLabel, metroAccLabel, metroBeatLabel;
    juce::ToggleButton clickButton { "CLICK" };
    juce::ComboBox countInBox;
    juce::Slider metroChSlider, metroAccSlider, metroBeatSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> clickAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> countInAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> metroChAtt, metroAccAtt, metroBeatAtt;

    // workspace
    juce::TextButton originalViewButton { "ORIGINAL" }, convertedViewButton { "CONVERTED" },
                     compareViewButton { "COMPARE" };
    juce::Label takeLabel;
    juce::ComboBox takeBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> takeAtt;
    juce::TextButton recordButton { "RECORD" }, stopButton { "STOP" }, playButton { "PLAY" };
    PianoRollView sourceRoll, resultRoll;

    // selection
    juce::Label selectionLabel;
    juce::Slider selFirstSlider, selLastSlider;
    juce::ToggleButton fromBarButton { "From Bar" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> fromBarAtt;

    // export
    juce::TextButton dragFullButton { "DRAG MIDI TO DAW" };
    juce::TextButton dragSelButton { "DRAG SELECTION" };
    juce::TextButton commitButton { "COMMIT" };
    juce::TextButton saveFileButton { "SAVE .MID" };
    juce::TextButton clearButton { "CLEAR" };
    juce::TextButton exportConvertedButton { "Converted" }, exportOriginalButton { "Original" },
                     exportSelectionButton { "Selection" };
    juce::Label exportLabel, dragHintLabel;

    int lastEventCount = -1;
    double lastRecordBeat = -1.0;
    bool dragArmedFromFull = false;
    bool dragInProgress = false;

    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TempoGateAudioProcessorEditor)
};
