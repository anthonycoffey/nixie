#pragma once

#include <JuceHeader.h>
#include "DrumKit.h"
#include "DrumVoice.h"
#include "Pattern.h"
#include "Sequencer.h"

//==============================================================================
// LM-1 — drum-machine instrument (foundation build).
//
// A MIDI-playable sampler with 12 authentic LM-1 voices, per-voice sample
// loading via a reference-counted DrumKit, a per-voice mixer, and a global
// lo-fi "crush" stage. The internal step sequencer + step-grid UI come next
// (see ROADMAP.md).
//==============================================================================
class LMOneAudioProcessor : public juce::AudioProcessor,
                            public juce::ChangeBroadcaster
{
public:
    //==============================================================================
    // One playable drum pad: a name, the General-MIDI note it answers to, and the
    // choke group it belongs to (voices in the same group cut each other off).
    struct Pad
    {
        juce::String name;
        int midiNote;       // GM drum note
        int chokeGroup;     // -1 = none
    };

    LMOneAudioProcessor();
    ~LMOneAudioProcessor() override = default;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "LM-1"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 1.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    // Used by the editor's on-screen pads to play notes without external MIDI.
    juce::MidiKeyboardState keyboardState;

    // Exposed so the editor can label its pads.
    const std::vector<Pad>& getPads() const { return pads; }

    // Mixer-strip label for a channel. The Hi-Hat channel is "Hi-hats" (it carries
    // both the closed and open hat), so the 12-channel layout reads clearly.
    juce::String getChannelName (int channel) const
    {
        if (channel == DrumKit::kHiHatChannel) return "Hi-hats";
        return (channel >= 0 && channel < DrumKit::kNumVoices)
                   ? juce::String (LMOne::kVoiceDefs[(size_t) channel].name) : juce::String();
    }

    // Load a user WAV/AIFF into one voice (message thread). Copy-on-write swap;
    // a failed load leaves the existing sample in place. Returns success.
    bool loadUserSample (int voiceIndex, const juce::File& file);

    // The sample currently loaded in a voice ("factory", "file", ...), for the UI.
    juce::String getVoiceSourceLabel (int voiceIndex) const;

    //==============================================================================
    // Sequencer transport (for the editor).
    void setInternalPlaying (bool shouldPlay) noexcept { internalPlaying.store (shouldPlay); }
    bool isInternalPlaying() const noexcept            { return internalPlaying.load(); }
    int  getCurrentStep() const noexcept               { return sequencer.getCurrentStepForUi(); }
    int  getNumSteps() const noexcept
    {
        return patternState.slots[(size_t) patternState.live.load (std::memory_order_acquire)].numSteps;
    }

    // Pattern access + editing (message thread). Edits modify the current bank
    // slot, then publish it to the audio thread.
    static constexpr int kNumPatterns = 8;
    Pattern getPattern() const { return patternBank[(size_t) currentPattern]; }
    int     getNumPatterns() const noexcept { return kNumPatterns; }
    int     getCurrentPattern() const noexcept { return currentPattern; }
    void    selectPattern (int index);
    void    setStep (int lane, int step, juce::uint8 velocity);
    void    setPatternLength (int numSteps);
    void    clearPattern();
    float   getSeqTempo() const noexcept { return seqTempoParam != nullptr ? seqTempoParam->load() : 120.0f; }

    // Full state capture/restore — shared by host state and the preset manager.
    juce::ValueTree captureStateTree();
    void            restoreStateTree (const juce::ValueTree& tree);

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void triggerPad (int padIndex, float velocity, DrumKit* kit);
    static float applyCrush (float sample, int bits, float mix);

    // The open-hat voice (12) has no mixer strip — it renders through the Hi-Hat
    // channel (2). Every other voice maps to its own channel 1:1.
    static int channelForVoice (int v) noexcept
    {
        return v == DrumKit::kOpenHatVoice ? DrumKit::kHiHatChannel : v;
    }

    // Publish a freshly built kit to the audio thread (message thread only).
    void setKit (DrumKit::Ptr newKit);

    std::vector<Pad>       pads;    // playable pads (12 LM-1 instruments)
    std::vector<DrumVoice> voices;  // one voice per pad

    // Reference-counted kit hand-off. currentKit + retiredKits keep instances
    // alive (message thread); liveKit is the raw pointer the audio thread reads.
    DrumKit::Ptr                      currentKit;
    std::atomic<DrumKit*>             liveKit { nullptr };
    juce::ReferenceCountedArray<DrumKit> retiredKits;

    double currentSampleRate = 44100.0;

    // Lo-fi sample-rate-reduction state (per channel hold).
    float  srrHoldL = 0.0f, srrHoldR = 0.0f;
    double srrPhase = 0.0;

    std::atomic<float>* masterGainDb = nullptr;
    std::atomic<float>* lofiAmount   = nullptr;
    std::atomic<float>* globalTune   = nullptr;

    // Cached per-voice mixer parameter pointers (read on the audio thread).
    struct VoiceParams
    {
        std::atomic<float>* level = nullptr;  // 0 .. 1.5
        std::atomic<float>* pan   = nullptr;  // -1 .. 1
        std::atomic<float>* tune  = nullptr;  // -12 .. 12 semitones
        std::atomic<float>* mute  = nullptr;  // 0/1
        std::atomic<float>* solo  = nullptr;  // 0/1
    };
    std::array<VoiceParams, (size_t) DrumKit::kNumChannels> voiceParams;

    //==============================================================================
    // Sequencer. The pattern is double-buffered: the editor writes the off-slot
    // and flips an atomic index; the audio thread reads (copies) the live slot.
    struct PatternState
    {
        Pattern slots[2];
        std::atomic<int> live { 0 };
    };
    PatternState patternState;
    Sequencer    sequencer;

    // Canonical pattern bank (message thread). The live slot of patternState is a
    // copy of patternBank[currentPattern] that the audio thread reads.
    std::array<Pattern, (size_t) kNumPatterns> patternBank;
    int currentPattern = 0;

    std::atomic<bool>   internalPlaying { false };
    std::atomic<float>* seqTempoParam = nullptr;
    std::atomic<float>* shuffleParam  = nullptr;

    void    publishPattern (const Pattern& p);          // message thread
    Pattern getPatternSnapshot() const;                 // message thread

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LMOneAudioProcessor)
};
