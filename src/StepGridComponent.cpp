#include "StepGridComponent.h"
#include "LMOneLookAndFeel.h"

namespace
{
    // Grid row display order: closed hat (lane 2) then open hat (lane 12), then
    // the rest — so the two hats sit next to each other even though the open hat
    // is voice 12 internally.
    constexpr int kRowToLane[13] = { 0, 1, 2, 12, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
    int laneForRow (int row) { return (row >= 0 && row < 13) ? kRowToLane[row] : row; }
}

StepGridComponent::StepGridComponent (LMOneAudioProcessor& p)
    : processor (p)
{
    reloadFromProcessor();
}

void StepGridComponent::reloadFromProcessor()
{
    pattern = processor.getPattern();
    repaint();
}

void StepGridComponent::setPlayingStep (int step)
{
    if (step == playingStep)
        return;
    playingStep = step;
    repaint();
}

StepGridComponent::Cell StepGridComponent::cellAt (juce::Point<int> pos) const
{
    const int numSteps = juce::jmax (1, pattern.numSteps);
    const int numLanes = juce::jmax (1, (int) processor.getPads().size());

    if (pos.x < kLabelW || pos.x >= getWidth() || pos.y < kHeaderH || pos.y >= getHeight())
        return {};

    const float cellW = (float) (getWidth() - kLabelW) / (float) numSteps;
    const int   laneH = juce::jmax (1, (getHeight() - kHeaderH) / numLanes);

    const int row = juce::jlimit (0, numLanes - 1, (pos.y - kHeaderH) / laneH);
    return { laneForRow (row),
             juce::jlimit (0, numSteps - 1, (int) ((float) (pos.x - kLabelW) / cellW)) };
}

void StepGridComponent::applyCell (int lane, int step, juce::uint8 velocity)
{
    if (pattern.vel[(size_t) lane][(size_t) step] == velocity)
        return;

    pattern.vel[(size_t) lane][(size_t) step] = velocity;
    processor.setStep (lane, step, velocity);
    repaint();
}

void StepGridComponent::mouseDown (const juce::MouseEvent& e)
{
    const auto c = cellAt (e.getPosition());
    if (! c.valid())
        return;

    const bool wasOn = pattern.vel[(size_t) c.lane][(size_t) c.step] > 0;
    paintValue = wasOn ? (juce::uint8) 0 : (juce::uint8) kDefaultVel;
    painting   = true;
    applyCell (c.lane, c.step, paintValue);
}

void StepGridComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (! painting)
        return;

    const auto c = cellAt (e.getPosition());
    if (c.valid())
        applyCell (c.lane, c.step, paintValue);
}

void StepGridComponent::mouseUp (const juce::MouseEvent&)
{
    painting = false;
}

void StepGridComponent::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const auto c = cellAt (e.getPosition());
    if (! c.valid())
        return;

    const juce::uint8 cur = pattern.vel[(size_t) c.lane][(size_t) c.step];
    if (cur == 0)
        return; // only adjust lit steps

    const int delta = (w.deltaY > 0.0f ? 8 : -8);
    applyCell (c.lane, c.step, (juce::uint8) juce::jlimit (1, 127, (int) cur + delta));
}

void StepGridComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff141417));

    const int numSteps = juce::jmax (1, pattern.numSteps);
    const int numLanes = juce::jmax (1, (int) processor.getPads().size());
    const auto& pads   = processor.getPads();

    const int   gridTop = kHeaderH;
    const int   laneH   = juce::jmax (1, (getHeight() - gridTop) / numLanes);
    const float cellW   = (float) (getWidth() - kLabelW) / (float) numSteps;

    // Beat-group boundaries (meter-aware): drive accents, tinting + divider lines.
    bool beatStart[Pattern::kMaxSteps] = {};
    TimeGrid::fillBeatStarts (pattern.tsNum, pattern.tsDen, pattern.rate, numSteps, beatStart);

    // --- Header: a row of LED lamps + step numbers above each column ---------
    const juce::Colour led (0xffff3322);
    for (int step = 0; step < numSteps; ++step)
    {
        const float cx = (float) kLabelW + ((float) step + 0.5f) * cellW;
        const bool  on = (step == playingStep);

        juce::Rectangle<float> dot (0.0f, 0.0f, 6.0f, 6.0f);
        dot.setCentre (cx, 8.0f);
        if (on) { g.setColour (led.withAlpha (0.30f)); g.fillEllipse (dot.expanded (2.5f)); }  // glow
        g.setColour (on ? led : led.withAlpha (0.14f));
        g.fillEllipse (dot);

        const bool beat = beatStart[step];
        g.setColour (beat ? LMColours::orange.withAlpha (0.9f) : juce::Colours::grey.withAlpha (0.6f));
        g.setFont (juce::FontOptions (9.0f, beat ? juce::Font::bold : juce::Font::plain));
        g.drawText (juce::String (step + 1),
                    juce::Rectangle<float> (cx - cellW * 0.5f, 15.0f, cellW, 12.0f),
                    juce::Justification::centred);
    }

    // --- Lanes ---------------------------------------------------------------
    for (int row = 0; row < numLanes; ++row)
    {
        const int lane = laneForRow (row);
        const int y = gridTop + row * laneH;

        if (lane < (int) pads.size())
        {
            g.setColour (LMColours::orange.withAlpha (0.9f));
            g.setFont (juce::FontOptions (11.0f));
            g.drawText (pads[(size_t) lane].name, 6, y, kLabelW - 10, laneH,
                        juce::Justification::centredLeft);
        }

        for (int step = 0; step < numSteps; ++step)
        {
            const int x  = kLabelW + (int) std::round ((float) step * cellW);
            const int x2 = kLabelW + (int) std::round ((float) (step + 1) * cellW);
            const juce::Rectangle<int> inner = juce::Rectangle<int> (x, y, juce::jmax (1, x2 - x), laneH).reduced (1);

            juce::Colour bg = beatStart[step] ? juce::Colour (0xff26262c) : juce::Colour (0xff1d1d22);
            if (step == playingStep)
                bg = bg.brighter (0.16f);   // subtle column tint; the LED above is the main cue
            g.setColour (bg);
            g.fillRect (inner);

            const juce::uint8 v = pattern.vel[(size_t) lane][(size_t) step];
            if (v > 0)
            {
                const float a = 0.35f + 0.65f * (float) v / 127.0f; // brightness ~ velocity
                g.setColour (juce::Colours::orange.withAlpha (a));
                g.fillRect (inner.reduced (1));
            }
        }
    }

    // Beat-group dividers (stronger lines at each beat start) make the meter legible.
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    for (int step = 1; step < numSteps; ++step)
        if (beatStart[step])
            g.drawVerticalLine (kLabelW + (int) std::round ((float) step * cellW),
                                (float) gridTop, (float) getHeight());

    g.setColour (juce::Colours::black);
    g.drawVerticalLine (kLabelW, (float) gridTop, (float) getHeight());
}
