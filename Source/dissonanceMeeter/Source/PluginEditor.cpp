/*
	==============================================================================

		This file contains the basic framework code for a JUCE plugin editor.

	==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
	struct UiTheme
	{
		static constexpr int pad = 14;
		static constexpr int radius = 10;
		static constexpr int titleH = 20;
		static constexpr int labelH = 16;
		static constexpr int controlH = 26;
		static constexpr int knobSize = 110;
		static constexpr int smallBtn = 20;
		static constexpr int meterW = 20;
		static constexpr int meterLabelW = 42;
		static constexpr int oscLabelW = 110;
		static constexpr int headerH = 36;

		static const juce::Colour background;
		static const juce::Colour panel;
		static const juce::Colour panelAlt;
		static const juce::Colour accent;
		static const juce::Colour accentBlue;
		static const juce::Colour text;
		static const juce::Colour textDim;
		static const juce::Colour warning;
		static const juce::Colour grid;
	};
}

const juce::Colour UiTheme::background{ 0xFF141918 };
const juce::Colour UiTheme::panel{ 0xFF1C2622 };
const juce::Colour UiTheme::panelAlt{ 0xFF162020 };
const juce::Colour UiTheme::accent{ 0xFF2EC9A0 };
const juce::Colour UiTheme::accentBlue{ 0xFF25A882 };
const juce::Colour UiTheme::text{ 0xFFE0EBE6 };
const juce::Colour UiTheme::textDim{ 0xFF5E8070 };
const juce::Colour UiTheme::warning{ 0xFFFF7A40 };
const juce::Colour UiTheme::grid{ 0xFF222C2A };

class DissonanceMeeterAudioProcessorEditor::DissonanceLookAndFeel : public juce::LookAndFeel_V4
{
public:
	DissonanceLookAndFeel()
	{
		setColour(juce::Slider::thumbColourId, UiTheme::accent);
		setColour(juce::Slider::trackColourId, UiTheme::accentBlue);
		setColour(juce::Slider::rotarySliderOutlineColourId, UiTheme::grid);
		setColour(juce::Slider::rotarySliderFillColourId, UiTheme::accentBlue);
		setColour(juce::Slider::textBoxTextColourId, UiTheme::text);
		setColour(juce::Slider::textBoxBackgroundColourId, UiTheme::panelAlt);
		setColour(juce::Slider::textBoxOutlineColourId, UiTheme::grid);
		setColour(juce::TextEditor::textColourId, UiTheme::text);
		setColour(juce::TextEditor::backgroundColourId, UiTheme::panel);
		setColour(juce::Label::textColourId, UiTheme::text);
		setColour(juce::Label::outlineColourId, UiTheme::grid);
		setColour(juce::Label::backgroundColourId, UiTheme::panelAlt);
		setColour(juce::ComboBox::textColourId, UiTheme::text);
		setColour(juce::ComboBox::backgroundColourId, UiTheme::panelAlt);
		setColour(juce::ComboBox::outlineColourId, UiTheme::grid);
		setColour(juce::TextButton::buttonColourId, UiTheme::panelAlt);
		setColour(juce::TextButton::buttonOnColourId, UiTheme::accentBlue);
		setColour(juce::TextButton::textColourOffId, UiTheme::text);
		setColour(juce::TextButton::textColourOnId, UiTheme::text);
	}

	void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
		float rotaryStartAngle, float rotaryEndAngle, juce::Slider& slider) override
	{
		const float size = (float)juce::jmin(width, height);
		const float radius = size * 0.42f;
		const float cx = (float)x + (float)width * 0.5f;
		const float cy = (float)y + (float)height * 0.5f;
		const float arcThickness = juce::jmax(3.0f, size * 0.06f);
		const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

		const float bgRadius = radius - arcThickness * 0.5f - 3.0f;
		g.setColour(UiTheme::background);
		g.fillEllipse(cx - bgRadius, cy - bgRadius, bgRadius * 2.0f, bgRadius * 2.0f);

		juce::Path backgroundArc;
		backgroundArc.addCentredArc(cx, cy, radius, radius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
		g.setColour(UiTheme::grid);
		g.strokePath(backgroundArc, juce::PathStrokeType(arcThickness));

		juce::Path valueArc;
		valueArc.addCentredArc(cx, cy, radius, radius, 0.0f, rotaryStartAngle, angle, true);
		g.setColour(slider.isMouseOverOrDragging() ? UiTheme::accent : UiTheme::accentBlue);
		g.strokePath(valueArc, juce::PathStrokeType(arcThickness));

		const float thumbDot = juce::jmax(15.0f, size * 0.075f);
		const float thumbRadius = radius;
		const float tx = cx + thumbRadius * std::sin(angle);
		const float ty = cy - thumbRadius * std::cos(angle);
		juce::Path thumb;
		thumb.addEllipse(tx - thumbDot * 0.5f, ty - thumbDot * 0.5f, thumbDot, thumbDot);
		g.setColour(UiTheme::text);
		g.fillPath(thumb);
	}

	void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
		float, float, const juce::Slider::SliderStyle, juce::Slider& slider) override
	{
		juce::Rectangle<float> track((float)x, (float)y + (float)height * 0.45f, (float)width, 4.0f);
		g.setColour(UiTheme::grid);
		g.fillRoundedRectangle(track, 2.0f);

		juce::Rectangle<float> valueTrack(track.getX(), track.getY(), sliderPos - track.getX(), track.getHeight());
		g.setColour(slider.isMouseOverOrDragging() ? UiTheme::accent : UiTheme::accentBlue);
		g.fillRoundedRectangle(valueTrack, 2.0f);

		const float thumbW = 10.0f;
		const float thumbH = (float)height * 0.65f;
		juce::Rectangle<float> thumb(sliderPos - thumbW * 0.5f, (float)y + ((float)height - thumbH) * 0.5f, thumbW, thumbH);
		g.setColour(UiTheme::text);
		g.fillRoundedRectangle(thumb, 3.0f);
	}

	void drawButtonBackground(juce::Graphics& g, juce::Button& button,
		const juce::Colour& backgroundColour, bool isMouseOverButton, bool isButtonDown) override
	{
		auto bounds = button.getLocalBounds().toFloat();
		auto colour = backgroundColour;
		if (isButtonDown)
			colour = UiTheme::accentBlue;
		else if (isMouseOverButton)
			colour = UiTheme::accent;
		g.setColour(colour);
		g.fillRoundedRectangle(bounds, 6.0f);
		g.setColour(UiTheme::grid);
		g.drawRoundedRectangle(bounds, 6.0f, 1.0f);
	}
};

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
	setResizable(true, false);
#endif

	customLookAndFeel = new DissonanceLookAndFeel();
	setLookAndFeel(customLookAndFeel);

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
		s->setTextBoxStyle(Slider::TextBoxBelow, false, 64, 18);
		s->setColour(Slider::textBoxBackgroundColourId, UiTheme::panelAlt);
		s->setColour(Slider::textBoxOutlineColourId, UiTheme::grid);
		s->setColour(Slider::textBoxTextColourId, UiTheme::text);
	}

	// --- Slider Distortion con attachment ---
	aAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(
		distortionProcessor.treeState, "A", aSlider);
	aSlider.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
	aSlider.setRotaryParameters(MathConstants<float>::pi * 1.2f,
		MathConstants<float>::pi * 2.8f, true);
	aSlider.setTextBoxStyle(Slider::TextBoxBelow, false, 64, 18);
	aSlider.setColour(Slider::textBoxBackgroundColourId, UiTheme::panelAlt);
	aSlider.setColour(Slider::textBoxOutlineColourId, UiTheme::grid);
	aSlider.setColour(Slider::textBoxTextColourId, UiTheme::text);

	// --- Oscillatori ---
	for (auto* s : { &oscFreq1Slider, &oscFreq2Slider })
	{
		s->setSliderStyle(Slider::LinearHorizontal);
		s->setTextBoxStyle(Slider::TextBoxRight, false, 64, 18);
		s->setRange(20.0, 20000.0, 1.0);
		s->setSkewFactorFromMidPoint(440.0);
		s->setColour(Slider::textBoxBackgroundColourId, UiTheme::panelAlt);
		s->setColour(Slider::textBoxOutlineColourId, UiTheme::grid);
		s->setColour(Slider::textBoxTextColourId, UiTheme::text);
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
	masterGainSlider.setTextBoxStyle(Slider::TextBoxRight, false, 64, 18);
	masterGainSlider.setRange(0.0, 4.0, 0.01);
	masterGainSlider.setValue(audioProcessor.getOutputGain(), dontSendNotification);
	masterGainSlider.onValueChange = [this] {
		audioProcessor.setOutputGain((float)masterGainSlider.getValue());
		};
	masterGainSlider.setColour(Slider::textBoxBackgroundColourId, UiTheme::panelAlt);
	masterGainSlider.setColour(Slider::textBoxOutlineColourId, UiTheme::grid);
	masterGainSlider.setColour(Slider::textBoxTextColourId, UiTheme::text);

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
	auto setupLabel = [](Label& l, const String& text, Justification just) {
		l.setText(text, dontSendNotification);
		l.setColour(Label::textColourId, UiTheme::text);
		l.setJustificationType(just);
		l.setInterceptsMouseClicks(false, false);
		};
	setupLabel(minFreqLabel, "FREQ MIN", Justification::centred);
	setupLabel(maxFreqLabel, "FREQ MAX", Justification::centred);
	setupLabel(aLabel, "NONLINEARITY", Justification::centred);
	setupLabel(osc1Label, "OSC 1 (Hz)", Justification::centredLeft);
	setupLabel(osc2Label, "OSC 2 (Hz)", Justification::centredLeft);
	setupLabel(masterGainLabel, "MASTER GAIN", Justification::centredLeft);

	// --- Waveform ---
	audioProcessor.getWaveForm().setColours(UiTheme::panel, UiTheme::accent);

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
	addAndMakeVisible(audioProcessor.getWaveForm());

	setSize(900, 580);
	setResizable(true, true);
	setResizeLimits(820, 560, 30000, 30000);
	setOpaque(true);
	startTimerHz(30);
}

DissonanceMeeterAudioProcessorEditor::~DissonanceMeeterAudioProcessorEditor()
{
	setLookAndFeel(nullptr);
	delete customLookAndFeel;
}

//==============================================================================
void DissonanceMeeterAudioProcessorEditor::paint(juce::Graphics& g)
{
	g.fillAll(UiTheme::background);

	// Header
	auto header = getLocalBounds().removeFromTop(UiTheme::headerH).reduced(UiTheme::pad, 6);
	g.setColour(UiTheme::text);
	g.setFont(juce::Font(juce::FontOptions().withHeight(16.0f).withStyle("Bold")));
	g.drawText("dissonanceMeeter", header, juce::Justification::centredLeft);
	g.setFont(juce::Font(juce::FontOptions().withHeight(12.0f)));
	g.setColour(UiTheme::textDim);
	g.drawText(audioProcessor.getInputMode() == DissonanceMeeterAudioProcessor::InputMode::ExternalInput
		? "External Input" : "Internal Oscillator",
		header.removeFromRight(170), juce::Justification::centredRight);

	auto drawCard = [&g](juce::Rectangle<int> area)
		{
			g.setColour(UiTheme::panel);
			g.fillRoundedRectangle(area.toFloat(), (float)UiTheme::radius);
			g.setColour(UiTheme::grid);
			g.drawRoundedRectangle(area.toFloat(), (float)UiTheme::radius, 1.0f);
		};

	for (auto area : { sectionMaster, sectionFreq, sectionOsc, sectionViz })
		drawCard(area);

	// Section titles — use LOCAL copies so stored rects are never mutated
	g.setFont(juce::Font(juce::FontOptions().withHeight(12.0f).withStyle("Bold")));
	g.setColour(UiTheme::textDim);
	{
		auto r = sectionMaster;
		g.drawText("MASTER / METER", r.removeFromTop(UiTheme::titleH).reduced(UiTheme::pad, 0), juce::Justification::centredLeft);
	}
	{
		auto r = sectionFreq;
		g.drawText("PARAMETERS", r.removeFromTop(UiTheme::titleH).reduced(UiTheme::pad, 0), juce::Justification::centredLeft);
	}
	{
		auto r = sectionOsc;
		g.drawText("OSCILLATORS", r.removeFromTop(UiTheme::titleH).reduced(UiTheme::pad, 0), juce::Justification::centredLeft);
	}
	{
		auto r = sectionViz;
		g.drawText("VISUALIZATION", r.removeFromTop(UiTheme::titleH).reduced(UiTheme::pad, 0), juce::Justification::centredLeft);
	}

	// Dissonance bar
	{
		const float diss = audioProcessor.getDissonance();
		g.setColour(UiTheme::textDim);
		g.setFont(juce::Font(juce::FontOptions().withHeight(10.0f).withStyle("Bold")));
		g.drawText("DISSONANCE", dissBarBounds.withY(dissBarBounds.getY() - 14).withHeight(14),
			juce::Justification::centredLeft);

		g.setColour(UiTheme::panelAlt);
		g.fillRoundedRectangle(dissBarBounds.toFloat(), 4.0f);

		if (diss > 0.001f)
		{
			auto fill = dissBarBounds.withWidth(juce::roundToInt(dissBarBounds.getWidth() * diss));
			juce::ColourGradient grad(UiTheme::accentBlue, fill.getTopLeft().toFloat(),
				UiTheme::warning, fill.getTopRight().toFloat(), false);
			g.setGradientFill(grad);
			g.fillRoundedRectangle(fill.toFloat(), 4.0f);
		}

		g.setColour(UiTheme::grid);
		g.drawRoundedRectangle(dissBarBounds.toFloat(), 4.0f, 1.0f);

		g.setColour(UiTheme::text);
		g.setFont(10.0f);
		g.drawText(juce::String(diss, 2), dissBarBounds, juce::Justification::centred);
	}

	// OUT meter
	{
		const float db = jlimit(meterMinDb, meterMaxDb, audioProcessor.getOutputLevelRms());
		const float norm = (db - meterMinDb) / (meterMaxDb - meterMinDb);
		const int fillH = (int)std::round(norm * (float)meterH);
		juce::Rectangle<int> bg{ meterX, meterY, meterW, meterH };
		g.setColour(UiTheme::panelAlt);
		g.fillRoundedRectangle(bg.toFloat(), 4.0f);

		if (fillH > 2)
		{
			juce::Rectangle<int> fill{ meterX + 1, meterY + meterH - fillH + 1, meterW - 2, fillH - 2 };
			juce::ColourGradient grad(UiTheme::accentBlue, fill.getBottomLeft().toFloat(),
				UiTheme::warning, fill.getTopLeft().toFloat(), false);
			g.setGradientFill(grad);
			g.fillRoundedRectangle(fill.toFloat(), 3.0f);
		}

		g.setColour(UiTheme::textDim);
		g.setFont(10.0f);
		g.drawText("OUT", meterX - 2, meterY + meterH + 2, meterW + 4, 12, juce::Justification::centred);
	}

	// BAND meter
	{
		const float db = jlimit(meterMinDb, meterMaxDb, bandPassProcessor.getBandIntensityDb());
		const float norm = (db - meterMinDb) / (meterMaxDb - meterMinDb);
		const int fillH = (int)std::round(norm * (float)meterH);
		juce::Rectangle<int> bg{ bandMeterX, meterY, meterW, meterH };
		g.setColour(UiTheme::panelAlt);
		g.fillRoundedRectangle(bg.toFloat(), 4.0f);

		if (fillH > 2)
		{
			juce::Rectangle<int> fill{ bandMeterX + 1, meterY + meterH - fillH + 1, meterW - 2, fillH - 2 };
			juce::ColourGradient grad(UiTheme::accent, fill.getBottomLeft().toFloat(),
				UiTheme::accentBlue, fill.getTopLeft().toFloat(), false);
			g.setGradientFill(grad);
			g.fillRoundedRectangle(fill.toFloat(), 3.0f);
		}

		g.setColour(UiTheme::textDim);
		g.setFont(10.0f);
		g.drawText("BAND", bandMeterX - 6, meterY + meterH + 2, meterW + 12, 12, juce::Justification::centred);
	}

	// dB tick marks
	g.setColour(UiTheme::textDim);
	g.setFont(9.0f);
	for (float db : { 0.0f, -6.0f, -12.0f, -24.0f, -48.0f })
	{
		const float norm = (db - meterMinDb) / (meterMaxDb - meterMinDb);
		const int y = meterY + meterH - (int)std::round(norm * (float)meterH);
		g.drawHorizontalLine(y, (float)meterX, (float)(bandMeterX + meterW));
		g.drawText(String((int)db), bandMeterX + meterW + 4, y - 6, UiTheme::meterLabelW, 12,
			juce::Justification::centredLeft);
	}
}

void DissonanceMeeterAudioProcessorEditor::resized()
{
	const int pad = UiTheme::pad;
	const int titleH = UiTheme::titleH;
	const int labelH = UiTheme::labelH;
	const int knobSize = UiTheme::knobSize;
	const int btnW = UiTheme::smallBtn;

	auto bounds = getLocalBounds();
	bounds.removeFromTop(UiTheme::headerH);
	auto contentArea = bounds.reduced(pad);

	// Left column wide enough for: pad+meterW+pad+meterW+4+meterLabelW+pad
	// and comfortable controls (gain slider, mode selector).
	const int leftColW = 200;
	const int rightW = contentArea.getWidth() - leftColW - pad;
	const int rightX = contentArea.getX() + leftColW + pad;

	// Vertical: viz at the bottom (bigger), params+osc above it.
	// 3/8 of content height goes to waveform, capped at 200 px.
	const int vizH = juce::jmin(200, contentArea.getHeight() * 3 / 8);
	const int nonVizH = contentArea.getHeight() - vizH - pad;

	// PARAMETERS section height driven by its content: title clearance + label + knob.
	const int titleClear = juce::jmax(0, titleH - pad);  // = 6
	const int paramsH = 2 * pad + titleClear + labelH + 4 + knobSize;
	// Osc section gets whatever's left; clamp to a sane minimum.
	const int oscH = juce::jmax(80, nonVizH - paramsH - pad);

	// Section rectangles — stored, must NOT be mutated inside paint()
	sectionMaster = juce::Rectangle<int>(contentArea.getX(), contentArea.getY(),
		leftColW, contentArea.getHeight());

	// sectionFreq is now the merged PARAMETERS card (freq min/max + nonlinearity A)
	sectionFreq = juce::Rectangle<int>(rightX, contentArea.getY(), rightW, paramsH);
	sectionNonlin = {};   // no longer a separate card

	sectionOsc = juce::Rectangle<int>(rightX, sectionFreq.getBottom() + pad, rightW, oscH);

	sectionViz = juce::Rectangle<int>(rightX, sectionOsc.getBottom() + pad, rightW, vizH);

	// ---- Master section ----
	{
		auto inner = sectionMaster.reduced(pad);
		int y = inner.getY() + titleH;

		// Mode selector — full width, taller for readability
		modeSelector.setBounds(inner.getX(), y, inner.getWidth(), 30);
		y += 36;

		// Gain
		masterGainLabel.setBounds(inner.getX(), y, inner.getWidth(), labelH);
		y += labelH + 2;
		masterGainSlider.setBounds(inner.getX(), y, inner.getWidth(), 30);
		y += 36;

		// Dissonance bar — label drawn 14 px above it inside paint()
		dissBarBounds = juce::Rectangle<int>(inner.getX(), y + 14, inner.getWidth(), 20);
		y += 14 + 20 + pad;

		// Vertical meters fill whatever remains
		meterX = inner.getX();
		meterY = y;
		meterW = UiTheme::meterW;
		bandMeterX = meterX + meterW + pad;
		// Reserve 14 px below meters for the "OUT" / "BAND" labels
		meterH = juce::jmax(20, sectionMaster.getBottom() - pad - 14 - meterY);
	}

	// ---- PARAMETERS section (min freq, max freq, nonlinearity A — all in one row) ----
	{
		auto inner = sectionFreq.reduced(pad);
		const int labelY = inner.getY() + titleClear;
		const int sliderY = labelY + labelH + 4;

		minFreqLabel.setBounds(inner.getX(), labelY, knobSize, labelH);
		maxFreqLabel.setBounds(inner.getX() + (knobSize + pad), labelY, knobSize, labelH);
		aLabel.setBounds(inner.getX() + (knobSize + pad) * 2, labelY, knobSize, labelH);

		minFreqSlider.setBounds(inner.getX(), sliderY, knobSize, knobSize);
		maxFreqSlider.setBounds(inner.getX() + (knobSize + pad), sliderY, knobSize, knobSize);
		aSlider.setBounds(inner.getX() + (knobSize + pad) * 2, sliderY, knobSize, knobSize);
	}

	// ---- OSCILLATORS section (below parameters, full right width) ----
	{
		auto inner = sectionOsc.reduced(pad);
		const int oscBtnW  = 28;   // wider than generic smallBtn so "+" text is not clipped
		const int btnGap   = 6;    // gap between slider textbox and the minus button
		const int oscSliderW = inner.getWidth() - UiTheme::oscLabelW - oscBtnW * 2 - pad - btnGap;
		const int rowGap = 10;
		const int row1Y = inner.getY() + titleH + 4;

		osc1Label.setBounds(inner.getX(), row1Y, UiTheme::oscLabelW, labelH);
		oscFreq1Slider.setBounds(inner.getX() + UiTheme::oscLabelW, row1Y, oscSliderW, 24);
		oscFreq1Minus.setBounds(inner.getRight() - oscBtnW * 2 - pad, row1Y, oscBtnW, 24);
		oscFreq1Plus.setBounds(inner.getRight() - oscBtnW, row1Y, oscBtnW, 24);

		const int row2Y = row1Y + 24 + rowGap;
		osc2Label.setBounds(inner.getX(), row2Y, UiTheme::oscLabelW, labelH);
		oscFreq2Slider.setBounds(inner.getX() + UiTheme::oscLabelW, row2Y, oscSliderW, 24);
		oscFreq2Minus.setBounds(inner.getRight() - oscBtnW * 2 - pad, row2Y, oscBtnW, 24);
		oscFreq2Plus.setBounds(inner.getRight() - oscBtnW, row2Y, oscBtnW, 24);
	}

	// ---- Waveform: below the title strip in the viz card ----
	{
		auto vizInner = sectionViz.reduced(pad);
		vizInner.removeFromTop(titleH);
		audioProcessor.getWaveForm().setBounds(vizInner);
	}
}
void DissonanceMeeterAudioProcessorEditor::timerCallback()
{
	repaint();
}