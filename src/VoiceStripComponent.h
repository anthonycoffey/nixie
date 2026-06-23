#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "RetroWidgets.h"

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
// The audition pad at the top of each strip: plays the voice on click and shows
// the instrument name styled like the section labels (orange, bold, fitted).
//==============================================================================
class PadButton : public juce::Button
{
public:
    PadButton() : juce::Button ("pad") {}

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        getLookAndFeel().drawButtonBackground (g, *this,
            findColour (juce::TextButton::buttonColourId), over, down);

        g.setColour (juce::Colour (0xfffc5824));   // orange, like the section labels
        g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
        g.drawFittedText (getButtonText(), getLocalBounds().reduced (3),
                          juce::Justification::centred, 2);
    }
};

//==============================================================================
// Per-channel output routing toggle: off = Main mix, on = this channel's direct
// out (which shows up in LUNA's multi-out mixer). Draws a "→" that lights green.
//==============================================================================
class OutButton : public juce::Button
{
public:
    OutButton() : juce::Button ("out") { setClickingTogglesState (true); }

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        getLookAndFeel().drawButtonBackground (g, *this,
            findColour (juce::TextButton::buttonColourId), over, down);

        auto r = getLocalBounds().toFloat();
        g.setColour (getToggleState() ? juce::Colour (0xff37d067)               // routing to direct
                                      : juce::Colours::grey.withAlpha (0.55f));
        const float cy = r.getCentreY();
        const float x0 = r.getX() + r.getWidth() * 0.30f;
        const float x1 = r.getRight() - r.getWidth() * 0.30f;
        g.drawLine (x0, cy, x1, cy, 1.4f);                 // shaft
        g.drawLine (x1, cy, x1 - 3.0f, cy - 3.0f, 1.4f);   // arrow head
        g.drawLine (x1, cy, x1 - 3.0f, cy + 3.0f, 1.4f);
    }
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

    // Re-read this track's shuffle setting into the LED (driven by the editor timer).
    void refreshShuffle();

private:
    void chooseSample();
    void updateSourceLabel();
    void stepShuffle (int delta);

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    LMOneAudioProcessor& processor;
    int index = 0;
    int midiNote = 0;

    PadButton        padButton;
    SampleSlotButton loadButton;
    juce::Label      sourceLabel;
    juce::Slider     levelSlider, panSlider, tuneSlider;
    juce::Label      levelCaption, panCaption, tuneCaption, swingCaption;
    juce::TextButton muteButton { "M" }, soloButton { "S" };
    OutButton        outButton;   // route this channel to its direct out

    // Per-track shuffle: < > steppers + an LED readout (no knob).
    StepArrow shufPrev { true,  juce::Colour (0xfffc5824) };
    StepArrow shufNext { false, juce::Colour (0xfffc5824) };
    LedText   shufLed;

    std::unique_ptr<SliderAttachment> levelAtt, panAtt, tuneAtt;
    std::unique_ptr<ButtonAttachment> muteAtt, soloAtt, outAtt;
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoiceStripComponent)
};
