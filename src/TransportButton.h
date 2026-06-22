#pragma once

#include <JuceHeader.h>

//==============================================================================
// A vintage-style transport button: a rectangular labelled button with a small
// LED indicator above it. The label and body never change with state — the LED
// is the only state cue (lit = active). The owner wires onClick and drives the
// lamp via setLedOn().
//==============================================================================
class TransportButton : public juce::Button
{
public:
    TransportButton (const juce::String& labelText, juce::Colour ledColour)
        : juce::Button (labelText), text (labelText), led (ledColour) {}

    void setLedOn (bool shouldBeOn)
    {
        if (shouldBeOn != ledOn) { ledOn = shouldBeOn; repaint(); }
    }

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        auto b = getLocalBounds();
        auto ledRow = b.removeFromTop (juce::jmin (12, b.getHeight() / 3));

        // Indicator lamp.
        const float d = juce::jmin (7.0f, (float) ledRow.getHeight() - 1.0f);
        juce::Rectangle<float> dot (0.0f, 0.0f, d, d);
        dot.setCentre (ledRow.toFloat().getCentre());
        if (ledOn)
        {
            g.setColour (led.withAlpha (0.30f));
            g.fillEllipse (dot.expanded (3.0f));            // glow
        }
        g.setColour (ledOn ? led : led.withAlpha (0.16f));
        g.fillEllipse (dot);
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.drawEllipse (dot, 0.8f);

        // Rectangular body (subtle bevel — vintage).
        auto body = b.reduced (1).toFloat();
        auto base = juce::Colour (0xff2c2926);
        if (down)      base = base.darker (0.25f);
        else if (over) base = base.brighter (0.10f);
        g.setGradientFill (juce::ColourGradient (base.brighter (0.18f), body.getX(), body.getY(),
                                                 base.darker (0.20f),    body.getX(), body.getBottom(), false));
        g.fillRect (body);
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.drawRect (body, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.07f));     // top highlight
        g.drawLine (body.getX() + 1.0f, body.getY() + 1.0f, body.getRight() - 1.0f, body.getY() + 1.0f, 1.0f);

        g.setColour (juce::Colour (0xffe8e2d8));
        g.setFont (juce::FontOptions (11.5f, juce::Font::bold));
        g.drawText (text, body, juce::Justification::centred);
    }

private:
    juce::String text;
    juce::Colour led;
    bool ledOn = false;
};
