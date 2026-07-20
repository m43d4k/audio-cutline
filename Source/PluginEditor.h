#pragma once

#include "PluginProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>

class CutlineAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit CutlineAudioProcessorEditor (CutlineAudioProcessor&);
    ~CutlineAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    class ResponseGraph;
    class CutlineLookAndFeel;

    static void configureFrequencySlider (juce::Slider&);
    static void configureGainSlider (juce::Slider&);
    static void configurePoleSelector (juce::ComboBox&);

    CutlineAudioProcessor& pluginProcessor;
    std::unique_ptr<CutlineLookAndFeel> lookAndFeel;
    std::unique_ptr<ResponseGraph> responseGraph;

    juce::ToggleButton highPassEnabled { "HP" };
    juce::ComboBox highPassPoles;
    juce::Slider highPassFrequency;
    juce::ToggleButton lowPassEnabled { "LP" };
    juce::ComboBox lowPassPoles;
    juce::Slider lowPassFrequency;
    juce::Slider outputGain;

    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<ButtonAttachment> highPassEnabledAttachment;
    std::unique_ptr<ComboBoxAttachment> highPassPolesAttachment;
    std::unique_ptr<SliderAttachment> highPassFrequencyAttachment;
    std::unique_ptr<ButtonAttachment> lowPassEnabledAttachment;
    std::unique_ptr<ComboBoxAttachment> lowPassPolesAttachment;
    std::unique_ptr<SliderAttachment> lowPassFrequencyAttachment;
    std::unique_ptr<SliderAttachment> outputGainAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CutlineAudioProcessorEditor)
};
