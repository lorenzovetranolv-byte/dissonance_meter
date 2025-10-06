/*
  ==============================================================================

    Distortion.h
    Created: 17 Oct 2022 11:24:53pm
    Author:  Lorenzo

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "ProcessorBase.h"

class Distortion : public ProcessorBase
{
public:
  Distortion()
  {
    distortion.functionToUse = [](float x)
    {
      return juce::jlimit(float(-0.1), float(0.1), x); // [6]
    };
  }

  void prepareToPlay(double sampleRate, int samplesPerBlock) override
  {
   
    dsp::ProcessSpec spec{ sampleRate, static_cast<juce::uint32> (samplesPerBlock),2};
    distortion.prepare(spec);
  }

  void processBlock(juce::AudioSampleBuffer& buffer, juce::MidiBuffer&) override
  {
    dsp::AudioBlock<float> block(buffer);
    dsp::ProcessContextReplacing<float> context(block);
    distortion.process(context);
  }

  void updateFilter()
  {
    //distortion.functionToUse = [](float x)
    //{
    //  return juce::jlimit(float(-0.1), float(0.1), x); // [6]
    //};
//OVERRIDE DELLA FUNZIONE
     //auto paramCasted = static_cast<Type> (myParam);
  }

  void reset() override
  {
    distortion.reset();
  }

  const juce::String getName() const override { return "Distortion"; }

private:
  dsp::WaveShaper<float> distortion;
};
