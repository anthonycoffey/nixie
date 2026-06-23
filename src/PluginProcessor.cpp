#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "FactoryGrooves.h"

//==============================================================================
// Shuffle: map the discrete "shuffle" choice index to a swing amount (0..1),
// where 0 = straight (50%) and 1 = hard (75%). Values chosen so the named
// settings land on musical subdivisions (triplet = ~66.7%).
static float shuffleAmountForIndex (int idx) noexcept
{
    static const float amounts[] = { 0.0f, 0.24f, 0.48f, 0.667f, 1.0f };
    return amounts[(size_t) juce::jlimit (0, 4, idx)];
}

//==============================================================================
// Output buses: a main stereo mix + one stereo "direct out" per mixer channel,
// named by voice so LUNA's multi-out mixer labels them clearly. The direct outs
// are disabled by default (the host/LUNA enables the ones it routes).
auto LMOneAudioProcessor::makeBusesProperties() -> BusesProperties
{
    BusesProperties props;
    props = props.withOutput ("Main", juce::AudioChannelSet::stereo(), true);
    // Generic direct outs — any channel can route to any of these (grouping), so
    // they're numbered, not per-voice. The user fills Out 1, Out 2... in order.
    for (int i = 0; i < DrumKit::kNumChannels; ++i)
        props = props.withOutput ("Out " + juce::String (i + 1),
                                  juce::AudioChannelSet::stereo(), false);
    return props;
}

//==============================================================================
LMOneAudioProcessor::LMOneAudioProcessor()
    : AudioProcessor (makeBusesProperties()),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    // Build the 12 LM-1 pads from the canonical voice table.
    pads.reserve (DrumKit::kNumVoices);
    for (int i = 0; i < DrumKit::kNumVoices; ++i)
        pads.push_back ({ juce::String (LMOne::kVoiceDefs[(size_t) i].name),
                          LMOne::kVoiceDefs[(size_t) i].midiNote,
                          LMOne::kVoiceDefs[(size_t) i].chokeGroup });

    voices.resize (pads.size());

    masterGainDb = apvts.getRawParameterValue ("masterGain");
    lofiAmount   = apvts.getRawParameterValue ("lofi");
    globalTune   = apvts.getRawParameterValue ("tune");

    for (int i = 0; i < DrumKit::kNumChannels; ++i)
    {
        const auto id = "v" + juce::String (i);
        voiceParams[(size_t) i].level = apvts.getRawParameterValue (id + "_level");
        voiceParams[(size_t) i].pan   = apvts.getRawParameterValue (id + "_pan");
        voiceParams[(size_t) i].tune  = apvts.getRawParameterValue (id + "_tune");
        voiceParams[(size_t) i].mute  = apvts.getRawParameterValue (id + "_mute");
        voiceParams[(size_t) i].solo  = apvts.getRawParameterValue (id + "_solo");
        voiceParams[(size_t) i].swing = apvts.getRawParameterValue (id + "_swing");
        voiceParams[(size_t) i].out   = apvts.getRawParameterValue (id + "_out");
    }

    seqTempoParam = apvts.getRawParameterValue ("seqTempo");
    shuffleParam  = apvts.getRawParameterValue ("shuffle");

    // Preset library: factory banks 1-10 (genre) + any user-saved banks. Start on
    // the first factory groove so the sequencer plays something out of the box.
    loadFactoryLibrary();
    loadUserLibrary();
    currentBank     = 0;
    workingPattern  = library[0][0].pattern;   // "Billie Jean" (Bank 1, slot 1)
    workingPattern.numLanes = DrumKit::kNumVoices;
    publishPattern (workingPattern);

    // Build the default kit and publish it to the audio thread.
    setKit (KitFactory::buildFactoryKit());
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
LMOneAudioProcessor::createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "masterGain", 1 }, "Master",
        NormalisableRange<float> (-48.0f, 6.0f, 0.1f), 0.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "lofi", 1 }, "Lo-Fi",
        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "tune", 1 }, "Tune",
        NormalisableRange<float> (-12.0f, 12.0f, 0.01f), 0.0f));

    // Sequencer.
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "seqTempo", 1 }, "Seq Tempo",
        NormalisableRange<float> (40.0f, 240.0f, 0.1f), 120.0f));

    // Shuffle as discrete, musical settings so the knob lands on good grooves.
    // Index -> swing amount handled in shuffleAmountForIndex(); 50/56/62/67/75%.
    // Only Triplet equals a clean note fraction (1/8T), so the rest are named words.
    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { "shuffle", 1 }, "Shuffle",
        juce::StringArray { "Straight", "Light", "Medium", "Triplet", "Hard" }, 0));

    // Per-channel mixer: level / pan / tune / mute / solo, generated in a loop.
    // (12 channels; the open-hat voice shares the Hi-Hat channel.)
    for (int i = 0; i < DrumKit::kNumChannels; ++i)
    {
        const auto id = "v" + String (i);
        const String n  = LMOne::kVoiceDefs[(size_t) i].name;

        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { id + "_level", 1 }, n + " Level",
            NormalisableRange<float> (0.0f, 1.5f, 0.001f), 1.0f));

        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { id + "_pan", 1 }, n + " Pan",
            NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.0f));

        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { id + "_tune", 1 }, n + " Tune",
            NormalisableRange<float> (-12.0f, 12.0f, 0.01f), 0.0f));

        layout.add (std::make_unique<AudioParameterBool> (
            ParameterID { id + "_mute", 1 }, n + " Mute", false));

        layout.add (std::make_unique<AudioParameterBool> (
            ParameterID { id + "_solo", 1 }, n + " Solo", false));

        // Per-track shuffle: same notched settings as the global knob, plus
        // "Follow" (default) which uses the global Shuffle for this track.
        layout.add (std::make_unique<AudioParameterChoice> (
            ParameterID { id + "_swing", 1 }, n + " Swing",
            juce::StringArray { "Follow", "Straight", "Light", "Medium", "Triplet", "Hard" }, 0));

        // Output routing: Main (default) or one of the numbered direct outs. Several
        // channels can share an "Out N" to group them; in LUNA, add that out in the
        // multi-out mixer. Filling Out 1, Out 2... in order keeps the buses contiguous.
        juce::StringArray outChoices { "Main" };
        for (int o = 1; o <= DrumKit::kNumChannels; ++o)
            outChoices.add ("Out " + juce::String (o));
        layout.add (std::make_unique<AudioParameterChoice> (
            ParameterID { id + "_out", 1 }, n + " Output", outChoices, 0));
    }

    return layout;
}

//==============================================================================
void LMOneAudioProcessor::setKit (DrumKit::Ptr newKit)
{
    // Reap previously-retired kits that no playing voice references anymore
    // (only the retired array holds them => refcount 1). Frees on this thread.
    for (int i = retiredKits.size(); --i >= 0;)
        if (auto* k = retiredKits.getObjectPointer (i))
            if (k->getReferenceCount() <= 1)
                retiredKits.remove (i);

    // Keep the outgoing kit alive until any in-flight voices finish with it.
    if (currentKit != nullptr)
        retiredKits.add (currentKit);

    currentKit = newKit;
    liveKit.store (newKit.get(), std::memory_order_release);

    sendChangeMessage(); // let the editor refresh per-voice sample labels
}

bool LMOneAudioProcessor::loadUserSample (int voiceIndex, const juce::File& file)
{
    if (currentKit == nullptr)
        return false;

    // Copy-on-write: clone, load one voice into the clone, publish atomically.
    DrumKit::Ptr next = new DrumKit (*currentKit);
    if (! KitFactory::loadVoiceFromFile (*next, voiceIndex, file))
        return false;

    setKit (next);
    return true;
}

bool LMOneAudioProcessor::restoreVoiceToFactory (int voiceIndex)
{
    if (currentKit == nullptr)
        return false;

    // Copy-on-write: clone, reset one voice to factory in the clone, publish.
    DrumKit::Ptr next = new DrumKit (*currentKit);
    if (! KitFactory::restoreVoiceToFactory (*next, voiceIndex))
        return false;

    setKit (next);
    return true;
}

bool LMOneAudioProcessor::voiceHasUserSample (int voiceIndex) const
{
    if (currentKit == nullptr || voiceIndex < 0 || voiceIndex >= DrumKit::kNumVoices)
        return false;

    return currentKit->voice (voiceIndex).sourceTag == "file";
}

juce::String LMOneAudioProcessor::getVoiceSourceLabel (int voiceIndex) const
{
    if (currentKit == nullptr || voiceIndex < 0 || voiceIndex >= DrumKit::kNumVoices)
        return {};

    const auto& vs = currentKit->voice (voiceIndex);
    if (vs.sourceTag == "file")
        return juce::File (vs.sourcePath).getFileName();

    return {};   // default/factory sound → show nothing (only user-loaded files get a label)
}

//==============================================================================
void LMOneAudioProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = sampleRate;

    // The kit is rate-independent (DrumVoice converts at playback), so we only
    // tell each voice the host rate here — no per-rate kit rebuild needed.
    for (auto& v : voices)
        v.prepare (sampleRate);

    sequencer.prepare (sampleRate);

    srrHoldL = srrHoldR = 0.0f;
    srrPhase = 0.0;
}

//==============================================================================
void LMOneAudioProcessor::publishPattern (const Pattern& p)
{
    const int next = 1 - patternState.live.load (std::memory_order_relaxed);
    patternState.slots[(size_t) next] = p;
    patternState.live.store (next, std::memory_order_release);
}

void LMOneAudioProcessor::pushRecordEvent (int lane, int step, juce::uint8 vel) noexcept
{
    int start1, size1, start2, size2;
    recFifo.prepareToWrite (1, start1, size1, start2, size2);
    if (size1 > 0)      recFifoBuf[(size_t) start1] = { lane, step, vel };
    else if (size2 > 0) recFifoBuf[(size_t) start2] = { lane, step, vel };
    recFifo.finishedWrite (size1 + size2);
}

bool LMOneAudioProcessor::pollRecordedNotes()
{
    const int ready = recFifo.getNumReady();
    if (ready <= 0)
        return false;

    int start1, size1, start2, size2;
    recFifo.prepareToRead (ready, start1, size1, start2, size2);

    auto apply = [this] (int start, int count)
    {
        for (int i = 0; i < count; ++i)
        {
            const auto& e = recFifoBuf[(size_t) (start + i)];
            if (e.lane >= 0 && e.lane < Pattern::kMaxLanes
                && e.step >= 0 && e.step < Pattern::kMaxSteps)
                workingPattern.vel[(size_t) e.lane][(size_t) e.step] = e.vel;  // overdub
        }
    };
    apply (start1, size1);
    apply (start2, size2);
    recFifo.finishedRead (size1 + size2);

    publishPattern (workingPattern);
    return true;
}

void LMOneAudioProcessor::setStep (int lane, int step, juce::uint8 velocity)
{
    if (lane < 0 || lane >= Pattern::kMaxLanes || step < 0 || step >= Pattern::kMaxSteps)
        return;

    workingPattern.vel[(size_t) lane][(size_t) step] = velocity;
    publishPattern (workingPattern);
}

void LMOneAudioProcessor::setPatternMeter (int num, int den, int rate)
{
    workingPattern.setMeter (num, den, rate);   // sets time sig + rate, recomputes numSteps
    publishPattern (workingPattern);
}

void LMOneAudioProcessor::clearPattern()
{
    const int keepSteps = workingPattern.numSteps;
    workingPattern.clear();
    workingPattern.numSteps = keepSteps;
    workingPattern.numLanes = DrumKit::kNumVoices;
    publishPattern (workingPattern);
    // Keep the current slot selected so Clear + Save wipes that preset.
}

//==============================================================================
void LMOneAudioProcessor::setCurrentBank (int bank)
{
    currentBank = juce::jlimit (0, kNumBanks - 1, bank);
}

void LMOneAudioProcessor::setCurrentSlot (int slot) noexcept
{
    currentSlot = juce::jlimit (-1, kBankSlots - 1, slot);
}

void LMOneAudioProcessor::loadSlot (int slot)
{
    if (slot < 0 || slot >= kBankSlots) return;

    currentSlot = slot;
    const auto& s = library[(size_t) currentBank][(size_t) slot];

    if (s.filled)
    {
        workingPattern = s.pattern;
        workingPattern.numLanes = DrumKit::kNumVoices;
        publishPattern (workingPattern);

        if (auto* p = apvts.getParameter ("seqTempo"))
            p->setValueNotifyingHost (p->convertTo0to1 ((float) s.tempo));
    }
    else
    {
        // Empty (user) preset -> empty grid, keeping the current length.
        const int keepSteps = workingPattern.numSteps;
        workingPattern.clear();
        workingPattern.numSteps = keepSteps;
        workingPattern.numLanes = DrumKit::kNumVoices;
        publishPattern (workingPattern);
    }
}

void LMOneAudioProcessor::saveSlot (int slot)
{
    if (slot < 0 || slot >= kBankSlots || currentBank < kNumFactoryBanks)
        return;   // factory banks are read-only

    currentSlot = slot;

    // Saving an empty grid wipes the slot (delete); otherwise store the pattern.
    bool empty = true;
    for (int lane = 0; lane < Pattern::kMaxLanes && empty; ++lane)
        for (int step = 0; step < Pattern::kMaxSteps; ++step)
            if (workingPattern.vel[(size_t) lane][(size_t) step] > 0) { empty = false; break; }

    auto& s = library[(size_t) currentBank][(size_t) slot];
    s.pattern = workingPattern;
    s.tempo   = juce::roundToInt (getSeqTempo());
    s.factory = false;
    s.filled  = ! empty;
    if (empty)
        s.name = {};
    else if (s.name.isEmpty())
        s.name = "User " + juce::String (currentBank + 1) + "." + juce::String (slot + 1);

    saveUserLibrary();
}

bool LMOneAudioProcessor::slotFilled (int slot) const
{
    return slot >= 0 && slot < kBankSlots && library[(size_t) currentBank][(size_t) slot].filled;
}

juce::String LMOneAudioProcessor::slotName (int slot) const
{
    return (slot >= 0 && slot < kBankSlots) ? library[(size_t) currentBank][(size_t) slot].name
                                            : juce::String();
}

juce::String LMOneAudioProcessor::slotGenre (int slot) const
{
    return (slot >= 0 && slot < kBankSlots) ? library[(size_t) currentBank][(size_t) slot].genre
                                            : juce::String();
}

//==============================================================================
void LMOneAudioProcessor::loadFactoryLibrary()
{
    const auto banks = FactoryGrooves::build();   // 10 genre banks x 10
    for (int bk = 0; bk < (int) banks.size() && bk < kNumBanks; ++bk)
        for (int sl = 0; sl < (int) banks[(size_t) bk].size() && sl < kBankSlots; ++sl)
        {
            auto& dst = library[(size_t) bk][(size_t) sl];
            dst.pattern = banks[(size_t) bk][(size_t) sl].pattern;
            dst.tempo   = banks[(size_t) bk][(size_t) sl].tempo;
            dst.name    = banks[(size_t) bk][(size_t) sl].name;
            dst.genre   = banks[(size_t) bk][(size_t) sl].genre;
            dst.filled  = true;
            dst.factory = true;
        }
}

juce::File LMOneAudioProcessor::userLibraryFile()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("LM-1");
    dir.createDirectory();
    return dir.getChildFile ("UserBanks.xml");
}

void LMOneAudioProcessor::loadUserLibrary()
{
    const auto f = userLibraryFile();
    if (! f.existsAsFile()) return;

    auto xml = juce::XmlDocument::parse (f);
    if (xml == nullptr) return;

    auto root = juce::ValueTree::fromXml (*xml);
    if (! root.hasType ("USERBANKS")) return;

    for (const auto& v : root)
    {
        const int bk = (int) v.getProperty ("bank", -1);
        const int sl = (int) v.getProperty ("slot", -1);
        if (bk < kNumFactoryBanks || bk >= kNumBanks || sl < 0 || sl >= kBankSlots)
            continue;

        auto& s = library[(size_t) bk][(size_t) sl];
        s.tempo = (int) v.getProperty ("tempo", 120);
        s.name  = v.getProperty ("name", "").toString();
        if (auto pt = v.getChildWithName ("PATTERN"); pt.isValid())
            s.pattern.fromValueTree (pt);
        s.pattern.numLanes = DrumKit::kNumVoices;
        s.filled  = true;
        s.factory = false;
    }
}

void LMOneAudioProcessor::saveUserLibrary() const
{
    juce::ValueTree root ("USERBANKS");
    for (int bk = kNumFactoryBanks; bk < kNumBanks; ++bk)
        for (int sl = 0; sl < kBankSlots; ++sl)
        {
            const auto& s = library[(size_t) bk][(size_t) sl];
            if (! s.filled) continue;

            juce::ValueTree v ("SLOT");
            v.setProperty ("bank",  bk,      nullptr);
            v.setProperty ("slot",  sl,      nullptr);
            v.setProperty ("tempo", s.tempo, nullptr);
            v.setProperty ("name",  s.name,  nullptr);
            v.appendChild (s.pattern.toValueTree(), nullptr);
            root.appendChild (v, nullptr);
        }

    if (auto xml = root.createXml())
        xml->writeTo (userLibraryFile());
}

bool LMOneAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Main out must be stereo or mono.
    const auto main = layouts.getMainOutputChannelSet();
    if (main != juce::AudioChannelSet::stereo() && main != juce::AudioChannelSet::mono())
        return false;

    // Each direct out must be stereo or disabled (LUNA enables the ones it uses).
    for (int b = 1; b < layouts.outputBuses.size(); ++b)
    {
        const auto s = layouts.outputBuses.getUnchecked (b);
        if (! s.isDisabled() && s != juce::AudioChannelSet::stereo())
            return false;
    }
    return true;
}

//==============================================================================
void LMOneAudioProcessor::triggerPad (int padIndex, float velocity, DrumKit* kit)
{
    if (kit == nullptr || padIndex < 0 || padIndex >= (int) voices.size())
        return;

    // Choke: stop other active voices that share this voice's choke group.
    const int group = pads[(size_t) padIndex].chokeGroup;
    if (group >= 0)
        for (int i = 0; i < (int) pads.size(); ++i)
            if (i != padIndex && pads[(size_t) i].chokeGroup == group)
                voices[(size_t) i].stop();

    const int ch = channelForVoice (padIndex);
    const float voiceTune = voiceParams[(size_t) ch].tune->load() + globalTune->load();
    voices[(size_t) padIndex].assign (kit, padIndex);
    voices[(size_t) padIndex].trigger (velocity, voiceTune);
}

float LMOneAudioProcessor::applyCrush (float s, int bits, float mix)
{
    const float steps = (float) (1 << bits);
    const float crushed = std::round (s * steps) / steps;
    return s + mix * (crushed - s);
}

void LMOneAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                        juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    buffer.clear();

    // One lock-free snapshot of the live kit for the whole block.
    DrumKit* kit = liveKit.load (std::memory_order_acquire);

    const int n = buffer.getNumSamples();

    // Merge notes generated by the on-screen pads into the incoming MIDI stream.
    keyboardState.processNextMidiBuffer (midiMessages, 0, n, true);

    // --- Collect trigger events (voice, sample offset, velocity) --------------
    struct Ev { int voice; int offset; float vel; };
    constexpr int kMaxEvents = 256;
    std::array<Ev, (size_t) kMaxEvents> events;
    int numEvents = 0;
    auto addEvent = [&] (int v, int off, float vel)
    {
        if (numEvents < kMaxEvents)
            events[(size_t) numEvents++] = { v, juce::jlimit (0, juce::jmax (0, n - 1), off), vel };
    };

    // Sequencer steps (run first so live-record can quantize against this block's clock).
    const Pattern pat = patternState.slots[(size_t) patternState.live.load (std::memory_order_acquire)];
    {
        Sequencer::Transport tr;
        tr.sampleRate      = currentSampleRate;
        tr.seqEnabled      = true;   // runs whenever the transport (host or Play) is rolling
        tr.internalPlaying = internalPlaying.load();
        tr.internalBpm     = seqTempoParam->load();
        tr.swing           = shuffleAmountForIndex ((int) shuffleParam->load());   // choice index -> amount
        if (auto* ph = getPlayHead())
            if (auto pos = ph->getPosition())
            {
                tr.hostPlaying = pos->getIsPlaying();
                tr.bpm         = pos->getBpm().orFallback (tr.bpm);
                tr.hostPpq     = pos->getPpqPosition().orFallback (0.0);
            }

        // Per-lane swing amount. Each track is either "Follow" (index 0 -> use the
        // global Shuffle) or its own notched setting. A lane follows the channel it
        // renders through, so the open hat (voice 12) tracks the Hi-hats channel.
        const float globalSwing = tr.swing;   // already mapped from the global choice
        float laneSwing[Pattern::kMaxLanes];
        for (int lane = 0; lane < Pattern::kMaxLanes; ++lane)
        {
            float amt = globalSwing;
            if (lane < DrumKit::kNumVoices)
                if (auto* sp = voiceParams[(size_t) channelForVoice (lane)].swing)
                {
                    const int t = (int) sp->load();   // 0 = Follow, else setting (1..5)
                    amt = (t <= 0) ? globalSwing : shuffleAmountForIndex (t - 1);
                }
            laneSwing[(size_t) lane] = amt;
        }
        tr.laneSwing = laneSwing;

        sequencer.process (pat, tr, n, [&] (int lane, int off, float vel) { addEvent (lane, off, vel); });
    }

    // Live MIDI/pad note-ons, at their sample positions. When record is armed and
    // the sequencer is rolling, write each hit onto the nearest step (overdub).
    const bool recording = recordArmed.load();
    for (const auto metadata : midiMessages)
    {
        const auto msg = metadata.getMessage();
        if (msg.isNoteOn())
            for (int p = 0; p < (int) pads.size(); ++p)
                if (pads[(size_t) p].midiNote == msg.getNoteNumber())
                {
                    addEvent (p, metadata.samplePosition, msg.getFloatVelocity());
                    if (recording)
                    {
                        const int step = sequencer.quantizeToNearestStep (metadata.samplePosition, pat.numSteps,
                                                                          TimeGrid::stepPpq (pat.rate));
                        if (step >= 0)
                            pushRecordEvent (p, step,
                                (juce::uint8) juce::jlimit (1, 127, juce::roundToInt (msg.getFloatVelocity() * 127.0f)));
                    }
                }
    }

    // Sort events by sample offset (insertion sort — events arrive nearly sorted).
    for (int i = 1; i < numEvents; ++i)
    {
        const Ev key = events[(size_t) i];
        int j = i - 1;
        while (j >= 0 && events[(size_t) j].offset > key.offset)
        {
            events[(size_t) (j + 1)] = events[(size_t) j];
            --j;
        }
        events[(size_t) (j + 1)] = key;
    }

    // Per-channel mix gains, computed once. Solo wins over mute.
    bool anySolo = false;
    std::array<bool,  (size_t) DrumKit::kNumChannels> soloed {};
    std::array<float, (size_t) DrumKit::kNumChannels> lvl {}, pn {};
    for (int c = 0; c < DrumKit::kNumChannels; ++c)
    {
        soloed[(size_t) c] = voiceParams[(size_t) c].solo->load() > 0.5f;
        anySolo = anySolo || soloed[(size_t) c];
    }
    for (int c = 0; c < DrumKit::kNumChannels; ++c)
    {
        const bool audible = anySolo ? soloed[(size_t) c]
                                     : voiceParams[(size_t) c].mute->load() <= 0.5f;
        lvl[(size_t) c] = audible ? voiceParams[(size_t) c].level->load() : 0.0f;
        pn[(size_t) c]  = voiceParams[(size_t) c].pan->load();
    }

    // Output routing: Main mix vs each channel's direct out. Grab the bus views
    // once; a channel set to Direct falls back to Main if LUNA hasn't enabled
    // that bus yet (so it's never silent).
    auto mainBus = getBusBuffer (buffer, false, 0);
    std::array<juce::AudioBuffer<float>, (size_t) DrumKit::kNumChannels> directBus;
    std::array<int, (size_t) DrumKit::kNumChannels> outIdx {};
    for (int c = 0; c < DrumKit::kNumChannels; ++c)
    {
        directBus[(size_t) c] = getBusBuffer (buffer, false, 1 + c);
        outIdx[(size_t) c]    = voiceParams[(size_t) c].out != nullptr
                              ? (int) voiceParams[(size_t) c].out->load() : 0;   // 0=Main, k=Out k
    }

    // --- Segmented render: fire events at their offsets, render between them --
    // (DrumVoice::render is additive and advances each voice's phase, so a note
    //  triggered at offset i starts exactly at sample i — sample-accurate.)
    int cursor = 0, e = 0;
    while (cursor < n)
    {
        while (e < numEvents && events[(size_t) e].offset == cursor)
        {
            triggerPad (events[(size_t) e].voice, events[(size_t) e].vel, kit);
            ++e;
        }
        const int next = (e < numEvents) ? events[(size_t) e].offset : n;
        for (int v = 0; v < (int) voices.size(); ++v)
        {
            const int c = channelForVoice (v);
            const int o = outIdx[(size_t) c];                      // 0 = Main, 1..N = Out o
            const bool direct = o >= 1 && o <= DrumKit::kNumChannels
                              && directBus[(size_t) (o - 1)].getNumChannels() > 0;
            auto& dest = direct ? directBus[(size_t) (o - 1)] : mainBus;
            voices[(size_t) v].render (dest, cursor, next - cursor, lvl[(size_t) c], pn[(size_t) c]);
        }
        cursor = next;
    }

    // --- Lo-fi stage: bit crush + sample-rate reduction, blended by 'lofi' -----
    const float amt = lofiAmount->load();
    if (amt > 0.0001f)
    {
        const int   bits      = (int) juce::jmap (amt, 0.0f, 1.0f, 14.0f, 5.0f); // 14 -> 5 bits
        const float targetSR  = juce::jmap (amt, 0.0f, 1.0f,
                                            (float) currentSampleRate, 6000.0f);
        const double inc      = targetSR / currentSampleRate;

        const int numCh = buffer.getNumChannels();
        float* L = buffer.getWritePointer (0);
        float* R = numCh > 1 ? buffer.getWritePointer (1) : nullptr;

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            srrPhase += inc;
            if (srrPhase >= 1.0)
            {
                srrPhase -= 1.0;
                srrHoldL = L[i];
                if (R) srrHoldR = R[i];
            }
            L[i] = applyCrush (srrHoldL, bits, amt);
            if (R) R[i] = applyCrush (srrHoldR, bits, amt);
        }
    }

    // --- Master gain ----------------------------------------------------------
    buffer.applyGain (juce::Decibels::decibelsToGain (masterGainDb->load()));
}

//==============================================================================
juce::AudioProcessorEditor* LMOneAudioProcessor::createEditor()
{
    return new LMOneAudioProcessorEditor (*this);
}

juce::ValueTree LMOneAudioProcessor::captureStateTree()
{
    auto state = apvts.copyState();   // type "PARAMS" (params + globals)

    // Drop any stale children we re-add below (a prior restore may have left them).
    for (auto* name : { "KIT", "PATTERN", "PATTERNS", "BANKSEL" })
        if (auto stale = state.getChildWithName (name); stale.isValid())
            state.removeChild (stale, nullptr);

    // Kit: which sample is loaded per voice + trim, so a project/preset reopens right.
    juce::ValueTree kit ("KIT");
    if (currentKit != nullptr)
    {
        for (int i = 0; i < DrumKit::kNumVoices; ++i)
        {
            const auto& vs = currentKit->voice (i);
            juce::ValueTree v ("VOICE");
            v.setProperty ("index",  i,              nullptr);
            v.setProperty ("source", vs.sourceTag,   nullptr);
            v.setProperty ("path",   vs.sourcePath,  nullptr);
            v.setProperty ("start",  vs.startSample, nullptr);
            v.setProperty ("end",    vs.endSample,   nullptr);
            kit.appendChild (v, nullptr);
        }
    }
    state.appendChild (kit, nullptr);

    // Working sequence.
    state.appendChild (workingPattern.toValueTree(), nullptr);   // type "PATTERN"

    // Bank/slot selection, so the highlighted preset survives reopen / reload.
    juce::ValueTree banksel ("BANKSEL");
    banksel.setProperty ("bank", currentBank, nullptr);
    banksel.setProperty ("slot", currentSlot, nullptr);
    state.appendChild (banksel, nullptr);

    return state;
}

void LMOneAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = captureStateTree().createXml())
        copyXmlToBinary (*xml, destData);
}

void LMOneAudioProcessor::restoreStateTree (const juce::ValueTree& tree)
{
    if (! tree.isValid() || ! tree.hasType (apvts.state.getType()))
        return;

    apvts.replaceState (tree); // restores all parameters (mixer + globals)

    // Rebuild the kit on this (message) thread, then publish it atomically.
    if (auto kitTree = tree.getChildWithName ("KIT"); kitTree.isValid())
    {
        DrumKit::Ptr next = KitFactory::buildFactoryKit(); // start from defaults
        for (const auto& v : kitTree)
        {
            const int idx = (int) v.getProperty ("index", -1);
            if (idx < 0 || idx >= DrumKit::kNumVoices)
                continue;

            const juce::String src = v.getProperty ("source", "factory");
            if (src == "file")
            {
                const juce::File f (v.getProperty ("path", "").toString());
                if (f.existsAsFile())
                    KitFactory::loadVoiceFromFile (*next, idx, f);
                // else: keep the default sample for that voice
            }

            auto& vs = next->voice (idx);
            vs.startSample = (int) v.getProperty ("start", 0);
            vs.endSample   = (int) v.getProperty ("end",  -1);
        }
        setKit (next);
    }

    // Restore the working sequence (legacy bank format: take its current slot).
    if (auto pt = tree.getChildWithName ("PATTERN"); pt.isValid())
    {
        workingPattern.fromValueTree (pt);
    }
    else if (auto pts = tree.getChildWithName ("PATTERNS"); pts.isValid())
    {
        const int cur = juce::jlimit (0, kBankSlots - 1, (int) pts.getProperty ("current", 0));
        for (const auto& pv : pts)
            if ((int) pv.getProperty ("slot", -1) == cur) { workingPattern.fromValueTree (pv); break; }
    }

    // Restore the highlighted bank/slot (cosmetic — the pattern itself is above).
    if (auto bs = tree.getChildWithName ("BANKSEL"); bs.isValid())
    {
        currentBank = juce::jlimit (0, kNumBanks - 1, (int) bs.getProperty ("bank", 0));
        currentSlot = juce::jlimit (-1, kBankSlots - 1, (int) bs.getProperty ("slot", 0));
    }

    // Lane count is fixed by the instrument, not by the saved pattern.
    workingPattern.numLanes = DrumKit::kNumVoices;
    publishPattern (workingPattern);

    sendChangeMessage(); // refresh the editor (grid + sample labels) after a restore
}

void LMOneAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        restoreStateTree (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
// This creates new instances of the plugin.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LMOneAudioProcessor();
}
