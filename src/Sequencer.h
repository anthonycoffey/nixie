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
//
// Swing: each lane carries its own swing amount (Transport::laneSwing, 0..1),
// so a lane's odd 16ths are delayed by its own amount — straight lanes stay on
// the grid while swung lanes fire late, each by however much it's dialled in.
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
        double swing           = 0.0;     // global fallback amount (used if laneSwing is null)
        const float* laneSwing = nullptr; // per-lane swing amount [kMaxLanes], 0..1
    };

    void prepare (double newSampleRate) { sampleRate = newSampleRate; reset(); }

    void reset()
    {
        arm();
        internalPpq = 0.0;
        lastSource = Source::None;
        currentStepForUi.store (-1, std::memory_order_relaxed);
    }

    int  getCurrentStepForUi() const noexcept { return currentStepForUi.load (std::memory_order_relaxed); }

    // Nearest step to a note arriving at sampleOffset within the current block —
    // used by real-time record. Returns -1 when the sequencer isn't running.
    int quantizeToNearestStep (int sampleOffset, int numSteps, double stepPpq) const noexcept
    {
        if (lastSource == Source::None) return -1;
        const double ppq = lastBlockPpq + lastPpqPerSample * (double) sampleOffset;
        const double sp  = stepPpq > 0.0 ? stepPpq : 0.25;
        const long long nearest = (long long) std::llround (ppq / sp);
        const int n = juce::jmax (1, numSteps);
        return (int) (((nearest % n) + n) % n);
    }

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
            arm();
            if (src == Source::Internal)
                internalPpq = 0.0;
            lastSource = src;
        }

        double bpm, ppqAtStart;
        if (src == Source::Host) { bpm = tr.bpm;         ppqAtStart = tr.hostPpq; }
        else                     { bpm = tr.internalBpm; ppqAtStart = internalPpq; }

        const int    numSteps     = juce::jlimit (1, Pattern::kMaxSteps, pattern.numSteps);
        const int    numLanes     = juce::jlimit (1, Pattern::kMaxLanes, pattern.numLanes);
        const double stepPpq      = TimeGrid::stepPpq (pattern.rate);   // 1/16, 1/8T, ...
        const double ppqPerSample = (bpm / 60.0 / juce::jmax (1.0, tr.sampleRate)) ;

        lastBlockPpq     = ppqAtStart;    // remembered so record can quantize to nearest step
        lastPpqPerSample = ppqPerSample;

        // Swing delays odd steps on a duple grid; a triplet grid is already swung,
        // so the shuffle amount is ignored there.
        const bool tripletGrid = TimeGrid::isTriplet (pattern.rate);

        auto laneSwingAmt = [&tr] (int lane) -> double
        {
            return tr.laneSwing != nullptr ? (double) tr.laneSwing[(size_t) lane] : tr.swing;
        };

        for (int i = 0; i < numSamples; ++i)
        {
            const double    ppq      = ppqAtStart + ppqPerSample * i;
            const long long baseStep = (long long) std::floor (ppq / stepPpq);
            const bool      odd      = (((baseStep % 2) + 2) % 2) == 1;
            const double    gridPpq  = (double) baseStep * stepPpq;
            const int       step     = (int) (((baseStep % numSteps) + numSteps) % numSteps);

            // The UI playhead advances on the grid (so it always reads musically).
            if (baseStep != lastUiStep && ppq + 1.0e-12 >= gridPpq)
            {
                lastUiStep = baseStep;
                currentStepForUi.store (step, std::memory_order_relaxed);
            }

            // Each lane fires at its own swung time: straight lanes on the grid,
            // swung lanes delayed by their own amount on odd 16ths.
            for (int lane = 0; lane < numLanes; ++lane)
            {
                const double swingPpq = (odd && ! tripletGrid)
                                            ? juce::jlimit (0.0, 1.0, laneSwingAmt (lane)) * 0.5 * stepPpq
                                            : 0.0;
                const double laneTrig = gridPpq + swingPpq;

                if (baseStep != laneLastStep[(size_t) lane] && ppq + 1.0e-12 >= laneTrig)
                {
                    laneLastStep[(size_t) lane] = baseStep;
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

    void arm() noexcept
    {
        lastUiStep = std::numeric_limits<long long>::min();
        for (auto& s : laneLastStep)
            s = std::numeric_limits<long long>::min();
    }

    double sampleRate = 44100.0;
    double internalPpq = 0.0;
    double lastBlockPpq = 0.0;       // ppq at the start of the current block
    double lastPpqPerSample = 0.0;   // ppq advance per sample this block
    long long lastUiStep = std::numeric_limits<long long>::min();
    long long laneLastStep[Pattern::kMaxLanes] {};
    Source lastSource = Source::None;
    std::atomic<int> currentStepForUi { -1 };
};
