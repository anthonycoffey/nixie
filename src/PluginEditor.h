#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "VoiceStripComponent.h"
#include "StepGridComponent.h"
#include "MidiDragSource.h"
#include "PresetManager.h"
#include "LMOneLookAndFeel.h"
#include "LedDisplay.h"

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
    void updatePatternButtons();  // reflect the active pattern slot

    LMOneAudioProcessor& processor;
    LMOneLookAndFeel     lookAndFeel;
    PresetManager        presetManager { processor };

    juce::OwnedArray<VoiceStripComponent> strips;

    StepGridComponent grid;
    MidiDragSource    midiDrag;

    // Pattern bank selector.
    juce::OwnedArray<juce::TextButton> patternButtons;
    juce::Label patternLabel;
    std::unique_ptr<juce::FileChooser> presetChooser;

    // Transport bar.
    juce::TextButton playButton { "Play" };
    juce::Slider     tempoSlider;
    juce::Label      tempoLabel;
    LedDisplay       stepLed { 2 }, tempoLed { 3 }, patternLed { 1 };
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
    static constexpr int kCheek = 26;
    static constexpr int kLabelStrip = 14;
    juce::Image          woodImage;
    juce::Rectangle<int> rGlobals, rMixer, rSeq;
    static void drawSection (juce::Graphics&, juce::Rectangle<int>, const juce::String& title);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LMOneAudioProcessorEditor)
};
