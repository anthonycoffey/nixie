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

    // Restore one voice to its bundled factory sound (undo a user load).
    bool restoreVoiceToFactory (int voiceIndex);

    // True when the voice currently plays a user-loaded file (vs. the factory kit).
    bool voiceHasUserSample (int voiceIndex) const;

    // The sample currently loaded in a voice ("factory", "file", ...), for the UI.
    juce::String getVoiceSourceLabel (int voiceIndex) const;

    //==============================================================================
    // Sequencer transport (for the editor).
    void setInternalPlaying (bool shouldPlay) noexcept { internalPlaying.store (shouldPlay); }
    bool isInternalPlaying() const noexcept            { return internalPlaying.load(); }

    // Real-time record: while armed, incoming notes (MIDI + on-screen pads) are
    // written onto the working pattern at the playing step. The audio thread
    // queues hits lock-free; the editor drains them on the message thread.
    void setRecordArmed (bool shouldRecord) noexcept { recordArmed.store (shouldRecord); }
    bool isRecordArmed() const noexcept              { return recordArmed.load(); }
    bool pollRecordedNotes();   // message thread — returns true if the pattern changed
    int  getCurrentStep() const noexcept               { return sequencer.getCurrentStepForUi(); }
    int  getNumSteps() const noexcept
    {
        return patternState.slots[(size_t) patternState.live.load (std::memory_order_acquire)].numSteps;
    }

    // Working sequence (single, edited in the grid). Edits publish to the audio thread.
    Pattern getPattern() const { return workingPattern; }
    void    setStep (int lane, int step, juce::uint8 velocity);
    void    setPatternMeter (int num, int den, int rate);   // sets time sig + rate; derives steps
    void    clearPattern();

    // Current working-pattern meter + rate (for the editor's Meter / Rate selectors).
    int  getTsNum() const noexcept { return workingPattern.tsNum; }
    int  getTsDen() const noexcept { return workingPattern.tsDen; }
    int  getRate()  const noexcept { return workingPattern.rate; }
    float   getSeqTempo() const noexcept { return seqTempoParam != nullptr ? seqTempoParam->load() : 120.0f; }

    // Preset library: 12 banks x 10 slots. Banks 1-10 are factory genre grooves;
    // banks 11-12 are user-saveable (persisted to disk). The editor works with the
    // "current bank"; slots load into / save from the working sequence.
    static constexpr int kNumBanks        = 12;   // banks 1-10 factory (genre), 11-12 user
    static constexpr int kBankSlots       = 10;
    static constexpr int kNumFactoryBanks = 10;
    int  getCurrentBank() const noexcept { return currentBank; }
    void setCurrentBank (int bank);
    int  getCurrentSlot() const noexcept { return currentSlot; }   // highlighted slot (-1 = none)
    void setCurrentSlot (int slot) noexcept;
    void loadSlot (int slot);                  // current bank -> working sequence (+ tempo)
    void saveSlot (int slot);                  // working sequence -> current bank (user banks only)
    bool slotFilled (int slot) const;          // is a slot in the current bank populated?
    juce::String slotName (int slot) const;    // name of a slot in the current bank
    juce::String slotGenre (int slot) const;   // genre tag of a slot in the current bank ("" if none)
    bool currentBankIsFactory() const noexcept { return currentBank < kNumFactoryBanks; }

    // Full state capture/restore — shared by host state and the preset manager.
    juce::ValueTree captureStateTree();
    void            restoreStateTree (const juce::ValueTree& tree);

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    static BusesProperties makeBusesProperties();   // main stereo + 12 numbered direct outs

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
        std::atomic<float>* swing = nullptr;  // 0/1 — follow the global Shuffle amount
        std::atomic<float>* out   = nullptr;  // 0 = Main mix, 1..N = direct out N (Out 1..12)
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

    // The single working sequence (message-thread canonical). The live slot of
    // patternState is a copy that the audio thread reads.
    Pattern workingPattern;

    // Preset library (message thread). Banks 0..4 factory, 5..99 user-saveable.
    struct PresetSlot
    {
        Pattern      pattern;
        int          tempo   = 120;
        bool         filled  = false;
        bool         factory = false;
        juce::String name;
        juce::String genre;   // factory genre tag (e.g. "Funk"); empty for user slots
    };
    std::array<std::array<PresetSlot, (size_t) kBankSlots>, (size_t) kNumBanks> library;
    int currentBank = 0;
    int currentSlot = 0;   // highlighted/loaded slot within the current bank (-1 = none)

    void loadFactoryLibrary();
    void loadUserLibrary();
    void saveUserLibrary() const;
    static juce::File userLibraryFile();

    std::atomic<bool>   internalPlaying { false };
    std::atomic<float>* seqTempoParam = nullptr;
    std::atomic<float>* shuffleParam  = nullptr;

    // Real-time record: audio thread queues hits, message thread applies them.
    std::atomic<bool> recordArmed { false };
    struct RecEvent { int lane; int step; juce::uint8 vel; };
    static constexpr int kRecFifoSize = 256;
    RecEvent recFifoBuf[kRecFifoSize];
    juce::AbstractFifo recFifo { kRecFifoSize };
    void pushRecordEvent (int lane, int step, juce::uint8 vel) noexcept; // audio thread

    void publishPattern (const Pattern& p);             // message thread

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LMOneAudioProcessor)
};
