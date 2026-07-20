#include "PluginProcessor.h"

#include "Parameters.h"
#include "PluginEditor.h"
#include "State/StatePersistence.h"

#include <array>
#include <cmath>
#include <memory>

namespace
{
juce::String formatFrequencyParameter (float value, int)
{
    if (value >= 1000.0f)
    {
        const auto decimals = value < 10000.0f ? 2 : 1;
        return juce::String (value / 1000.0f, decimals).trimCharactersAtEnd ("0").trimCharactersAtEnd (".")
             + " kHz";
    }

    return juce::String (value, value < 100.0f ? 1 : 0) + " Hz";
}

float parseFrequencyParameter (const juce::String& source)
{
    auto input = source.trim().toLowerCase().removeCharacters (" ");
    auto multiplier = 1.0f;

    if (input.endsWith ("khz"))
    {
        multiplier = 1000.0f;
        input = input.dropLastCharacters (3);
    }
    else if (input.endsWithChar ('k'))
    {
        multiplier = 1000.0f;
        input = input.dropLastCharacters (1);
    }
    else if (input.endsWith ("hz"))
    {
        input = input.dropLastCharacters (2);
    }

    return input.getFloatValue() * multiplier;
}

juce::String formatGainParameter (float value, int)
{
    if (std::abs (value) < 0.005f)
        value = 0.0f;
    return juce::String (value, 2) + " dB";
}

float parseGainParameter (const juce::String& source)
{
    auto input = source.trim().toLowerCase().removeCharacters (" ");
    if (input.endsWith ("db"))
        input = input.dropLastCharacters (2);
    return input.getFloatValue();
}
}

CutlineAudioProcessor::CutlineAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, cutline::parameters::stateTreeType, createParameterLayout())
{
    atomicParameters.highPassEnabled = parameters.getRawParameterValue (cutline::parameters::highPassEnabled);
    atomicParameters.highPassPoles = parameters.getRawParameterValue (cutline::parameters::highPassPoles);
    atomicParameters.highPassFrequency = parameters.getRawParameterValue (cutline::parameters::highPassFrequency);
    atomicParameters.lowPassEnabled = parameters.getRawParameterValue (cutline::parameters::lowPassEnabled);
    atomicParameters.lowPassPoles = parameters.getRawParameterValue (cutline::parameters::lowPassPoles);
    atomicParameters.lowPassFrequency = parameters.getRawParameterValue (cutline::parameters::lowPassFrequency);
    atomicParameters.outputGain = parameters.getRawParameterValue (cutline::parameters::outputGain);
    atomicParameters.leftRightSwap = parameters.getRawParameterValue (cutline::parameters::leftRightSwap);
    atomicParameters.filterBypass = parameters.getRawParameterValue (cutline::parameters::filterBypass);
    atomicParameters.mono = parameters.getRawParameterValue (cutline::parameters::mono);
}

juce::AudioProcessorValueTreeState::ParameterLayout CutlineAudioProcessor::createParameterLayout()
{
    using Parameter = juce::AudioProcessorValueTreeState::ParameterLayout;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> result;
    const auto parameterId = [] (const char* id) { return juce::ParameterID { id, 1 }; };
    auto frequencyRange = juce::NormalisableRange<float> { 20.0f, 20000.0f, 0.0f, 0.5f };
    const auto frequencyAttributes = juce::AudioParameterFloatAttributes()
        .withLabel ("Hz")
        .withStringFromValueFunction (formatFrequencyParameter)
        .withValueFromStringFunction (parseFrequencyParameter);
    const auto gainAttributes = juce::AudioParameterFloatAttributes()
        .withLabel ("dB")
        .withStringFromValueFunction (formatGainParameter)
        .withValueFromStringFunction (parseGainParameter);

    result.push_back (std::make_unique<juce::AudioParameterBool> (
        parameterId (cutline::parameters::highPassEnabled), "HP On", false));
    result.push_back (std::make_unique<juce::AudioParameterChoice> (
        parameterId (cutline::parameters::highPassPoles), "HP Pole",
        juce::StringArray { "6 dB/Oct", "12 dB/Oct", "18 dB/Oct", "24 dB/Oct",
                            "30 dB/Oct", "36 dB/Oct", "42 dB/Oct", "48 dB/Oct" }, 0));
    result.push_back (std::make_unique<juce::AudioParameterFloat> (
        parameterId (cutline::parameters::highPassFrequency), "HP Freq",
        frequencyRange, 20.0f, frequencyAttributes));
    result.push_back (std::make_unique<juce::AudioParameterBool> (
        parameterId (cutline::parameters::lowPassEnabled), "LP On", false));
    result.push_back (std::make_unique<juce::AudioParameterChoice> (
        parameterId (cutline::parameters::lowPassPoles), "LP Pole",
        juce::StringArray { "6 dB/Oct", "12 dB/Oct", "18 dB/Oct", "24 dB/Oct",
                            "30 dB/Oct", "36 dB/Oct", "42 dB/Oct", "48 dB/Oct" }, 0));
    result.push_back (std::make_unique<juce::AudioParameterFloat> (
        parameterId (cutline::parameters::lowPassFrequency), "LP Freq",
        frequencyRange, 20000.0f, frequencyAttributes));
    result.push_back (std::make_unique<juce::AudioParameterFloat> (
        parameterId (cutline::parameters::outputGain), "Output Gain",
        juce::NormalisableRange<float> { -12.0f, 12.0f, 0.01f }, 0.0f,
        gainAttributes));
    result.push_back (std::make_unique<juce::AudioParameterBool> (
        parameterId (cutline::parameters::leftRightSwap), "LR Swap", false));
    result.push_back (std::make_unique<juce::AudioParameterBool> (
        parameterId (cutline::parameters::filterBypass), "Filter Bypass", false));
    result.push_back (std::make_unique<juce::AudioParameterBool> (
        parameterId (cutline::parameters::mono), "Mono", false));

    return Parameter { result.begin(), result.end() };
}

void CutlineAudioProcessor::prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock)
{
    sampleRateIsValid = std::isfinite (sampleRate) && sampleRate > 0.0;
    jassert (sampleRateIsValid);
    setLatencySamples (0);
    floatEngine.prepare (sampleRate, maximumExpectedSamplesPerBlock);
    doubleEngine.prepare (sampleRate, maximumExpectedSamplesPerBlock);
}

void CutlineAudioProcessor::releaseResources() {}

bool CutlineAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

cutline::dsp::Parameters CutlineAudioProcessor::readParameters() const noexcept
{
    cutline::dsp::Parameters result;
    result.highPassEnabled = atomicParameters.highPassEnabled->load() >= 0.5f;
    result.highPassPoles = static_cast<int> (std::lround (atomicParameters.highPassPoles->load())) + 1;
    result.highPassFrequency = atomicParameters.highPassFrequency->load();
    result.lowPassEnabled = atomicParameters.lowPassEnabled->load() >= 0.5f;
    result.lowPassPoles = static_cast<int> (std::lround (atomicParameters.lowPassPoles->load())) + 1;
    result.lowPassFrequency = atomicParameters.lowPassFrequency->load();
    result.outputGainDb = atomicParameters.outputGain->load();
    result.leftRightSwap = atomicParameters.leftRightSwap->load() >= 0.5f;
    result.filterBypass = atomicParameters.filterBypass->load() >= 0.5f;
    result.mono = atomicParameters.mono->load() >= 0.5f;
    return result;
}

template <typename Sample>
void CutlineAudioProcessor::process (juce::AudioBuffer<Sample>& buffer,
                                     cutline::dsp::CutlineEngine<Sample>& engine) noexcept
{
    juce::ScopedNoDenormals noDenormals;

    if (! sampleRateIsValid || buffer.getNumSamples() == 0)
        return;

    engine.setTargets (readParameters());
    engine.process (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), buffer.getNumSamples());
}

void CutlineAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    process (buffer, floatEngine);
}

void CutlineAudioProcessor::processBlock (juce::AudioBuffer<double>& buffer, juce::MidiBuffer&)
{
    process (buffer, doubleEngine);
}

juce::AudioProcessorEditor* CutlineAudioProcessor::createEditor()
{
    return new CutlineAudioProcessorEditor (*this);
}

void CutlineAudioProcessor::getStateInformation (juce::MemoryBlock& destinationData)
{
    auto state = parameters.copyState();
    state.setProperty (cutline::parameters::schemaProperty,
                       cutline::parameters::currentSchemaVersion, nullptr);
    juce::MemoryOutputStream stream (destinationData, false);
    state.writeToStream (stream);
}

void CutlineAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto state = cutline::state::readValidatedState (data, sizeInBytes);
    if (state.isValid())
        parameters.replaceState (state);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CutlineAudioProcessor();
}
