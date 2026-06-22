#include "StepGridComponent.h"

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

    if (pos.x < kLabelW || pos.x >= getWidth() || pos.y < 0 || pos.y >= getHeight())
        return {};

    const float cellW = (float) (getWidth() - kLabelW) / (float) numSteps;
    const int   laneH = juce::jmax (1, getHeight() / numLanes);

    const int row = juce::jlimit (0, numLanes - 1, pos.y / laneH);
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

    const int   laneH = getHeight() / numLanes;
    const float cellW = (float) (getWidth() - kLabelW) / (float) numSteps;

    g.setFont (juce::FontOptions (11.0f));

    for (int row = 0; row < numLanes; ++row)
    {
        const int lane = laneForRow (row);
        const int y = row * laneH;

        if (lane < (int) pads.size())
        {
            g.setColour (juce::Colours::white.withAlpha (0.72f));
            g.drawText (pads[(size_t) lane].name, 6, y, kLabelW - 10, laneH,
                        juce::Justification::centredLeft);
        }

        for (int step = 0; step < numSteps; ++step)
        {
            const int x  = kLabelW + (int) std::round ((float) step * cellW);
            const int x2 = kLabelW + (int) std::round ((float) (step + 1) * cellW);
            const juce::Rectangle<int> inner = juce::Rectangle<int> (x, y, juce::jmax (1, x2 - x), laneH).reduced (1);

            juce::Colour bg = (step % 4 == 0) ? juce::Colour (0xff26262c) : juce::Colour (0xff1d1d22);
            if (step == playingStep)
                bg = bg.brighter (0.30f);
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

    // Playhead column outline.
    if (playingStep >= 0 && playingStep < numSteps)
    {
        const int x  = kLabelW + (int) std::round ((float) playingStep * cellW);
        const int x2 = kLabelW + (int) std::round ((float) (playingStep + 1) * cellW);
        g.setColour (juce::Colours::white.withAlpha (0.5f));
        g.drawRect (juce::Rectangle<int> (x, 0, juce::jmax (1, x2 - x), getHeight()), 1);
    }

    g.setColour (juce::Colours::black);
    g.drawVerticalLine (kLabelW, 0.0f, (float) getHeight());
}
