/*
	==============================================================================

		This file contains the basic framework code for a JUCE plugin editor.

	==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
/**
*/
class DissonanceMeeterAudioProcessorEditor :
	public juce::AudioProcessorEditor,
	public juce::Timer
#if JucePlugin_Enable_ARA
	, public juce::AudioProcessorEditorARAExtension
#endif
{
public:
	DissonanceMeeterAudioProcessorEditor(DissonanceMeeterAudioProcessor&, BandPassFilter&, Distortion&);
	~DissonanceMeeterAudioProcessorEditor() override;

	//==============================================================================
	void paint(juce::Graphics&) override;
	void resized() override;
	void timerCallback() override;

private:
	class DissonanceLookAndFeel;

	// Draws a meter caption ("OUT", "POST CHAIN", ...) in a labelW-wide box,
	// centred on meterCentreX and clamped within sectionMaster.
	void drawMeterLabel(juce::Graphics& g, const juce::String& text, int meterCentreX, int labelW) const;

	// This reference is provided as a quick way for your editor to
	// access the processor object that created it.
	DissonanceMeeterAudioProcessor& audioProcessor;
	BandPassFilter& bandPassProcessor;
	Distortion& distortionProcessor;
	DissonanceLookAndFeel* customLookAndFeel = nullptr;

	// --- Parametri BandPass ---
	juce::Slider centerFreqSlider; // CENTER_FREQ
	juce::Slider qFactorSlider;    // Q_FACTOR
	juce::Label  centerFreqLabel;
	juce::Label  qFactorLabel;

	std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> centerFreqAttachment;
	std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> qFactorAttachment;

	// --- Parametro Distortion ---
	juce::Slider aSlider;         // A (non-linearità)
	juce::Label  aLabel;
	std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> aAttachment;

	juce::Slider gammaOmegaSlider; // GAMMA_OMEGA (γ = ω)
	juce::Label  gammaOmegaLabel;
	std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gammaOmegaAttachment;

	// --- Oscillatori ---
	juce::Slider oscFreq1Slider;
	juce::Slider oscFreq2Slider;
	juce::Label  osc1Label;
	juce::Label  osc2Label;
	juce::TextButton oscFreq1Minus{ "-" }, oscFreq1Plus{ "+" };
	juce::TextButton oscFreq2Minus{ "-" }, oscFreq2Plus{ "+" };

	// --- Selezione modalità e gain ---
	juce::ComboBox modeSelector;
	juce::Slider   masterGainSlider;
	juce::Label    masterGainLabel;

	// --- Meter smoothing (EMA alpha shared by dissonance, OUT and POST CHAIN meters) ---
	juce::Slider meterSmoothingSlider; // METER_SMOOTHING (alpha)
	juce::Label  meterSmoothingLabel;

	// --- Meter banda (BandPass intensity) ---
	// Il meter principale (output RMS) è disegnato in paint()
	// I meter POST CHAIN e PRE DIST sono disegnati separatamente a destra
	int meterX = 0, meterY = 0, meterW = 18, meterH = 200;
	int bandMeterX = 0;
	int preDistMeterX = 0;
	juce::Rectangle<int> sectionFreq;
	juce::Rectangle<int> sectionOsc;
	juce::Rectangle<int> sectionMaster;
	juce::Rectangle<int> sectionViz;
	juce::Rectangle<int> dissBarBounds;

	static constexpr float meterMinDb = -60.0f;
	static constexpr float meterMaxDb = 0.0f;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DissonanceMeeterAudioProcessorEditor)
};
