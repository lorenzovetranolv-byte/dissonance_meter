/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

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
  lastSampleRate     = sampleRate;
  lastBlockSize      = samplesPerBlock;
  numInputChannels   = getMainBusNumInputChannels();
  numOutputChannels  = getMainBusNumOutputChannels();

  mainProcessor->setPlayConfigDetails (numInputChannels, numOutputChannels, sampleRate, samplesPerBlock);
  mainProcessor->prepareToPlay (sampleRate, samplesPerBlock);

  initialiseGraph();
}

void DissonanceMeeter::initialiseGraph()
{
  mainProcessor->clear();
  //INVERTIRE NODI -> INPUT - DISTORSIONE - BANDPASS FILTER  - OUTPUT
  audioInputNode  = mainProcessor->addNode (std::make_unique<AudioGraphIOProcessor>(AudioGraphIOProcessor::audioInputNode));
  bandPassNode    = mainProcessor->addNode (std::make_unique<BandPassFilter>());
  distortionNode  = mainProcessor->addNode (std::make_unique<Distortion>());
  audioOutputNode = mainProcessor->addNode (std::make_unique<AudioGraphIOProcessor>(AudioGraphIOProcessor::audioOutputNode));

  connectAudioNodes();
}

void DissonanceMeeter::connectAudioNodes()
{
  // Ensure play config for each internal processor
  for (auto* node : mainProcessor->getNodes())
    node->getProcessor()->setPlayConfigDetails (numInputChannels, numOutputChannels, lastSampleRate, lastBlockSize);

  // Stereo connections: Input -> BandPass -> Distortion -> Output
  for (int ch = 0; ch < juce::jmin (2, numOutputChannels); ++ch)
  {
    mainProcessor->addConnection ({ { audioInputNode->nodeID,  ch }, { bandPassNode->nodeID,    ch } });
    mainProcessor->addConnection ({ { bandPassNode->nodeID,    ch }, { distortionNode->nodeID,  ch } });
    mainProcessor->addConnection ({ { distortionNode->nodeID,  ch }, { audioOutputNode->nodeID, ch } });
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
  // Support only mono or stereo symmetrical
  if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
      && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
    return false;

  if (layouts.getMainInputChannelSet() != layouts.getMainOutputChannelSet())
    return false;

  return true;
}
#endif

void DissonanceMeeter::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
  juce::ScopedNoDenormals noDenormals;

  // Clear extra outputs if any
  for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
    buffer.clear (ch, 0, buffer.getNumSamples());

  if (mainProcessor != nullptr)
    mainProcessor->processBlock (buffer, midi);

  waveForm.pushBuffer (buffer);
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
