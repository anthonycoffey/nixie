#include "PluginProcessor.h"
#include "PluginEditor.h"

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
    }

    seqEnabledParam = apvts.getRawParameterValue ("seqEnabled");
    seqTempoParam   = apvts.getRawParameterValue ("seqTempo");

    // Seed a simple demo beat into slot 1 so the sequencer plays out of the box.
    {
        Pattern& p = patternBank[0];
        p.numSteps = 16;
        p.numLanes = DrumKit::kNumVoices;
        for (int s = 0; s < 16; s += 4) p.setStep (0, s, 110);  // Bass: four-on-the-floor
        p.setStep (1, 4, 100);  p.setStep (1, 12, 100);         // Snare: backbeat
        for (int s = 0; s < 16; s += 2) p.setStep (2, s, 80);   // Hi-Hat: eighths
    }
    currentPattern = 0;
    publishPattern (patternBank[0]);

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
    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { "seqEnabled", 1 }, "Sequencer", true));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "seqTempo", 1 }, "Seq Tempo",
        NormalisableRange<float> (40.0f, 240.0f, 0.1f), 120.0f));

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

juce::String LMOneAudioProcessor::getVoiceSourceLabel (int voiceIndex) const
{
    if (currentKit == nullptr || voiceIndex < 0 || voiceIndex >= DrumKit::kNumVoices)
        return {};

    const auto& vs = currentKit->voice (voiceIndex);
    if (vs.sourceTag == "file")
        return juce::File (vs.sourcePath).getFileName();

    return vs.isProcedural ? juce::String ("(procedural)") : vs.sourceTag;
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

Pattern LMOneAudioProcessor::getPatternSnapshot() const
{
    return patternState.slots[(size_t) patternState.live.load (std::memory_order_acquire)];
}

void LMOneAudioProcessor::selectPattern (int index)
{
    if (index < 0 || index >= kNumPatterns || index == currentPattern)
        return;

    currentPattern = index;
    publishPattern (patternBank[(size_t) index]);
}

void LMOneAudioProcessor::setStep (int lane, int step, juce::uint8 velocity)
{
    if (lane < 0 || lane >= Pattern::kMaxLanes || step < 0 || step >= Pattern::kMaxSteps)
        return;

    auto& p = patternBank[(size_t) currentPattern];
    p.vel[(size_t) lane][(size_t) step] = velocity;
    publishPattern (p);
}

void LMOneAudioProcessor::setPatternLength (int numSteps)
{
    auto& p = patternBank[(size_t) currentPattern];
    p.numSteps = juce::jlimit (1, Pattern::kMaxSteps, numSteps);
    publishPattern (p);
}

void LMOneAudioProcessor::clearPattern()
{
    auto& p = patternBank[(size_t) currentPattern];
    p.clear();
    publishPattern (p);
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

    // Live MIDI note-ons, at their sample positions.
    for (const auto metadata : midiMessages)
    {
        const auto msg = metadata.getMessage();
        if (msg.isNoteOn())
            for (int p = 0; p < (int) pads.size(); ++p)
                if (pads[(size_t) p].midiNote == msg.getNoteNumber())
                    addEvent (p, metadata.samplePosition, msg.getFloatVelocity());
    }

    // Sequencer steps.
    {
        Sequencer::Transport tr;
        tr.sampleRate      = currentSampleRate;
        tr.seqEnabled      = seqEnabledParam->load() > 0.5f;
        tr.internalPlaying = internalPlaying.load();
        tr.internalBpm     = seqTempoParam->load();
        if (auto* ph = getPlayHead())
            if (auto pos = ph->getPosition())
            {
                tr.hostPlaying = pos->getIsPlaying();
                tr.bpm         = pos->getBpm().orFallback (tr.bpm);
                tr.hostPpq     = pos->getPpqPosition().orFallback (0.0);
            }

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
    for (auto* name : { "KIT", "PATTERN", "PATTERNS" })
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

    // Pattern bank (all slots + which one is current).
    juce::ValueTree patterns ("PATTERNS");
    patterns.setProperty ("current", currentPattern, nullptr);
    for (int i = 0; i < kNumPatterns; ++i)
    {
        auto pv = patternBank[(size_t) i].toValueTree();   // type "PATTERN"
        pv.setProperty ("slot", i, nullptr);
        patterns.appendChild (pv, nullptr);
    }
    state.appendChild (patterns, nullptr);

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

    // Restore the pattern bank (with a legacy single-PATTERN fallback).
    if (auto pts = tree.getChildWithName ("PATTERNS"); pts.isValid())
    {
        for (const auto& pv : pts)
        {
            const int slot = (int) pv.getProperty ("slot", -1);
            if (slot >= 0 && slot < kNumPatterns)
                patternBank[(size_t) slot].fromValueTree (pv);
        }
        currentPattern = juce::jlimit (0, kNumPatterns - 1, (int) pts.getProperty ("current", 0));
    }
    else if (auto pt = tree.getChildWithName ("PATTERN"); pt.isValid())
    {
        patternBank[0].fromValueTree (pt);
        currentPattern = 0;
    }

    // Lane count is fixed by the instrument, not by the saved pattern — keep every
    // slot at the current voice count so older saves still show/fire all lanes.
    for (auto& bp : patternBank)
        bp.numLanes = DrumKit::kNumVoices;

    publishPattern (patternBank[(size_t) currentPattern]);

    sendChangeMessage(); // refresh the editor (grid + selector + sample labels)
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
