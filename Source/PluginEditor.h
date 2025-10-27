/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
/**
*/
class DissonanceMeeterAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
  //riferimento a oggetto bandpass, costruttore di base senza il filtro
    DissonanceMeeterAudioProcessorEditor (DissonanceMeeter&, BandPassFilter&, Distortion&);
    //DissonanceMeeterAudioProcessorEditor(ProcessorBase&);
    ~DissonanceMeeterAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    Slider minFrequency;
    Slider maxFrequency;
    Slider AValue;
    Slider oscFreq1Slider;
    Slider oscFreq2Slider;
    TextButton oscFreq1Minus {"-"};
    TextButton oscFreq1Plus  {"+"};
    TextButton oscFreq2Minus {"-"};
    TextButton oscFreq2Plus  {"+"};
    Slider masterGainSlider;
    juce::Label masterGainLabel;
    // Labels for sliders
    juce::Label freqLabel;
    juce::Label qLabel;
    juce::Label aLabel;
    juce::Label osc1Label;
    juce::Label osc2Label;
    ComboBox modeSelector;
    DissonanceMeeter& audioProcessor;
    BandPassFilter& bandPassProcessor;
    Distortion& distortionProcessor;

public:
  std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> sliderAttachment1;
  std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> sliderAttachment2;
  std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> sliderAttachment3;
  std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> masterGainAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DissonanceMeeterAudioProcessorEditor)
};
