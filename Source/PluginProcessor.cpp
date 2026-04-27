/*
  ==============================================================================

    This file contains the basic framework code for a JUce plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <cstring>

//==============================================================================
DissonanceMeeter::DissonanceMeeter()
#ifndef JucePlugin_PreferredChannelConfigurations
  : AudioProcessor (BusesProperties()
#if ! JucePlugin_IsMidiEffect
 #if ! JucePlugin_IsSynth
      .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
 #endif
      .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
#endif
    )
#endif
{
  mainProcessor = std::make_unique<juce::AudioProcessorGraph>();
  waveForm.setRepaintRate (30);
  waveForm.setBufferSize (512);
  waveForm.setSamplesPerBlock (256);
  waveForm.setColours (juce::Colours::black, juce::Colours::lime);

  // Build the processing graph once, and keep nodes stable for the editor
  initialiseGraph();
}

DissonanceMeeter::~DissonanceMeeter()
{
  if (mainProcessor != nullptr)
    mainProcessor->releaseResources();
}

//==============================================================================
const juce::String DissonanceMeeter::getName() const { return JucePlugin_Name; }
bool DissonanceMeeter::acceptsMidi() const { return false; }
bool DissonanceMeeter::producesMidi() const { return false; }
bool DissonanceMeeter::isMidiEffect() const { return false; }
double DissonanceMeeter::getTailLengthSeconds() const { return 0.0; }

int DissonanceMeeter::getNumPrograms() { return 1; }
int DissonanceMeeter::getCurrentProgram() { return 0; }
void DissonanceMeeter::setCurrentProgram (int) {}
const juce::String DissonanceMeeter::getProgramName (int) { return {}; }
void DissonanceMeeter::changeProgramName (int, const juce::String&) {}

//==============================================================================
void DissonanceMeeter::prepareToPlay (double sampleRate, int samplesPerBlock)
{
  lastSampleRate = sampleRate;
  lastBlockSize = samplesPerBlock;
  numInputChannels = getMainBusNumInputChannels();
  numOutputChannels = getMainBusNumOutputChannels();

  // Configure the outer graph and its child nodes with current runtime details
  mainProcessor->setPlayConfigDetails (numInputChannels, numOutputChannels, sampleRate, samplesPerBlock);

  // Rebuild connections to reflect actual channel count
  mainProcessor->clear();
  audioInputNode  = nullptr;
  bandPassNode    = nullptr;
  distortionNode  = nullptr;
  audioOutputNode = nullptr;
  initialiseGraph();

  // Play config is already set correctly per-node inside connectAudioNodes()

  // Prepare the graph (this will prepare child nodes too)
  mainProcessor->prepareToPlay (sampleRate, samplesPerBlock);

  initialiseOscillator (sampleRate);
}

void DissonanceMeeter::initialiseOscillator (double sampleRate) noexcept
{
  (void) sampleRate;
  oscPhase1 = 0.0;
  oscPhase2 = 0.0;
}

void DissonanceMeeter::initialiseGraph()
{
  // Build nodes only once to keep stable references for the editor
  if (mainProcessor == nullptr)
    mainProcessor = std::make_unique<juce::AudioProcessorGraph>();

  if (audioInputNode == nullptr)
    audioInputNode = mainProcessor->addNode (std::make_unique<AudioGraphIOProcessor>(AudioGraphIOProcessor::audioInputNode));
  if (bandPassNode == nullptr)
    bandPassNode = mainProcessor->addNode (std::make_unique<BandPassFilter>());
  if (distortionNode == nullptr)
    distortionNode = mainProcessor->addNode (std::make_unique<Distortion>());
  if (audioOutputNode == nullptr)
    audioOutputNode = mainProcessor->addNode (std::make_unique<AudioGraphIOProcessor>(AudioGraphIOProcessor::audioOutputNode));

  connectAudioNodes();
}

void DissonanceMeeter::connectAudioNodes()
{
  // IO nodes are asymmetric; internal processing nodes must be symmetric (out x out)
  audioInputNode->getProcessor()->setPlayConfigDetails  (0,                numInputChannels,  lastSampleRate, lastBlockSize);
  audioOutputNode->getProcessor()->setPlayConfigDetails (numOutputChannels, 0,                 lastSampleRate, lastBlockSize);
  bandPassNode->getProcessor()->setPlayConfigDetails    (numOutputChannels, numOutputChannels, lastSampleRate, lastBlockSize);
  distortionNode->getProcessor()->setPlayConfigDetails  (numOutputChannels, numOutputChannels, lastSampleRate, lastBlockSize);

  // Connections: Input -> BandPass -> Distortion -> Output
  // If input is mono but output is stereo, duplicate channel 0 to both outputs
  for (int ch = 0; ch < numOutputChannels; ++ch)
  {
    const int srcCh = juce::jmin (ch, numInputChannels - 1);
    mainProcessor->addConnection ({ { audioInputNode->nodeID,  srcCh }, { bandPassNode->nodeID,    ch } });
    mainProcessor->addConnection ({ { bandPassNode->nodeID,    ch    }, { distortionNode->nodeID,  ch } });
    mainProcessor->addConnection ({ { distortionNode->nodeID,  ch    }, { audioOutputNode->nodeID, ch } });
  }

  for (auto* node : mainProcessor->getNodes())
    node->getProcessor()->enableAllBuses();
}

void DissonanceMeeter::releaseResources()
{
  if (mainProcessor != nullptr)
    mainProcessor->releaseResources();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool DissonanceMeeter::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  // Output must be mono or stereo
  if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
      && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
    return false;

  // Input must be mono or stereo (can differ from output, or be disabled)
  if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::mono()
      && layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo()
      && !layouts.getMainInputChannelSet().isDisabled())
    return false;

  return true;
}
#endif

void DissonanceMeeter::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
  juce::ScopedNoDenormals noDenormals;

  // Clear extra outputs if any
  for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
    buffer.clear (ch,0, buffer.getNumSamples());

  // If in ExternalInput mode, pass through main graph; otherwise generate internal oscillator signal
  if (getInputMode() == InputMode::ExternalInput)
  {
    if (mainProcessor != nullptr)
      mainProcessor->processBlock (buffer, midi);
  }
  else
  {
    // Oscillator mode: generate two-sine blend and write to all output channels
    const int numSamples = buffer.getNumSamples();
    const int numCh = buffer.getNumChannels();
    const double sr = lastSampleRate >0.0 ? lastSampleRate :44100.0;
    const float f1 = oscFreq1.load();
    const float f2 = oscFreq2.load();
    for (int ch =0; ch < numCh; ++ch)
    {
      float* data = buffer.getWritePointer (ch);
      for (int i =0; i < numSamples; ++i)
      {
        const float s1 = static_cast<float> (std::sin (oscPhase1));
        const float s2 = static_cast<float> (std::sin (oscPhase2));
        data[i] =0.5f * s1 +0.5f * s2;
        oscPhase1 += juce::MathConstants<double>::twoPi * (f1 / sr);
        oscPhase2 += juce::MathConstants<double>::twoPi * (f2 / sr);
        if (oscPhase1 > juce::MathConstants<double>::twoPi) oscPhase1 -= juce::MathConstants<double>::twoPi;
        if (oscPhase2 > juce::MathConstants<double>::twoPi) oscPhase2 -= juce::MathConstants<double>::twoPi;
      }
    }
  }

  // If selected to use a single external channel, copy that channel to outputs
  if (getInputMode() == InputMode::ExternalInput && getSelectedInputChannel() >=0)
  {
    const int sel = getSelectedInputChannel();
    if (sel < buffer.getNumChannels())
    {
      // duplicate selected input channel into all outputs
      const int samples = buffer.getNumSamples();
      const float* src = buffer.getReadPointer (sel);
      for (int ch =0; ch < buffer.getNumChannels(); ++ch)
      {
        if (ch == sel) continue;
        float* dst = buffer.getWritePointer (ch);
        std::memcpy (dst, src, (size_t) samples * sizeof (float));
      }
    }
  }

  waveForm.pushBuffer (buffer);

  // Apply master output gain to entire buffer
  const float gain = juce::jlimit (0.0f,4.0f, getOutputGain()); // clamp gain0..4
  if (gain !=1.0f)
  {
    for (int ch =0; ch < buffer.getNumChannels(); ++ch)
      buffer.applyGain (ch,0, buffer.getNumSamples(), gain);
  }

  // Compute RMS in dBFS across all channels for the block
  double sumSquares =0.0;
  int totalSamples = buffer.getNumSamples() * buffer.getNumChannels();
  for (int ch =0; ch < buffer.getNumChannels(); ++ch)
  {
    const float* d = buffer.getReadPointer (ch);
    for (int i =0; i < buffer.getNumSamples(); ++i)
      sumSquares += (double)d[i] * (double)d[i];
  }
  float rms = totalSamples >0 ? (float) std::sqrt (sumSquares / (double) totalSamples) :0.0f;
  float dbfs = rms >0.0f ?20.0f * std::log10 (rms) : -100.0f;
  updateOutputLevelRms (dbfs);
}

//==============================================================================
bool DissonanceMeeter::hasEditor() const { return true; }

juce::AudioProcessorEditor* DissonanceMeeter::createEditor()
{
  // Safe cast references to internal nodes
  jassert (bandPassNode  != nullptr && bandPassNode->getProcessor()   != nullptr);
  jassert (distortionNode!= nullptr && distortionNode->getProcessor() != nullptr);
  return new DissonanceMeeterAudioProcessorEditor (*this,
          static_cast<BandPassFilter&>(*bandPassNode->getProcessor()),
          static_cast<Distortion&>(*distortionNode->getProcessor()));
}

//==============================================================================
void DissonanceMeeter::getStateInformation (juce::MemoryBlock& destData)
{
  // Could aggregate child states if needed later
  juce::ignoreUnused (destData);
}

void DissonanceMeeter::setStateInformation (const void* data, int sizeInBytes)
{
  juce::ignoreUnused (data, sizeInBytes);
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
  return new DissonanceMeeter();
}
