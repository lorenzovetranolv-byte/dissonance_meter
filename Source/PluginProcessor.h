/*
  ==============================================================================
    PluginProcessor.h
    Ordered & cleaned: core JUCE overrides kept; custom processors (BandPass, Distortion)
    chained for: Input -> BandPass -> Distortion -> Visualiser (waveForm)
  ==============================================================================
*/
#pragma once

#include <JuceHeader.h>
#include <vector>

//==============================================================================
// Base class for simple internal effect processors used inside the graph
class ProcessorBase : public juce::AudioProcessor
{
public:
  ProcessorBase() : juce::AudioProcessor (BusesProperties()
                    .withInput ("Input",  juce::AudioChannelSet::stereo())
                    .withOutput("Output", juce::AudioChannelSet::stereo())) {}

  void prepareToPlay (double, int) override {}
  void releaseResources() override {}
  void processBlock (juce::AudioSampleBuffer&, juce::MidiBuffer&) override {}
  juce::AudioProcessorEditor* createEditor() override { return nullptr; }
  bool hasEditor() const override { return false; }

  const juce::String getName() const override { return {}; }
  bool acceptsMidi() const override { return false; }
  bool producesMidi() const override { return false; }
  double getTailLengthSeconds() const override { return 0.0; }
  int getNumPrograms() override { return 1; }
  int getCurrentProgram() override { return 0; }
  void setCurrentProgram (int) override {}
  const juce::String getProgramName (int) override { return {}; }
  void changeProgramName (int, const juce::String&) override {}
  void getStateInformation (juce::MemoryBlock&) override {}
  void setStateInformation (const void*, int) override {}

private:
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProcessorBase)
};

//==============================================================================
// BandPass Filter node with smoothed frequency & Q parameters
class BandPassFilter : public ProcessorBase
{
public:
  BandPassFilter() : treeState (*this, nullptr, "BP_PARAMS", createLayout()) {}

  void prepareToPlay (double sampleRate, int samplesPerBlock) override
  {
    currentSampleRate = static_cast<float>(sampleRate);
    juce::dsp::ProcessSpec spec { sampleRate, static_cast<juce::uint32> (samplesPerBlock), 2 };
    frequency.reset (sampleRate, 0.02); // 20ms smoothing
    q.reset         (sampleRate, 0.02);
    updateCoefficients();
    filter.prepare (spec);
  }

  void processBlock (juce::AudioSampleBuffer& buffer, juce::MidiBuffer&) override
  {
    frequency.setTargetValue (*treeState.getRawParameterValue ("FREQUENCY"));
    q.setTargetValue         (*treeState.getRawParameterValue ("Q"));
    updateCoefficients();

    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> ctx (block);
    filter.process (ctx);
  }

  void reset() override { filter.reset(); }
  const juce::String getName() const override { return "BandPass"; }

  juce::AudioProcessorValueTreeState treeState;
  juce::LinearSmoothedValue<float> frequency { 150.0f };
  juce::LinearSmoothedValue<float> q         { 2.5f  };

private:
  juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
  {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    params.push_back (std::make_unique<juce::AudioParameterFloat>("FREQUENCY", "Frequency", juce::NormalisableRange<float>(50.0f, 10000.0f, 0.0f, 0.25f), 150.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>("Q",         "Q",         juce::NormalisableRange<float>(0.1f, 10.0f,   0.0f, 0.4f),   2.5f));
    return { params.begin(), params.end() };
  }

  void updateCoefficients()
  {
    *filter.state = *juce::dsp::IIR::Coefficients<float>::makeBandPass (currentSampleRate,
                      juce::jlimit (50.0f, 10000.0f, frequency.getNextValue()),
                      juce::jlimit (0.1f,  10.0f,    q.getNextValue()));
  }

  float currentSampleRate = 44100.0f;
  juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> filter;
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BandPassFilter)
};

//==============================================================================
// Distortion node implementing ODE-based nonlinear system:
// f(t) = x'' + 60 x' + 900 x + A x^2  ->  x'' = f(t) - 60 x' - 900 x - A x^2
// Integration (forward Euler):
// x'(t) = x'(t-1) + x''(t-1) * dt
// x(t)  = x(t-1)  + x'(t-1) * dt
// Output: x(t)
class Distortion : public ProcessorBase
{
public:
  Distortion() : treeState (*this, nullptr, "DIST_PARAMS", createLayout()) {}

  void prepareToPlay (double sampleRate, int samplesPerBlock) override
  {
    currentSampleRate = static_cast<float>(sampleRate);
    drive.reset (sampleRate, 0.02); // smoothing 20 ms
    (void) samplesPerBlock;

    auto channels = getTotalNumOutputChannels();
    xState.assign (channels, 0.0f);
    xPrimeState.assign (channels, 0.0f);
    xPrimePrev.assign (channels, 0.0f);
    xDoublePrev.assign (channels, 0.0f);
  }

  void processBlock (juce::AudioSampleBuffer& buffer, juce::MidiBuffer&) override
  {
    drive.setTargetValue (*treeState.getRawParameterValue ("A"));
    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();
    const float dt = currentSampleRate > 0.0f ? (1.0f / currentSampleRate) : 0.0f;

    // Ensure state size if channel count changed dynamically
    if ((int)xState.size() < numChannels)
    {
      xState.resize (numChannels, 0.0f);
      xPrimeState.resize (numChannels, 0.0f);
      xPrimePrev.resize (numChannels, 0.0f);
      xDoublePrev.resize (numChannels, 0.0f);
    }

    for (int ch = 0; ch < numChannels; ++ch)
    {
      auto* data = buffer.getWritePointer (ch);
      float& x    = xState[ch];
      float& dx   = xPrimeState[ch];
      float& dxPrev = xPrimePrev[ch];
      float& ddxPrev= xDoublePrev[ch];

      for (int i = 0; i < numSamples; ++i)
      {
        float in  = data[i];              // f(t) driving term from input audio
        float A    = drive.getNextValue();

        // Compute current second derivative from previous state (explicit Euler)
        float ddx = in - 60.0f * dx - 900.0f * x - A * (x * x);

        // Integrate using previous derivatives
        float dxNew = dx + ddx * dt;
        float xNew  = x  + dx * dt;

        // Optional simple limiting to avoid runaway (safety)
        xNew  = juce::jlimit (-5.0f, 5.0f, xNew);
        dxNew = juce::jlimit (-500.0f, 500.0f, dxNew);

        // Write output sample
        data[i] = xNew;

        // Update state for next sample
        x = xNew;
        dx = dxNew;
        dxPrev = dxNew;        // (Kept if future higher-order methods added)
        ddxPrev = ddx;         // store last acceleration
      }
    }
  }

  void reset() override
  {
    std::fill (xState.begin(), xState.end(), 0.0f);
    std::fill (xPrimeState.begin(), xPrimeState.end(), 0.0f);
    std::fill (xPrimePrev.begin(), xPrimePrev.end(), 0.0f);
    std::fill (xDoublePrev.begin(), xDoublePrev.end(), 0.0f);
    drive.reset (currentSampleRate, 0.02);
  }

  const juce::String getName() const override { return "Distortion"; }

  juce::AudioProcessorValueTreeState treeState;
  juce::LinearSmoothedValue<float> drive { 1.0f }; // Parameter A

private:
  juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
  {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    params.push_back (std::make_unique<juce::AudioParameterFloat>("A", "A", juce::NormalisableRange<float>(1.0f, 15.0f, 0.0f, 0.4f), 1.0f));
    return { params.begin(), params.end() };
  }

  float currentSampleRate = 44100.0f;
  std::vector<float> xState;       // x(t)
  std::vector<float> xPrimeState;  // x'(t)
  std::vector<float> xPrimePrev;   // stored previous derivative (placeholder for future schemes)
  std::vector<float> xDoublePrev;  // stored previous second derivative (if needed)

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Distortion)
};

//==============================================================================
class DissonanceMeeter : public juce::AudioProcessor
{
public:
  using AudioGraphIOProcessor = juce::AudioProcessorGraph::AudioGraphIOProcessor;
  using Node                 = juce::AudioProcessorGraph::Node;

  DissonanceMeeter();
  ~DissonanceMeeter() override;

  const juce::String getName() const override;
  bool acceptsMidi() const override;
  bool producesMidi() const override;
  bool isMidiEffect() const override;
  double getTailLengthSeconds() const override;

  int getNumPrograms() override;        
  int getCurrentProgram() override;     
  void setCurrentProgram (int) override; 
  const juce::String getProgramName (int) override; 
  void changeProgramName (int, const juce::String&) override; 

  void prepareToPlay (double sampleRate, int samplesPerBlock) override;
  void releaseResources() override;
  bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
  void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

  bool hasEditor() const override;
  juce::AudioProcessorEditor* createEditor() override;

  void getStateInformation (juce::MemoryBlock&) override;
  void setStateInformation (const void*, int) override;

  juce::AudioVisualiserComponent waveForm { 1 };

private:
  void initialiseGraph();
  void connectAudioNodes();

  double lastSampleRate   = 44100.0;
  int    lastBlockSize    = 512;
  int    numInputChannels = 2;
  int    numOutputChannels= 2;

  std::unique_ptr<juce::AudioProcessorGraph> mainProcessor;
  juce::AudioProcessorGraph::Node::Ptr audioInputNode;
  juce::AudioProcessorGraph::Node::Ptr bandPassNode;
  juce::AudioProcessorGraph::Node::Ptr distortionNode;
  juce::AudioProcessorGraph::Node::Ptr audioOutputNode;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DissonanceMeeter)
};