#include "PluginProcessor.h"
#include "PluginEditor.h"

#if LMONE_HAS_BINARY_DATA
 #include "BinaryData.h"
#endif

//==============================================================================
LMOneAudioProcessorEditor::LMOneAudioProcessorEditor (LMOneAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p), grid (p), midiDrag (p)
{
    setLookAndFeel (&lookAndFeel);

   #if LMONE_HAS_BINARY_DATA
    for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
        if (juce::String (BinaryData::originalFilenames[i]).equalsIgnoreCase ("wood.jpeg"))
        {
            int sz = 0;
            if (auto* d = BinaryData::getNamedResource (BinaryData::namedResourceList[i], sz))
                woodImage = juce::ImageFileFormat::loadFrom (d, (size_t) sz);
            break;
        }
   #endif

    // One mixer strip per channel (12). The open-hat voice shares the Hi-Hat
    // channel, so it has no strip of its own.
    for (int i = 0; i < DrumKit::kNumChannels; ++i)
    {
        auto* s = new VoiceStripComponent (processor, i);
        addAndMakeVisible (s);
        strips.add (s);
    }

    auto setupSlider = [this] (juce::Slider& s, juce::Label& lab, const juce::String& text)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
        addAndMakeVisible (s);
        lab.setText (text, juce::dontSendNotification);
        lab.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (lab);
    };

    setupSlider (masterSlider,  masterLabel,  "Master");
    setupSlider (lofiSlider,    lofiLabel,    "Lo-Fi");
    setupSlider (tuneSlider,    tuneLabel,    "Tune");
    setupSlider (shuffleSlider, shuffleLabel, "Shuffle");

    masterAttach  = std::make_unique<SliderAttachment> (processor.apvts, "masterGain", masterSlider);
    lofiAttach    = std::make_unique<SliderAttachment> (processor.apvts, "lofi",       lofiSlider);
    tuneAttach    = std::make_unique<SliderAttachment> (processor.apvts, "tune",       tuneSlider);
    shuffleAttach = std::make_unique<SliderAttachment> (processor.apvts, "shuffle",    shuffleSlider);

    // --- Transport bar -------------------------------------------------------
    // PLAY toggles the internal clock; the lamp (not the label/colour) shows state.
    playButton.onClick = [this]
    {
        const bool nowPlaying = ! processor.isInternalPlaying();
        processor.setInternalPlaying (nowPlaying);
        if (! nowPlaying)
            processor.setRecordArmed (false);   // stopping the transport disarms record
    };
    addAndMakeVisible (playButton);

    // REC arms recording and auto-starts the transport; turning it off leaves the
    // sequencer rolling.
    recButton.onClick = [this]
    {
        const bool arm = ! processor.isRecordArmed();
        processor.setRecordArmed (arm);
        if (arm)
            processor.setInternalPlaying (true);
    };
    addAndMakeVisible (recButton);

    tempoLabel.setText ("Tempo", juce::dontSendNotification);
    tempoLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (tempoLabel);

    tempoSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    tempoSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0); // value shows on the LED
    addAndMakeVisible (tempoSlider);
    tempoAttach = std::make_unique<SliderAttachment> (processor.apvts, "seqTempo", tempoSlider);

    addAndMakeVisible (stepLed);
    addAndMakeVisible (tempoLed);

    stepsLabel.setText ("Steps", juce::dontSendNotification);
    stepsLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (stepsLabel);

    stepsBox.addItem ("8",  8);
    stepsBox.addItem ("16", 16);
    stepsBox.addItem ("32", 32);
    stepsBox.setSelectedId (processor.getNumSteps(), juce::dontSendNotification);
    stepsBox.onChange = [this]
    {
        const int s = stepsBox.getSelectedId();
        if (s > 0) { processor.setPatternLength (s); grid.reloadFromProcessor(); }
    };
    addAndMakeVisible (stepsBox);

    clearButton.setTooltip (juce::String::fromUTF8 (
        "Clears the grid only \xE2\x80\x94 bank grooves are safe; click a slot to reload one"));
    clearButton.onClick = [this]
    {
        juce::NativeMessageBox::showOkCancelBox (
            juce::MessageBoxIconType::QuestionIcon,
            "Clear grid?",
            juce::String::fromUTF8 ("Clears the current sequence. Your saved grooves are "
                                    "safe \xE2\x80\x94 click a slot to reload one."),
            this,
            juce::ModalCallbackFunction::create ([this] (int result)
            {
                if (result == 0)        // 0 = Cancel
                    return;

                processor.clearPattern();   // also sets the processor's slot to "none"
                grid.reloadFromProcessor();
                refreshBankUI();
            }));
    };
    addAndMakeVisible (clearButton);

    // Gear/options menu — keeps low-frequency actions off the panel.
    optionsButton.setButtonText (juce::String::fromUTF8 ("\xE2\x9A\x99")); // gear
    optionsButton.onClick = [this]
    {
        juce::PopupMenu m;
        m.addItem (1, juce::String::fromUTF8 ("Export MIDI to file\xE2\x80\xA6"));
        m.addSeparator();
        m.addItem (2, juce::String::fromUTF8 ("Save preset\xE2\x80\xA6"));
        m.addItem (3, juce::String::fromUTF8 ("Load preset\xE2\x80\xA6"));

        const auto presets = presetManager.list();
        if (! presets.isEmpty())
        {
            juce::PopupMenu sub;
            for (int i = 0; i < presets.size(); ++i)
                sub.addItem (100 + i, presets[i].getFileNameWithoutExtension());
            m.addSubMenu ("Load recent", sub);
        }

        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&optionsButton),
            [this, presets] (int result)
            {
                if      (result == 1) exportMidiToFile();
                else if (result == 2) savePresetDialog();
                else if (result == 3) loadPresetDialog();
                else if (result >= 100 && result - 100 < presets.size())
                    presetManager.load (presets[result - 100]);
            });
    };
    addAndMakeVisible (optionsButton);

    // Preset library: Bank LED + prev/next, 8 slot buttons, Save.
    bankLabel.setText ("Bank", juce::dontSendNotification);
    bankLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (bankLabel);
    addAndMakeVisible (bankLed);

    bankPrev.onClick = [this] { gotoBank (processor.getCurrentBank() - 1); };
    bankNext.onClick = [this] { gotoBank (processor.getCurrentBank() + 1); };
    addAndMakeVisible (bankPrev);
    addAndMakeVisible (bankNext);

    for (int i = 0; i < LMOneAudioProcessor::kBankSlots; ++i)
    {
        auto* b = new juce::TextButton (juce::String (i + 1));
        b->setColour (juce::TextButton::buttonOnColourId, juce::Colours::orange);
        b->onClick = [this, i]
        {
            processor.setCurrentSlot (i);
            if (processor.slotFilled (i))
            {
                processor.loadSlot (i);
                grid.reloadFromProcessor();
            }
            refreshBankUI();
        };
        addAndMakeVisible (b);
        slotButtons.add (b);
    }

    saveButton.onClick = [this] { processor.saveSlot (processor.getCurrentSlot()); refreshBankUI(); };
    addAndMakeVisible (saveButton);

    refreshBankUI();

    addAndMakeVisible (grid);
    addAndMakeVisible (midiDrag);

    processor.addChangeListener (this); // refresh sample labels + grid when state changes
    startTimerHz (20);                  // step readout + playhead

    const int stripW = 74;
    setSize (stripW * DrumKit::kNumChannels + 20 + 2 * kCheek, 820 + kBottomLip + 12);
}

LMOneAudioProcessorEditor::~LMOneAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
    stopTimer();
    processor.removeChangeListener (this);
}

void LMOneAudioProcessorEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    for (auto* s : strips)
        s->refreshSourceLabel();

    grid.reloadFromProcessor();
    stepsBox.setSelectedId (processor.getNumSteps(), juce::dontSendNotification);
    refreshBankUI();
}

void LMOneAudioProcessorEditor::timerCallback()
{
    const int step = processor.getCurrentStep();
    stepLed.setText (step < 0 ? juce::String ("--") : juce::String (step + 1));
    tempoLed.setText (juce::String (juce::roundToInt (processor.getSeqTempo())));
    grid.setPlayingStep (step);

    playButton.setLedOn (step >= 0);                 // lit while the sequencer rolls
    recButton.setLedOn (processor.isRecordArmed());

    if (processor.pollRecordedNotes())               // drain live-recorded hits onto the grid
        grid.reloadFromProcessor();
}

void LMOneAudioProcessorEditor::exportMidiToFile()
{
    midiChooser = std::make_unique<juce::FileChooser> (
        "Export pattern as MIDI",
        juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("LM-1 pattern.mid"),
        "*.mid");
    midiChooser->launchAsync (
        juce::FileBrowserComponent::saveMode
        | juce::FileBrowserComponent::canSelectFiles
        | juce::FileBrowserComponent::warnAboutOverwriting,
        [this] (const juce::FileChooser& fc)
        {
            auto f = fc.getResult();
            if (f == juce::File()) return;
            if (! f.hasFileExtension ("mid")) f = f.withFileExtension ("mid");
            const auto mf = MidiExport::build (processor.getPattern(), (double) processor.getSeqTempo());
            if (auto os = std::unique_ptr<juce::FileOutputStream> (f.createOutputStream()))
            {
                os->setPosition (0);
                os->truncate();
                mf.writeTo (*os, 1);
            }
        });
}

void LMOneAudioProcessorEditor::refreshBankUI()
{
    bankLed.setText (juce::String (processor.getCurrentBank() + 1));
    for (int i = 0; i < slotButtons.size(); ++i)
    {
        const bool filled = processor.slotFilled (i);
        slotButtons[i]->setToggleState (i == processor.getCurrentSlot(), juce::dontSendNotification);
        slotButtons[i]->setColour (juce::TextButton::buttonColourId,
                                   filled ? juce::Colour (0xff2c2926) : juce::Colour (0xff191817));
        slotButtons[i]->setTooltip (filled ? processor.slotName (i) : juce::String ("(empty)"));
    }
    saveButton.setEnabled (! processor.currentBankIsFactory());
}

void LMOneAudioProcessorEditor::gotoBank (int newBank)
{
    processor.setCurrentBank (newBank);

    // Auto-load the groove under the selected slot so flipping banks previews
    // them (the jukebox feel). After a Clear nothing is selected, so fall back
    // to the first slot. An empty slot loads nothing and leaves the grid alone,
    // which keeps an unsaved beat intact when you browse to a blank user bank.
    int slot = processor.getCurrentSlot();
    if (slot < 0)
        slot = 0;
    processor.setCurrentSlot (slot);

    if (processor.slotFilled (slot))
    {
        processor.loadSlot (slot);
        grid.reloadFromProcessor();
    }

    refreshBankUI();
}

void LMOneAudioProcessorEditor::savePresetDialog()
{
    presetChooser = std::make_unique<juce::FileChooser> (
        "Save preset",
        PresetManager::getPresetDir().getChildFile ("My Preset.lm1preset"),
        "*.lm1preset");
    presetChooser->launchAsync (
        juce::FileBrowserComponent::saveMode
        | juce::FileBrowserComponent::canSelectFiles
        | juce::FileBrowserComponent::warnAboutOverwriting,
        [this] (const juce::FileChooser& fc)
        {
            const auto f = fc.getResult();
            if (f != juce::File())
                presetManager.save (f);
        });
}

void LMOneAudioProcessorEditor::loadPresetDialog()
{
    presetChooser = std::make_unique<juce::FileChooser> (
        "Load preset", PresetManager::getPresetDir(), "*.lm1preset");
    presetChooser->launchAsync (
        juce::FileBrowserComponent::openMode
        | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            const auto f = fc.getResult();
            if (f != juce::File())
                presetManager.load (f);   // restoreStateTree -> changeListener refreshes UI
        });
}

//==============================================================================
void LMOneAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (LMColours::faceplate);

    // Wood side cheeks.
    auto full = getLocalBounds();
    auto leftCheek  = full.removeFromLeft (kCheek);
    auto rightCheek = full.removeFromRight (kCheek);
    if (woodImage.isValid())
    {
        // Clip each draw to its cheek — fillDestination scales to cover and would
        // otherwise overflow the strip and cover the whole panel.
        {
            juce::Graphics::ScopedSaveState s (g);
            g.reduceClipRegion (leftCheek);
            g.drawImage (woodImage, leftCheek.toFloat(), juce::RectanglePlacement::fillDestination);
        }
        {
            juce::Graphics::ScopedSaveState s (g);
            g.reduceClipRegion (rightCheek);
            g.drawImage (woodImage, rightCheek.toFloat(), juce::RectanglePlacement::fillDestination);
        }
    }
    else
    {
        g.setGradientFill (juce::ColourGradient (LMColours::wood1, (float) leftCheek.getX(), 0.0f,
                                                 LMColours::wood2, (float) leftCheek.getRight(), 0.0f, false));
        g.fillRect (leftCheek);
        g.setGradientFill (juce::ColourGradient (LMColours::wood2, (float) rightCheek.getX(), 0.0f,
                                                 LMColours::wood1, (float) rightCheek.getRight(), 0.0f, false));
        g.fillRect (rightCheek);
    }

    // Title.
    g.setColour (LMColours::orange);
    g.setFont (juce::FontOptions (20.0f, juce::Font::bold));
    g.drawText ("LM-1", kCheek + 12, 8, 200, 26, juce::Justification::centredLeft);
    g.setColour (juce::Colours::grey);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("12-channel drum machine inspired by the LM-1",
                kCheek + 92, 12, getWidth() - 2 * kCheek - 100, 18, juce::Justification::centredLeft);

    // Orange section frames with labels on the top border.
    drawSection (g, rGlobals, "GLOBAL");
    drawSection (g, rMixer,   "MIXER");
    drawSection (g, rSeq,     "SEQUENCER");
}

void LMOneAudioProcessorEditor::drawSection (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title)
{
    if (r.getWidth() < 24 || r.getHeight() < 24)
        return;

    g.setColour (LMColours::orange.withAlpha (0.8f));
    g.drawRoundedRectangle (r.toFloat().reduced (3.0f), 5.0f, 1.3f);

    if (title.isNotEmpty())
    {
        g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
        const int tw = g.getCurrentFont().getStringWidth (title) + 12;
        juce::Rectangle<int> lab (r.getX() + 16, r.getY() + 2, tw, 13);
        g.setColour (LMColours::faceplate);   // break the border behind the text
        g.fillRect (lab);
        g.setColour (LMColours::orange);
        g.drawText (title, lab, juce::Justification::centred);
    }
}

void LMOneAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();
    area.removeFromLeft  (kCheek);                  // wood cheeks
    area.removeFromRight (kCheek);
    area.removeFromTop   (34);                       // title bar
    area.removeFromBottom (kBottomLip);              // wood lip / gap beneath the sequencer

    // MASTER — global knobs.
    rGlobals = area.removeFromTop (94);
    {
        auto g = rGlobals;
        g.removeFromTop (kLabelStrip);              // room for the section label
        g = g.reduced (12, 6);
        const int kW = 92;
        auto place = [&] (juce::Slider& s, juce::Label& lab)
        {
            auto cell = g.removeFromLeft (kW);
            lab.setBounds (cell.removeFromTop (16));
            s.setBounds (cell);
        };
        place (masterSlider,  masterLabel);
        place (lofiSlider,    lofiLabel);
        place (tuneSlider,    tuneLabel);
        place (shuffleSlider, shuffleLabel);
    }

    // SEQUENCER — transport controls + pattern slots + step grid, all together.
    rSeq = area.removeFromBottom (kLabelStrip + 44 + 28 + 226);
    {
        auto seq = rSeq;
        seq.removeFromTop (kLabelStrip);            // room for the section label

        // Transport controls (taller row to fit the LED-over-button transports).
        auto trb = seq.removeFromTop (44).reduced (8, 4);
        midiDrag.setBounds      (trb.removeFromRight (100));
        trb.removeFromRight (6);
        optionsButton.setBounds (trb.removeFromRight (30));
        trb.removeFromRight (12);
        stepLed.setBounds       (trb.removeFromRight (50));
        trb.removeFromRight (14);
        playButton.setBounds (trb.removeFromLeft (52));
        trb.removeFromLeft (6);
        recButton.setBounds  (trb.removeFromLeft (52));
        trb.removeFromLeft (14);
        stepsLabel.setBounds (trb.removeFromLeft (40));
        stepsBox.setBounds   (trb.removeFromLeft (52));
        trb.removeFromLeft (8);
        clearButton.setBounds (trb.removeFromLeft (52));
        trb.removeFromLeft (12);
        tempoLabel.setBounds (trb.removeFromLeft (46));
        tempoLed.setBounds (trb.removeFromRight (54).reduced (0, 2));
        trb.removeFromRight (6);
        tempoSlider.setBounds (trb.removeFromLeft (trb.getWidth() / 2));   // half-width

        // Bank nav + 8 slot buttons + save.
        auto pr = seq.removeFromTop (28).reduced (8, 3);
        bankLabel.setBounds (pr.removeFromLeft (34));
        bankPrev.setBounds  (pr.removeFromLeft (22));
        bankLed.setBounds   (pr.removeFromLeft (42).reduced (0, 1));
        bankNext.setBounds  (pr.removeFromLeft (22));
        pr.removeFromLeft (10);
        saveButton.setBounds (pr.removeFromRight (52));
        pr.removeFromRight (10);
        const int nb = slotButtons.size();
        if (nb > 0)
        {
            const int bw = juce::jmin (48, pr.getWidth() / nb);
            for (int i = 0; i < nb; ++i)
                slotButtons[i]->setBounds (pr.removeFromLeft (bw).reduced (1, 0));
        }

        // Step grid fills the rest.
        grid.setBounds (seq.reduced (8, 4));
    }

    // MIXER — voice strips fill the remaining middle.
    rMixer = area;
    {
        auto m = rMixer;
        m.removeFromTop (kLabelStrip);              // room for the section label
        auto stripsArea = m.reduced (8, 4);
        const int n = strips.size();
        if (n > 0)
        {
            const int w = stripsArea.getWidth() / n;
            for (int i = 0; i < n; ++i)
                strips[i]->setBounds (stripsArea.getX() + i * w, stripsArea.getY(),
                                      w - 2, stripsArea.getHeight());
        }
    }
}
