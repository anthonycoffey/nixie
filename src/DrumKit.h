#pragma once

#include <JuceHeader.h>

//==============================================================================
// The canonical 12-voice LM-1 instrument layout (single source of truth for the
// pad table, the per-voice mixer params, and the kit's voice names).
//
//   chokeGroup: voices sharing a non-negative group cut each other off when
//   triggered (mono behaviour). Hi-Hat is its own group so it stays monophonic
//   and is ready to host a closed/open pair later.
//==============================================================================
namespace LMOne
{
    struct VoiceDef { const char* name; int midiNote; int chokeGroup; };

    inline constexpr std::array<VoiceDef, 13> kVoiceDefs = { {
        { "Bass",        36, -1 },
        { "Snare",       38, -1 },
        { "Hi-Hat",      42,  0 },   // closed hat — choke group 0
        { "Cabasa",      69, -1 },
        { "Tambourine",  54, -1 },
        { "Tom Lo",      45, -1 },
        { "Tom Hi",      48, -1 },
        { "Conga Lo",    64, -1 },
        { "Conga Hi",    63, -1 },
        { "Cowbell",     56, -1 },
        { "Clave",       75, -1 },
        { "Clap",        39, -1 },
        { "Open Hat",    46,  0 },   // open hat — same choke group, shares the Hi-Hat fader
    } };
}

//==============================================================================
// A loaded drum kit: 12 mono voice samples plus their metadata.
//
// Reference-counted so a freshly built/edited kit can be published to the audio
// thread with a single atomic pointer store — no locks, no copies on the audio
// thread. Build and edit a DrumKit ONLY on the message thread; the audio thread
// just reads the published instance (kept alive by the processor).
//==============================================================================
class DrumKit : public juce::ReferenceCountedObject
{
public:
    using Ptr = juce::ReferenceCountedObjectPtr<DrumKit>;
    static constexpr int kNumVoices    = 13;  // 12 instruments + open hi-hat
    static constexpr int kNumChannels  = 12;  // mixer strips / per-channel params
    static constexpr int kOpenHatVoice = 12;  // voice 12 renders through the Hi-Hat channel
    static constexpr int kHiHatChannel = 2;

    struct VoiceSample
    {
        juce::AudioBuffer<float> buffer;            // mono sample data, owned here
        double       sourceSampleRate = 44100.0;    // rate the buffer was decoded at
        juce::String name;                          // "Bass", "Snare", ...
        juce::String sourceTag = "factory";         // "factory" | "file" | "embedded"
        juce::String sourcePath;                    // original file path (when "file")
        int  startSample = 0;                       // trim start (samples)
        int  endSample   = -1;                      // trim end (-1 => play to end)
        bool isProcedural = true;                   // generated fallback, not a real sample
    };

    DrumKit();
    DrumKit (const DrumKit&) = default;             // deep copy => copy-on-write swaps

    VoiceSample&       voice (int i)       noexcept { return voices[(size_t) i]; }
    const VoiceSample& voice (int i) const noexcept { return voices[(size_t) i]; }

    const juce::AudioBuffer<float>* buffer (int i) const noexcept { return &voices[(size_t) i].buffer; }
    double sourceRate (int i) const noexcept { return voices[(size_t) i].sourceSampleRate; }

private:
    std::array<VoiceSample, (size_t) kNumVoices> voices;
};

//==============================================================================
// Kit building / loading. All message-thread only (allocates, does file I/O).
//==============================================================================
namespace KitFactory
{
    // Build the default kit. Prefers a bundled WAV per voice (when binary data is
    // present), otherwise falls back to a procedural one-shot. Samples are kept at
    // their native rate; DrumVoice handles host-rate conversion at playback.
    DrumKit::Ptr buildFactoryKit (double proceduralRate = 44100.0);

    // Decode a WAV/AIFF into one voice of an existing kit (mono downmix). Returns
    // false on failure and leaves the voice untouched.
    bool loadVoiceFromFile (DrumKit& kit, int voiceIndex, const juce::File& file);
}
