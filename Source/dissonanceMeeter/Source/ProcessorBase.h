/*
	==============================================================================

		ProcessorBase.h
		Created: 17 Oct 2022 11:24:19pm
		Author:  Lorenzo

	==============================================================================
*/
#pragma once

#include <JuceHeader.h>

class ProcessorBase : public juce::AudioProcessor
{
public:
	//==============================================================================
	ProcessorBase()
		: juce::AudioProcessor(BusesProperties()
			.withInput("Input", juce::AudioChannelSet::stereo())
			.withOutput("Output", juce::AudioChannelSet::stereo()))
	{}

	//==============================================================================
	void prepareToPlay(double, int) override {}
	void releaseResources() override {}
	void processBlock(juce::AudioSampleBuffer&, juce::MidiBuffer&) override {}

	//==============================================================================
	juce::AudioProcessorEditor* createEditor() override { return nullptr; }
	bool hasEditor() const override { return false; }

	//==============================================================================
	const juce::String getName() const override { return {}; }
	bool acceptsMidi() const override { return false; }
	bool producesMidi() const override { return false; }
	double getTailLengthSeconds() const override { return 0; }

	//==============================================================================
	int getNumPrograms() override { return 0; }
	int getCurrentProgram() override { return 0; }
	void setCurrentProgram(int) override {}
	const juce::String getProgramName(int) override { return {}; }
	void changeProgramName(int, const juce::String&) override {}

	//==============================================================================
	void getStateInformation(juce::MemoryBlock&) override {}
	void setStateInformation(const void*, int) override {}

	//==============================================================================
	bool isBusesLayoutSupported(const BusesLayout& layouts) const override
	{
		// Supporta solo layout stereo (2 canali in input e output)
		if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()
			&& layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono())
			return false;

		// Assicura che il numero di canali di input corrisponda a quello di output
		if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
			return false;

		return true;
	}

private:
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProcessorBase)
};
