#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
// The per-voice sample-slot button: a folder icon when the voice plays the
// factory sound (click to load a file), an "x" when a user sample is loaded
// (click to restore the factory sound). Icons are drawn, not font glyphs, so
// they always render; the action is spelled out in the tooltip.
//==============================================================================
class SampleSlotButton : public juce::Button
{
public:
    SampleSlotButton() : juce::Button ("sample") {}

    void setLoaded (bool isLoaded)
    {
        loaded = isLoaded;
        setTooltip (loaded ? juce::String::fromUTF8 ("Remove sample \xE2\x80\x94 restore the factory sound")
                           : juce::String ("Load a sample (WAV / AIFF)"));
        repaint();
    }
    bool isLoaded() const noexcept { return loaded; }

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        getLookAndFeel().drawButtonBackground (g, *this,
            findColour (juce::TextButton::buttonColourId), over, down);

        auto area = getLocalBounds().toFloat();
        if (loaded)
        {
            // "remove" — an x in a warning tint.
            const auto c = area.getCentre();
            const float s = juce::jmin (area.getWidth(), area.getHeight()) * 0.22f;
            g.setColour (juce::Colours::firebrick.brighter (0.25f));
            g.drawLine (c.x - s, c.y - s, c.x + s, c.y + s, 1.7f);
            g.drawLine (c.x - s, c.y + s, c.x + s, c.y - s, 1.7f);
        }
        else
        {
            // "load" — a folder glyph, drawn directly (not fitted), so it shows at
            // full size or not at all — never truncated to "...".
            g.setColour (juce::Colours::white.withAlpha (0.9f));
            g.setFont (juce::FontOptions (14.0f));
            g.drawText (juce::String::fromUTF8 ("\xF0\x9F\x93\x81"),   // U+1F4C1 file folder
                        getLocalBounds(), juce::Justification::centred);
        }
    }

private:
    bool loaded = false;
};

//==============================================================================
// One vertical mixer strip for a single voice: an audition pad, a sample-slot
// button, the loaded-sample name, a level fader, pan + tune knobs, mute/solo.
// All controls are attached to the processor's APVTS parameters.
//==============================================================================
class VoiceStripComponent : public juce::Component
{
public:
    VoiceStripComponent (LMOneAudioProcessor& proc, int voiceIndex);

    void resized() override;
    void paint (juce::Graphics&) override;

    // Re-read the loaded-sample name from the processor (e.g. after a state restore).
    void refreshSourceLabel() { updateSourceLabel(); }

private:
    void chooseSample();
    void updateSourceLabel();

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    LMOneAudioProcessor& processor;
    int index = 0;
    int midiNote = 0;

    juce::TextButton padButton;
    SampleSlotButton loadButton;
    juce::Label      sourceLabel;
    juce::Slider     levelSlider, panSlider, tuneSlider, swingSlider;
    juce::Label      levelCaption, panCaption, tuneCaption, swingCaption;
    juce::TextButton muteButton { "M" }, soloButton { "S" };

    std::unique_ptr<SliderAttachment> levelAtt, panAtt, tuneAtt, swingAtt;
    std::unique_ptr<ButtonAttachment> muteAtt, soloAtt;
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoiceStripComponent)
};
