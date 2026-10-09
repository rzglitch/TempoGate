/*
  ==============================================================================
   TempoGate - Editor implementation
  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
TempoGateAudioProcessorEditor::TempoGateAudioProcessorEditor (TempoGateAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setSize (940, 760);
    setWantsKeyboardFocus (false);

    //---- header -----------------------------------------------------------------
    titleLabel.setText ("TEMPOGATE MIDI " + juce::String (ProjectInfo::versionString) + " - MIDI Performance Workspace",
                        juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (17.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (titleLabel);

    ratioLabel.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    ratioLabel.setColour (juce::Label::textColourId, juce::Colour (0xff8fd3ff));
    ratioLabel.setJustificationType (juce::Justification::right);
    addAndMakeVisible (ratioLabel);

    hostLabel.setFont (juce::FontOptions (11.0f));
    hostLabel.setColour (juce::Label::textColourId, juce::Colour (0xff9aa0a8));
    hostLabel.setJustificationType (juce::Justification::right);
    addAndMakeVisible (hostLabel);

    //---- tempo -------------------------------------------------------------------
    sourceTempoLabel.setText ("SOURCE TEMPO", juce::dontSendNotification);
    sourceTempoLabel.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    sourceTempoLabel.setColour (juce::Label::textColourId, juce::Colour (0xff9aa0a8));
    addAndMakeVisible (sourceTempoLabel);

    sourceTempoSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    sourceTempoSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 70, 20);
    sourceTempoSlider.setTextValueSuffix (" BPM");
    addAndMakeVisible (sourceTempoSlider);

    projectTempoLabel.setText ("PROJECT TEMPO", juce::dontSendNotification);
    projectTempoLabel.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    projectTempoLabel.setColour (juce::Label::textColourId, juce::Colour (0xff9aa0a8));
    addAndMakeVisible (projectTempoLabel);

    projectTempoSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    projectTempoSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 70, 20);
    projectTempoSlider.setTextValueSuffix (" BPM");
    addAndMakeVisible (projectTempoSlider);

    addAndMakeVisible (followHostButton);

    sourceTempoAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        audioProcessor.apvts, "sourceTempo", sourceTempoSlider);
    projectTempoAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        audioProcessor.apvts, "projectTempo", projectTempoSlider);
    followHostAtt   = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        audioProcessor.apvts, "followHost", followHostButton);

    //---- bar gate -----------------------------------------------------------------
    gateTitleLabel.setText ("BAR GATE", juce::dontSendNotification);
    gateTitleLabel.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    gateTitleLabel.setColour (juce::Label::textColourId, juce::Colour (0xff9aa0a8));
    addAndMakeVisible (gateTitleLabel);

    barLabel.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    barLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (barLabel);

    stateLabel.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    addAndMakeVisible (stateLabel);

    // time signature boxes are plain UI driving APVTS ints
    timeSigNumBox.addItemList ({ "1","2","3","4","5","6","7","8","9","10","11","12" }, 1);
    timeSigNumBox.setSelectedId (4, juce::dontSendNotification);
    timeSigNumBox.onChange = [this]
    {
        if (auto* par = audioProcessor.apvts.getParameter ("timeSigNum"))
            par->setValueNotifyingHost (par->convertTo0to1 ((float) timeSigNumBox.getSelectedId()));
    };
    addAndMakeVisible (timeSigNumBox);

    timeSigDenBox.addItemList ({ "2","4","8","16" }, 1);
    timeSigDenBox.setSelectedId (2, juce::dontSendNotification);
    timeSigDenBox.onChange = [this]
    {
        // choice index = selectedId - 1 -> normalised
        if (auto* par = audioProcessor.apvts.getParameter ("timeSigDen"))
            par->setValueNotifyingHost ((float) (timeSigDenBox.getSelectedId() - 1) / 3.0f);
    };
    addAndMakeVisible (timeSigDenBox);

    triggerBox.addItemList ({ "Any Note-On", "Note C4 (60)", "Velocity >= 100", "Sustain Pedal (CC64)" }, 1);
    triggerBox.setSelectedId (1, juce::dontSendNotification);
    addAndMakeVisible (triggerBox);
    triggerAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        audioProcessor.apvts, "triggerMode", triggerBox);

    addAndMakeVisible (waitButton);
    waitAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        audioProcessor.apvts, "waitForTrigger", waitButton);

    //---- metronome --------------------------------------------------------------------
    clickTitleLabel.setText ("CLICK", juce::dontSendNotification);
    clickTitleLabel.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    clickTitleLabel.setColour (juce::Label::textColourId, juce::Colour (0xff9aa0a8));
    addAndMakeVisible (clickTitleLabel);

    addAndMakeVisible (clickButton);
    clickAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        audioProcessor.apvts, "metroOn", clickButton);

    countInLabel.setText ("Count-In", juce::dontSendNotification);
    countInLabel.setFont (juce::FontOptions (11.0f));
    countInLabel.setColour (juce::Label::textColourId, juce::Colour (0xff9aa0a8));
    addAndMakeVisible (countInLabel);

    countInBox.addItemList ({ "Off", "1 Bar", "2 Bars" }, 1);
    countInBox.setSelectedId (2, juce::dontSendNotification);
    addAndMakeVisible (countInBox);
    countInAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        audioProcessor.apvts, "countIn", countInBox);

    metroChLabel.setText ("Ch", juce::dontSendNotification);
    metroChLabel.setFont (juce::FontOptions (11.0f));
    metroChLabel.setColour (juce::Label::textColourId, juce::Colour (0xff9aa0a8));
    addAndMakeVisible (metroChLabel);

    metroChSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    metroChSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, 18);
    addAndMakeVisible (metroChSlider);
    metroChAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        audioProcessor.apvts, "metroChannel", metroChSlider);

    metroAccLabel.setText ("Acc", juce::dontSendNotification);
    metroAccLabel.setFont (juce::FontOptions (11.0f));
    metroAccLabel.setColour (juce::Label::textColourId, juce::Colour (0xff9aa0a8));
    addAndMakeVisible (metroAccLabel);

    metroAccSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    metroAccSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, 18);
    addAndMakeVisible (metroAccSlider);
    metroAccAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        audioProcessor.apvts, "metroAccent", metroAccSlider);

    metroBeatLabel.setText ("Beat", juce::dontSendNotification);
    metroBeatLabel.setFont (juce::FontOptions (11.0f));
    metroBeatLabel.setColour (juce::Label::textColourId, juce::Colour (0xff9aa0a8));
    addAndMakeVisible (metroBeatLabel);

    metroBeatSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    metroBeatSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, 18);
    addAndMakeVisible (metroBeatSlider);
    metroBeatAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        audioProcessor.apvts, "metroNote", metroBeatSlider);

    //---- workspace -----------------------------------------------------------------
    for (auto* b : { &originalViewButton, &convertedViewButton, &compareViewButton })
    {
        b->setRadioGroupId (1001);
        b->setClickingTogglesState (true);
        addAndMakeVisible (b);
    }
    originalViewButton.onClick = [this]
    { if (auto* par = audioProcessor.apvts.getParameter ("viewMode")) par->setValueNotifyingHost (0.0f); updateViewButtons(); };
    convertedViewButton.onClick = [this]
    { if (auto* par = audioProcessor.apvts.getParameter ("viewMode")) par->setValueNotifyingHost (0.5f); updateViewButtons(); };
    compareViewButton.onClick = [this]
    { if (auto* par = audioProcessor.apvts.getParameter ("viewMode")) par->setValueNotifyingHost (1.0f); updateViewButtons(); };

    takeLabel.setText ("Take:", juce::dontSendNotification);
    takeLabel.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    takeLabel.setColour (juce::Label::textColourId, juce::Colour (0xff9aa0a8));
    takeLabel.setJustificationType (juce::Justification::right);
    addAndMakeVisible (takeLabel);

    takeBox.addItemList ({ "Replace All", "Overdub", "Punch Bars" }, 1);
    takeBox.setSelectedId (1, juce::dontSendNotification);
    addAndMakeVisible (takeBox);
    takeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        audioProcessor.apvts, "recordMode", takeBox);

    recordButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff8c2b2b));
    recordButton.onClick = [this] { audioProcessor.startRecording(); refreshAll (true); };
    addAndMakeVisible (recordButton);

    stopButton.onClick = [this]
    { audioProcessor.stopRecording(); audioProcessor.stopPlayback(); refreshAll (true); };
    addAndMakeVisible (stopButton);

    playButton.onClick = [this]
    {
        if (audioProcessor.isPlayingBack()) audioProcessor.stopPlayback();
        else audioProcessor.startPlayback();
        refreshAll (true);
    };
    addAndMakeVisible (playButton);

    addAndMakeVisible (sourceRoll);
    addAndMakeVisible (resultRoll);
    timelineScrollBar.addListener (this);
    timelineScrollBar.setColour (juce::ScrollBar::backgroundColourId, juce::Colour (0xff0c0d10));
    timelineScrollBar.setColour (juce::ScrollBar::thumbColourId, juce::Colour (0xff3a3f47));
    timelineScrollBar.setColour (juce::ScrollBar::trackColourId, juce::Colour (0xff16181d));
    addAndMakeVisible (timelineScrollBar);
    auto scrollHandler = [this] (float delta)
    {
        const double maxScroll = juce::jmax (0.0, (double) totalBars - visibleBars);
        scrollOffsetBars = juce::jlimit (0.0, maxScroll, scrollOffsetBars + (double) delta * 2.0);
        timelineScrollBar.setCurrentRange (scrollOffsetBars, visibleBars, juce::dontSendNotification);
        refreshAll (true);
    };
    sourceRoll.onScrollDelta = scrollHandler;
    resultRoll.onScrollDelta = scrollHandler;

    // horizontal timeline zoom: Cmd/Ctrl + wheel over a roll, or the +/- buttons
    auto zoomHandler = [this] (double factor, double anchorFrac)
    { setVisibleBars (visibleBars * (factor > 0.0 ? 1.25 : (1.0 / 1.25)), anchorFrac); };
    sourceRoll.onZoomDelta = zoomHandler;
    resultRoll.onZoomDelta = zoomHandler;

    for (auto* b : { &zoomOutButton, &zoomInButton })
    {
        b->setTooltip ("Timeline horizontal zoom (Cmd/Ctrl + scroll over the rolls)");
        addAndMakeVisible (b);
    }
    zoomOutButton.onClick = [this] { setVisibleBars (visibleBars * 1.25, 0.5); };
    zoomInButton.onClick  = [this] { setVisibleBars (visibleBars / 1.25, 0.5); };

    //---- selection -------------------------------------------------------------------
    selectionLabel.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    selectionLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (selectionLabel);

    for (auto* s : { &selFirstSlider, &selLastSlider })
    {
        s->setSliderStyle (juce::Slider::LinearHorizontal);
        s->setTextBoxStyle (juce::Slider::TextBoxRight, false, 50, 18);
        s->setRange (1.0, 16.0, 1.0);
        addAndMakeVisible (s);
    }
    selFirstSlider.setValue (1.0, juce::dontSendNotification);
    selLastSlider.setValue (4.0, juce::dontSendNotification);
    auto selChanged = [this]
    {
        int first = (int) selFirstSlider.getValue();
        int last  = (int) selLastSlider.getValue();
        if (last < first) { last = first; selLastSlider.setValue ((double) last, juce::dontSendNotification); }
        audioProcessor.getPerformance().setSelectedBarRange (first, last);
        refreshAll (true);
    };
    selFirstSlider.onValueChange = selChanged;
    selLastSlider.onValueChange = selChanged;

    fromBarButton.setTooltip ("When on, RECORD starts at the selected bar instead of the top");
    addAndMakeVisible (fromBarButton);
    fromBarAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        audioProcessor.apvts, "startFromBar", fromBarButton);

    healthLabel.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    healthLabel.setJustificationType (juce::Justification::right);
    healthLabel.setTooltip ("Take input timing: order inversions / zero-length notes / audio-callback stalls seen while recording");
    addAndMakeVisible (healthLabel);

    //---- export ------------------------------------------------------------------------
    exportLabel.setText ("EXPORT  (default: Converted -> Project Tempo)", juce::dontSendNotification);
    exportLabel.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    exportLabel.setColour (juce::Label::textColourId, juce::Colour (0xff9aa0a8));
    addAndMakeVisible (exportLabel);

    for (auto* b : { &exportConvertedButton, &exportOriginalButton, &exportSelectionButton })
    {
        b->setRadioGroupId (1002);
        b->setClickingTogglesState (true);
        addAndMakeVisible (b);
    }
    exportConvertedButton.onClick = [this]
    { if (auto* par = audioProcessor.apvts.getParameter ("exportSource")) par->setValueNotifyingHost (0.0f); updateExportButtons(); };
    exportOriginalButton.onClick = [this]
    { if (auto* par = audioProcessor.apvts.getParameter ("exportSource")) par->setValueNotifyingHost (0.5f); updateExportButtons(); };
    exportSelectionButton.onClick = [this]
    { if (auto* par = audioProcessor.apvts.getParameter ("exportSource")) par->setValueNotifyingHost (1.0f); updateExportButtons(); };

    dragFullButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2b5a8c));
    dragFullButton.setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    // NOTE: external OS drag must start from mouseDown/mouseDrag, not onClick.
    dragFullButton.addMouseListener (this, false);
    addAndMakeVisible (dragFullButton);

    dragSelButton.setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    dragSelButton.addMouseListener (this, false);
    addAndMakeVisible (dragSelButton);

    commitButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2b7a3d));
    commitButton.onClick = [this] { audioProcessor.commitToDaw(); refreshAll (true); };
    addAndMakeVisible (commitButton);

    saveFileButton.onClick = [this]
    { doSaveMidiFile (TempoGateAudioProcessor::ExportScope::Full); };
    addAndMakeVisible (saveFileButton);

    clearButton.onClick = [this] { audioProcessor.clearPerformance(); refreshAll (true); };
    addAndMakeVisible (clearButton);

    dragHintLabel.setText ("Drag a MIDI button onto a DAW track to drop a .mid region. "
                           "Commit plays the full Converted performance out as MIDI.",
                           juce::dontSendNotification);
    dragHintLabel.setFont (juce::FontOptions (11.0f, juce::Font::italic));
    dragHintLabel.setColour (juce::Label::textColourId, juce::Colour (0xff8a8f98));
    addAndMakeVisible (dragHintLabel);

    audioProcessor.addChangeListener (this);
    cleanupStaleTempFiles();
    updateViewButtons();
    updateExportButtons();
    refreshAll (true);
    startTimerHz (10);
}

TempoGateAudioProcessorEditor::~TempoGateAudioProcessorEditor()
{
    stopTimer();
    audioProcessor.removeChangeListener (this);
}

//==============================================================================
void TempoGateAudioProcessorEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    refreshAll (true);
}

void TempoGateAudioProcessorEditor::timerCallback()
{
    refreshAll (false);
}

void TempoGateAudioProcessorEditor::updateViewButtons()
{
    int v = audioProcessor.getViewMode();
    originalViewButton.setToggleState (v == 0, juce::dontSendNotification);
    convertedViewButton.setToggleState (v == 1, juce::dontSendNotification);
    compareViewButton.setToggleState (v == 2, juce::dontSendNotification);
}

void TempoGateAudioProcessorEditor::updateExportButtons()
{
    int s = audioProcessor.getExportSource();
    exportConvertedButton.setToggleState (s == 0, juce::dontSendNotification);
    exportOriginalButton.setToggleState (s == 1, juce::dontSendNotification);
    exportSelectionButton.setToggleState (s == 2, juce::dontSendNotification);
}

void TempoGateAudioProcessorEditor::refreshAll (bool force)
{
    int count = audioProcessor.getPerformance().size();
    double rb = audioProcessor.getRecordBeat();
    if (! force && count == lastEventCount && std::abs (rb - lastRecordBeat) < 1e-9)
    {
        // cheap text updates only
    }
    lastEventCount = count;
    lastRecordBeat = rb;

    const double srcT = audioProcessor.getSourceTempoParam();
    const double effT = audioProcessor.getEffectiveProjectTempo();
    const double ratio = (effT > 0.0) ? (srcT / effT) : 1.0;
    ratioLabel.setText ("RATIO  " + juce::String (ratio, 4), juce::dontSendNotification);

    juce::String hostTxt;
    if (audioProcessor.getFollowHost())
        hostTxt = "Host: " + juce::String (audioProcessor.getLastHostTempo(), 1) + " BPM"
                + (audioProcessor.getHostIsPlaying() ? " | PLAYING" : " | STOPPED")
                + " | Project = Host";
    else
        hostTxt = "Project = Manual (" + juce::String (audioProcessor.getProjectTempoParam(), 1) + " BPM)";
    hostLabel.setText (hostTxt, juce::dontSendNotification);

    auto& gate = audioProcessor.getBarGate();
    barLabel.setText ("Current Bar: " + juce::String (gate.getCurrentBar()).paddedLeft ('0', 2)
                      + "   " + juce::String (gate.getNumerator()) + "/"
                      + juce::String (gate.getDenominator()), juce::dontSendNotification);
    auto st = gate.getState();
    if (audioProcessor.isCountingIn())
        st = tempogate::BarGate::State::CountIn;
    else if (audioProcessor.isRecording() && gate.isWaiting())
        st = tempogate::BarGate::State::Waiting;
    else if (audioProcessor.isRecording())
        st = tempogate::BarGate::State::Recording;
    stateLabel.setText ("State: " + tempogate::BarGate::stateToString (st), juce::dontSendNotification);
    stateLabel.setColour (juce::Label::textColourId,
                          (st == tempogate::BarGate::State::Waiting
                           || st == tempogate::BarGate::State::CountIn) ? juce::Colour (0xffffb84d)
                          : st == tempogate::BarGate::State::Recording ? juce::Colour (0xff7de08d)
                          : juce::Colours::white);

    recordButton.setToggleState (audioProcessor.isRecording(), juce::dontSendNotification);
    playButton.setButtonText (audioProcessor.isPlayingBack() ? " STOP PLAY" : " PLAY");

    // selection UI range follows recorded bars
    const double barLen = gate.getBarLengthBeats();
    int numBars = audioProcessor.getPerformance().getNumBars (barLen);
    if (numBars < 1) numBars = 4;
    totalBars = juce::jmax (16, numBars + 1); // allow selecting one past for count-in room

    // While recording, let the timeline grow with the live playhead so silence
    // at the end of a take does not stop the view from following it.
    const double curBar = (barLen > 0.0) ? (audioProcessor.getRecordBeat() / barLen + 1.0) : 1.0;
    if (audioProcessor.isRecording() && barLen > 0.0)
        totalBars = juce::jmax (totalBars, (int) std::floor (curBar) + 2);

    visibleBars = juce::jlimit (2.0, juce::jmax (16.0, (double) totalBars), visibleBars);
    selFirstSlider.setRange (1.0, (double) totalBars, 1.0);
    selLastSlider.setRange (1.0, (double) totalBars, 1.0);

    // Auto-scroll (catch playhead) while recording: when the live playhead
    // moves past the right edge of the visible window, page the view along so
    // the head stays in sight (one bar in from the right).
    const double maxScrollOffset = juce::jmax (0.0, (double) totalBars - visibleBars);
    if (audioProcessor.isRecording() && curBar > scrollOffsetBars + visibleBars)
        scrollOffsetBars = juce::jlimit (0.0, maxScrollOffset, curBar - visibleBars + 1.0);
    scrollOffsetBars = juce::jlimit (0.0, maxScrollOffset, scrollOffsetBars);
    timelineScrollBar.setRangeLimits (0.0, (double) totalBars);
    timelineScrollBar.setCurrentRange (scrollOffsetBars, visibleBars, juce::dontSendNotification);

    int sf = 1, sl = totalBars;
    if (audioProcessor.getPerformance().getHasBarSelection())
        audioProcessor.getPerformance().getBarSelection (sf, sl);
    sf = juce::jlimit (1, totalBars, sf); sl = juce::jlimit (sf, totalBars, sl);
    if ((int) selLastSlider.getValue() < sf || (int) selLastSlider.getValue() > totalBars)
        selLastSlider.setValue ((double) sl, juce::dontSendNotification);
    if ((int) selFirstSlider.getValue() != sf && ! selFirstSlider.isMouseButtonDown())
        selFirstSlider.setValue ((double) sf, juce::dontSendNotification);

    juce::String selPrefix = (audioProcessor.getRecordMode() == 2 ? "Punch - " : "");
    selectionLabel.setText (selPrefix
                            + "Selection: Bar " + juce::String (sf) + "-" + juce::String (sl)
                            + "  |  " + juce::String (count) + " events",
                            juce::dontSendNotification);

    // Take-health indicator: OK, UNSTABLE with counts, or -- when no take.
    if (count > 0)
    {
        const int h = audioProcessor.getInputHealth();
        healthLabel.setText ("In: " + audioProcessor.getInputHealthText(),
                             juce::dontSendNotification);
        healthLabel.setColour (juce::Label::textColourId,
                               h == 0 ? juce::Colour (0xff7de08d)
                                      : juce::Colour (0xffffb84d));
        healthLabel.setTooltip ("Take input timing: order inversions="
                                + juce::String (audioProcessor.getHealthInversions())
                                + " zero-length notes="
                                + juce::String (audioProcessor.getHealthZeroLen())
                                + " audio-callback stalls="
                                + juce::String (audioProcessor.getHealthStalls()));
    }
    else
    {
        healthLabel.setText ("In: --", juce::dontSendNotification);
        healthLabel.setColour (juce::Label::textColourId, juce::Colour (0xff6a6f78));
        healthLabel.setTooltip ("Take input timing: no take recorded yet");
    }

    // piano rolls
    auto evts = audioProcessor.getPerformance().snapshot();
    auto notes = PianoRollView::pairNotes (evts);
    const int viewMode = audioProcessor.getViewMode();

    juce::Colour origCol (0xff8fd3ff), convCol (0xffb49aff);
    int hasSel = 0, sFirst = sf, sLast = sl;
    hasSel = audioProcessor.getPerformance().getHasBarSelection() ? 1 : 0;

    if (viewMode == 0) // Original only
    {
        sourceRoll.setData (notes, barLen, totalBars, scrollOffsetBars, visibleBars, "SOURCE PERFORMANCE (Original)", origCol);
        resultRoll.setData ({}, barLen, totalBars, scrollOffsetBars, visibleBars, "PROJECT RESULT (Converted) - switch view", convCol, true);
    }
    else if (viewMode == 1) // Converted only
    {
        sourceRoll.setData ({}, barLen, totalBars, scrollOffsetBars, visibleBars, "SOURCE PERFORMANCE - switch view", origCol, true);
        resultRoll.setData (notes, barLen, totalBars, scrollOffsetBars, visibleBars,
                            "PROJECT RESULT (Converted @ " + juce::String (effT, 1) + " BPM)", convCol);
    }
    else // Compare
    {
        sourceRoll.setData (notes, barLen, totalBars, scrollOffsetBars, visibleBars, "SOURCE PERFORMANCE (Original)", origCol);
        resultRoll.setData (notes, barLen, totalBars, scrollOffsetBars, visibleBars,
                            "PROJECT RESULT (Converted overlay @ " + juce::String (effT, 1) + " BPM)", convCol);
    }
    sourceRoll.setSelection (sFirst, sLast, hasSel != 0);
    resultRoll.setSelection (sFirst, sLast, hasSel != 0);

    updateViewButtons();
    updateExportButtons();
}

//==============================================================================
// Realtime processing ≠ Workspace export ≠ OS drag source.
// Each drag re-exports a fresh temp .mid, then hands it to the OS.
void TempoGateAudioProcessorEditor::doDragExport (TempoGateAudioProcessor::ExportScope scope)
{
    if (audioProcessor.getPerformance().size() == 0)
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon,
            "TempoGate", "Nothing to drag yet - press RECORD and play first.");
        return;
    }

    juce::File mid = audioProcessor.exportTempFileForDrag (scope);
    if (! mid.existsAsFile())
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
            "TempoGate", "Could not render MIDI file for drag.");
        return;
    }

    juce::StringArray files;
    files.add (mid.getFullPathName());
    // canMoveFiles=false: DAW imports/copies, our temp file stays until cleanup.
    performExternalDragDropOfFiles (files, false, this, nullptr);
}

void TempoGateAudioProcessorEditor::doSaveMidiFile (TempoGateAudioProcessor::ExportScope scope)
{
    if (audioProcessor.getPerformance().size() == 0)
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon,
            "TempoGate", "Nothing to save yet - press RECORD and play first.");
        return;
    }

    fileChooser = std::make_unique<juce::FileChooser> (
        "Save TempoGate MIDI", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                                   .getChildFile ("TempoGate.mid"),
        "*.mid");

    fileChooser->launchAsync (juce::FileBrowserComponent::saveMode
                              | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
        [this, scope] (const juce::FileChooser& fc)
        {
            auto f = fc.getResult().withFileExtension (".mid");
            if (f.getParentDirectory().exists() || f.getParentDirectory().createDirectory())
                if (! audioProcessor.writeExportToFile (scope, f))
                    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                        "TempoGate", "Could not write MIDI file.");
        });
}

void TempoGateAudioProcessorEditor::cleanupStaleTempFiles()
{
    // Best-effort: remove our own drag files older than a day.
    auto tmp = juce::File::getSpecialLocation (juce::File::SpecialLocationType::tempDirectory);
    juce::Array<juce::File> kids;
    tmp.findChildFiles (kids, juce::File::findFiles, false, "TempoGate-*.mid");
    const auto now = juce::Time::getCurrentTime();
    for (auto& f : kids)
        if ((now - f.getLastModificationTime()).inDays() >= 1)
            f.deleteFile();
}

//==============================================================================
void TempoGateAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1e2126));

    // section separators
    g.setColour (juce::Colour (0xff2e3238));
    const float ys[] = { 64.0f, 150.0f, 210.0f, 560.0f, 606.0f };
    for (float y : ys) g.drawHorizontalLine ((int) y, 12.0f, (float) getWidth() - 12.0f);
}

// External OS drag must begin inside a mouseDown/mouseDrag gesture, so the
// DRAG buttons arm here (via the mouse listener registered above) and fire
// on mouseDrag. A plain click only flashes the hint.
void TempoGateAudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    dragInProgress = false;
    if (e.eventComponent == &dragFullButton || e.eventComponent == &dragSelButton)
        dragArmedFromFull = (e.eventComponent == &dragFullButton);
}

void TempoGateAudioProcessorEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (dragInProgress) return;
    if (e.eventComponent == &dragFullButton || e.eventComponent == &dragSelButton)
    {
        dragInProgress = true;
        doDragExport (dragArmedFromFull ? TempoGateAudioProcessor::ExportScope::Full
                                       : TempoGateAudioProcessor::ExportScope::BarSelection);
    }
}

void TempoGateAudioProcessorEditor::resized()
{
    const int W = getWidth();
    auto area = getLocalBounds().reduced (12);
    int y = area.getY();

    titleLabel.setBounds (area.getX(), y, W - 260, 26);
    ratioLabel.setBounds (W - 260, y, 220, 26);
    y += 26;
    hostLabel.setBounds (W - 320, y, 280, 16);
    y += 22; // -> 64 separator

    // tempo row
    y += 8;
    sourceTempoLabel.setBounds (area.getX(), y, 110, 22);
    sourceTempoSlider.setBounds (area.getX() + 110, y, 300, 22);
    projectTempoLabel.setBounds (area.getX() + 430, y, 110, 22);
    projectTempoSlider.setBounds (area.getX() + 540, y, 260, 22);
    followHostButton.setBounds (area.getX() + 806, y, 110, 22);
    y += 30;

    // bar gate row
    gateTitleLabel.setBounds (area.getX(), y, 70, 22);
    timeSigNumBox.setBounds (area.getX() + 76, y, 52, 22);
    timeSigDenBox.setBounds (area.getX() + 132, y, 52, 22);
    barLabel.setBounds (area.getX() + 200, y, 220, 22);
    stateLabel.setBounds (area.getX() + 430, y, 200, 22);
    waitButton.setBounds (area.getX() + 640, y, 276, 22);
    y += 26;

    // metronome row (MIDI click at source tempo + count-in)
    clickTitleLabel.setBounds (area.getX(), y, 50, 22);
    clickButton.setBounds (area.getX() + 54, y, 80, 22);
    countInLabel.setBounds (area.getX() + 144, y, 60, 22);
    countInBox.setBounds (area.getX() + 208, y, 110, 22);
    metroChLabel.setBounds (area.getX() + 330, y, 30, 22);
    metroChSlider.setBounds (area.getX() + 360, y, 130, 22);
    metroAccLabel.setBounds (area.getX() + 500, y, 34, 22);
    metroAccSlider.setBounds (area.getX() + 534, y, 130, 22);
    metroBeatLabel.setBounds (area.getX() + 674, y, 40, 22);
    metroBeatSlider.setBounds (area.getX() + 714, y, 130, 22);
    y += 26;

    // workspace controls row (cursor layout, must fit area width)
    const int wsY = 218;
    int cx = area.getX();
    const int gap = 6;
    auto place = [&] (juce::Component& c, int w)
    { c.setBounds (cx, wsY, w, 24); cx += w + gap; };
    place (originalViewButton, 92);
    place (convertedViewButton, 92);
    place (compareViewButton, 92);
    place (triggerBox, 156);
    place (takeLabel, 40);
    place (takeBox, 112);
    place (recordButton, 100);
    place (stopButton, 80);
    playButton.setBounds (cx, wsY, area.getRight() - cx, 24);

    sourceRoll.setBounds (area.getX(), wsY + 30, area.getWidth(), 144);
    resultRoll.setBounds (area.getX(), wsY + 180, area.getWidth(), 144);

    const int sbX = area.getX() + 46;
    const int zoomW = 28, zoomGap = 4, zoomBlock = zoomW * 2 + zoomGap;
    const int sbW = area.getWidth() - 46 - 16 - zoomBlock;
    timelineScrollBar.setBounds (sbX, wsY + 325, sbW, 12);
    zoomOutButton.setBounds (sbX + sbW + 6, wsY + 322, zoomW, 17);
    zoomInButton.setBounds (zoomOutButton.getRight() + zoomGap, wsY + 322, zoomW, 17);

    // selection row
    const int selY = 566;
    selectionLabel.setBounds (area.getX(), selY, 250, 22);
    selFirstSlider.setBounds (area.getX() + 256, selY, 210, 22);
    selLastSlider.setBounds (area.getX() + 472, selY, 210, 22);
    fromBarButton.setBounds (area.getX() + 688, selY, 100, 22);
    healthLabel.setBounds (area.getX() + 794, selY, 110, 22);

    // export row
    const int exY = 612;
    exportLabel.setBounds (area.getX(), exY, 400, 20);
    exportConvertedButton.setBounds (area.getX() + 410, exY, 100, 20);
    exportOriginalButton.setBounds (area.getX() + 514, exY, 90, 20);
    exportSelectionButton.setBounds (area.getX() + 608, exY, 90, 20);

    const int btnY = 638;
    dragFullButton.setBounds (area.getX(), btnY, 220, 40);
    dragSelButton.setBounds (area.getX() + 228, btnY, 170, 40);
    commitButton.setBounds (area.getX() + 406, btnY, 130, 40);
    saveFileButton.setBounds (area.getX() + 544, btnY, 130, 40);
    clearButton.setBounds (area.getX() + 682, btnY, 90, 40);
    dragHintLabel.setBounds (area.getX(), btnY + 44, area.getWidth(), 34);
}

void TempoGateAudioProcessorEditor::setVisibleBars (double newVisibleBars, double anchorFrac)
{
    const double maxVisible = juce::jmax (16.0, (double) totalBars);
    newVisibleBars = juce::jlimit (2.0, maxVisible, newVisibleBars);
    if (std::abs (newVisibleBars - visibleBars) < 1e-6)
        return;

    // Keep the bar under the anchor point fixed while the window resizes.
    anchorFrac = juce::jlimit (0.0, 1.0, anchorFrac);
    const double anchorBar = scrollOffsetBars + anchorFrac * visibleBars;
    visibleBars = newVisibleBars;
    const double maxScroll = juce::jmax (0.0, (double) totalBars - visibleBars);
    scrollOffsetBars = juce::jlimit (0.0, maxScroll, anchorBar - anchorFrac * visibleBars);
    timelineScrollBar.setCurrentRange (scrollOffsetBars, visibleBars, juce::dontSendNotification);
    refreshAll (true);
}

void TempoGateAudioProcessorEditor::scrollBarMoved (juce::ScrollBar* bar, double newRangeStart)
{
    if (bar == &timelineScrollBar)
    {
        scrollOffsetBars = newRangeStart;
        refreshAll (true);
    }
}
