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
  minFrequency.setBounds(10, 20, 120, 120);
  minFrequency.setRotaryParameters(juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
  minFrequency.setTextBoxStyle(Slider::TextBoxBelow, true, 80, 20);
  // Frequency slider: limit to audible/usable range and use logarithmic skew
  // Band-pass frequency range aligned to processor: 16..200
  minFrequency.setRange (16.0, 200.0, 1.0);
  minFrequency.setSkewFactorFromMidPoint (50.0);

  maxFrequency.setSliderStyle(Slider::SliderStyle::RotaryHorizontalVerticalDrag);
  maxFrequency.setBounds(140, 20, 120, 120);
  maxFrequency.setRotaryParameters(juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
  maxFrequency.setTextBoxStyle(Slider::TextBoxBelow, true, 80, 20);
  // Q slider (attached here): reasonable bounds to avoid division by zero / unstable filter
  // Q range 1..5
  maxFrequency.setRange (1.0, 5.0, 0.01);
  
  AValue.setSliderStyle(Slider::SliderStyle::RotaryHorizontalVerticalDrag);
  AValue.setBounds(270, 20, 120, 120);
  AValue.setRotaryParameters(juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
  AValue.setTextBoxStyle(Slider::TextBoxBelow, true, 80, 20);
  // Distortion drive A: keep within stable limits
  // Nonlinearity A range 0.2..1.2
  AValue.setRange (0.2, 1.2, 0.01);

  // Oscillator frequency sliders
  oscFreq1Slider.setSliderStyle(Slider::SliderStyle::LinearHorizontal);
  oscFreq1Slider.setBounds(10, 150, 460, 28);
  oscFreq1Slider.setTextBoxStyle(Slider::TextBoxRight, true, 80, 20);
  oscFreq1Slider.setRange (20.0, 20000.0, 1.0);
  oscFreq1Slider.setSkewFactorFromMidPoint (440.0);

  oscFreq2Slider.setSliderStyle(Slider::SliderStyle::LinearHorizontal);
  oscFreq2Slider.setBounds(10, 190, 460, 28);
  oscFreq2Slider.setTextBoxStyle(Slider::TextBoxRight, true, 80, 20);
  oscFreq2Slider.setRange (20.0, 20000.0, 1.0);
  oscFreq2Slider.setSkewFactorFromMidPoint (440.0);

  masterGainSlider.setSliderStyle(Slider::SliderStyle::LinearHorizontal);
  // place master gain in the top-right area under the mode selector
  masterGainSlider.setBounds(400, 90, 300, 24);
  masterGainSlider.setTextBoxStyle(Slider::TextBoxRight, true, 80, 20);
  masterGainSlider.setRange(0.0, 4.0, 0.01);
  masterGainSlider.setValue (audioProcessor.getOutputGain(), juce::dontSendNotification);
  masterGainSlider.onValueChange = [this]() { audioProcessor.setOutputGain ((float) masterGainSlider.getValue()); };

  masterGainLabel.setText("Master Gain", juce::dontSendNotification);
  masterGainLabel.setColour (juce::Label::textColourId, juce::Colours::white);
  masterGainLabel.setJustificationType (juce::Justification::centredLeft);

  audioProcessor.waveForm.setBounds(10, 220, 360, 160);
  audioProcessor.waveForm.setColours(Colours::black, Colours::white);

  // Mode selector: External input vs Oscillator
  modeSelector.addItem ("External Input", 1);
  modeSelector.addItem ("Internal Oscillator", 2);
  modeSelector.setSelectedId (1);
  modeSelector.onChange = [this]() {
    auto m = modeSelector.getSelectedId() == 1 ? DissonanceMeeter::InputMode::ExternalInput : DissonanceMeeter::InputMode::Oscillator;
    audioProcessor.setInputMode (m);
  };

  // Channel selector removed from UI (kept in processor if needed)

  addAndMakeVisible (&modeSelector);
  addAndMakeVisible(&minFrequency);
  addAndMakeVisible(&maxFrequency);
  addAndMakeVisible(&AValue);
  addAndMakeVisible(&oscFreq1Slider);
  addAndMakeVisible(&oscFreq2Slider);
  addAndMakeVisible(&oscFreq1Minus);
  addAndMakeVisible(&oscFreq1Plus);
  addAndMakeVisible(&oscFreq2Minus);
  addAndMakeVisible(&oscFreq2Plus);
  addAndMakeVisible(&masterGainSlider);
  addAndMakeVisible(&masterGainLabel);
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
  modeSelector.setBounds (400, 20, 180, 28);
  // Update oscillator frequencies when sliders change
  minFrequency.onValueChange = [this]() {
    if (audioProcessor.getInputMode() == DissonanceMeeter::InputMode::Oscillator)
      audioProcessor.setOscillatorFrequencies ((float) minFrequency.getValue(), audioProcessor.getOscillatorFrequencies().second);
  };
  maxFrequency.onValueChange = [this]() {
    if (audioProcessor.getInputMode() == DissonanceMeeter::InputMode::Oscillator)
      audioProcessor.setOscillatorFrequencies (audioProcessor.getOscillatorFrequencies().first, (float) maxFrequency.getValue());
  };

  // Oscillator sliders callbacks
  auto freqs = audioProcessor.getOscillatorFrequencies();
  // default oscillator values often used in tests: 440 and 470 or as provided by processor
  if (freqs.first <= 0.0f && freqs.second <= 0.0f)
  {
    oscFreq1Slider.setValue (440.0, juce::dontSendNotification);
    oscFreq2Slider.setValue (470.0, juce::dontSendNotification);
  }
  else
  {
    oscFreq1Slider.setValue (freqs.first, juce::dontSendNotification);
    oscFreq2Slider.setValue (freqs.second, juce::dontSendNotification);
  }

  oscFreq1Slider.onValueChange = [this]() {
    if (audioProcessor.getInputMode() == DissonanceMeeter::InputMode::Oscillator)
      audioProcessor.setOscillatorFrequencies ((float) oscFreq1Slider.getValue(), audioProcessor.getOscillatorFrequencies().second);
  };
  oscFreq2Slider.onValueChange = [this]() {
    if (audioProcessor.getInputMode() == DissonanceMeeter::InputMode::Oscillator)
      audioProcessor.setOscillatorFrequencies (audioProcessor.getOscillatorFrequencies().first, (float) oscFreq2Slider.getValue());
  };

  // Position +/- buttons next to numeric text boxes
  oscFreq1Minus.setBounds (480, 150, 24, 24);
  oscFreq1Plus.setBounds  (508, 150, 24, 24);
  oscFreq2Minus.setBounds (480, 190, 24, 24);
  oscFreq2Plus.setBounds  (508, 190, 24, 24);

  masterGainLabel.setBounds (400, 60, 300, 24);

  // +/- buttons behaviour
  oscFreq1Minus.onClick = [this]() {
    oscFreq1Slider.setValue (oscFreq1Slider.getValue() - 1.0, juce::sendNotification);
  };
  oscFreq1Plus.onClick = [this]() {
    oscFreq1Slider.setValue (oscFreq1Slider.getValue() + 1.0, juce::sendNotification);
  };
  oscFreq2Minus.onClick = [this]() {
    oscFreq2Slider.setValue (oscFreq2Slider.getValue() - 1.0, juce::sendNotification);
  };
  oscFreq2Plus.onClick = [this]() {
    oscFreq2Slider.setValue (oscFreq2Slider.getValue() + 1.0, juce::sendNotification);
  };

}
