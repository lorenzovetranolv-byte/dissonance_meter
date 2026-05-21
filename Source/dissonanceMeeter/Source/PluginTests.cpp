/*
		==============================================================================
		PluginTests.cpp

		Unit test per DissonanceMeeter usando il framework juce::UnitTest.
		Eseguiti automaticamente all'avvio in modalitÃ  Debug tramite
		juce::UnitTestRunner.
		==============================================================================
*/

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "../../DissonanceAnalyser.h"

//==============================================================================
// TEST 1 â€” DissonanceAnalyser: sinusoide singola â†’ dissonanza minima
//==============================================================================
class DissonanceAnalyserSingleSineTest : public juce::UnitTest
{
public:
		DissonanceAnalyserSingleSineTest()
				: juce::UnitTest ("DissonanceAnalyser - Sinusoide singola", "DissonanceMeeter") {}

		void runTest() override
		{
				beginTest ("440 Hz pura deve produrre dissonanza < 0.1");

				DissonanceAnalyser analyser;
				analyser.prepare (44100.0);

				constexpr int numSamples = 4096;
				for (int i = 0; i < numSamples; ++i)
				{
						float sample = std::sin (
								2.0f * juce::MathConstants<float>::pi * 440.0f * (float)i / 44100.0f);
						analyser.pushSample (sample);
				}

				float d = analyser.getDissonance();
				expectLessThan (d, 0.1f);
		}
};

//==============================================================================
// TEST 2 â€” DissonanceAnalyser: semitono â†’ dissonanza alta
//==============================================================================
class DissonanceAnalyserSemitoneTest : public juce::UnitTest
{
public:
		DissonanceAnalyserSemitoneTest()
				: juce::UnitTest ("DissonanceAnalyser - Semitono", "DissonanceMeeter") {}

		void runTest() override
		{
				beginTest ("440 Hz + 466 Hz (semitono) deve produrre dissonanza > 0.3");

				DissonanceAnalyser analyser;
				analyser.prepare (44100.0);

				constexpr int numSamples = 8192;
				for (int i = 0; i < numSamples; ++i)
				{
						float s = std::sin (2.0f * juce::MathConstants<float>::pi * 440.0f   * (float)i / 44100.0f)
										+ std::sin (2.0f * juce::MathConstants<float>::pi * 466.16f  * (float)i / 44100.0f);
						analyser.pushSample (s * 0.5f);
				}

				float d = analyser.getDissonance();
				expectGreaterThan (d, 0.3f);
		}
};

//==============================================================================
// TEST 3 â€” DissonanceAnalyser: quinta giusta < semitono
//==============================================================================
class DissonanceAnalyserFifthVsSemitoneTest : public juce::UnitTest
{
public:
		DissonanceAnalyserFifthVsSemitoneTest()
				: juce::UnitTest ("DissonanceAnalyser - Quinta vs Semitono", "DissonanceMeeter") {}

		void runTest() override
		{
				beginTest ("Quinta giusta (440+660 Hz) meno dissonante del semitono (440+466 Hz)");

				constexpr int numSamples = 8192;
				constexpr float sr = 44100.0f;

				// Quinta giusta
				DissonanceAnalyser fifth;
				fifth.prepare (sr);
				for (int i = 0; i < numSamples; ++i)
				{
						float s = std::sin (2.0f * juce::MathConstants<float>::pi * 440.0f * (float)i / sr)
										+ std::sin (2.0f * juce::MathConstants<float>::pi * 660.0f * (float)i / sr);
						fifth.pushSample (s * 0.5f);
				}

				// Semitono
				DissonanceAnalyser semitone;
				semitone.prepare (sr);
				for (int i = 0; i < numSamples; ++i)
				{
						float s = std::sin (2.0f * juce::MathConstants<float>::pi * 440.0f  * (float)i / sr)
										+ std::sin (2.0f * juce::MathConstants<float>::pi * 466.16f * (float)i / sr);
						semitone.pushSample (s * 0.5f);
				}

				float dFifth    = fifth.getDissonance();
				float dSemitone = semitone.getDissonance();

				expect (dFifth < dSemitone,
						"La quinta giusta dovrebbe essere meno dissonante del semitono. "
						"Quinta=" + juce::String (dFifth) + " Semitono=" + juce::String (dSemitone));
		}
};

//==============================================================================
// TEST 4 â€” DissonanceAnalyser: reset azzera lo stato
//==============================================================================
class DissonanceAnalyserResetTest : public juce::UnitTest
{
public:
		DissonanceAnalyserResetTest()
				: juce::UnitTest ("DissonanceAnalyser - Reset", "DissonanceMeeter") {}

		void runTest() override
		{
				beginTest ("Dopo reset() la dissonanza deve essere 0.0");

				DissonanceAnalyser analyser;
				analyser.prepare (44100.0);

				// Spingi campioni ad alta dissonanza
				for (int i = 0; i < 4096; ++i)
				{
						float s = std::sin (2.0f * juce::MathConstants<float>::pi * 440.0f  * (float)i / 44100.0f)
										+ std::sin (2.0f * juce::MathConstants<float>::pi * 466.16f * (float)i / 44100.0f);
						analyser.pushSample (s * 0.5f);
				}

				analyser.reset();

				expectEquals (analyser.getDissonance(), 0.0f);
		}
};

//==============================================================================
// TEST 5 â€” Distortion: output non Ã¨ silenzioso con segnale in input
//==============================================================================
class DistortionNotSilentTest : public juce::UnitTest
{
public:
		DistortionNotSilentTest()
				: juce::UnitTest ("Distortion - Output non silenzioso", "DissonanceMeeter") {}

		void runTest() override
		{
				beginTest ("Con segnale sinusoidale in input, l'output non deve essere silenzioso");

				Distortion dist;
				dist.prepareToPlay (44100.0, 512);

				constexpr int bufferSize = 512;
				juce::AudioBuffer<float> buffer (1, bufferSize);

				// Riempi con sinusoide a 440 Hz
				for (int i = 0; i < bufferSize; ++i)
						buffer.setSample (0, i,
								std::sin (2.0f * juce::MathConstants<float>::pi * 440.0f * (float)i / 44100.0f));

				juce::MidiBuffer midi;
				dist.processBlock (buffer, midi);

				// Calcola RMS output
				float sumSq = 0.0f;
				for (int i = 0; i < bufferSize; ++i)
				{
						float s = buffer.getSample (0, i);
						sumSq += s * s;
				}
				float rms = std::sqrt (sumSq / (float)bufferSize);

				expectGreaterThan (rms, 1e-4f);
		}
};

//==============================================================================
// TEST 6 â€” Distortion: output entro [-5, +5]  (clamp implementato in processBlock)
//==============================================================================
class DistortionOutputRangeTest : public juce::UnitTest
{
public:
		DistortionOutputRangeTest()
				: juce::UnitTest ("Distortion - Output entro [-5, +5]", "DissonanceMeeter") {}

		void runTest() override
		{
				beginTest ("L'output della Distortion deve sempre essere entro [-5.0, +5.0]");

				Distortion dist;
				dist.prepareToPlay (44100.0, 512);

				// Parametro A al massimo
				// Imposta A al massimo (valore normalizzato 1.0 = 5.0)
				auto* paramA = dist.treeState.getParameter ("A");
				if (paramA != nullptr)
						paramA->setValueNotifyingHost (1.0f);

				constexpr int bufferSize = 512;
				juce::AudioBuffer<float> buffer (1, bufferSize);

				// Segnale a piena ampiezza (0 dBFS)
				for (int i = 0; i < bufferSize; ++i)
						buffer.setSample (0, i,
								std::sin (2.0f * juce::MathConstants<float>::pi * 440.0f * (float)i / 44100.0f));

				juce::MidiBuffer midi;

								// Processa piÃ¹ blocchi consecutivi (simula flusso continuo)
				for (int block = 0; block < 10; ++block)
				{
						dist.processBlock (buffer, midi);

						for (int i = 0; i < bufferSize; ++i)
						{
								float s = buffer.getSample (0, i);
								expect (s >= -5.0f && s <= 5.0f,
										"Campione fuori range al blocco " + juce::String (block)
										+ " campione " + juce::String (i) + ": " + juce::String (s));
						}
				}
		}
};

//==============================================================================
// TEST 7 â€” Distortion: nessun NaN nell'output
//==============================================================================
class DistortionNoNaNTest : public juce::UnitTest
{
public:
		DistortionNoNaNTest()
				: juce::UnitTest ("Distortion - Nessun NaN nell'output", "DissonanceMeeter") {}

		void runTest() override
		{
				beginTest ("L'output della Distortion non deve contenere NaN o Inf");

				Distortion dist;
				dist.prepareToPlay (44100.0, 512);

				auto* paramA = dist.treeState.getParameter ("A");
				if (paramA != nullptr)
						paramA->setValueNotifyingHost (1.0f);

				constexpr int bufferSize = 512;
				juce::AudioBuffer<float> buffer (2, bufferSize); // stereo

				for (int i = 0; i < bufferSize; ++i)
				{
						float s = std::sin (2.0f * juce::MathConstants<float>::pi * 440.0f * (float)i / 44100.0f);
						buffer.setSample (0, i, s);
						buffer.setSample (1, i, s);
				}

				juce::MidiBuffer midi;

				for (int block = 0; block < 20; ++block)
				{
						dist.processBlock (buffer, midi);

						for (int ch = 0; ch < 2; ++ch)
								for (int i = 0; i < bufferSize; ++i)
								{
										float s = buffer.getSample (ch, i);
										expect (std::isfinite (s),
												"NaN/Inf trovato al blocco " + juce::String (block)
												+ " ch=" + juce::String (ch)
												+ " campione=" + juce::String (i));
								}
				}
		}
};

//==============================================================================
// Registrazione automatica di tutti i test
//==============================================================================
static DissonanceAnalyserSingleSineTest    dissonanceTest1;
static DissonanceAnalyserSemitoneTest      dissonanceTest2;
static DissonanceAnalyserFifthVsSemitoneTest dissonanceTest3;
static DissonanceAnalyserResetTest         dissonanceTest4;
static DistortionNotSilentTest             distortionTest1;
static DistortionOutputRangeTest           distortionTest2;
static DistortionNoNaNTest                 distortionTest3;

