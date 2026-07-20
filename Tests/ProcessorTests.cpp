#include "Parameters.h"
#include "PluginProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string_view>

namespace
{
int failures = 0;

void expect (bool condition, std::string_view message)
{
    if (! condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void setParameter (CutlineAudioProcessor& processor, const char* id, float plainValue)
{
    auto* parameter = processor.getParameters().getParameter (id);
    expect (parameter != nullptr, "parameter must exist");
    if (parameter != nullptr)
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (plainValue));
}

float getParameter (const CutlineAudioProcessor& processor, const char* id)
{
    return processor.getParameters().getRawParameterValue (id)->load();
}

juce::MemoryBlock stateWithSchema (const juce::MemoryBlock& source, int schema)
{
    juce::MemoryInputStream input (source.getData(), source.getSize(), false);
    auto state = juce::ValueTree::readFromStream (input);
    state.setProperty (cutline::parameters::schemaProperty, schema, nullptr);
    juce::MemoryBlock result;
    juce::MemoryOutputStream output (result, false);
    state.writeToStream (output);
    return result;
}

void testProcessorContract()
{
    CutlineAudioProcessor processor;
    for (const auto id : cutline::parameters::ids)
        expect (processor.getParameters().getParameter (id.data()) != nullptr,
                "all seven automatable parameters must be exposed");
    const auto* frequencyParameter = processor.getParameters().getParameter (
        cutline::parameters::highPassFrequency);
    const auto* gainParameter = processor.getParameters().getParameter (
        cutline::parameters::outputGain);
    expect (frequencyParameter->getText (frequencyParameter->convertTo0to1 (1500.0f), 16).contains ("kHz"),
            "frequency values must display kHz units");
    expect (std::abs (frequencyParameter->convertFrom0to1 (
                         frequencyParameter->getValueForText ("1.5 kHz")) - 1500.0f) < 0.01f,
            "frequency parameters must parse unit-bearing input");
    expect (gainParameter->getText (gainParameter->convertTo0to1 (-3.0f), 16).contains ("dB"),
            "gain values must display dB units");
    expect (std::abs (gainParameter->convertFrom0to1 (
                         gainParameter->getValueForText ("-3 dB")) + 3.0f) < 0.01f,
            "gain parameters must parse unit-bearing input");
    expect (processor.supportsDoublePrecisionProcessing(), "double precision must be supported");
    expect (processor.getLatencySamples() == 0, "reported latency must be zero");

    juce::AudioProcessor::BusesLayout stereo;
    stereo.inputBuses.add (juce::AudioChannelSet::stereo());
    stereo.outputBuses.add (juce::AudioChannelSet::stereo());
    expect (processor.checkBusesLayoutSupported (stereo), "stereo layout must be accepted");

    auto mono = stereo;
    mono.inputBuses.set (0, juce::AudioChannelSet::mono());
    mono.outputBuses.set (0, juce::AudioChannelSet::mono());
    expect (! processor.checkBusesLayoutSupported (mono), "mono layout must be rejected");

    processor.prepareToPlay (48000.0, 512);
    juce::MidiBuffer midi;
    juce::AudioBuffer<float> floatBuffer (2, 0);
    juce::AudioBuffer<double> doubleBuffer (2, 0);
    processor.processBlock (floatBuffer, midi);
    processor.processBlock (doubleBuffer, midi);

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    expect (editor != nullptr && editor->getWidth() == 720 && editor->getHeight() == 420,
            "editor must have a fixed initial size of 720 by 420");
    juce::Image image (juce::Image::ARGB, 720, 420, true);
    juce::Graphics graphics (image);
    editor->paintEntireComponent (graphics, true);
    expect (image.getPixelAt (10, 10).getAlpha() != 0, "editor must render an opaque UI");

    if (const auto* snapshotPath = std::getenv ("CUTLINE_UI_SNAPSHOT"))
    {
        auto output = juce::File (snapshotPath).createOutputStream();
        juce::PNGImageFormat png;
        expect (output != nullptr && png.writeImageToStream (image, *output),
                "optional UI snapshot must be writable");
    }
}

void testStateRoundTripAndRejection()
{
    CutlineAudioProcessor processor;
    setParameter (processor, cutline::parameters::highPassEnabled, 1.0f);
    setParameter (processor, cutline::parameters::highPassPoles, 6.0f);
    setParameter (processor, cutline::parameters::highPassFrequency, 1234.0f);
    setParameter (processor, cutline::parameters::outputGain, -4.5f);

    juce::MemoryBlock saved;
    processor.getStateInformation (saved);
    expect (! saved.isEmpty(), "saved state must not be empty");

    setParameter (processor, cutline::parameters::highPassEnabled, 0.0f);
    setParameter (processor, cutline::parameters::highPassPoles, 0.0f);
    setParameter (processor, cutline::parameters::highPassFrequency, 20.0f);
    setParameter (processor, cutline::parameters::outputGain, 0.0f);
    processor.setStateInformation (saved.getData(), static_cast<int> (saved.getSize()));

    expect (getParameter (processor, cutline::parameters::highPassEnabled) == 1.0f,
            "enabled state must round-trip");
    expect (getParameter (processor, cutline::parameters::highPassPoles) == 6.0f,
            "pole state must round-trip");
    expect (std::abs (getParameter (processor, cutline::parameters::highPassFrequency) - 1234.0f) < 0.01f,
            "frequency state must round-trip");
    expect (std::abs (getParameter (processor, cutline::parameters::outputGain) + 4.5f) < 0.01f,
            "gain state must round-trip");

    const auto rejectedBaseline = getParameter (processor, cutline::parameters::outputGain);
    const auto future = stateWithSchema (saved, cutline::parameters::currentSchemaVersion + 1);
    processor.setStateInformation (future.getData(), static_cast<int> (future.getSize()));
    expect (std::abs (getParameter (processor, cutline::parameters::outputGain) - rejectedBaseline) < 0.001f,
            "rejected future state must preserve current values");

    constexpr unsigned char invalid[] { 0xde, 0xad, 0xbe, 0xef };
    processor.setStateInformation (invalid, static_cast<int> (sizeof (invalid)));
    expect (std::abs (getParameter (processor, cutline::parameters::outputGain) - rejectedBaseline) < 0.001f,
            "corrupt state must preserve current values");
}
}

int main()
{
    juce::ScopedJuceInitialiser_GUI initialiseJuce;
    testProcessorContract();
    testStateRoundTripAndRejection();
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
