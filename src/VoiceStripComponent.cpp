#include "VoiceStripComponent.h"

VoiceStripComponent::VoiceStripComponent (LMOneAudioProcessor& proc, int voiceIndex)
    : processor (proc), index (voiceIndex)
{
    const auto& pad = processor.getPads()[(size_t) index];
    midiNote = pad.midiNote;

    // Top: audition pad (click to play the voice without external MIDI).
    padButton.setButtonText (processor.getChannelName (index));
    padButton.onClick = [this] { processor.keyboardState.noteOn (1, midiNote, 0.9f); };
    addAndMakeVisible (padButton);

    loadButton.onClick = [this]
    {
        if (processor.voiceHasUserSample (index))
        {
            processor.restoreVoiceToFactory (index);   // undo a user load -> factory sound
            updateSourceLabel();
        }
        else
        {
            chooseSample();
        }
    };
    addAndMakeVisible (loadButton);

    sourceLabel.setJustificationType (juce::Justification::centred);
    sourceLabel.setFont (juce::FontOptions (10.0f));
    sourceLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    sourceLabel.setMinimumHorizontalScale (0.6f);
    addAndMakeVisible (sourceLabel);

    levelSlider.setSliderStyle (juce::Slider::LinearVertical);
    levelSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);   // the fader itself is the cue
    addAndMakeVisible (levelSlider);

    auto setupKnob = [this] (juce::Slider& s)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);   // knob pointer is the cue
        s.setDoubleClickReturnValue (true, 0.0); // center detent / reset
        addAndMakeVisible (s);
    };
    setupKnob (panSlider);
    setupKnob (tuneSlider);

    // Shuffle: < > steppers + an LED readout (no knob).
    shufPrev.onClick = [this] { stepShuffle (-1); };
    shufNext.onClick = [this] { stepShuffle (+1); };
    addAndMakeVisible (shufPrev);
    addAndMakeVisible (shufNext);
    shufLed.setFontHeight (9.0f);
    shufLed.onDoubleClick = [this]
    {
        if (auto* p = processor.apvts.getParameter ("v" + juce::String (index) + "_swing"))
            p->setValueNotifyingHost (0.0f);   // index 0 = "Follow"
        refreshShuffle();
    };
    addAndMakeVisible (shufLed);

    auto setupCaption = [this] (juce::Label& l, const juce::String& t)
    {
        l.setText (t, juce::dontSendNotification);
        l.setJustificationType (juce::Justification::centred);
        l.setFont (juce::FontOptions (9.0f));
        l.setColour (juce::Label::textColourId, juce::Colours::grey);
        addAndMakeVisible (l);
    };
    setupCaption (levelCaption, "VOL");
    setupCaption (panCaption,   "PAN");
    setupCaption (tuneCaption,  "TUNE");
    setupCaption (swingCaption, "SHUFFLE");

    muteButton.setClickingTogglesState (true);
    soloButton.setClickingTogglesState (true);
    muteButton.setColour (juce::TextButton::buttonOnColourId, juce::Colours::firebrick);
    soloButton.setColour (juce::TextButton::buttonOnColourId, juce::Colours::goldenrod);
    addAndMakeVisible (muteButton);
    addAndMakeVisible (soloButton);

    outButton.setTooltip ("Route this channel to its own direct out (LUNA multi-out mixer)");
    addAndMakeVisible (outButton);

    const auto id = "v" + juce::String (index);
    levelAtt = std::make_unique<SliderAttachment> (processor.apvts, id + "_level", levelSlider);
    if (auto* p = processor.apvts.getParameter (id + "_level"))   // double-click fader -> default
        levelSlider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
    panAtt   = std::make_unique<SliderAttachment> (processor.apvts, id + "_pan",   panSlider);
    tuneAtt  = std::make_unique<SliderAttachment> (processor.apvts, id + "_tune",  tuneSlider);
    muteAtt  = std::make_unique<ButtonAttachment> (processor.apvts, id + "_mute",  muteButton);
    soloAtt  = std::make_unique<ButtonAttachment> (processor.apvts, id + "_solo",  soloButton);
    outAtt   = std::make_unique<ButtonAttachment> (processor.apvts, id + "_out",   outButton);

    updateSourceLabel();
    refreshShuffle();
}

void VoiceStripComponent::stepShuffle (int delta)
{
    ChoiceParam::step (processor.apvts.getParameter ("v" + juce::String (index) + "_swing"), delta);
    refreshShuffle();
}

void VoiceStripComponent::refreshShuffle()
{
    shufLed.setText (ChoiceParam::name (processor.apvts.getParameter ("v" + juce::String (index) + "_swing")));
}

void VoiceStripComponent::updateSourceLabel()
{
    sourceLabel.setText (processor.getVoiceSourceLabel (index), juce::dontSendNotification);
    loadButton.setLoaded (processor.voiceHasUserSample (index));   // folder vs. remove icon
}

void VoiceStripComponent::chooseSample()
{
    chooser = std::make_unique<juce::FileChooser> (
        "Load sample for " + processor.getPads()[(size_t) index].name,
        juce::File(), "*.wav;*.aif;*.aiff");

    chooser->launchAsync (juce::FileBrowserComponent::openMode
                        | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            const auto f = fc.getResult();
            if (f == juce::File())
                return;                                 // cancelled

            if (processor.loadUserSample (index, f))
                updateSourceLabel();
        });
}

void VoiceStripComponent::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
}

void VoiceStripComponent::resized()
{
    auto r = getLocalBounds().reduced (4);

    padButton.setBounds (r.removeFromTop (50));   // squarer, pad-like
    r.removeFromTop (3);
    loadButton.setBounds (r.removeFromTop (24));   // more padding above/below the icon
    sourceLabel.setBounds (r.removeFromTop (13));
    r.removeFromTop (4);

    // VOL label sits above the fader (labels go above their controls).
    levelCaption.setBounds (r.removeFromTop (11));

    // Bottom-up: mute/solo, shuffle, tune, pan. Each label sits above its control.
    auto bottom = r.removeFromBottom (20);
    const int third = bottom.getWidth() / 3;
    muteButton.setBounds (bottom.removeFromLeft (third).reduced (1));
    soloButton.setBounds (bottom.removeFromLeft (third).reduced (1));
    outButton.setBounds  (bottom.reduced (1));
    r.removeFromBottom (4);

    // Shuffle: SHUFFLE label, LED readout, a small gap, then < > arrows beneath.
    auto shufArea = r.removeFromBottom (46);
    swingCaption.setBounds (shufArea.removeFromTop (11));
    auto shufArrows = shufArea.removeFromBottom (15);
    shufArea.removeFromBottom (4);   // margin above the arrows
    shufLed.setBounds (shufArea.reduced (2, 0));
    {
        auto arrows = shufArrows.withSizeKeepingCentre (44, juce::jmin (shufArrows.getHeight(), 14));
        shufPrev.setBounds (arrows.removeFromLeft (22));
        shufNext.setBounds (arrows);
    }

    auto tuneArea = r.removeFromBottom (58);
    tuneCaption.setBounds (tuneArea.removeFromTop (11));
    tuneSlider.setBounds (tuneArea);

    auto panArea = r.removeFromBottom (58);
    panCaption.setBounds (panArea.removeFromTop (11));
    panSlider.setBounds (panArea);

    r.removeFromBottom (4);

    // Remaining space is the level fader (its VOL label is above it).
    levelSlider.setBounds (r);
}
