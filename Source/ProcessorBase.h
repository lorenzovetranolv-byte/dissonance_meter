/*
  ==============================================================================

    ProcessorBase.h
    Created: 17 Oct 2022 11:24:19pm
    Author:  Lorenzo

  ==============================================================================
*/
#pragma once

#include <JuceHeader.h>

class ProcessorBase : public AudioProcessor
{
public:
  //==============================================================================
  ProcessorBase()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
  {}

  bool isBusesLayoutSupported (const BusesLayout& layouts) const override
  {
    auto mono   = juce::AudioChannelSet::mono();
    auto stereo = juce::AudioChannelSet::stereo();

    // Support mono/stereo symmetric
    if (! (layouts.getMainInputChannelSet() == mono || layouts.getMainInputChannelSet() == stereo))
      return false;

    if (! (layouts.getMainOutputChannelSet() == mono || layouts.getMainOutputChannelSet() == stereo))
      return false;

    return layouts.getMainInputChannelSet() == layouts.getMainOutputChannelSet();
  }

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
  const juce::String getProgramName(int) override { return {}; }
  void changeProgramName(int, const juce::String&) override {}

  //==============================================================================
  void getStateInformation(juce::MemoryBlock&) override {}
  void setStateInformation(const void*, int) override {}

private:
  //==============================================================================
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProcessorBase)
};
