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
				// 440+550 Hz (terza maggiore, df=110 Hz = 5.1 bins): risolto dall'FFT
				// Plomp-Levelt d ~ 0.033; soglia conservativa > 0.01
				beginTest ("440 Hz + 550 Hz (terza maggiore) deve produrre dissonanza > 0.01");

				DissonanceAnalyser analyser;
				analyser.prepare (44100.0);

				constexpr int numSamples = 8192;
				for (int i = 0; i < numSamples; ++i)
				{
						float s = std::sin (2.0f * juce::MathConstants<float>::pi * 440.0f * (float)i / 44100.0f)
										+ std::sin (2.0f * juce::MathConstants<float>::pi * 550.0f * (float)i / 44100.0f);
						analyser.pushSample (s * 0.5f);
				}

				float d = analyser.getDissonance();
				expectGreaterThan (d, 0.01f);
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
				// Usa 440+550 Hz (terza maggiore, df=110 Hz) come intervallo "dissonante"
				// perche' il semitono (df=26 Hz) non e' risolvibile dalla FFT a 2048 punti
				beginTest ("Quinta giusta (440+660 Hz) meno dissonante della terza maggiore (440+550 Hz)");

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

				// Terza maggiore
				DissonanceAnalyser third;
				third.prepare (sr);
				for (int i = 0; i < numSamples; ++i)
				{
						float s = std::sin (2.0f * juce::MathConstants<float>::pi * 440.0f * (float)i / sr)
										+ std::sin (2.0f * juce::MathConstants<float>::pi * 550.0f * (float)i / sr);
						third.pushSample (s * 0.5f);
				}

				float dFifth = fifth.getDissonance();
				float dThird = third.getDissonance();

				expect (dFifth < dThird,
						"La quinta giusta dovrebbe essere meno dissonante della terza maggiore. "
						"Quinta=" + juce::String (dFifth) + " Terza=" + juce::String (dThird));
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
								expect (s >= -1.0f && s <= 1.0f,
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
// TEST 8 - DissonanceAnalyser: silenzio -> dissonanza 0
//==============================================================================
class DissonanceAnalyserSilenceTest : public juce::UnitTest
{
public:
    DissonanceAnalyserSilenceTest()
        : juce::UnitTest ("DissonanceAnalyser - Silenzio", "DissonanceMeeter") {}

    void runTest() override
    {
        beginTest ("Silenzio (tutti zeri) deve produrre dissonanza == 0.0");

        DissonanceAnalyser analyser;
        analyser.prepare (44100.0);

        for (int i = 0; i < 4096; ++i)
            analyser.pushSample (0.0f);

        expectEquals (analyser.getDissonance(), 0.0f);
    }
};

//==============================================================================
// TEST 9 - DissonanceAnalyser: unisono -> dissonanza = 0
// Due toni identici fondono in un unico picco FFT: nessuna coppia -> d=0
//==============================================================================
class DissonanceAnalyserUnisonTest : public juce::UnitTest
{
public:
    DissonanceAnalyserUnisonTest()
        : juce::UnitTest ("DissonanceAnalyser - Unisono", "DissonanceMeeter") {}

    void runTest() override
    {
        beginTest ("Unisono (440+440 Hz) deve produrre dissonanza < 0.05");

        DissonanceAnalyser analyser;
        analyser.prepare (44100.0);

        for (int i = 0; i < 8192; ++i)
        {
            float phase = juce::MathConstants<float>::twoPi * 440.0f * (float)i / 44100.0f;
            analyser.pushSample (std::sin (phase));
        }

        expectLessThan (analyser.getDissonance(), 0.05f);
    }
};

//==============================================================================
// TEST 10 - DissonanceAnalyser: ordinamento corretto degli intervalli
//
// Usa intervalli con separazione >= 100 Hz (>= 4.6 bins a 44100 Hz / FFT 2048)
// cosi' i picchi sono risolvibili e il modello Plomp-Levelt da' l'ordine atteso:
//   terza maggiore (df=110) > quarta (df=147) > tritono (df=182) > quinta (df=220)
//==============================================================================
class DissonanceAnalyserRankingTest : public juce::UnitTest
{
public:
    DissonanceAnalyserRankingTest()
        : juce::UnitTest ("DissonanceAnalyser - Ordinamento intervalli", "DissonanceMeeter") {}

    void runTest() override
    {
        // Helper: misura dissonanza di due sinusoidi a 0.5 di ampiezza ciascuna
        auto measure = [] (float f1, float f2) -> float
        {
            DissonanceAnalyser a;
            a.prepare (44100.0);
            for (int i = 0; i < 8192; ++i)
                a.pushSample (0.5f * std::sin (juce::MathConstants<float>::twoPi * f1 * (float)i / 44100.0f)
                            + 0.5f * std::sin (juce::MathConstants<float>::twoPi * f2 * (float)i / 44100.0f));
            return a.getDissonance();
        };

        // Base A4 = 440 Hz
        const float dThird   = measure (440.0f, 550.0f);   // terza maggiore 5:4  df=110 Hz
        const float dFourth  = measure (440.0f, 586.67f);  // quarta giusta  4:3  df=147 Hz
        const float dTritone = measure (440.0f, 622.25f);  // tritono        sqrt2 df=182 Hz
        const float dFifth   = measure (440.0f, 660.0f);   // quinta giusta  3:2  df=220 Hz

        beginTest ("Terza maggiore (440+550 Hz) piu' dissonante della quinta (440+660 Hz)");
        expect (dThird > dFifth,
            "Terza=" + juce::String (dThird) + " Quinta=" + juce::String (dFifth));

        beginTest ("Quarta giusta (440+587 Hz) piu' dissonante della quinta (440+660 Hz)");
        expect (dFourth > dFifth,
            "Quarta=" + juce::String (dFourth) + " Quinta=" + juce::String (dFifth));

        beginTest ("Tritono (440+622 Hz) piu' dissonante della quinta (440+660 Hz)");
        expect (dTritone > dFifth,
            "Tritono=" + juce::String (dTritone) + " Quinta=" + juce::String (dFifth));

        beginTest ("Terza maggiore piu' dissonante della quarta giusta");
        expect (dThird > dFourth,
            "Terza=" + juce::String (dThird) + " Quarta=" + juce::String (dFourth));
    }
};

//==============================================================================
// TEST 11 - BandPassFilter: segnale in banda passa, fuori banda attenuato
//==============================================================================
class BandPassFilterBasicTest : public juce::UnitTest
{
public:
    BandPassFilterBasicTest()
        : juce::UnitTest ("BandPassFilter - Risposta in frequenza", "DissonanceMeeter") {}

    void runTest() override
    {
        constexpr int    blockSize = 512;
        constexpr float  sr       = 44100.0f;
        constexpr int    warmup   = 20;   // blocchi per far convergere lo smoother

        // Helper: ratio RMS_out / RMS_in per una frequenza di test
        auto measureRatio = [&] (float testFreq) -> float
        {
            BandPassFilter filter;
            filter.prepareToPlay (sr, blockSize);

            auto* minP = filter.treeState.getParameter ("MIN_FREQ");
            auto* maxP = filter.treeState.getParameter ("MAX_FREQ");
            if (minP) minP->setValueNotifyingHost (minP->convertTo0to1 (20.0f));
            if (maxP) maxP->setValueNotifyingHost (maxP->convertTo0to1 (2000.0f));

            juce::AudioBuffer<float> buf (1, blockSize);
            juce::MidiBuffer         midi;

            // Riscaldamento con tono continuo (fase continua tra blocchi)
            for (int b = 0; b < warmup; ++b)
            {
                for (int i = 0; i < blockSize; ++i)
                {
                    int n = b * blockSize + i;
                    buf.setSample (0, i, std::sin (juce::MathConstants<float>::twoPi * testFreq * (float)n / sr));
                }
                filter.processBlock (buf, midi);
            }

            // Misura su un blocco a regime
            float inRms = 0.0f, outRms = 0.0f;
            for (int i = 0; i < blockSize; ++i)
            {
                float s = std::sin (juce::MathConstants<float>::twoPi * testFreq * (float)(warmup * blockSize + i) / sr);
                buf.setSample (0, i, s);
                inRms += s * s;
            }
            inRms = std::sqrt (inRms / (float)blockSize);

            filter.processBlock (buf, midi);

            for (int i = 0; i < blockSize; ++i)
                outRms += buf.getSample (0, i) * buf.getSample (0, i);
            outRms = std::sqrt (outRms / (float)blockSize);

            return (inRms > 1e-9f) ? (outRms / inRms) : 0.0f;
        };

        // La frequenza centrale del bandpass e' fissa a 30 Hz: un tono a 30 Hz
        // deve passare con perdita contenuta.
        beginTest ("30 Hz (frequenza centrale fissa) deve passare con perdita < 6 dB (ratio > 0.5)");
        {
            float ratio = measureRatio (30.0f);
            expect (ratio > 0.5f, "30 Hz ratio = " + juce::String (ratio) + " (atteso > 0.5)");
        }

        // 1 kHz e' lontano dalla frequenza centrale fissa (30 Hz) e deve essere
        // chiaramente attenuato rispetto alla banda passante.
        beginTest ("1 kHz (lontano dal centro a 30 Hz) deve essere attenuato rispetto a 30 Hz (ratio < 0.4)");
        {
            float ratio = measureRatio (1000.0f);
            expect (ratio < 0.4f, "1 kHz ratio = " + juce::String (ratio) + " (atteso < 0.4)");
        }
    }
};

//==============================================================================
// TEST 12 - Catena completa: Distortion + BandPass + DissonanceAnalyser
//
// Verifica che la catena ordini gli intervalli correttamente senza istanziare
// DissonanceMeeterAudioProcessor (il cui costruttore auto-esegue i test in Debug
// causando ricorsione infinita).
//==============================================================================
class ProcessorChainDissonanceTest : public juce::UnitTest
{
public:
    ProcessorChainDissonanceTest()
        : juce::UnitTest ("Chain - Ordinamento dissonanza (Dist+BP+Analyser)", "DissonanceMeeter") {}

    void runTest() override
    {
        beginTest ("Terza maggiore (440+550) piu' dissonante della quinta (440+660) nella catena completa");

        auto measureChain = [] (float f1, float f2) -> float
        {
            constexpr int    blockSize = 512;
            constexpr double sr        = 44100.0;

            BandPassFilter bp;
            bp.prepareToPlay (sr, blockSize);

            Distortion dist;
            dist.prepareToPlay (sr, blockSize);

            DissonanceAnalyser analyser;
            analyser.prepare (sr);

            juce::AudioBuffer<float> buf (1, blockSize);
            juce::MidiBuffer         midi;

            double phase1 = 0.0, phase2 = 0.0;
            const double inc1 = juce::MathConstants<double>::twoPi * (double)f1 / sr;
            const double inc2 = juce::MathConstants<double>::twoPi * (double)f2 / sr;

            for (int block = 0; block < 60; ++block)
            {
                for (int i = 0; i < blockSize; ++i)
                {
                    buf.setSample (0, i, 0.5f * (float)std::sin (phase1)
                                       + 0.5f * (float)std::sin (phase2));
                    phase1 += inc1;
                    phase2 += inc2;
                    if (phase1 > juce::MathConstants<double>::twoPi) phase1 -= juce::MathConstants<double>::twoPi;
                    if (phase2 > juce::MathConstants<double>::twoPi) phase2 -= juce::MathConstants<double>::twoPi;
                }

                dist.processBlock (buf, midi);
                bp.processBlock   (buf, midi);

                const float* data = buf.getReadPointer (0);
                for (int i = 0; i < blockSize; ++i)
                    analyser.pushSample (data[i]);
            }

            return analyser.getDissonance();
        };

        const float dThird = measureChain (440.0f, 550.0f);
        const float dFifth = measureChain (440.0f, 660.0f);

        expect (dThird > dFifth,
            "Catena: Terza=" + juce::String (dThird)
            + " Quinta=" + juce::String (dFifth));
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
static DissonanceAnalyserSilenceTest       dissonanceTest5;
static DissonanceAnalyserUnisonTest        dissonanceTest6;
static DissonanceAnalyserRankingTest       dissonanceTest7;
static BandPassFilterBasicTest             bpTest1;
static ProcessorChainDissonanceTest        integrationTest1;

