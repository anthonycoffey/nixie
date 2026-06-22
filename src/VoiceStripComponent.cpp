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

    loadButton.onClick = [this] { chooseSample(); };
    addAndMakeVisible (loadButton);

    sourceLabel.setJustificationType (juce::Justification::centred);
    sourceLabel.setFont (juce::FontOptions (10.0f));
    sourceLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    sourceLabel.setMinimumHorizontalScale (0.6f);
    addAndMakeVisible (sourceLabel);

    levelSlider.setSliderStyle (juce::Slider::LinearVertical);
    levelSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 56, 14);
    addAndMakeVisible (levelSlider);

    auto setupKnob = [this] (juce::Slider& s)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 56, 13);
        s.setDoubleClickReturnValue (true, 0.0); // center detent / reset
        addAndMakeVisible (s);
    };
    setupKnob (panSlider);
    setupKnob (tuneSlider);

    auto setupCaption = [this] (juce::Label& l, const juce::String& t)
    {
        l.setText (t, juce::dontSendNotification);
        l.setJustificationType (juce::Justification::centred);
        l.setFont (juce::FontOptions (9.0f));
        l.setColour (juce::Label::textColourId, juce::Colours::grey);
        addAndMakeVisible (l);
    };
    setupCaption (panCaption,  "PAN");
    setupCaption (tuneCaption, "TUNE");

    muteButton.setClickingTogglesState (true);
    soloButton.setClickingTogglesState (true);
    muteButton.setColour (juce::TextButton::buttonOnColourId, juce::Colours::firebrick);
    soloButton.setColour (juce::TextButton::buttonOnColourId, juce::Colours::goldenrod);
    addAndMakeVisible (muteButton);
    addAndMakeVisible (soloButton);

    const auto id = "v" + juce::String (index);
    levelAtt = std::make_unique<SliderAttachment> (processor.apvts, id + "_level", levelSlider);
    panAtt   = std::make_unique<SliderAttachment> (processor.apvts, id + "_pan",   panSlider);
    tuneAtt  = std::make_unique<SliderAttachment> (processor.apvts, id + "_tune",  tuneSlider);
    muteAtt  = std::make_unique<ButtonAttachment> (processor.apvts, id + "_mute",  muteButton);
    soloAtt  = std::make_unique<ButtonAttachment> (processor.apvts, id + "_solo",  soloButton);

    updateSourceLabel();
}

void VoiceStripComponent::updateSourceLabel()
{
    sourceLabel.setText (processor.getVoiceSourceLabel (index), juce::dontSendNotification);
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

    padButton.setBounds (r.removeFromTop (26));
    r.removeFromTop (3);
    loadButton.setBounds (r.removeFromTop (18));
    sourceLabel.setBounds (r.removeFromTop (13));
    r.removeFromTop (4);

    // Bottom-up: mute/solo row, then tune knob, then pan knob.
    auto bottom = r.removeFromBottom (20);
    muteButton.setBounds (bottom.removeFromLeft (bottom.getWidth() / 2).reduced (1));
    soloButton.setBounds (bottom.reduced (1));
    r.removeFromBottom (4);

    auto tuneArea = r.removeFromBottom (54);
    tuneCaption.setBounds (tuneArea.removeFromBottom (11));
    tuneSlider.setBounds (tuneArea);

    auto panArea = r.removeFromBottom (54);
    panCaption.setBounds (panArea.removeFromBottom (11));
    panSlider.setBounds (panArea);

    r.removeFromBottom (4);

    // Remaining space is the level fader.
    levelSlider.setBounds (r);
}
