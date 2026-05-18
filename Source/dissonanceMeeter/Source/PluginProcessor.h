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

		for (auto& f : filters)
			*f.coefficients = *juce::dsp::IIR::Coefficients<float>::makeBandPass(
				(double)currentSampleRate, (double)center, (double)q); // aggiorna tutti i filtri duplicati
	}

	float currentSampleRate = 44100.0f;
	std::atomic<float> bandIntensityDb{ -100.0f };

	std::vector<juce::dsp::IIR::Filter<float>> filters;

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
		drive.reset(sampleRate, 0.02);
		(void)samplesPerBlock;

		const int channels = juce::jmax(1, getTotalNumOutputChannels());
		xState.assign(channels, 0.0f);
		xPrimeState.assign(channels, 0.0f);
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

		for (int i = 0; i < numSamples; ++i)
		{

			const float A = drive.getNextValue();

			for (int ch = 0; ch < numChannels; ++ch)
			{
				float* data = buffer.getWritePointer(ch);
				float& x = xState[ch];       // stato: posizione
				float& dx = xPrimeState[ch];  // stato: velocità
				const float in = data[i];

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
		drive.reset(currentSampleRate > 0.0f ? currentSampleRate : 44100.0, 0.02);
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

	juce::AudioVisualiserComponent& getWaveForm() noexcept { return waveForm; }

	std::atomic<float> outputGain{ 1.0f };
	std::atomic<float> outputLevelRms{ -100.0f };

private:
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
