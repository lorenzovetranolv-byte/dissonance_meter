/*
	==============================================================================

		This file contains the basic framework code for a JUCE plugin processor.

	==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

#if defined(JUCE_DEBUG) || defined(DEBUG)
  #include "PluginTests.cpp"
#endif

//==============================================================================
DissonanceMeeterAudioProcessor::DissonanceMeeterAudioProcessor()
	: AudioProcessor(BusesProperties()
#if ! JucePlugin_IsMidiEffect
#if ! JucePlugin_IsSynth
		.withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
		.withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
	)
{
	mainProcessor = std::make_unique<juce::AudioProcessorGraph>();
	waveForm.setRepaintRate(30);
	waveForm.setBufferSize(256);
	waveForm.setSamplesPerBlock(512);
	waveForm.setColours(juce::Colours::black, juce::Colours::lime);
	initialiseGraph();

#if defined(JUCE_DEBUG) || defined(DEBUG)
	juce::UnitTestRunner runner;
	runner.setAssertOnFailure(false);
	runner.runAllTests();

	for (int i = 0; i < runner.getNumResults(); ++i)
	{
		auto* result = runner.getResult(i);
		juce::String msg = "[TEST] " + result->unitTestName
			+ " | " + result->subcategoryName
			+ " | Failures: " + juce::String(result->failures);
		juce::Logger::writeToLog(msg);
		DBG(msg);
	}
#endif
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

void DissonanceMeeterAudioProcessor::setCurrentProgram(int)
{
}

const juce::String DissonanceMeeterAudioProcessor::getProgramName(int)
{
	return {};
}

void DissonanceMeeterAudioProcessor::changeProgramName(int, const juce::String&)
{
}

//==============================================================================
void DissonanceMeeterAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
	lastSampleRate = sampleRate;
	numInputChannels = getMainBusNumInputChannels();
	numOutputChannels = getMainBusNumOutputChannels();

	// Rimuove solo le connessioni e le ricrea con il layout canali corretto,
	// senza distruggere i nodi (l'editor potrebbe tenere riferimenti ai processor)
	for (auto& c : mainProcessor->getConnections())
		mainProcessor->removeConnection(c);

	connectAudioNodes();

	mainProcessor->setPlayConfigDetails(numInputChannels, numOutputChannels, sampleRate, samplesPerBlock);
	mainProcessor->prepareToPlay(sampleRate, samplesPerBlock);

	dissonanceAnalyser.prepare(sampleRate);
	initialiseOscillator();
}

void DissonanceMeeterAudioProcessor::releaseResources()
{
	oscPhase1 = 0.0;
	oscPhase2 = 0.0;
	if (mainProcessor != nullptr)
		mainProcessor->releaseResources();
}

void DissonanceMeeterAudioProcessor::initialiseOscillator() noexcept
{
	oscPhase1 = 0.0;
	oscPhase2 = 0.0;
}

void DissonanceMeeterAudioProcessor::initialiseGraph()
{
	if (mainProcessor == nullptr)
		mainProcessor = std::make_unique<juce::AudioProcessorGraph>();

	if (audioInputNode == nullptr)
		audioInputNode = mainProcessor->addNode(std::make_unique<AudioGraphIOProcessor>(AudioGraphIOProcessor::audioInputNode));
	if (bandPassNode == nullptr)
	{
		bandPassNode = mainProcessor->addNode(std::make_unique<BandPassFilter>());
		// Dissonance is measured on the band-pass filtered signal — the last
		// stage of the chain (Distortion -> BandPass) — not on the raw,
		// unfiltered distortion output.
		static_cast<BandPassFilter*>(bandPassNode->getProcessor())->setDissonanceAnalyser(&dissonanceAnalyser);
	}
	if (distortionNode == nullptr)
		distortionNode = mainProcessor->addNode(std::make_unique<Distortion>());
	if (audioOutputNode == nullptr)
		audioOutputNode = mainProcessor->addNode(std::make_unique<AudioGraphIOProcessor>(AudioGraphIOProcessor::audioOutputNode));
}

void DissonanceMeeterAudioProcessor::connectAudioNodes()
{
	for (auto* node : mainProcessor->getNodes())
		node->getProcessor()->enableAllBuses();

	// Stereo: Input -> Distortion -> BandPass -> Output
	for (int ch = 0; ch < juce::jmin(2, numOutputChannels); ++ch)
	{
		mainProcessor->addConnection({ { audioInputNode->nodeID,  ch }, { distortionNode->nodeID, ch } });
		mainProcessor->addConnection({ { distortionNode->nodeID,  ch }, { bandPassNode->nodeID,   ch } });
		mainProcessor->addConnection({ { bandPassNode->nodeID,    ch }, { audioOutputNode->nodeID,ch } });
	}

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

		// Genera i campioni sul canale 0 avanzando la fase una sola volta per campione
		{
			float* data = buffer.getWritePointer(0);
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
		// Copia il segnale generato sugli altri canali
		for (int ch = 1; ch < numCh; ++ch)
			buffer.copyFrom(ch, 0, buffer, 0, 0, numSamples);

		// Normalizzazione peak (identica all'input esterno)
		float peak = 0.0f;
		for (int ch = 0; ch < numCh; ++ch)
			peak = juce::jmax(peak, buffer.getMagnitude(ch, 0, numSamples));
		if (peak > 1e-6f)
			buffer.applyGain(1.0f / peak);

		// Applica la catena Distortion → BandPass anche all'oscillatore
		if (mainProcessor != nullptr)
			mainProcessor->processBlock(buffer, midiMessages);
	}

	// Guadagno master
	const float gain = juce::jlimit(0.0f, 4.0f, getOutputGain());
	if (gain != 1.0f)
		for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
			buffer.applyGain(ch, 0, buffer.getNumSamples(), gain);

	// Post-chain peak normalisation: prevents hard clipping so metering stays stable.
	// Only scales down when the chain produces a peak above 0 dBFS.
	{
		const int numCh = buffer.getNumChannels();
		const int numS  = buffer.getNumSamples();
		float peak = 0.0f;
		for (int ch = 0; ch < numCh; ++ch)
			peak = juce::jmax(peak, buffer.getMagnitude(ch, 0, numS));
		if (peak > 1.0f)
			buffer.applyGain(1.0f / peak);
	}

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
	// Recupera i processor dai nodi
	auto* bp = dynamic_cast<BandPassFilter*>(bandPassNode->getProcessor());
	auto* dist = dynamic_cast<Distortion*>(distortionNode->getProcessor());

	if (bp == nullptr || dist == nullptr)
		return;

	// Crea un XML radice che contiene lo stato di entrambi
	juce::XmlElement root("DissonanceMeeterState");

	// Salva BandPassFilter
	if (auto xmlBP = bp->treeState.copyState().createXml())
	{
		xmlBP->setTagName("BandPass");
		root.addChildElement(xmlBP.release());
	}

	// Salva Distortion
	if (auto xmlDist = dist->treeState.copyState().createXml())
	{
		xmlDist->setTagName("Distortion");
		root.addChildElement(xmlDist.release());
	}

	copyXmlToBinary(root, destData);
}

void DissonanceMeeterAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
	auto* bp = dynamic_cast<BandPassFilter*>(bandPassNode->getProcessor());
	auto* dist = dynamic_cast<Distortion*>(distortionNode->getProcessor());

	if (bp == nullptr || dist == nullptr)
		return;

	// Legge l'XML salvato
	if (auto xmlState = getXmlFromBinary(data, sizeInBytes))
	{
		// Ripristina BandPassFilter
		if (auto* xmlBP = xmlState->getChildByName("BandPass"))
			bp->treeState.replaceState(juce::ValueTree::fromXml(*xmlBP));

		// Ripristina Distortion
		if (auto* xmlDist = xmlState->getChildByName("Distortion"))
			dist->treeState.replaceState(juce::ValueTree::fromXml(*xmlDist));
	}
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
	return new DissonanceMeeterAudioProcessor();
}
