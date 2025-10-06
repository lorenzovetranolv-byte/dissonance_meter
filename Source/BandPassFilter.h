/*
  ==============================================================================

    BandPassFilter.h
    Created: 17 Oct 2022 11:24:38pm
    Author:  Lorenzo

  ==============================================================================
*/
#pragma once

#include <JuceHeader.h>
#include "ProcessorBase.h"

class BandPassFilter : public ProcessorBase
{
public:
  BandPassFilter()
  {
    bandPassFilter = new dsp::ProcessorDuplicator<dsp::IIR::Filter <float>, dsp::IIR::Coefficients<float>>();
  }

  void prepareToPlay(double sampleRate, int samplesPerBlock) override
  {
    //*filter.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 1000.0f);
    bandPassFilter->state = *dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, 20000.0f);
    dsp::ProcessSpec spec{ sampleRate, static_cast<juce::uint32> (samplesPerBlock),2};
    //bandPass.prepare(spec);
    bandPassFilter->prepare(spec);
  }

  void processBlock(juce::AudioSampleBuffer& buffer, juce::MidiBuffer&) override
  {
    dsp::AudioBlock<float> block(buffer);
    dsp::ProcessContextReplacing<float> context(block);
    bandPassFilter->process(context);
    //bandPass.process(context);
  }

  void updateFilter()
  {
    //*bandPassFilter.state = *juce::dsp::IIR::Coefficients<float>::makeBandPass(lastSampleRate, freq, q);
  }

  void reset() override
  {
    bandPassFilter->reset();
    //bandPass.reset();
  }

  const juce::String getName() const override { return "BandPass"; }

private:
  dsp::ProcessorDuplicator<dsp::IIR::Filter <float>, dsp::IIR::Coefficients<float>>* bandPassFilter;
  //dsp::IIR::Filter<float> bandPass;
  //dsp::IIR::Coefficients<float> bandPass;
};