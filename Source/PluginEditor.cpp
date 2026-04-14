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
  minFrequency.setBounds(10,20,120,120);
  minFrequency.setRotaryParameters(juce::MathConstants<float>::pi *1.2f, juce::MathConstants<float>::pi *2.8f, true);
  minFrequency.setTextBoxStyle(Slider::TextBoxBelow, true,80,20);
  // Frequency slider: wider usable range and logarithmic skew
  minFrequency.setRange (20.0,20000.0,1.0);
  minFrequency.setSkewFactorFromMidPoint (1000.0);

  maxFrequency.setSliderStyle(Slider::SliderStyle::RotaryHorizontalVerticalDrag);
  maxFrequency.setBounds(140,20,120,120);
  maxFrequency.setRotaryParameters(juce::MathConstants<float>::pi *1.2f, juce::MathConstants<float>::pi *2.8f, true);
  maxFrequency.setTextBoxStyle(Slider::TextBoxBelow, true,80,20);
  // Q slider: extend range for narrower/wider bandwidths
  maxFrequency.setRange (0.2,15.0,0.01);
  
  AValue.setSliderStyle(Slider::SliderStyle::RotaryHorizontalVerticalDrag);
  AValue.setBounds(270,20,120,120);
  AValue.setRotaryParameters(juce::MathConstants<float>::pi *1.2f, juce::MathConstants<float>::pi *2.8f, true);
  AValue.setTextBoxStyle(Slider::TextBoxBelow, true,80,20);
  // Distortion drive A: increased headroom for stronger effect
  AValue.setRange (0.1,5.0,0.01);

  masterGainSlider.setSliderStyle(Slider::SliderStyle::LinearHorizontal);
  // place master gain in the right panel under the mode selector
  masterGainSlider.setTextBoxStyle(Slider::TextBoxRight, true,80,20);
  masterGainSlider.setRange(0.0,4.0,0.01);
  masterGainSlider.setValue (audioProcessor.getOutputGain(), juce::dontSendNotification);
  masterGainSlider.onValueChange = [this]() { audioProcessor.setOutputGain ((float) masterGainSlider.getValue()); };

  // Labels setup
  auto setupLabel = [] (juce::Label& l, const juce::String& text) {
    l.setText (text, juce::dontSendNotification);
    l.setColour (juce::Label::textColourId, juce::Colours::white);
    l.setJustificationType (juce::Justification::centred);
    l.setInterceptsMouseClicks (false, false);
  };
  setupLabel (freqLabel, "Frequenza");
  setupLabel (qLabel, "Q");
  setupLabel (aLabel, "A (Distorsione)");

  audioProcessor.waveForm.setColours(Colours::black, Colours::white);

  addAndMakeVisible(&minFrequency);
  addAndMakeVisible(&maxFrequency);
  addAndMakeVisible(&AValue);
  addAndMakeVisible(&masterGainSlider);
  addAndMakeVisible(&masterGainLabel);
  addAndMakeVisible(freqLabel);
  addAndMakeVisible(qLabel);
  addAndMakeVisible(aLabel);
  addAndMakeVisible(audioProcessor.waveForm);

  // Master gain label
  masterGainLabel.setText("Master Gain", juce::dontSendNotification);
  masterGainLabel.setColour (juce::Label::textColourId, juce::Colours::white);
  masterGainLabel.setJustificationType (juce::Justification::centredLeft);

  setSize(640,420);
  setOpaque (true);
  startTimerHz (30);
}

DissonanceMeeterAudioProcessorEditor::~DissonanceMeeterAudioProcessorEditor()
{
}

//==============================================================================
void DissonanceMeeterAudioProcessorEditor::paint (juce::Graphics& g)
{
  g.fillAll (juce::Colours::black);

  // Background
  g.setGradientFill (juce::ColourGradient { juce::Colours::darkgrey, getLocalBounds().toFloat().getCentre(), juce::Colours::darkgrey.darker (0.7f), {}, true });
  g.fillRect (getLocalBounds());

  // Compute meter geometry: left side, full height with bottom margin
  const int padding =16;
  meterX = padding;
  meterY = padding;
  meterH = getHeight() - padding * 3; // quasi tutta l'altezza
  meterW =18; // slightly wider

  // Draw meter background
  juce::Rectangle<int> meterBounds { meterX, meterY, meterW, meterH };
  g.setColour (juce::Colours::darkgrey.withAlpha (0.8f));
  g.fillRect (meterBounds);
  g.setColour (juce::Colours::grey);
  g.drawRect (meterBounds,1);

  // Get current level and map to fill height
  const float db = juce::jlimit (meterMinDb, meterMaxDb, audioProcessor.getOutputLevelRms());
  const float norm = (db - meterMinDb) / (meterMaxDb - meterMinDb); //0..1
  const int fillH = (int) std::round (norm * (float) meterH);
  juce::Rectangle<int> fillBounds { meterX +1, meterY + meterH - fillH +1, meterW -2, fillH -2 };

  // Gradient from green (bottom) -> yellow (mid) -> red (top)
  auto colBottom = juce::Colours::green;
  auto colMid = juce::Colours::yellow;
  auto colTop = juce::Colours::red;
  juce::ColourGradient grad (colBottom, fillBounds.getBottomLeft().toFloat(), colTop, fillBounds.getTopLeft().toFloat(), false);
  grad.addColour (0.5, colMid);
  g.setGradientFill (grad);
  g.fillRect (fillBounds);
}

void DissonanceMeeterAudioProcessorEditor::resized()
{
 // Layout constants
 const int padding =16;
 const int leftMeterW =24; // fixed meter column width
 const int contentLeft = padding + leftMeterW + padding; // start of content area
 const int labelH =18;
 const int knobSizeDefault =120;
 const int rightPanelW =240; // fixed right panel width
 const int rightPanelX = getWidth() - padding - rightPanelW;

 // Compute dynamic knob width to avoid overlap with right panel
 const int availableLeftWidth = juce::jmax(0, rightPanelX - contentLeft);
 const int computedKnobW = (availableLeftWidth -2 * padding) /3; //3 knobs,2 gaps
 const int knobW = juce::jlimit(90, knobSizeDefault, computedKnobW);

 // Row1: three rotary knobs: Freq, Q, A
 const int row1Y = padding +28;
 minFrequency.setBounds(contentLeft, row1Y, knobW, knobW);
 maxFrequency.setBounds(contentLeft + knobW + padding, row1Y, knobW, knobW);
 AValue.setBounds(contentLeft +2 * (knobW + padding), row1Y, knobW, knobW);

 // Labels above the knobs
 freqLabel.setBounds(minFrequency.getX(), row1Y - labelH -6, knobW, labelH);
 qLabel.setBounds(maxFrequency.getX(), row1Y - labelH -6, knobW, labelH);
 aLabel.setBounds(AValue.getX(), row1Y - labelH -6, knobW, labelH);

 // Mode selector and master gain on right
 masterGainLabel.setBounds(rightPanelX, row1Y +20, rightPanelW,20);
 masterGainSlider.setBounds(rightPanelX, row1Y +42, rightPanelW,22);
 // Waveform view bottom
 const int waveTop = row1Y + knobW + 2 * padding;
 audioProcessor.waveForm.setBounds(contentLeft, waveTop, getWidth() - contentLeft - padding, getHeight() - waveTop - padding);

 // Meter geometry (left column, full height with bottom margin)
 meterX = padding;
 meterY = padding;
 meterW = leftMeterW;
 meterH = getHeight() - padding * 3;

}

void DissonanceMeeterAudioProcessorEditor::timerCallback()
{
 repaint();
}
