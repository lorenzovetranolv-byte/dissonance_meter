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

	// This reference is provided as a quick way for your editor to
	// access the processor object that created it.
	DissonanceMeeterAudioProcessor& audioProcessor;
	BandPassFilter& bandPassProcessor;
	Distortion& distortionProcessor;
	DissonanceLookAndFeel* customLookAndFeel = nullptr;

	// --- Parametri BandPass ---
	juce::Slider minFreqSlider;   // MIN_FREQ
	juce::Slider maxFreqSlider;   // MAX_FREQ
	juce::Label  minFreqLabel;
	juce::Label  maxFreqLabel;

	std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> minFreqAttachment;
	std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> maxFreqAttachment;

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

	// --- Meter banda (BandPass intensity) ---
	// Il meter principale (output RMS) è disegnato in paint()
	// Il meter della banda è disegnato separatamente a destra
	int meterX = 0, meterY = 0, meterW = 18, meterH = 200;
	int bandMeterX = 0;
	juce::Rectangle<int> sectionFreq;
	juce::Rectangle<int> sectionNonlin;
	juce::Rectangle<int> sectionOsc;
	juce::Rectangle<int> sectionMaster;
	juce::Rectangle<int> sectionViz;
	juce::Rectangle<int> dissBarBounds;

	static constexpr float meterMinDb = -60.0f;
	static constexpr float meterMaxDb = 0.0f;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DissonanceMeeterAudioProcessorEditor)
};
