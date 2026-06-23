#pragma once

#include <JuceHeader.h>
#include <array>
#include <cstring>

//==============================================================================
// A step pattern: up to 16 lanes x up to 48 steps. Each cell stores a velocity
// (0 = off, 1..127 = on). The pattern also carries a time signature + step rate
// (subdivision); the active step count is DERIVED from them (see TimeGrid). The
// struct is fixed-size and trivially copyable, which keeps the lock-free hand-off
// to the audio thread cheap (see the double-buffer in PluginProcessor).
//==============================================================================
struct Pattern
{
    static constexpr int kMaxLanes = 16;   // 13 in use (12 instruments + open hat) + headroom
    static constexpr int kMaxSteps = 48;   // fits the densest meter x rate (12/8 @ 1/16T = 36)

    int   numSteps = 16;     // active length, derived from meter x rate
    int   numLanes = 13;
    int   tsNum    = 4;      // time signature numerator
    int   tsDen    = 4;      // time signature denominator (4 or 8)
    int   rate     = 2;      // step subdivision (TimeGrid::Rate; 2 = 1/16)
    float swing    = 0.0f;   // 0..1, delays odd steps (used on duple grids)

    std::array<std::array<juce::uint8, kMaxSteps>, kMaxLanes> vel {};

    bool isOn (int lane, int step) const noexcept
    {
        return vel[(size_t) lane][(size_t) step] > 0;
    }

    void setStep (int lane, int step, juce::uint8 velocity) noexcept
    {
        vel[(size_t) lane][(size_t) step] = velocity;
    }

    void clear() noexcept
    {
        for (auto& row : vel)
            row.fill (0);
    }

    // Set time signature + rate and recompute numSteps (defined below TimeGrid).
    void setMeter (int num, int den, int r) noexcept;

    //==========================================================================
    juce::ValueTree toValueTree() const
    {
        juce::ValueTree t ("PATTERN");
        t.setProperty ("numSteps", numSteps, nullptr);
        t.setProperty ("numLanes", numLanes, nullptr);
        t.setProperty ("tsNum",    tsNum,    nullptr);
        t.setProperty ("tsDen",    tsDen,    nullptr);
        t.setProperty ("rate",     rate,     nullptr);
        t.setProperty ("swing",    swing,    nullptr);

        juce::MemoryBlock mb (vel.data(), sizeof (vel));
        t.setProperty ("grid", mb.toBase64Encoding(), nullptr);
        return t;
    }

    void fromValueTree (const juce::ValueTree& t)
    {
        if (! t.hasType ("PATTERN"))
            return;

        numSteps = juce::jlimit (1, kMaxSteps, (int) t.getProperty ("numSteps", 16));
        numLanes = juce::jlimit (1, kMaxLanes, (int) t.getProperty ("numLanes", 12));
        tsNum    = juce::jlimit (1, 32, (int) t.getProperty ("tsNum", 4));
        tsDen    = (int) t.getProperty ("tsDen", 4);
        rate     = juce::jlimit (0, 4, (int) t.getProperty ("rate", 2));
        swing    = (float) t.getProperty ("swing", 0.0);

        juce::MemoryBlock mb;
        if (! mb.fromBase64Encoding (t.getProperty ("grid", "").toString()))
            return;

        if (mb.getSize() == sizeof (vel))
        {
            std::memcpy (vel.data(), mb.getData(), sizeof (vel));
        }
        else if (mb.getSize() == (size_t) kMaxLanes * 32)   // migrate legacy 32-step grids
        {
            const auto* src = static_cast<const juce::uint8*> (mb.getData());
            for (int lane = 0; lane < kMaxLanes; ++lane)
                std::memcpy (&vel[(size_t) lane][0], src + (size_t) lane * 32, 32);
        }
    }
};

//==============================================================================
// Time-grid helpers: the step subdivision (rate), the supported meters, and the
// derived step count + beat grouping the sequencer and grid read.
//==============================================================================
namespace TimeGrid
{
    enum Rate { kQuarter = 0, kEighth, kSixteenth, kEighthT, kSixteenthT, kNumRates };

    inline double stepPpq (int rate) noexcept
    {
        switch (rate)
        {
            case kQuarter:    return 1.0;
            case kEighth:     return 0.5;
            case kSixteenth:  return 0.25;
            case kEighthT:    return 1.0 / 3.0;   // eighth-note triplet
            case kSixteenthT: return 1.0 / 6.0;   // sixteenth-note triplet
            default:          return 0.25;
        }
    }

    inline const char* rateName (int rate) noexcept
    {
        switch (rate)
        {
            case kQuarter:    return "1/4";
            case kEighth:     return "1/8";
            case kSixteenth:  return "1/16";
            case kEighthT:    return "1/8T";
            case kSixteenthT: return "1/16T";
            default:          return "1/16";
        }
    }

    inline bool isTriplet (int rate) noexcept { return rate == kEighthT || rate == kSixteenthT; }

    inline double barQuarters (int tsNum, int tsDen) noexcept
    {
        return (double) tsNum * 4.0 / (double) juce::jmax (1, tsDen);
    }

    inline int numStepsFor (int tsNum, int tsDen, int rate) noexcept
    {
        const double n = barQuarters (tsNum, tsDen) / stepPpq (rate);
        return juce::jlimit (1, Pattern::kMaxSteps, juce::roundToInt (n));
    }

    struct Meter { int num, den; };
    inline const std::array<Meter, 8>& meters() noexcept
    {
        static const std::array<Meter, 8> m { { {2,4},{3,4},{4,4},{5,8},{6,8},{7,8},{9,8},{12,8} } };
        return m;
    }

    inline int meterIndex (int num, int den) noexcept
    {
        const auto& m = meters();
        for (int i = 0; i < (int) m.size(); ++i)
            if (m[(size_t) i].num == num && m[(size_t) i].den == den)
                return i;
        return 2;   // default 4/4
    }

    // Mark the first step of every beat group (for grid dividers + accents). out
    // must hold at least numSteps entries.
    inline void fillBeatStarts (int tsNum, int tsDen, int rate, int numSteps, bool* out) noexcept
    {
        for (int i = 0; i < numSteps; ++i) out[i] = false;
        if (numSteps <= 0) return;
        out[0] = true;
        const double sp = stepPpq (rate);

        if (tsDen == 8 && (tsNum == 5 || tsNum == 7))   // odd meters: irregular eighth grouping
        {
            const int ePer = juce::jmax (1, juce::roundToInt (0.5 / sp));   // steps per eighth
            const int g5[2] = { 3, 2 };
            const int g7[3] = { 2, 2, 3 };
            const int* grp  = (tsNum == 5) ? g5 : g7;
            const int  cnt  = (tsNum == 5) ? 2  : 3;
            int idx = 0;
            for (int k = 0; k < cnt; ++k) { if (idx < numSteps) out[idx] = true; idx += grp[k] * ePer; }
            return;
        }

        double beatQ = 1.0;                              // simple meter: beat = quarter
        if (tsDen == 8 && (tsNum % 3 == 0) && ! isTriplet (rate))
            beatQ = 1.5;                                 // compound meter: beat = dotted quarter
        const int g = juce::jmax (1, juce::roundToInt (beatQ / sp));
        for (int i = 0; i < numSteps; i += g) out[i] = true;
    }
}

inline void Pattern::setMeter (int num, int den, int r) noexcept
{
    tsNum = num;
    tsDen = den;
    rate  = juce::jlimit (0, TimeGrid::kNumRates - 1, r);
    numSteps = TimeGrid::numStepsFor (num, den, rate);
}
