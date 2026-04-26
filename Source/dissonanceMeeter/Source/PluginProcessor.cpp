/*
	==============================================================================

		This file contains the basic framework code for a JUCE plugin processor.

	==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
DissonanceMeeterAudioProcessor::DissonanceMeeterAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
	: AudioProcessor(BusesProperties()
#if ! JucePlugin_IsMidiEffect
#if ! JucePlugin_IsSynth
		.withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
		.withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
	)
#endif
{
	mainProcessor = std::make_unique<juce::AudioProcessorGraph>();
	waveForm.setRepaintRate(30);
	waveForm.setBufferSize(512);
	waveForm.setSamplesPerBlock(256);
	waveForm.setColours(juce::Colours::black, juce::Colours::lime);
	initialiseGraph();
}

DissonanceMeeterAudioProcessor::~DissonanceMeeterAudioProcessor()
{
	if (mainProcessor != nullptr)
		mainProcessor->releaseResources();
}

//==============================================================================
const juce::String DissonanceMeeterAudioProcessor::getName() const
{
	return JucePlugin_Name;
}

bool DissonanceMeeterAudioProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
	return true;
#else
	return false;
#endif
}

bool DissonanceMeeterAudioProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
	return true;
#else
	return false;
#endif
}

bool DissonanceMeeterAudioProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
	return true;
#else
	return false;
#endif
}

double DissonanceMeeterAudioProcessor::getTailLengthSeconds() const
{
	return 0.0;
}

int DissonanceMeeterAudioProcessor::getNumPrograms()
{
	return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
	// so this should be at least 1, even if you're not really implementing programs.
}

int DissonanceMeeterAudioProcessor::getCurrentProgram()
{
	return 0;
}

void DissonanceMeeterAudioProcessor::setCurrentProgram(int index)
{}

const juce::String DissonanceMeeterAudioProcessor::getProgramName(int index)
{
	return {};
}

void DissonanceMeeterAudioProcessor::changeProgramName(int index, const juce::String& newName)
{}

//==============================================================================
void DissonanceMeeterAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
	lastSampleRate = sampleRate;
	lastBlockSize = samplesPerBlock;
	numInputChannels = getMainBusNumInputChannels();
	numOutputChannels = getMainBusNumOutputChannels();

	mainProcessor->setPlayConfigDetails(numInputChannels, numOutputChannels, sampleRate, samplesPerBlock);

	for (auto* node : mainProcessor->getNodes())
		node->getProcessor()->setPlayConfigDetails(numInputChannels, numOutputChannels, sampleRate, samplesPerBlock);

	mainProcessor->prepareToPlay(sampleRate, samplesPerBlock);
	initialiseOscillator(sampleRate);
}

void DissonanceMeeterAudioProcessor::releaseResources()
{
	oscPhase1 = 0.0;
	oscPhase2 = 0.0;
	if (mainProcessor != nullptr)
		mainProcessor->releaseResources();
}

void DissonanceMeeterAudioProcessor::initialiseGraph()
{
	if (mainProcessor == nullptr)
		mainProcessor = std::make_unique<juce::AudioProcessorGraph>();

	if (audioInputNode == nullptr)
		audioInputNode = mainProcessor->addNode(std::make_unique<AudioGraphIOProcessor>(AudioGraphIOProcessor::audioInputNode));
	if (bandPassNode == nullptr)
		bandPassNode = mainProcessor->addNode(std::make_unique<BandPassFilter>());
	if (distortionNode == nullptr)
		distortionNode = mainProcessor->addNode(std::make_unique<Distortion>());
	if (audioOutputNode == nullptr)
		audioOutputNode = mainProcessor->addNode(std::make_unique<AudioGraphIOProcessor>(AudioGraphIOProcessor::audioOutputNode));

	connectAudioNodes();
}

void DissonanceMeeterAudioProcessor::connectAudioNodes()
{
	for (auto* node : mainProcessor->getNodes())
		node->getProcessor()->setPlayConfigDetails(numInputChannels, numOutputChannels, lastSampleRate, lastBlockSize);

	// Stereo: Input -> BandPass -> Distortion -> Output
	for (int ch = 0; ch < juce::jmin(2, numOutputChannels); ++ch)
	{
		mainProcessor->addConnection({ { audioInputNode->nodeID,  ch }, { bandPassNode->nodeID,   ch } });
		mainProcessor->addConnection({ { bandPassNode->nodeID,    ch }, { distortionNode->nodeID, ch } });
		mainProcessor->addConnection({ { distortionNode->nodeID,  ch }, { audioOutputNode->nodeID,ch } });
	}

	for (auto* node : mainProcessor->getNodes())
		node->getProcessor()->enableAllBuses();
}


#ifndef JucePlugin_PreferredChannelConfigurations
bool DissonanceMeeterAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
#if JucePlugin_IsMidiEffect
	juce::ignoreUnused(layouts);
	return true;
#else
	// This is the place where you check if the layout is supported.
	// In this template code we only support mono or stereo.
	// Some plugin hosts, such as certain GarageBand versions, will only
	// load plugins that support stereo bus layouts.
	if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
		&& layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
		return false;

	// This checks if the input layout matches the output layout
#if ! JucePlugin_IsSynth
	if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
		return false;
#endif

	return true;
#endif
}
#endif

void DissonanceMeeterAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
	juce::ScopedNoDenormals noDenormals;

	for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
		buffer.clear(ch, 0, buffer.getNumSamples());

	if (getInputMode() == InputMode::ExternalInput)
	{
		if (mainProcessor != nullptr)
			mainProcessor->processBlock(buffer, midiMessages);
	}
	else
	{
		// Modalità oscillatore: genera due sinusoidi miscelate
		const int    numSamples = buffer.getNumSamples();
		const int    numCh = buffer.getNumChannels();
		const double sr = lastSampleRate > 0.0 ? lastSampleRate : 44100.0;
		const float  f1 = oscFreq1.load();
		const float  f2 = oscFreq2.load();

		for (int ch = 0; ch < numCh; ++ch)
		{
			float* data = buffer.getWritePointer(ch);
			for (int i = 0; i < numSamples; ++i)
			{
				data[i] = 0.5f * (float)std::sin(oscPhase1)
					+ 0.5f * (float)std::sin(oscPhase2);
				oscPhase1 += juce::MathConstants<double>::twoPi * (f1 / sr);
				oscPhase2 += juce::MathConstants<double>::twoPi * (f2 / sr);
				if (oscPhase1 > juce::MathConstants<double>::twoPi) oscPhase1 -= juce::MathConstants<double>::twoPi;
				if (oscPhase2 > juce::MathConstants<double>::twoPi) oscPhase2 -= juce::MathConstants<double>::twoPi;
			}
		}
	}

	// Guadagno master
	const float gain = juce::jlimit(0.0f, 4.0f, getOutputGain());
	if (gain != 1.0f)
		for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
			buffer.applyGain(ch, 0, buffer.getNumSamples(), gain);

	// Calcolo RMS → dBFS per il meter principale
	double sumSq = 0.0;
	const int totalSamples = buffer.getNumSamples() * buffer.getNumChannels();
	for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
	{
		const float* d = buffer.getReadPointer(ch);
		for (int i = 0; i < buffer.getNumSamples(); ++i)
			sumSq += (double)d[i] * (double)d[i];
	}
	float rms = totalSamples > 0 ? (float)std::sqrt(sumSq / totalSamples) : 0.0f;
	float dbfs = rms > 1e-9f ? 20.0f * std::log10(rms) : -100.0f;
	updateOutputLevelRms(dbfs);

	waveForm.pushBuffer(buffer);
}

//==============================================================================
bool DissonanceMeeterAudioProcessor::hasEditor() const
{
	return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* DissonanceMeeterAudioProcessor::createEditor()
{
	jassert(bandPassNode != nullptr && bandPassNode->getProcessor() != nullptr);
	jassert(distortionNode != nullptr && distortionNode->getProcessor() != nullptr);
	return new DissonanceMeeterAudioProcessorEditor(
		*this,
		static_cast<BandPassFilter&> (*bandPassNode->getProcessor()),
		static_cast<Distortion&>     (*distortionNode->getProcessor()));
}

//==============================================================================
void DissonanceMeeterAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
	juce::ignoreUnused(destData);
}

void DissonanceMeeterAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
	juce::ignoreUnused(data, sizeInBytes);
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
	return new DissonanceMeeterAudioProcessor();
}
