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

			float monoSum = 0.0f;
			for (int ch = 0; ch < numChannels; ++ch)
			{
				float* data = buffer.getWritePointer(ch);
				// Processa un singolo campione per canale
				data[i] = filters[ch].processSample(data[i]); // ← sample by sample
				monoSum += data[i];
			}

			// Dissonance is measured on the band-pass filtered signal: with the
			// chain ordered Distortion -> BandPass, this is the final processed
			// signal, not the raw (unfiltered) distortion output.
			if (dissonanceAnalyser != nullptr && numChannels > 0)
				dissonanceAnalyser->pushSample(monoSum / (float)numChannels);
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

	// Collega l'analizzatore di dissonanza: il bandpass (ultimo stadio della
	// catena Distortion -> BandPass) alimenta il dissonance meter con il
	// segnale finale filtrato.
	void setDissonanceAnalyser(DissonanceAnalyser* analyser) noexcept { dissonanceAnalyser = analyser; }

	juce::AudioProcessorValueTreeState treeState;
	juce::LinearSmoothedValue<float> minFreqSmooth{ 0.0f };
	juce::LinearSmoothedValue<float> maxFreqSmooth{ 0.0f };

private:
	juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
	{
		std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

		// Frequenza minima della banda: 10..100 Hz, default 20 Hz
		params.push_back(std::make_unique<juce::AudioParameterFloat>(
			"MIN_FREQ", "Min Freq",
			juce::NormalisableRange<float>(10.0f, 100.0f, 0.01f, 0.5f), 20.0f));

		// Frequenza massima della banda: 30..20000 Hz, default 2000 Hz
		params.push_back(std::make_unique<juce::AudioParameterFloat>(
			"MAX_FREQ", "Max Freq",
			juce::NormalisableRange<float>(30.0f, 20000.0f, 0.01f, 0.25f), 2000.0f));

		return { params.begin(), params.end() };
	}

	void updateCoefficients()
	{
		float minF = juce::jlimit(10.0f, 100.0f, minFreqSmooth.getNextValue());
		float maxF = juce::jlimit(30.0f, 20000.0f, maxFreqSmooth.getNextValue());

		// Center frequency is fixed at 30 Hz (not derived geometrically from the
		// band edges); only the Q factor is derived from center + bandwidth.
		const float center = 30.0f;
		const float bw     = juce::jmax(1.0f, maxF - minF);
		const float q      = juce::jlimit(0.1f, 30.0f, center / bw);

		for (auto& f : filters)
			*f.coefficients = *juce::dsp::IIR::Coefficients<float>::makeBandPass(
				currentSampleRate, center, q);
	}

	float currentSampleRate = 44100.0f;
	std::atomic<float> bandIntensityDb{ -100.0f };

	DissonanceAnalyser* dissonanceAnalyser = nullptr;

	std::vector<juce::dsp::IIR::Filter<float>> filters;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BandPassFilter)
};

//==============================================================================
// Distortion: Nonlinear damped harmonic oscillator (ODE-based intermodulation)
//
// Differential equation: x'' + damping·x' + stiffness·x + A·x² = f(t)
//   - f(t) = input signal (forcing term)
//   - x(t) = oscillator position (output)
//   - x'(t) = oscillator velocity (state)
//   - A = nonlinearity parameter (0-5000)
//
// Numerical solution (explicit Euler), per sample of length dt:
//   x''(t-1) = f(t) - damping·x'(t-1) - stiffness·x(t-1) - A·x²(t-1)
//   x'(t)    = x'(t-1) + x''(t-1) · dt
//   x(t)     = x(t-1)  + x'(t-1)  · dt
//
// Physical model: resonant system with:
//   - γ = ω = GAMMA_OMEGA parameter, expressed in Hz (1-200, default 30 Hz),
//     converted to angular frequency ω₀ = 2π·f (rad/s) for the ODE terms;
//     critically damped (ζ = 1) → damping = 2·ω₀, stiffness = ω₀²
//   - A·x² term generates intermodulation products and beating
//
// Processing: Parallel mix of clean signal + ODE output
//   - A=0 → bypass (100% clean signal, no ODE contribution)
//   - A>0 → blend clean + ODE nonlinear response (battimenti/intermodulation)
//   - Output compensated (×stiffness) to match input level at low frequencies
//==============================================================================
class Distortion : public ProcessorBase
{
public:
	Distortion() : treeState(*this, nullptr, "DIST_PARAMS", createLayout()) {}

	void prepareToPlay(double sampleRate, int samplesPerBlock) override
	{
		currentSampleRate = static_cast<float>(sampleRate);
		dt = 1.0f / static_cast<float>(sampleRate); // time step for Euler integration
		drive.reset(sampleRate, 0.02);
		gammaOmega.reset(sampleRate, 0.02);
		gammaOmega.setTargetValue(*treeState.getRawParameterValue("GAMMA_OMEGA"));
		(void)samplesPerBlock;

		const int channels = juce::jmax(1, getTotalNumOutputChannels());
		x.assign(channels, 0.0f);      // oscillator position per channel
		xDot.assign(channels, 0.0f);   // oscillator velocity per channel
	}

	void processBlock(juce::AudioSampleBuffer& buffer, juce::MidiBuffer&) override
	{
		drive.setTargetValue(*treeState.getRawParameterValue("A"));
		gammaOmega.setTargetValue(*treeState.getRawParameterValue("GAMMA_OMEGA"));

		const int numChannels = buffer.getNumChannels();
		const int numSamples  = buffer.getNumSamples();

		if ((int)x.size() < numChannels)
		{
			x.resize(numChannels, 0.0f);
			xDot.resize(numChannels, 0.0f);
		}

		for (int i = 0; i < numSamples; ++i)
		{
			const float A = drive.getNextValue();
			const float wet = juce::jlimit(0.0f, 1.0f, A / 5000.0f); // wet mix ratio [0,1]

			// GAMMA_OMEGA is expressed in Hz (consistent with BandPassFilter and
			// DissonanceAnalyser, which are also Hz-denominated); convert to the
			// angular frequency ω₀ (rad/s) used by the ODE's natural-frequency terms.
			// γ = ω (critically damped, ζ = 1): damping = 2·ω₀, stiffness = ω₀²
			const float gammaOmegaHz = gammaOmega.getNextValue();
			const float omega        = juce::MathConstants<float>::twoPi * gammaOmegaHz;
			const float damping      = 2.0f * omega;
			const float stiffness    = omega * omega;
			const float gainComp     = stiffness; // compensate DC attenuation (1/stiffness)

			for (int ch = 0; ch < numChannels; ++ch)
			{
				float* data = buffer.getWritePointer(ch);
				const float input = data[i]; // clean signal (forcing term)

				// ── ODE numerical integration (Explicit Euler) ──────────────
				// x''(t-1) = f(t) - damping·x'(t-1) - stiffness·x(t-1) - A·x²(t-1)
				const float xPrev    = x[ch];
				const float xDotPrev = xDot[ch];
				const float xDotDot = input
					- damping * xDotPrev
					- stiffness * xPrev
					- A * xPrev * xPrev; // nonlinearity → intermodulation

				// x'(t) = x'(t-1) + x''(t-1) · dt
				xDot[ch] = xDotPrev + xDotDot * dt;

				// x(t) = x(t-1) + x'(t-1) · dt
				x[ch] = xPrev + xDotPrev * dt;

				// Numerical-stability safety bounds: with A up to 5000 the
				// -A·x² term can overwhelm the linear restoring force and
				// drive explicit Euler to diverge. Clamp state, not the ODE.
				x[ch]    = juce::jlimit (-2.0f,   2.0f,   x[ch]);
				xDot[ch] = juce::jlimit (-200.0f, 200.0f, xDot[ch]);

				// ── Parallel Processing: clean + ODE ────────────────────────
				// Amplify ODE output to match input level at low freq
				const float odeOut = x[ch] * gainComp;

				// A=0 → 100% input (bypass)
				// A>0 → blend input + ODE distorted signal
				data[i] = input * (1.0f - wet) + odeOut * wet;
			}
		}
	}

	void reset() override
	{
		std::fill(x.begin(), x.end(), 0.0f);
		std::fill(xDot.begin(), xDot.end(), 0.0f);
		drive.reset(currentSampleRate > 0.0f ? currentSampleRate : 44100.0, 0.02);
		gammaOmega.reset(currentSampleRate > 0.0f ? currentSampleRate : 44100.0, 0.02);
	}

	const juce::String getName() const override { return "Distortion"; }

	juce::AudioProcessorValueTreeState treeState;
	juce::LinearSmoothedValue<float> drive{ 0.0f };      // parametro A (nonlinearity)
	juce::LinearSmoothedValue<float> gammaOmega{ 30.0f }; // parametro γ = ω

private:
	juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
	{
		std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
		params.push_back(std::make_unique<juce::AudioParameterFloat>(
			"A", "A (Non-linearity)",
			juce::NormalisableRange<float>(0.0f, 5000.0f, 0.1f, 0.5f), 0.0f));
		params.push_back(std::make_unique<juce::AudioParameterFloat>(
			"GAMMA_OMEGA", "Gamma/Omega",
			juce::NormalisableRange<float>(1.0f, 200.0f, 0.1f, 0.5f), 30.0f));
		return { params.begin(), params.end() };
	}

	float currentSampleRate = 44100.0f;
	float dt = 1.0f / 44100.0f; // time step for integration
	std::vector<float> x;       // oscillator position per channel
	std::vector<float> xDot;    // oscillator velocity per channel

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
