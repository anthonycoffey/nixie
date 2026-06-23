#pragma once

#include <JuceHeader.h>

//==============================================================================
// LM-1 visual language: dark faceplate, orange (#fc5824) accents, vintage
// vector-drawn knobs, faders and buttons. Set this on the editor and every
// child control inherits it.
//==============================================================================
namespace LMColours
{
    inline const juce::Colour orange     { 0xfffc5824 };
    inline const juce::Colour faceplate  { 0xff141312 };
    inline const juce::Colour panel      { 0xff1c1b19 };
    inline const juce::Colour metalHi    { 0xffcdcdcd };
    inline const juce::Colour metalLo    { 0xff6f6f6f };
    inline const juce::Colour wood1      { 0xff8a5a2b };
    inline const juce::Colour wood2      { 0xff3d2613 };
}

class LMOneLookAndFeel : public juce::LookAndFeel_V4
{
public:
    LMOneLookAndFeel()
    {
        using namespace juce;
        setColour (Slider::textBoxTextColourId,       Colour (0xffff3322));   // LED red readout
        setColour (Slider::textBoxBackgroundColourId, Colour (0xff160a08));   // dark glass
        setColour (Slider::textBoxOutlineColourId,    Colours::transparentBlack);
        setColour (Label::textColourId,               LMColours::orange.brighter (0.15f));
        setColour (TextButton::buttonColourId,        Colour (0xff262422));
        setColour (TextButton::buttonOnColourId,      LMColours::orange);
        setColour (TextButton::textColourOffId,       Colour (0xffd6d2cc));
        setColour (TextButton::textColourOnId,        Colours::black);
        setColour (ComboBox::backgroundColourId,      Colour (0xff262422));
        setColour (ComboBox::textColourId,            Colours::white);
        setColour (ComboBox::outlineColourId,         Colours::black);
        setColour (ComboBox::arrowColourId,           LMColours::orange);
        setColour (PopupMenu::backgroundColourId,             Colour (0xff1c1b19));
        setColour (PopupMenu::textColourId,                   Colours::white);
        setColour (PopupMenu::highlightedBackgroundColourId,  LMColours::orange);
        setColour (PopupMenu::highlightedTextColourId,        Colours::black);
    }

    // Compact combo font so short values (e.g. "1/16T", "12/8") fit without "...".
    juce::Font getComboBoxFont (juce::ComboBox&) override
    {
        return juce::Font (juce::FontOptions (13.0f, juce::Font::bold));
    }

    //==========================================================================
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle,
                           juce::Slider&) override
    {
        using namespace juce;
        auto bounds = Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (4.0f);
        const auto centre = bounds.getCentre();
        const auto radius = jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto angle  = startAngle + sliderPos * (endAngle - startAngle);

        // Domed black body.
        g.setGradientFill (ColourGradient (Colour (0xff3a3734), centre.x, centre.y - radius,
                                           Colour (0xff0c0b0a), centre.x, centre.y + radius, false));
        g.fillEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);

        // Rim.
        g.setColour (Colours::black);
        g.drawEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f, 1.5f);
        g.setColour (Colours::white.withAlpha (0.10f));
        g.drawEllipse (centre.x - radius + 1.4f, centre.y - radius + 1.4f,
                       (radius - 1.4f) * 2.0f, (radius - 1.4f) * 2.0f, 1.0f);

        // Orange indicator.
        const auto a = angle - MathConstants<float>::halfPi;
        const Point<float> root (centre.x + std::cos (a) * radius * 0.30f,
                                 centre.y + std::sin (a) * radius * 0.30f);
        const Point<float> tip  (centre.x + std::cos (a) * radius * 0.86f,
                                 centre.y + std::sin (a) * radius * 0.86f);
        g.setColour (LMColours::orange);
        g.drawLine (root.x, root.y, tip.x, tip.y, 2.6f);
    }

    //==========================================================================
    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float minPos, float maxPos,
                           juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        using namespace juce;
        const auto orange = LMColours::orange;

        if (style == Slider::LinearVertical)
        {
            const float cx = x + width * 0.5f;
            // Recessed track.
            g.setColour (Colour (0xff0b0a09));
            g.fillRoundedRectangle (cx - 3.0f, (float) y, 6.0f, (float) height, 3.0f);
            g.setColour (Colours::black);
            g.drawRoundedRectangle (cx - 3.0f, (float) y, 6.0f, (float) height, 3.0f, 1.0f);
            // Level fill below the cap (subtle).
            g.setColour (orange.withAlpha (0.30f));
            g.fillRoundedRectangle (cx - 2.0f, sliderPos, 4.0f, (y + height) - sliderPos, 2.0f);
            // Charcoal cap — narrower, no centre line.
            const float capW = width * 0.6f, capH = 14.0f;
            Rectangle<float> cap (x + (width - capW) * 0.5f, sliderPos - capH * 0.5f, capW, capH);
            g.setGradientFill (ColourGradient (Colour (0xff45423f), cap.getX(), cap.getY(),
                                               Colour (0xff201e1d), cap.getX(), cap.getBottom(), false));
            g.fillRoundedRectangle (cap, 2.0f);
            g.setColour (Colours::black.withAlpha (0.7f));
            g.drawRoundedRectangle (cap, 2.0f, 1.0f);
        }
        else if (style == Slider::LinearHorizontal)
        {
            const float cy = y + height * 0.5f;
            g.setColour (Colour (0xff0b0a09));
            g.fillRoundedRectangle ((float) x, cy - 3.0f, (float) width, 6.0f, 3.0f);
            g.setColour (orange.withAlpha (0.35f));
            g.fillRoundedRectangle ((float) x, cy - 2.0f, sliderPos - x, 4.0f, 2.0f);
            const float capW = 13.0f, capH = height * 0.82f;
            Rectangle<float> cap (sliderPos - capW * 0.5f, y + (height - capH) * 0.5f, capW, capH);
            g.setGradientFill (ColourGradient (Colour (0xff45423f), cap.getX(), cap.getY(),
                                               Colour (0xff201e1d), cap.getX(), cap.getBottom(), false));
            g.fillRoundedRectangle (cap, 2.0f);
            g.setColour (Colours::black.withAlpha (0.7f));
            g.drawRoundedRectangle (cap, 2.0f, 1.0f);
        }
        else
        {
            LookAndFeel_V4::drawLinearSlider (g, x, y, width, height,
                                              sliderPos, minPos, maxPos, style, slider);
        }
    }

    //==========================================================================
    void drawButtonBackground (juce::Graphics& g, juce::Button& b,
                               const juce::Colour& backgroundColour,
                               bool over, bool down) override
    {
        using namespace juce;
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        auto base = backgroundColour;
        if (down)      base = base.darker (0.18f);
        else if (over) base = base.brighter (0.06f);

        g.setColour (base);                          // flat fill — no gradient or gloss
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (Colours::black.withAlpha (0.6f));
        g.drawRoundedRectangle (r, 3.0f, 1.0f);
    }
};
