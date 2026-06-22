#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "VoiceStripComponent.h"
#include "StepGridComponent.h"
#include "MidiDragSource.h"
#include "PresetManager.h"
#include "LMOneLookAndFeel.h"
#include "LedDisplay.h"
#include "TransportButton.h"

//==============================================================================
// Editor: a 12-channel voice mixer (one strip per LM-1 instrument) plus the
// global Master / Lo-Fi / Tune knobs. The step-grid sequencer UI is the next
// roadmap milestone.
//==============================================================================
class LMOneAudioProcessorEditor : public juce::AudioProcessorEditor,
                                  public juce::ChangeListener,
                                  public juce::Timer
{
public:
    explicit LMOneAudioProcessorEditor (LMOneAudioProcessor&);
    ~LMOneAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void exportMidiToFile();      // invoked from the options menu
    void savePresetDialog();
    void loadPresetDialog();
    void refreshBankUI();         // reflect the current bank + slot states
    void gotoBank (int newBank);  // change bank + auto-load the selected slot's groove

    LMOneAudioProcessor& processor;
    LMOneLookAndFeel     lookAndFeel;
    PresetManager        presetManager { processor };

    juce::OwnedArray<VoiceStripComponent> strips;

    StepGridComponent grid;
    MidiDragSource    midiDrag;

    // Preset library: bank nav + 8 slot buttons + save.
    juce::OwnedArray<juce::TextButton> slotButtons;
    juce::Label      bankLabel;
    juce::ArrowButton bankPrev { "bankPrev", 0.5f, LMColours::orange };  // points left  (prev bank)
    juce::ArrowButton bankNext { "bankNext", 0.0f, LMColours::orange };  // points right (next bank)
    juce::TextButton  saveButton { "Save" };
    std::unique_ptr<juce::FileChooser> presetChooser;

    // Transport bar.
    TransportButton  playButton { "PLAY", juce::Colour (0xff37d067) };  // green = playing
    TransportButton  recButton  { "REC",  juce::Colour (0xffe53935) };  // red   = recording
    juce::Slider     tempoSlider;
    juce::Label      tempoLabel;
    LedDisplay       stepLed { 2 }, tempoLed { 3 }, bankLed { 3 };
    juce::ComboBox   stepsBox;
    juce::Label      stepsLabel;
    juce::TextButton clearButton   { "Clear" };
    juce::TextButton optionsButton;            // gear: holds Export MIDI + future options
    std::unique_ptr<juce::FileChooser> midiChooser;
    std::unique_ptr<SliderAttachment> tempoAttach;

    juce::Slider masterSlider, lofiSlider, tuneSlider, shuffleSlider;
    juce::Label  masterLabel,  lofiLabel,  tuneLabel,  shuffleLabel;

    std::unique_ptr<SliderAttachment> masterAttach, lofiAttach, tuneAttach, shuffleAttach;

    // Styling: wood side cheeks + orange section frames.
    static constexpr int kCheek = 52;       // thicker wood cheeks
    static constexpr int kBottomLip = 12;   // wood lip / breathing room below the sequencer
    static constexpr int kLabelStrip = 14;
    juce::Image          woodImage;
    juce::Rectangle<int> rGlobals, rMixer, rSeq;
    static void drawSection (juce::Graphics&, juce::Rectangle<int>, const juce::String& title);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LMOneAudioProcessorEditor)
};
