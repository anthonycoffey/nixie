#pragma once

#include <JuceHeader.h>
#include <limits>
#include "Pattern.h"

//==============================================================================
// Sample-accurate step sequencer clock.
//
// Each block it is handed a Pattern snapshot and a Transport, and it calls
// fire(lane, sampleOffset, velocity) at the exact sample a step lands on. It
// owns no audio resources — the host integrates it by triggering voices in the
// fire callback. process() is a template so the callback inlines (no heap, no
// std::function on the audio thread).
//
// Clock arbitration: the host transport wins whenever it is actively playing
// (so bounces lock to the project grid); otherwise an internal Play/Stop clock
// drives playback (for the Standalone or a stopped host).
//==============================================================================
class Sequencer
{
public:
    struct Transport
    {
        bool   hostPlaying     = false;
        double hostPpq         = 0.0;
        double bpm             = 120.0;   // host bpm (used when hostPlaying)
        bool   internalPlaying = false;
        double internalBpm     = 120.0;
        double sampleRate      = 44100.0;
        bool   seqEnabled      = true;
        double swing           = 0.0;   // 0..1 shuffle amount (delays odd 16ths)
    };

    void prepare (double newSampleRate) { sampleRate = newSampleRate; reset(); }

    void reset()
    {
        lastAbsoluteStep = std::numeric_limits<long long>::min();
        internalPpq = 0.0;
        lastSource = Source::None;
        currentStepForUi.store (-1, std::memory_order_relaxed);
    }

    int  getCurrentStepForUi() const noexcept { return currentStepForUi.load (std::memory_order_relaxed); }

    // Scan the block and fire steps. fire signature: void(int lane, int sampleOffset, float velocity).
    template <typename FireFn>
    void process (const Pattern& pattern, const Transport& tr, int numSamples, FireFn&& fire)
    {
        if (! tr.seqEnabled)
        {
            currentStepForUi.store (-1, std::memory_order_relaxed);
            lastSource = Source::None;
            return;
        }

        // Resolve the active clock source.
        Source src;
        if (tr.hostPlaying)          src = Source::Host;
        else if (tr.internalPlaying) src = Source::Internal;
        else
        {
            currentStepForUi.store (-1, std::memory_order_relaxed);
            lastSource = Source::None;
            return;
        }

        // On a source change, re-arm so we don't spray catch-up triggers, and
        // restart the internal clock from the top (Play begins at step 1).
        if (src != lastSource)
        {
            lastAbsoluteStep = std::numeric_limits<long long>::min();
            if (src == Source::Internal)
                internalPpq = 0.0;
            lastSource = src;
        }

        double bpm, ppqAtStart;
        if (src == Source::Host) { bpm = tr.bpm;         ppqAtStart = tr.hostPpq; }
        else                     { bpm = tr.internalBpm; ppqAtStart = internalPpq; }

        const int    numSteps     = juce::jlimit (1, Pattern::kMaxSteps, pattern.numSteps);
        const int    numLanes     = juce::jlimit (1, Pattern::kMaxLanes, pattern.numLanes);
        const double stepPpq      = 0.25; // 16th note
        const double ppqPerSample = (bpm / 60.0 / juce::jmax (1.0, tr.sampleRate)) ;
        const double swingPpq     = juce::jlimit (0.0, 0.9, tr.swing) * 0.5 * stepPpq;

        for (int i = 0; i < numSamples; ++i)
        {
            const double ppq      = ppqAtStart + ppqPerSample * i;
            const long long baseStep = (long long) std::floor (ppq / stepPpq);
            const bool   odd      = (((baseStep % 2) + 2) % 2) == 1;
            const double trigPpq  = (double) baseStep * stepPpq + (odd ? swingPpq : 0.0);

            if (baseStep != lastAbsoluteStep && ppq + 1.0e-12 >= trigPpq)
            {
                lastAbsoluteStep = baseStep;
                const int step = (int) (((baseStep % numSteps) + numSteps) % numSteps);
                currentStepForUi.store (step, std::memory_order_relaxed);

                for (int lane = 0; lane < numLanes; ++lane)
                {
                    const juce::uint8 v = pattern.vel[(size_t) lane][(size_t) step];
                    if (v > 0)
                        fire (lane, i, (float) v / 127.0f);
                }
            }
        }

        if (src == Source::Internal)
            internalPpq = ppqAtStart + ppqPerSample * numSamples;
    }

private:
    enum class Source { None, Host, Internal };

    double sampleRate = 44100.0;
    double internalPpq = 0.0;
    long long lastAbsoluteStep = std::numeric_limits<long long>::min();
    Source lastSource = Source::None;
    std::atomic<int> currentStepForUi { -1 };
};
