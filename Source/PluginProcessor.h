#pragma once

#include "DSP/CutlineEngine.h"

#include <juce_audio_processors/juce_audio_processors.h>

class CutlineAudioProcessor final : public juce::AudioProcessor
{
public:
    CutlineAudioProcessor();

    void prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock (juce::AudioBuffer<double>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    bool supportsDoublePrecisionProcessing() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState& getParameters() noexcept { return parameters; }
    const juce::AudioProcessorValueTreeState& getParameters() const noexcept { return parameters; }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    struct AtomicParameters
    {
        std::atomic<float>* highPassEnabled {};
        std::atomic<float>* highPassPoles {};
        std::atomic<float>* highPassFrequency {};
        std::atomic<float>* lowPassEnabled {};
        std::atomic<float>* lowPassPoles {};
        std::atomic<float>* lowPassFrequency {};
        std::atomic<float>* outputGain {};
        std::atomic<float>* leftRightSwap {};
        std::atomic<float>* filterBypass {};
        std::atomic<float>* mono {};
    };

    cutline::dsp::Parameters readParameters() const noexcept;

    template <typename Sample>
    void process (juce::AudioBuffer<Sample>& buffer,
                  cutline::dsp::CutlineEngine<Sample>& engine) noexcept;

    juce::AudioProcessorValueTreeState parameters;
    AtomicParameters atomicParameters;
    cutline::dsp::CutlineEngine<float> floatEngine;
    cutline::dsp::CutlineEngine<double> doubleEngine;
    bool sampleRateIsValid {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CutlineAudioProcessor)
};
