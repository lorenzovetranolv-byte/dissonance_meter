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
  : AudioProcessor (BusesProperties()
      .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
      .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
    )
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

  DBG ("[DissonanceMeeter] prepareToPlay sr=" + juce::String (sampleRate)
       + " bs=" + juce::String (samplesPerBlock)
       + " inCh=" + juce::String (numInputChannels)
       + " outCh=" + juce::String (numOutputChannels));

  // Configure the outer graph and its child nodes with current runtime details
  mainProcessor->setPlayConfigDetails (numInputChannels, numOutputChannels, sampleRate, samplesPerBlock);

  // Ensure play config for each internal processor reflects current host settings
  for (auto* node : mainProcessor->getNodes())
    node->getProcessor()->setPlayConfigDetails (numInputChannels, numOutputChannels, lastSampleRate, lastBlockSize);

  // Prepare the graph (this will prepare child nodes too)
  mainProcessor->prepareToPlay (sampleRate, samplesPerBlock);
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
  // Ensure play config for each internal processor
  for (auto* node : mainProcessor->getNodes())
    node->getProcessor()->setPlayConfigDetails (numInputChannels, numOutputChannels, lastSampleRate, lastBlockSize);

  // Remove all existing connections before rebuilding (prevents duplicates / stale layouts)
  {
    const auto connections = mainProcessor->getConnections();
    for (const auto& c : connections)
      mainProcessor->removeConnection (c);
  }

  // Connections: route from input into the processing chain, then fan out to all outputs.
  // This avoids “silent output” on ASIO devices where the active output pair may not be 1/2.
  const int numIn  = juce::jmax (1, numInputChannels);
  const int numOut = juce::jmax (1, numOutputChannels);

  for (int outCh = 0; outCh < numOut; ++outCh)
  {
    const int inCh = juce::jmin (outCh, numIn - 1); // duplicate last available input if needed

    const bool c1 = mainProcessor->addConnection ({ { audioInputNode->nodeID,  inCh  }, { bandPassNode->nodeID,    outCh } });
    const bool c2 = mainProcessor->addConnection ({ { bandPassNode->nodeID,    outCh }, { distortionNode->nodeID,  outCh } });
    const bool c3 = mainProcessor->addConnection ({ { distortionNode->nodeID,  outCh }, { audioOutputNode->nodeID, outCh } });

    if (! (c1 && c2 && c3))
      DBG ("[DissonanceMeeter] Graph connection failed inCh=" + juce::String (inCh) + " outCh=" + juce::String (outCh));
  }

  for (auto* node : mainProcessor->getNodes())
    node->getProcessor()->enableAllBuses();
}

void DissonanceMeeter::releaseResources()
{
  if (mainProcessor != nullptr)
    mainProcessor->releaseResources();
}

bool DissonanceMeeter::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    auto mono = juce::AudioChannelSet::mono();
    auto stereo = juce::AudioChannelSet::stereo();

    return (layouts.getMainInputChannelSet() == mono || layouts.getMainInputChannelSet() == stereo) &&
        (layouts.getMainOutputChannelSet() == mono || layouts.getMainOutputChannelSet() == stereo);

}

void DissonanceMeeter::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
  juce::ScopedNoDenormals noDenormals;

  auto computeDbfs = [] (const juce::AudioBuffer<float>& b)
  {
    double sumSquares = 0.0;
    const int numCh = b.getNumChannels();
    const int numSamples = b.getNumSamples();
    const int totalSamples = numCh * numSamples;

    for (int ch = 0; ch < numCh; ++ch)
    {
      const float* d = b.getReadPointer (ch);
      for (int i = 0; i < numSamples; ++i)
        sumSquares += (double) d[i] * (double) d[i];
    }

    const float rms = totalSamples > 0 ? (float) std::sqrt (sumSquares / (double) totalSamples) : 0.0f;
    return rms > 0.0f ? 20.0f * std::log10 (rms) : -100.0f;
  };

  const float inputDbfs = computeDbfs (buffer);
  static int silentInputCounter = 0;
  if (inputDbfs <= -99.0f)
  {
    if (++silentInputCounter == 60) // ~1s at 60 blocks/s-ish
      DBG ("[DissonanceMeeter] Input appears silent (check standalone audio device input routing/enabled channels).");

#if JUCE_DEBUG
    // Diagnostic: if input is silent, generate an audible test tone BEFORE the processing graph.
    // This helps distinguish input-routing issues from graph/output issues.
    static double testPhase = 0.0;
    const double sr = lastSampleRate > 0.0 ? lastSampleRate : 48000.0;
    const double inc = juce::MathConstants<double>::twoPi * (250.0 / sr);
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
      auto* d = buffer.getWritePointer (ch);
      for (int i = 0; i < buffer.getNumSamples(); ++i)
      {
        d[i] = 0.1f * (float) std::sin (testPhase);
        testPhase += inc;
        if (testPhase > juce::MathConstants<double>::twoPi)
          testPhase -= juce::MathConstants<double>::twoPi;
      }
    }
#endif
  }
  else
  {
    silentInputCounter = 0;
  }

  // Clear extra outputs if any
  for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
    buffer.clear (ch,0, buffer.getNumSamples());

  // Process through main graph (Input -> BandPass -> Distortion -> Output)
  if (mainProcessor != nullptr)
    mainProcessor->processBlock (buffer, midi);

  waveForm.pushBuffer (buffer);

  // Apply master output gain to entire buffer
  const float gain = juce::jlimit (0.0f,4.0f, getOutputGain()); // clamp gain0..4
  if (gain !=1.0f)
  {
    for (int ch =0; ch < buffer.getNumChannels(); ++ch)
      buffer.applyGain (ch,0, buffer.getNumSamples(), gain);
  }

  // Compute RMS in dBFS across all channels for the block
  float dbfs = computeDbfs (buffer);
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
