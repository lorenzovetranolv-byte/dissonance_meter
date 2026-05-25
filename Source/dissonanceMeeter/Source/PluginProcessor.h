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
#include "../../DissonanceAnalyser.h"


class BandPassFilter : public ProcessorBase
{
public:
	BandPassFilter() : treeState(*this, nullptr, "BP_PARAMS", createLayout()) {}

	void prepareToPlay(double sampleRate, int samplesPerBlock) override
	{
		currentSampleRate = static_cast<float> (sampleRate);
		const int numChannels = juce::jmax(1, getTotalNumOutputChannels());

		juce::dsp::ProcessSpec spec{ sampleRate,
																	static_cast<juce::uint32> (samplesPerBlock),
																	1 };

		filters.clear();
		filters.resize(numChannels);
		for (auto& f : filters)
		{
			f.prepare(spec);  // ← prepara ogni filtro
			f.reset();
		}

		minFreqSmooth.reset(sampleRate, 0.02);
		maxFreqSmooth.reset(sampleRate, 0.02);
		// Imposta i target dagli attuali valori dei parametri prima di calcolare i coefficienti
		minFreqSmooth.setTargetValue(*treeState.getRawParameterValue("MIN_FREQ"));
		maxFreqSmooth.setTargetValue(*treeState.getRawParameterValue("MAX_FREQ"));
		updateCoefficients();
	}

	void processBlock(juce::AudioSampleBuffer& buffer, juce::MidiBuffer&) override
	{
		const int numSamples = buffer.getNumSamples();
		const int numChannels = buffer.getNumChannels();

		// Ridimensiona se necessario
		if ((int)filters.size() < numChannels)
		{
			juce::dsp::ProcessSpec spec{ currentSampleRate,
					static_cast<juce::uint32>(buffer.getNumSamples()),
					1 };

			const int oldSize = (int)filters.size(); // ← salva PRIMA del resize
			filters.resize(numChannels);

			for (int ch = oldSize; ch < numChannels; ++ch) // ← itera solo sui nuovi
			{
				filters[ch].prepare(spec);
				filters[ch].reset();
			}
		}

		// 1. Aggiorna coefficienti
		minFreqSmooth.setTargetValue(*treeState.getRawParameterValue("MIN_FREQ"));
		maxFreqSmooth.setTargetValue(*treeState.getRawParameterValue("MAX_FREQ"));

		// Aggiorna coefficienti per ogni campione → smoothing reale a sample rate
		for (int i = 0; i < numSamples; ++i)
		{
			updateCoefficients(); // avanza smoother di 1 campione

			for (int ch = 0; ch < numChannels; ++ch)
			{
				float* data = buffer.getWritePointer(ch);
				// Processa un singolo campione per canale
				data[i] = filters[ch].processSample(data[i]); // ← sample by sample
			}
		}

		// 3. RMS sul mid channel per la misurazione
		double sumSq = 0.0;
		for (int ch = 0; ch < numChannels; ++ch)
		{
			const float* data = buffer.getReadPointer(ch);
			for (int i = 0; i < numSamples; ++i)
				sumSq += (double)data[i] * (double)data[i];
		}
		float rms = (numSamples * numChannels) > 0
			? (float)std::sqrt(sumSq / (numSamples * numChannels))
			: 0.0f;
		float db = rms > 1e-9f ? 20.0f * std::log10(rms) : -100.0f;
		bandIntensityDb.store(juce::jlimit(-100.0f, 0.0f, db));
	}

	void reset() override
	{
		for (auto& f : filters)
			f.reset();
		bandIntensityDb.store(-100.0f);
	}

	const juce::String getName() const override { return "BandPass"; }

	// Leggi l'intensità della banda dal processore / editor
	float getBandIntensityDb() const noexcept { return bandIntensityDb.load(); }

	juce::AudioProcessorValueTreeState treeState;
	juce::LinearSmoothedValue<float> minFreqSmooth{ 0.0f };
	juce::LinearSmoothedValue<float> maxFreqSmooth{ 0.0f };

private:
	juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
	{
		std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

		// Frequenza minima della banda: 20..10000 Hz, default 20 Hz (leftmost)
		params.push_back(std::make_unique<juce::AudioParameterFloat>(
			"MIN_FREQ", "Min Freq",
			juce::NormalisableRange<float>(0.0f, 10000.0f, 0.0f, 0.25f), 0.0f));

		// Frequenza massima della banda: 20..20000 Hz, default 20000 Hz (fully open)
		params.push_back(std::make_unique<juce::AudioParameterFloat>(
			"MAX_FREQ", "Max Freq",
			juce::NormalisableRange<float>(0.0f, 20000.0f, 0.0f, 0.25f), 0.0f));

		return { params.begin(), params.end() };
	}

	void updateCoefficients()
	{
		float minF = juce::jlimit(0.0f, 10000.0f, minFreqSmooth.getNextValue());
		float maxF = juce::jlimit(minF + 1.0f, 20000.0f, maxFreqSmooth.getNextValue());

		// makeBandPass requires center > 0; clamp to 20 Hz minimum to prevent NaN
		const float minFeff = juce::jmax(20.0f, minF);
		const float maxFeff = juce::jmax(minFeff + 1.0f, maxF);
		const float center  = std::sqrt(minFeff * maxFeff);
		const float bw      = maxFeff - minFeff;
		const float q       = juce::jlimit(0.1f, 30.0f, center / bw);

		for (auto& f : filters)
			*f.coefficients = *juce::dsp::IIR::Coefficients<float>::makeBandPass(
				currentSampleRate, center, q);
	}

	float currentSampleRate = 44100.0f;
	std::atomic<float> bandIntensityDb{ -100.0f };

	std::vector<juce::dsp::IIR::Filter<float>> filters;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BandPassFilter)
};

//==============================================================================
// Distortion: Duffing-derived even-harmonic waveshaper
//
//   y = (in + k*in²) / (1+k)   k = A/100  →  adds 2nd harmonic (x² from ODE)
//   DC bias from in² is removed by a slow LP tracker.
//   Soft saturation blends linearly with tanh proportional to k.
//   A=0 → clean pass-through, A=100 → heavy even-harmonic distortion.
//==============================================================================
class Distortion : public ProcessorBase
{
public:
	Distortion() : treeState(*this, nullptr, "DIST_PARAMS", createLayout()) {}

	void prepareToPlay(double sampleRate, int samplesPerBlock) override
	{
		currentSampleRate = static_cast<float> (sampleRate);
		drive.reset(sampleRate, 0.02);
		(void)samplesPerBlock;

		const int channels = juce::jmax(1, getTotalNumOutputChannels());
		xState.assign(channels, 0.0f);  // DC tracker per channel
	}

	void processBlock(juce::AudioSampleBuffer& buffer, juce::MidiBuffer&) override
	{
		drive.setTargetValue(*treeState.getRawParameterValue("A"));

		const int numChannels = buffer.getNumChannels();
		const int numSamples  = buffer.getNumSamples();

		if ((int)xState.size() < numChannels)
			xState.resize(numChannels, 0.0f);

		for (int i = 0; i < numSamples; ++i)
		{
			const float A = drive.getNextValue();
			const float k = A / 100.0f;  // normalised non-linearity [0, 1]

			for (int ch = 0; ch < numChannels; ++ch)
			{
				float* data = buffer.getWritePointer(ch);
				const float in = data[i];

				// Duffing-derived even-harmonic distortion: y = in + k*in²
				// The x² term (as in the ODE) generates the 2nd harmonic.
				// Pre-normalised so |out| stays bounded when |in| ≤ 1.
				float out = (in + k * in * in) / (1.0f + k);

				// Remove DC bias introduced by the even-power term
				float& dc = xState[ch];
				dc += (out - dc) * 5.0e-4f;  // ~3.5 Hz LP tracker
				out -= dc;

				// Soft saturation: linear at k=0, tanh at k=1 — no hard jlimit
				data[i] = out + k * (std::tanh(out) - out);
			}
		}
	}

	void reset() override
	{
		std::fill(xState.begin(), xState.end(), 0.0f);
		drive.reset(currentSampleRate > 0.0f ? currentSampleRate : 44100.0, 0.02);
	}

	const juce::String getName() const override { return "Distortion"; }

	juce::AudioProcessorValueTreeState treeState;
	juce::LinearSmoothedValue<float> drive{ 0.0f }; // parametro A

private:
	juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
	{
		std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
		params.push_back(std::make_unique<juce::AudioParameterFloat>(
			"A", "A (Non-linearity)",
			juce::NormalisableRange<float>(0.0f, 100.0f, 0.0f, 0.5f), 0.0f));
		return { params.begin(), params.end() };
	}

	float currentSampleRate = 44100.0f;
	std::vector<float> xState;  // per-channel DC tracker

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

	bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

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

	enum class InputMode { ExternalInput = 0, Oscillator = 1 };

	void setInputMode(InputMode m) noexcept { inputMode.store((int)m); }
	InputMode getInputMode() const noexcept { return static_cast<InputMode> (inputMode.load()); }


	void setOscillatorFrequencies(float f1, float f2) noexcept { oscFreq1.store(f1); oscFreq2.store(f2); }
	std::pair<float, float> getOscillatorFrequencies() const noexcept { return { oscFreq1.load(), oscFreq2.load() }; }

	void  initialiseOscillator() noexcept;

	void  setOutputGain(float g) noexcept { outputGain.store(g); }
	float getOutputGain() const noexcept { return outputGain.load(); }

	void  updateOutputLevelRms(float dbfs) noexcept { outputLevelRms.store(dbfs); }
	float getOutputLevelRms() const noexcept { return outputLevelRms.load(); }

	float getDissonance() const noexcept { return dissonanceAnalyser.getDissonance(); }

	juce::AudioVisualiserComponent& getWaveForm() noexcept { return waveForm; }

	std::atomic<float> outputGain{ 1.0f };
	std::atomic<float> outputLevelRms{ -100.0f };

private:
	DissonanceAnalyser dissonanceAnalyser;
	void initialiseGraph();
	void connectAudioNodes();

	juce::AudioVisualiserComponent waveForm{ 2 };

	double lastSampleRate = 44100.0;
	int    numInputChannels = 2;
	int    numOutputChannels = 2;

	std::unique_ptr<juce::AudioProcessorGraph> mainProcessor;
	Node::Ptr audioInputNode;
	Node::Ptr bandPassNode;
	Node::Ptr distortionNode;
	Node::Ptr audioOutputNode;

	std::atomic<int>   inputMode{ 0 };
	std::atomic<float> oscFreq1{ 150.0f };
	std::atomic<float> oscFreq2{ 220.0f };
	double oscPhase1 = 0.0;
	double oscPhase2 = 0.0;
	//==============================================================================
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DissonanceMeeterAudioProcessor)
};
