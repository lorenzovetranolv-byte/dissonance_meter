/*
	==============================================================================

		This file contains the basic framework code for a JUCE plugin processor.

	==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <vector>
#include "ProcessorBase.h"
#include <atomic>
#include <cmath>


class BandPassFilter : public ProcessorBase
{
public:
	BandPassFilter() : treeState(*this, nullptr, "BP_PARAMS", createLayout()) {}

	void prepareToPlay(double sampleRate, int samplesPerBlock) override
	{
		currentSampleRate = static_cast<float> (sampleRate);
		juce::dsp::ProcessSpec spec{ sampleRate,
																	static_cast<juce::uint32> (samplesPerBlock),
																	1 }; // mono dopo il channel sum
		minFreqSmooth.reset(sampleRate, 0.02);
		maxFreqSmooth.reset(sampleRate, 0.02);
		updateCoefficients();
		filter.prepare(spec);
		monoBuffer.setSize(1, samplesPerBlock);
	}

	void processBlock(juce::AudioSampleBuffer& buffer, juce::MidiBuffer&) override
	{
		const int numSamples = buffer.getNumSamples();
		const int numChannels = buffer.getNumChannels();

		// --- 1. Somma tutti i canali in un buffer mono ---
		monoBuffer.setSize(1, numSamples, false, false, true);
		monoBuffer.clear();
		float* mono = monoBuffer.getWritePointer(0);

		for (int ch = 0; ch < numChannels; ++ch)
		{
			const float* src = buffer.getReadPointer(ch);
			for (int i = 0; i < numSamples; ++i)
				mono[i] += src[i];
		}
		// Normalizza per il numero di canali
		if (numChannels > 1)
			juce::FloatVectorOperations::multiply(mono, 1.0f / (float)numChannels, numSamples);

		// --- 2. Aggiorna coefficienti filtro dai parametri ---
		minFreqSmooth.setTargetValue(*treeState.getRawParameterValue("MIN_FREQ"));
		maxFreqSmooth.setTargetValue(*treeState.getRawParameterValue("MAX_FREQ"));
		updateCoefficients();

		// --- 3. Applica filtro passa-banda al segnale mono ---
		juce::dsp::AudioBlock<float> block(monoBuffer);
		juce::dsp::ProcessContextReplacing<float> ctx(block);
		filter.process(ctx);

		// --- 4. Calcola intensità (RMS → dBFS) del segnale filtrato ---
		double sumSq = 0.0;
		for (int i = 0; i < numSamples; ++i)
			sumSq += (double)mono[i] * (double)mono[i];
		float rms = numSamples > 0 ? (float)std::sqrt(sumSq / numSamples) : 0.0f;
		float db = rms > 1e-9f ? 20.0f * std::log10(rms) : -100.0f;
		bandIntensityDb.store(juce::jlimit(-100.0f, 0.0f, db));

		// --- 5. Copia il mono filtrato su tutti i canali del buffer in uscita ---
		for (int ch = 0; ch < numChannels; ++ch)
			buffer.copyFrom(ch, 0, monoBuffer, 0, 0, numSamples);
	}

	void reset() override
	{
		filter.reset();
		monoBuffer.clear();
		bandIntensityDb.store(-100.0f);
	}

	const juce::String getName() const override { return "BandPass"; }

	// Leggi l'intensità della banda dal processore / editor
	float getBandIntensityDb() const noexcept { return bandIntensityDb.load(); }

	juce::AudioProcessorValueTreeState treeState;
	juce::LinearSmoothedValue<float> minFreqSmooth{ 200.0f };
	juce::LinearSmoothedValue<float> maxFreqSmooth{ 2000.0f };

private:
	juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
	{
		std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

		// Frequenza minima della banda: 20..10000 Hz, default 200 Hz
		params.push_back(std::make_unique<juce::AudioParameterFloat>(
			"MIN_FREQ", "Min Freq",
			juce::NormalisableRange<float>(20.0f, 10000.0f, 0.0f, 0.25f), 200.0f));

		// Frequenza massima della banda: 20..20000 Hz, default 2000 Hz
		params.push_back(std::make_unique<juce::AudioParameterFloat>(
			"MAX_FREQ", "Max Freq",
			juce::NormalisableRange<float>(20.0f, 20000.0f, 0.0f, 0.25f), 2000.0f));

		return { params.begin(), params.end() };
	}

	void updateCoefficients()
	{
		// Deriva centro e Q dalla banda [minFreq, maxFreq]
		// centro = sqrt(min * max),  Q = centro / (max - min)
		float minF = juce::jlimit(20.0f, 10000.0f, minFreqSmooth.getNextValue());
		float maxF = juce::jlimit(minF + 1.0f, 20000.0f, maxFreqSmooth.getNextValue());

		float center = std::sqrt(minF * maxF);
		float bw = maxF - minF;
		float q = juce::jlimit(0.1f, 30.0f, center / bw);

		*filter.state = *juce::dsp::IIR::Coefficients<float>::makeBandPass(
			(double)currentSampleRate, (double)center, (double)q);
	}

	float currentSampleRate = 44100.0f;
	juce::AudioBuffer<float> monoBuffer;
	std::atomic<float> bandIntensityDb{ -100.0f };

	juce::dsp::ProcessorDuplicator<
		juce::dsp::IIR::Filter<float>,
		juce::dsp::IIR::Coefficients<float>> filter;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BandPassFilter)
};

//==============================================================================
// Distortion: oscillatore armonico smorzato non lineare
//
//   x'' = f(t) - 60*x' - 900*x - A*x^2
//
// Integrazione con Euler semi-implicito (Euler-Cromer):
//   1. x''(n) = in - 60*x'(n) - 900*x(n) - A*x(n)^2
//   2. x'(n+1) = x'(n) + x''(n) * dt          ← usa x'(n) corrente
//   3. x(n+1)  = x(n)  + x'(n+1) * dt          ← usa x'(n+1) aggiornato
//
// Euler-Cromer è più stabile di Euler esplicito per sistemi oscillatori
// perché conserva approssimativamente l'energia del sistema.
//==============================================================================
class Distortion : public ProcessorBase
{
public:
	Distortion() : treeState(*this, nullptr, "DIST_PARAMS", createLayout()) {}

	void prepareToPlay(double sampleRate, int samplesPerBlock) override
	{
		currentSampleRate = static_cast<float> (sampleRate);
		drive.reset(sampleRate, 0.02); // smoothing 20 ms sul parametro A
		(void)samplesPerBlock;

		const int channels = getTotalNumOutputChannels();
		// Pre-alloca stati per ogni canale (nessuna allocazione nel loop audio)
		xState.assign(channels, 0.0f);  // x(t)
		xPrimeState.assign(channels, 0.0f);  // x'(t)
	}

	void processBlock(juce::AudioSampleBuffer& buffer, juce::MidiBuffer&) override
	{
		drive.setTargetValue(*treeState.getRawParameterValue("A"));

		const int numChannels = buffer.getNumChannels();
		const int numSamples = buffer.getNumSamples();
		// dt = 1 / sampleRate, calcolato una volta per blocco
		const float dt = currentSampleRate > 0.0f ? (1.0f / currentSampleRate) : 0.0f;

		// Ridimensiona stati senza allocare nel hot path se il layout cambia
		if ((int)xState.size() < numChannels)
		{
			xState.resize(numChannels, 0.0f);
			xPrimeState.resize(numChannels, 0.0f);
		}

		for (int ch = 0; ch < numChannels; ++ch)
		{
			float* data = buffer.getWritePointer(ch);
			float& x = xState[ch];       // stato: posizione
			float& dx = xPrimeState[ch];  // stato: velocità

			for (int i = 0; i < numSamples; ++i)
			{
				const float in = data[i];
				const float A = drive.getNextValue();

				// --- Euler semi-implicito (Euler-Cromer) ---

				// 1. Calcola accelerazione con gli stati CORRENTI
				//    x'' = f(t) - 60*x' - 900*x - A*x^2
				const float ddx = in
					- 60.0f * dx
					- 900.0f * x
					- A * (x * x);

				// 2. Aggiorna la velocità: x'(n+1) = x'(n) + x''(n) * dt
				dx += ddx * dt;

				// 3. Aggiorna la posizione usando la NUOVA velocità (semi-implicito):
				//    x(n+1) = x(n) + x'(n+1) * dt
				x += dx * dt;

				// --- Soft saturation per stabilità numerica ---
				// Applica tanh come saturatore morbido prima del hard clamp
				x = std::tanh(x * 0.5f) * 2.0f;  // soft clip su ±~2
				dx = juce::jlimit(-500.0f, 500.0f, dx);

				data[i] = x;
			}
		}
	}

	void reset() override
	{
		std::fill(xState.begin(), xState.end(), 0.0f);
		std::fill(xPrimeState.begin(), xPrimeState.end(), 0.0f);
		drive.reset(currentSampleRate, 0.02);
	}

	const juce::String getName() const override { return "Distortion"; }

	juce::AudioProcessorValueTreeState treeState;
	juce::LinearSmoothedValue<float> drive{ 0.6f }; // parametro A

private:
	juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
	{
		std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
		// A: parametro di non linearità. Range esteso 0.1..5.0
		params.push_back(std::make_unique<juce::AudioParameterFloat>(
			"A", "A (Non-linearity)",
			juce::NormalisableRange<float>(0.1f, 5.0f, 0.0f, 0.3f), 0.6f));
		return { params.begin(), params.end() };
	}

	float currentSampleRate = 44100.0f;
	std::vector<float> xState;       // x(t):  posizione per ogni canale
	std::vector<float> xPrimeState;  // x'(t): velocità per ogni canale

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Distortion)
};


//==============================================================================
/**
*/
class DissonanceMeeterAudioProcessor : public juce::AudioProcessor
#if JucePlugin_Enable_ARA
	, public juce::AudioProcessorARAExtension
#endif
{
public:

	using AudioGraphIOProcessor = juce::AudioProcessorGraph::AudioGraphIOProcessor;
	using Node = juce::AudioProcessorGraph::Node;
	//==============================================================================
	DissonanceMeeterAudioProcessor();
	~DissonanceMeeterAudioProcessor() override;

	//==============================================================================
	void prepareToPlay(double sampleRate, int samplesPerBlock) override;
	void releaseResources() override;

#ifndef JucePlugin_PreferredChannelConfigurations
	bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
#endif

	void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

	//==============================================================================
	juce::AudioProcessorEditor* createEditor() override;
	bool hasEditor() const override;

	//==============================================================================
	const juce::String getName() const override;

	bool acceptsMidi() const override;
	bool producesMidi() const override;
	bool isMidiEffect() const override;
	double getTailLengthSeconds() const override;

	//==============================================================================
	int getNumPrograms() override;
	int getCurrentProgram() override;
	void setCurrentProgram(int index) override;
	const juce::String getProgramName(int index) override;
	void changeProgramName(int index, const juce::String& newName) override;

	//==============================================================================
	void getStateInformation(juce::MemoryBlock& destData) override;
	void setStateInformation(const void* data, int sizeInBytes) override;

	juce::AudioVisualiserComponent waveForm{ 1 };

	enum class InputMode { ExternalInput = 0, Oscillator = 1 };

	void setInputMode(InputMode m) noexcept { inputMode.store((int)m); }
	InputMode getInputMode() const noexcept { return static_cast<InputMode> (inputMode.load()); }

	void setSelectedInputChannel(int ch) noexcept { selectedInputChannel.store(ch); }
	int  getSelectedInputChannel() const noexcept { return selectedInputChannel.load(); }

	void setOscillatorFrequencies(float f1, float f2) noexcept { oscFreq1.store(f1); oscFreq2.store(f2); }
	std::pair<float, float> getOscillatorFrequencies() const noexcept { return { oscFreq1.load(), oscFreq2.load() }; }

	void  initialiseOscillator(double sampleRate) noexcept;

	void  setOutputGain(float g) noexcept { outputGain.store(g); }
	float getOutputGain() const noexcept { return outputGain.load(); }

	void  updateOutputLevelRms(float dbfs) noexcept { outputLevelRms.store(dbfs); }
	float getOutputLevelRms() const noexcept { return outputLevelRms.load(); }

	std::atomic<float> outputGain{ 1.0f };
	std::atomic<float> outputLevelRms{ -100.0f };

private:
	void initialiseGraph();
	void connectAudioNodes();

	double lastSampleRate = 44100.0;
	int    lastBlockSize = 512;
	int    numInputChannels = 2;
	int    numOutputChannels = 2;

	std::unique_ptr<juce::AudioProcessorGraph> mainProcessor;
	juce::AudioProcessorGraph::Node::Ptr audioInputNode;
	juce::AudioProcessorGraph::Node::Ptr bandPassNode;
	juce::AudioProcessorGraph::Node::Ptr distortionNode;
	juce::AudioProcessorGraph::Node::Ptr audioOutputNode;

	std::atomic<int>   inputMode{ 0 };
	std::atomic<int>   selectedInputChannel{ 0 };
	std::atomic<float> oscFreq1{ 150.0f };
	std::atomic<float> oscFreq2{ 220.0f };
	double oscPhase1 = 0.0;
	double oscPhase2 = 0.0;
	//==============================================================================
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DissonanceMeeterAudioProcessor)
};
