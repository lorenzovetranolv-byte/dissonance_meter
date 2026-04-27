/*
	==============================================================================
	DissonanceAnalyser.h

	Calcola la dissonanza percepita in tempo reale usando il modello
	Plomp-Levelt / Sethares (1993).

	Algoritmo:
		1. Accumula campioni in un buffer circolare di dimensione FFT_SIZE
		2. Quando il buffer e' pieno applica una finestra di Hann ed esegue la FFT
		3. Estrae i parziali dominanti (picchi dello spettro di ampiezza)
		4. Calcola la dissonanza a coppie con la curva di Plomp-Levelt
		5. Normalizza il risultato in [0,1] e lo espone via atomic

	Nessuna allocazione dinamica nel processBlock.
	==============================================================================
*/
#pragma once

#include <JuceHeader.h>
#include <cmath>
#include <array>

class DissonanceAnalyser
{
public:
	//============================================================================
	static constexpr int   FFT_SIZE = 2048;
	static constexpr int   FFT_ORDER = 11;    // 2^11 = 2048
	static constexpr int   MAX_PARTIALS = 24;
	static constexpr float AMPLITUDE_THRESHOLD = 0.01f;
	static constexpr float ALPHA1 = 3.5f;
	static constexpr float ALPHA2 = 5.75f;

	//============================================================================
	DissonanceAnalyser()
		: fft(FFT_ORDER)
	{
		juce::dsp::WindowingFunction<float>::fillWindowingTables(
			window.data(), FFT_SIZE,
			juce::dsp::WindowingFunction<float>::hann);
	}

	//============================================================================
	void prepare(double sampleRate) noexcept
	{
		currentSampleRate = static_cast<float> (sampleRate);
		reset();
	}

	//============================================================================
	// Chiamato per ogni campione mono — nessuna allocazione
	void pushSample(float sample) noexcept
	{
		accumBuffer[writePos] = sample;
		writePos = (writePos + 1) & (FFT_SIZE - 1);
		++sampleCount;

		if (sampleCount >= HOP_SIZE)
		{
			sampleCount = 0;
			analyseFrame();
		}
	}

	//============================================================================
	// Risultato normalizzato [0,1]: 0 = consonante, 1 = massima dissonanza
	float getDissonance() const noexcept { return dissonanceValue.load(); }

	//============================================================================
	void reset() noexcept
	{
		accumBuffer.fill(0.0f);
		fftBuffer.fill(0.0f);
		writePos = 0;
		sampleCount = 0;
		dissonanceValue.store(0.0f);
	}

private:
	static constexpr int HOP_SIZE = FFT_SIZE / 2;

	//============================================================================
	void analyseFrame() noexcept
	{
		// 1. Copia buffer circolare in ordine cronologico + finestra di Hann
		for (int i = 0; i < FFT_SIZE; ++i)
		{
			int idx = (writePos + i) & (FFT_SIZE - 1);
			fftBuffer[i] = accumBuffer[idx] * window[i];
		}
		for (int i = FFT_SIZE; i < FFT_SIZE * 2; ++i)
			fftBuffer[i] = 0.0f;

		// 2. FFT forward (risultato: magnitudini in fftBuffer[0..FFT_SIZE/2])
		fft.performFrequencyOnlyForwardTransform(fftBuffer.data());

		const int   numBins = FFT_SIZE / 2;
		const float normFactor = 2.0f / (float)FFT_SIZE;

		// 3. Estrai parziali dominanti (picchi locali sopra soglia)
		struct Partial { float freq; float amp; };
		std::array<Partial, MAX_PARTIALS> partials{};
		int numPartials = 0;

		for (int k = 1; k < numBins - 1 && numPartials < MAX_PARTIALS; ++k)
		{
			const float amp = fftBuffer[k] * normFactor;

			if (amp > AMPLITUDE_THRESHOLD
				&& amp > fftBuffer[k - 1] * normFactor
				&& amp > fftBuffer[k + 1] * normFactor)
			{
				// Interpolazione parabolica per stima precisa della frequenza
				const float alpha = fftBuffer[k - 1] * normFactor;
				const float beta = amp;
				const float gamma = fftBuffer[k + 1] * normFactor;
				const float delta = 0.5f * (alpha - gamma)
					/ (alpha - 2.0f * beta + gamma + 1e-10f);
				const float freq = ((float)k + delta) * currentSampleRate / (float)FFT_SIZE;

				if (freq > 20.0f && freq < 20000.0f)
					partials[numPartials++] = { freq, amp };
			}
		}

		// 4. Calcola dissonanza Plomp-Levelt su tutte le coppie
		float totalDissonance = 0.0f;
		float maxDissonance = 0.0f;

		for (int i = 0; i < numPartials; ++i)
		{
			for (int j = i + 1; j < numPartials; ++j)
			{
				const float f1 = juce::jmin(partials[i].freq, partials[j].freq);
				const float f2 = juce::jmax(partials[i].freq, partials[j].freq);
				const float a1 = partials[i].amp;
				const float a2 = partials[j].amp;

				totalDissonance += plompLevelt(f1, f2, a1, a2);
				maxDissonance += a1 * a2; // massimo teorico
			}
		}

		// 5. Normalizza in [0,1]
		float normalised = 0.0f;
		if (maxDissonance > 1e-6f)
			normalised = juce::jlimit(0.0f, 1.0f, totalDissonance / maxDissonance);

		dissonanceValue.store(normalised);
	}

	//============================================================================
	// Curva di Plomp-Levelt (Sethares 1993):
	//   d = a1 * a2 * (exp(-alpha1*s*df) - exp(-alpha2*s*df))
	//   s = 0.24 / (0.0207*f1 + 18.96)   <- scala sulla banda critica
	static float plompLevelt(float f1, float f2, float a1, float a2) noexcept
	{
		const float df = f2 - f1;
		if (df <= 0.0f) return 0.0f;

		const float s = 0.24f / (0.0207f * f1 + 18.96f);
		const float x = s * df;
		const float d = std::exp(-ALPHA1 * x) - std::exp(-ALPHA2 * x);

		return a1 * a2 * juce::jmax(0.0f, d);
	}

	//============================================================================
	juce::dsp::FFT fft;

	std::array<float, FFT_SIZE>     window{};
	std::array<float, FFT_SIZE>     accumBuffer{};
	std::array<float, FFT_SIZE * 2> fftBuffer{};

	int   writePos = 0;
	int   sampleCount = 0;
	float currentSampleRate = 44100.0f;

	std::atomic<float> dissonanceValue{ 0.0f };

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DissonanceAnalyser)
};
