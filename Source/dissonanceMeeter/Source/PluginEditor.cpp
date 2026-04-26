/*
	==============================================================================

		This file contains the basic framework code for a JUCE plugin editor.

	==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
DissonanceMeeterAudioProcessorEditor::DissonanceMeeterAudioProcessorEditor(
	DissonanceMeeterAudioProcessor& p, BandPassFilter& bp, Distortion& d)
	: AudioProcessorEditor(&p),
#if JucePlugin_Enable_ARA
	AudioProcessorEditorARAExtension(&p),
#endif
	audioProcessor(p),
	bandPassProcessor(bp),
	distortionProcessor(d)
{
#if JucePlugin_Enable_ARA
	// ARA plugins must be resizable for proper view embedding
	setResizable(true, false);
#endif

	// --- Slider BandPass con attachment ---
	minFreqAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(
		bandPassProcessor.treeState, "MIN_FREQ", minFreqSlider);
	maxFreqAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(
		bandPassProcessor.treeState, "MAX_FREQ", maxFreqSlider);

	for (auto* s : { &minFreqSlider, &maxFreqSlider })
	{
		s->setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
		s->setRotaryParameters(MathConstants<float>::pi * 1.2f,
			MathConstants<float>::pi * 2.8f, true);
		s->setTextBoxStyle(Slider::TextBoxBelow, false, 80, 20);
		s->setSkewFactorFromMidPoint(1000.0);
	}
	minFreqSlider.setRange(20.0, 10000.0, 1.0);
	maxFreqSlider.setRange(20.0, 20000.0, 1.0);

	// --- Slider Distortion con attachment ---
	aAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(
		distortionProcessor.treeState, "A", aSlider);
	aSlider.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
	aSlider.setRotaryParameters(MathConstants<float>::pi * 1.2f,
		MathConstants<float>::pi * 2.8f, true);
	aSlider.setTextBoxStyle(Slider::TextBoxBelow, false, 80, 20);
	aSlider.setRange(0.1, 5.0, 0.01);

	// --- Oscillatori ---
	for (auto* s : { &oscFreq1Slider, &oscFreq2Slider })
	{
		s->setSliderStyle(Slider::LinearHorizontal);
		s->setTextBoxStyle(Slider::TextBoxRight, false, 80, 20);
		s->setRange(20.0, 20000.0, 1.0);
		s->setSkewFactorFromMidPoint(440.0);
	}
	auto [f1, f2] = audioProcessor.getOscillatorFrequencies();
	oscFreq1Slider.setValue(f1, dontSendNotification);
	oscFreq2Slider.setValue(f2, dontSendNotification);

	oscFreq1Slider.onValueChange = [this] {
		if (audioProcessor.getInputMode() == DissonanceMeeterAudioProcessor::InputMode::Oscillator)
			audioProcessor.setOscillatorFrequencies((float)oscFreq1Slider.getValue(),
				audioProcessor.getOscillatorFrequencies().second);
		};
	oscFreq2Slider.onValueChange = [this] {
		if (audioProcessor.getInputMode() == DissonanceMeeterAudioProcessor::InputMode::Oscillator)
			audioProcessor.setOscillatorFrequencies(audioProcessor.getOscillatorFrequencies().first,
				(float)oscFreq2Slider.getValue());
		};
	oscFreq1Minus.onClick = [this] { oscFreq1Slider.setValue(oscFreq1Slider.getValue() - 1.0, sendNotification); };
	oscFreq1Plus.onClick = [this] { oscFreq1Slider.setValue(oscFreq1Slider.getValue() + 1.0, sendNotification); };
	oscFreq2Minus.onClick = [this] { oscFreq2Slider.setValue(oscFreq2Slider.getValue() - 1.0, sendNotification); };
	oscFreq2Plus.onClick = [this] { oscFreq2Slider.setValue(oscFreq2Slider.getValue() + 1.0, sendNotification); };

	// --- Master gain ---
	masterGainSlider.setSliderStyle(Slider::LinearHorizontal);
	masterGainSlider.setTextBoxStyle(Slider::TextBoxRight, false, 80, 20);
	masterGainSlider.setRange(0.0, 4.0, 0.01);
	masterGainSlider.setValue(audioProcessor.getOutputGain(), dontSendNotification);
	masterGainSlider.onValueChange = [this] {
		audioProcessor.setOutputGain((float)masterGainSlider.getValue());
		};

	// --- Mode selector ---
	modeSelector.addItem("External Input", 1);
	modeSelector.addItem("Internal Oscillator", 2);
	modeSelector.setSelectedId(1);
	modeSelector.onChange = [this] {
		audioProcessor.setInputMode(
			modeSelector.getSelectedId() == 1
			? DissonanceMeeterAudioProcessor::InputMode::ExternalInput
			: DissonanceMeeterAudioProcessor::InputMode::Oscillator);
		};

	// --- Label setup ---
	auto setupLabel = [](Label& l, const String& text) {
		l.setText(text, dontSendNotification);
		l.setColour(Label::textColourId, Colours::white);
		l.setJustificationType(Justification::centred);
		l.setInterceptsMouseClicks(false, false);
		};
	setupLabel(minFreqLabel, "Min Freq");
	setupLabel(maxFreqLabel, "Max Freq");
	setupLabel(aLabel, "A (Non-lin.)");
	setupLabel(osc1Label, "Osc 1 (Hz)");
	setupLabel(osc2Label, "Osc 2 (Hz)");
	setupLabel(masterGainLabel, "Master Gain");

	// --- Waveform ---
	audioProcessor.waveForm.setColours(Colours::black, Colours::white);

	// --- AddAndMakeVisible ---
	addAndMakeVisible(modeSelector);
	addAndMakeVisible(minFreqSlider);  addAndMakeVisible(minFreqLabel);
	addAndMakeVisible(maxFreqSlider);  addAndMakeVisible(maxFreqLabel);
	addAndMakeVisible(aSlider);        addAndMakeVisible(aLabel);
	addAndMakeVisible(oscFreq1Slider); addAndMakeVisible(osc1Label);
	addAndMakeVisible(oscFreq2Slider); addAndMakeVisible(osc2Label);
	addAndMakeVisible(oscFreq1Minus);  addAndMakeVisible(oscFreq1Plus);
	addAndMakeVisible(oscFreq2Minus);  addAndMakeVisible(oscFreq2Plus);
	addAndMakeVisible(masterGainSlider);
	addAndMakeVisible(masterGainLabel);
	addAndMakeVisible(audioProcessor.waveForm);

	setSize(700, 460);
	setOpaque(true);
	startTimerHz(30);
}

DissonanceMeeterAudioProcessorEditor::~DissonanceMeeterAudioProcessorEditor()
{}

//==============================================================================
void DissonanceMeeterAudioProcessorEditor::paint(juce::Graphics& g)
{
	// Sfondo
	g.setGradientFill(ColourGradient{ Colours::darkgrey,
																			getLocalBounds().toFloat().getCentre(),
																			Colours::darkgrey.darker(0.7f), {}, true });
	g.fillRect(getLocalBounds());

	const int pad = 16;

	// ── Meter output principale (sinistra) ──────────────────────────────────
	{
		Rectangle<int> bg{ meterX, meterY, meterW, meterH };
		g.setColour(Colours::darkgrey.withAlpha(0.8f));
		g.fillRect(bg);
		g.setColour(Colours::grey);
		g.drawRect(bg, 1);

		const float db = jlimit(meterMinDb, meterMaxDb, audioProcessor.getOutputLevelRms());
		const float norm = (db - meterMinDb) / (meterMaxDb - meterMinDb);
		const int   fillH = (int)std::round(norm * (float)meterH);
		Rectangle<int> fill{ meterX + 1, meterY + meterH - fillH + 1, meterW - 2, fillH - 2 };

		ColourGradient grad(Colours::green, fill.getBottomLeft().toFloat(),
			Colours::red, fill.getTopLeft().toFloat(), false);
		grad.addColour(0.5, Colours::yellow);
		g.setGradientFill(grad);
		g.fillRect(fill);

		// Etichetta
		g.setColour(Colours::white);
		g.setFont(10.0f);
		g.drawText("OUT", meterX, meterY + meterH + 2, meterW, 14, Justification::centred);
	}

	// ── Meter intensità banda BandPass (accanto al primo) ───────────────────
	{
		Rectangle<int> bg{ bandMeterX, meterY, meterW, meterH };
		g.setColour(Colours::darkgrey.withAlpha(0.8f));
		g.fillRect(bg);
		g.setColour(Colours::grey);
		g.drawRect(bg, 1);

		// Leggi l'intensità direttamente dal nodo BandPass
		const float db = jlimit(meterMinDb, meterMaxDb, bandPassProcessor.getBandIntensityDb());
		const float norm = (db - meterMinDb) / (meterMaxDb - meterMinDb);
		const int   fillH = (int)std::round(norm * (float)meterH);
		Rectangle<int> fill{ bandMeterX + 1, meterY + meterH - fillH + 1, meterW - 2, fillH - 2 };

		// Colore cianoide per distinguerlo dal meter output
		ColourGradient grad(Colours::cyan.darker(), fill.getBottomLeft().toFloat(),
			Colours::cyan, fill.getTopLeft().toFloat(), false);
		g.setGradientFill(grad);
		g.fillRect(fill);

		g.setColour(Colours::white);
		g.setFont(10.0f);
		g.drawText("BAND", bandMeterX, meterY + meterH + 2, meterW, 14, Justification::centred);
	}

	// ── Tick marks dB (condivisi per entrambi i meter) ──────────────────────
	g.setColour(Colours::lightgrey);
	g.setFont(9.0f);
	for (float db : { 0.0f, -6.0f, -12.0f, -24.0f, -48.0f })
	{
		const float norm = (db - meterMinDb) / (meterMaxDb - meterMinDb);
		const int   y = meterY + meterH - (int)std::round(norm * (float)meterH);
		g.drawHorizontalLine(y, (float)meterX, (float)(bandMeterX + meterW));
		g.drawText(String((int)db) + " dB",
			bandMeterX + meterW + 2, y - 6, 40, 12, Justification::centredLeft);
	}
}

void DissonanceMeeterAudioProcessorEditor::resized()
{
	const int pad = 16;
	const int meterColW = meterW * 2 + pad + 44; // 2 meter + tick labels
	const int contentLeft = pad + meterColW + pad;
	const int labelH = 18;
	const int knobSize = 110;
	const int rightPanelW = 220;
	const int rightPanelX = getWidth() - pad - rightPanelW;

	// Geometria meter
	meterX = pad;
	meterY = pad;
	meterH = getHeight() / 2 - pad;
	bandMeterX = pad + meterW + pad;

	// Riga 1: tre knob (Min Freq, Max Freq, A)
	const int row1Y = pad + 20;
	minFreqSlider.setBounds(contentLeft, row1Y, knobSize, knobSize);
	maxFreqSlider.setBounds(contentLeft + knobSize + pad, row1Y, knobSize, knobSize);
	aSlider.setBounds(contentLeft + 2 * (knobSize + pad), row1Y, knobSize, knobSize);

	minFreqLabel.setBounds(minFreqSlider.getX(), row1Y - labelH - 4, knobSize, labelH);
	maxFreqLabel.setBounds(maxFreqSlider.getX(), row1Y - labelH - 4, knobSize, labelH);
	aLabel.setBounds(aSlider.getX(), row1Y - labelH - 4, knobSize, labelH);

	// Pannello destro: modalità e gain
	modeSelector.setBounds(rightPanelX, row1Y - 24, rightPanelW, 28);
	masterGainLabel.setBounds(rightPanelX, row1Y + 20, rightPanelW, 20);
	masterGainSlider.setBounds(rightPanelX, row1Y + 42, rightPanelW, 22);

	// Riga 2 e 3: oscillatori
	const int row2Y = row1Y + knobSize + 36;
	const int labelW = 120;
	const int btnW = 24;
	const int sliderX = contentLeft + labelW + pad;
	const int sliderRight = getWidth() - pad;
	const int sliderW = jmax(120, sliderRight - sliderX - (btnW * 2 + pad + 6));

	osc1Label.setBounds(contentLeft, row2Y - 2, labelW, labelH);
	oscFreq1Slider.setBounds(sliderX, row2Y, sliderW, 24);
	oscFreq1Minus.setBounds(sliderRight - (btnW * 2 + 4), row2Y, btnW, 24);
	oscFreq1Plus.setBounds(sliderRight - btnW, row2Y, btnW, 24);

	const int row3Y = row2Y + 30 + pad;
	osc2Label.setBounds(contentLeft, row3Y - 2, labelW, labelH);
	oscFreq2Slider.setBounds(sliderX, row3Y, sliderW, 24);
	oscFreq2Minus.setBounds(sliderRight - (btnW * 2 + 4), row3Y, btnW, 24);
	oscFreq2Plus.setBounds(sliderRight - btnW, row3Y, btnW, 24);

	// Waveform in basso
	const int waveTop = row3Y + 24 + 2 * pad;
	audioProcessor.waveForm.setBounds(contentLeft, waveTop,
		getWidth() - contentLeft - pad,
		getHeight() - waveTop - pad);

}

void DissonanceMeeterAudioProcessorEditor::timerCallback()
{
	repaint();
}