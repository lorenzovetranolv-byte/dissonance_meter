/*
  ==============================================================================

    ProcessorBase.h
    Created: 17 Oct 2022 11:24:19pm
    Author:  Lorenzo

  ==============================================================================
*/
#pragma once

#include <JuceHeader.h>

class ProcessorBase : public juce::AudioProcessor
{
public:
  //==============================================================================
  ProcessorBase()
    : juce::AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo())
      .withOutput("Output", juce::AudioChannelSet::stereo()))
  {}

  //==============================================================================
  void prepareToPlay(double, int) override {}
  void releaseResources() override {}
  void processBlock(juce::AudioSampleBuffer&, juce::MidiBuffer&) override {}

  //==============================================================================
  juce::AudioProcessorEditor* createEditor() override { return nullptr; }
  bool hasEditor() const override { return false; }

  //==============================================================================
  const juce::String getName() const override { return {}; }
  bool acceptsMidi() const override { return false; }
  bool producesMidi() const override { return false; }
  double getTailLengthSeconds() const override { return 0; }

  //==============================================================================
  int getNumPrograms() override { return 0; }
  int getCurrentProgram() override { return 0; }
  void setCurrentProgram(int) override {}
};
