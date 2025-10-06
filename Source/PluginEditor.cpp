/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
DissonanceMeeterAudioProcessorEditor::DissonanceMeeterAudioProcessorEditor (DissonanceMeeter& p,BandPassFilter& b,Distortion& d)
    : AudioProcessorEditor (&p), audioProcessor (p), bandPassProcessor(b), distortionProcessor(d)
{
    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
  sliderAttachment1 = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(bandPassProcessor.treeState, "FREQUENCY", minFrequency);
  sliderAttachment2 = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(bandPassProcessor.treeState, "Q", maxFrequency);
  sliderAttachment3 = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(distortionProcessor.treeState, "A", AValue);
  
  minFrequency.setSliderStyle(Slider::SliderStyle::RotaryHorizontalVerticalDrag);
  minFrequency.setBounds(10, 20, 100, 100);
  minFrequency.setRotaryParameters(juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
  minFrequency.setTextBoxStyle(Slider::TextBoxBelow, true, 80, 20);

  maxFrequency.setSliderStyle(Slider::SliderStyle::RotaryHorizontalVerticalDrag);
  maxFrequency.setBounds(110, 20, 100, 100);
  maxFrequency.setRotaryParameters(juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
  maxFrequency.setTextBoxStyle(Slider::TextBoxBelow, true, 80, 20);
  
  AValue.setSliderStyle(Slider::SliderStyle::RotaryHorizontalVerticalDrag);
  AValue.setBounds(210, 20, 100, 100);
  AValue.setRotaryParameters(juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
  AValue.setTextBoxStyle(Slider::TextBoxBelow, true, 80, 20);

  audioProcessor.waveForm.setBounds(10, 150, 150, 150);
  audioProcessor.waveForm.setColours(Colours::black, Colours::white);

  addAndMakeVisible(&minFrequency);
  addAndMakeVisible(&maxFrequency);
  addAndMakeVisible(&AValue);
  addAndMakeVisible(audioProcessor.waveForm);

  setSize(600, 400);
}

DissonanceMeeterAudioProcessorEditor::~DissonanceMeeterAudioProcessorEditor()
{
}

//==============================================================================
void DissonanceMeeterAudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
  g.setColour(juce::Colours::black);
  g.setFont(15.0f);
  g.setGradientFill(juce::ColourGradient{ juce::Colours::darkgrey, getLocalBounds().toFloat().getCentre(), juce::Colours::darkgrey.darker(0.7f), {}, true });
  g.fillRect(getLocalBounds());
   
}

void DissonanceMeeterAudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
}
