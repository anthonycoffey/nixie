#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "FactoryGrooves.h"

//==============================================================================
LMOneAudioProcessor::LMOneAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output",
                                                    juce::AudioChannelSet::stereo(),
                                                    true)),
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
    }

    seqTempoParam = apvts.getRawParameterValue ("seqTempo");
    shuffleParam  = apvts.getRawParameterValue ("shuffle");

    // Preset library: factory banks 1-5 + any user-saved banks. Start on the
    // first factory groove so the sequencer plays something out of the box.
    loadFactoryLibrary();
    loadUserLibrary();
    currentBank     = 0;
    workingPattern  = library[0][0].pattern;   // "Basic Rock"
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

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "shuffle", 1 }, "Shuffle",
        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f));

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

        // Per-track shuffle amount, added on top of the global Shuffle for this
        // track (0 = no extra swing; the track just follows the global knob).
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { id + "_swing", 1 }, n + " Swing",
            NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f));
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

void LMOneAudioProcessor::setPatternLength (int numSteps)
{
    workingPattern.numSteps = juce::jlimit (1, Pattern::kMaxSteps, numSteps);
    publishPattern (workingPattern);
}

void LMOneAudioProcessor::clearPattern()
{
    const int keepSteps = workingPattern.numSteps;
    workingPattern.clear();
    workingPattern.numSteps = keepSteps;
    workingPattern.numLanes = DrumKit::kNumVoices;
    currentSlot = -1;                 // a cleared grid no longer matches any slot
    publishPattern (workingPattern);
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

    const auto& s = library[(size_t) currentBank][(size_t) slot];
    if (! s.filled) return;

    currentSlot = slot;
    workingPattern = s.pattern;
    workingPattern.numLanes = DrumKit::kNumVoices;
    publishPattern (workingPattern);

    if (auto* p = apvts.getParameter ("seqTempo"))
        p->setValueNotifyingHost (p->convertTo0to1 ((float) s.tempo));
}

void LMOneAudioProcessor::saveSlot (int slot)
{
    if (slot < 0 || slot >= kBankSlots || currentBank < kNumFactoryBanks)
        return;   // factory banks are read-only

    currentSlot = slot;
    auto& s = library[(size_t) currentBank][(size_t) slot];
    s.pattern = workingPattern;
    s.tempo   = juce::roundToInt (getSeqTempo());
    s.factory = false;
    s.filled  = true;
    if (s.name.isEmpty())
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

//==============================================================================
void LMOneAudioProcessor::loadFactoryLibrary()
{
    const auto banks = FactoryGrooves::build();   // 5 banks x 8
    for (int bk = 0; bk < (int) banks.size() && bk < kNumBanks; ++bk)
        for (int sl = 0; sl < (int) banks[(size_t) bk].size() && sl < kBankSlots; ++sl)
        {
            auto& dst = library[(size_t) bk][(size_t) sl];
            dst.pattern = banks[(size_t) bk][(size_t) sl].pattern;
            dst.tempo   = banks[(size_t) bk][(size_t) sl].tempo;
            dst.name    = banks[(size_t) bk][(size_t) sl].name;
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
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo()
        || out == juce::AudioChannelSet::mono();
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

    // Live MIDI note-ons, at their sample positions. When record is armed and the
    // sequencer is rolling, also write the hit onto the playing step (overdub).
    const int recStep = recordArmed.load() ? sequencer.getCurrentStepForUi() : -1;
    for (const auto metadata : midiMessages)
    {
        const auto msg = metadata.getMessage();
        if (msg.isNoteOn())
            for (int p = 0; p < (int) pads.size(); ++p)
                if (pads[(size_t) p].midiNote == msg.getNoteNumber())
                {
                    addEvent (p, metadata.samplePosition, msg.getFloatVelocity());
                    if (recStep >= 0)
                        pushRecordEvent (p, recStep,
                            (juce::uint8) juce::jlimit (1, 127, juce::roundToInt (msg.getFloatVelocity() * 127.0f)));
                }
    }

    // Sequencer steps.
    {
        Sequencer::Transport tr;
        tr.sampleRate      = currentSampleRate;
        tr.seqEnabled      = true;   // runs whenever the transport (host or Play) is rolling
        tr.internalPlaying = internalPlaying.load();
        tr.internalBpm     = seqTempoParam->load();
        tr.swing           = shuffleParam->load();
        if (auto* ph = getPlayHead())
            if (auto pos = ph->getPosition())
            {
                tr.hostPlaying = pos->getIsPlaying();
                tr.bpm         = pos->getBpm().orFallback (tr.bpm);
                tr.hostPpq     = pos->getPpqPosition().orFallback (0.0);
            }

        // Per-lane swing amount = global Shuffle + this lane's per-track Shuffle
        // (clamped 0..1). A lane follows the channel it renders through, so the
        // open hat (voice 12) tracks the Hi-hats channel.
        const float globalSwing = tr.swing;
        float laneSwing[Pattern::kMaxLanes];
        for (int lane = 0; lane < Pattern::kMaxLanes; ++lane)
        {
            float perTrack = 0.0f;
            if (lane < DrumKit::kNumVoices)
                if (auto* sp = voiceParams[(size_t) channelForVoice (lane)].swing)
                    perTrack = sp->load();
            laneSwing[(size_t) lane] = juce::jlimit (0.0f, 1.0f, globalSwing + perTrack);
        }
        tr.laneSwing = laneSwing;

        const Pattern pat = patternState.slots[(size_t) patternState.live.load (std::memory_order_acquire)];
        sequencer.process (pat, tr, n, [&] (int lane, int off, float vel) { addEvent (lane, off, vel); });
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
            voices[(size_t) v].render (buffer, cursor, next - cursor, lvl[(size_t) c], pn[(size_t) c]);
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
