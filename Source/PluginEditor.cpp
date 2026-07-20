#include "PluginEditor.h"

#include "DSP/Butterworth.h"
#include "Parameters.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace
{
const auto background = juce::Colour { 0xff111419 };
const auto panel = juce::Colour { 0xff191e25 };
const auto grid = juce::Colour { 0xff343c47 };
const auto text = juce::Colour { 0xffd8dee9 };
const auto mutedText = juce::Colour { 0xff8792a2 };
const auto highPassColour = juce::Colour { 0xff56b6c2 };
const auto lowPassColour = juce::Colour { 0xffc678dd };
const auto curveColour = juce::Colour { 0xffe5c07b };

juce::String formatFrequency (double value)
{
    if (value >= 1000.0)
    {
        const auto decimals = value < 10000.0 ? 2 : 1;
        return juce::String (value / 1000.0, decimals).trimCharactersAtEnd ("0").trimCharactersAtEnd (".")
             + " kHz";
    }

    return juce::String (value, value < 100.0 ? 1 : 0) + " Hz";
}

double parseFrequency (juce::String input)
{
    input = input.trim().toLowerCase().removeCharacters (" ");
    auto multiplier = 1.0;

    if (input.endsWith ("khz"))
    {
        multiplier = 1000.0;
        input = input.dropLastCharacters (3);
    }
    else if (input.endsWithChar ('k'))
    {
        multiplier = 1000.0;
        input = input.dropLastCharacters (1);
    }
    else if (input.endsWith ("hz"))
    {
        input = input.dropLastCharacters (2);
    }

    return input.getDoubleValue() * multiplier;
}

double parseGain (juce::String input)
{
    input = input.trim().toLowerCase().removeCharacters (" ");
    if (input.endsWith ("db"))
        input = input.dropLastCharacters (2);
    return input.getDoubleValue();
}
}

class CutlineAudioProcessorEditor::CutlineLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    CutlineLookAndFeel()
    {
        setColour (juce::Slider::rotarySliderFillColourId, curveColour);
        setColour (juce::Slider::rotarySliderOutlineColourId, grid);
        setColour (juce::Slider::thumbColourId, text);
        setColour (juce::Slider::textBoxTextColourId, text);
        setColour (juce::Slider::textBoxBackgroundColourId, background);
        setColour (juce::Slider::textBoxOutlineColourId, grid);
        setColour (juce::ComboBox::backgroundColourId, background);
        setColour (juce::ComboBox::textColourId, text);
        setColour (juce::ComboBox::outlineColourId, grid);
        setColour (juce::PopupMenu::backgroundColourId, panel);
        setColour (juce::PopupMenu::textColourId, text);
        setColour (juce::ToggleButton::textColourId, text);
    }
};

class CutlineAudioProcessorEditor::ResponseGraph final : public juce::Component,
                                                          private juce::Timer
{
public:
    explicit ResponseGraph (CutlineAudioProcessor& owner) : processor (owner)
    {
        startTimerHz (30);
    }

    void paint (juce::Graphics& graphics) override
    {
        graphics.fillAll (panel);
        auto plot = getLocalBounds().toFloat().reduced (44.0f, 18.0f);
        plot.removeFromBottom (8.0f);

        graphics.setColour (grid);
        graphics.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 6.0f, 1.0f);

        const auto frequencyToX = [&plot] (double frequency)
        {
            return plot.getX() + plot.getWidth()
                 * static_cast<float> (std::log (frequency / 20.0) / std::log (1000.0));
        };
        const auto decibelsToY = [&plot] (double decibels)
        {
            return plot.getY() + plot.getHeight()
                 * static_cast<float> ((6.0 - std::clamp (decibels, -48.0, 6.0)) / 54.0);
        };
        const auto responseDecibelsToY = [&plot] (double decibels)
        {
            return plot.getY() + plot.getHeight()
                 * static_cast<float> ((6.0 - decibels) / 54.0);
        };

        constexpr std::array gridFrequencies { 20.0, 50.0, 100.0, 200.0, 500.0,
                                               1000.0, 2000.0, 5000.0, 10000.0, 20000.0 };
        graphics.setFont (juce::FontOptions { 10.0f });

        for (const auto frequency : gridFrequencies)
        {
            const auto x = frequencyToX (frequency);
            graphics.setColour (grid.withAlpha (0.65f));
            graphics.drawVerticalLine (juce::roundToInt (x), plot.getY(), plot.getBottom());
            graphics.setColour (mutedText);
            const auto label = frequency >= 1000.0
                ? juce::String (frequency / 1000.0, 0) + "k"
                : juce::String (frequency, 0);
            graphics.drawText (label, juce::roundToInt (x - 18.0f),
                               juce::roundToInt (plot.getBottom() + 2.0f), 36, 14,
                               juce::Justification::centredTop, false);
        }

        graphics.setColour (grid.brighter (0.2f));
        graphics.drawHorizontalLine (juce::roundToInt (decibelsToY (0.0)), plot.getX(), plot.getRight());
        graphics.setColour (mutedText);
        graphics.drawText ("0 dB", 2, juce::roundToInt (decibelsToY (0.0) - 7.0f), 38, 14,
                           juce::Justification::centredRight, false);

        const auto& state = processor.getParameters();
        const auto value = [&state] (const char* id)
        {
            return static_cast<double> (state.getRawParameterValue (id)->load());
        };
        const auto hpEnabled = value (cutline::parameters::highPassEnabled) >= 0.5;
        const auto lpEnabled = value (cutline::parameters::lowPassEnabled) >= 0.5;
        const auto filtersBypassed = value (cutline::parameters::filterBypass) >= 0.5;
        const auto hpOrder = static_cast<int> (std::lround (value (cutline::parameters::highPassPoles))) + 1;
        const auto lpOrder = static_cast<int> (std::lround (value (cutline::parameters::lowPassPoles))) + 1;
        const auto hpFrequency = value (cutline::parameters::highPassFrequency);
        const auto lpFrequency = value (cutline::parameters::lowPassFrequency);
        const auto sampleRate = processor.getSampleRate() > 0.0 ? processor.getSampleRate() : 48000.0;
        const auto hp = cutline::dsp::designButterworth<double> (
            cutline::dsp::FilterType::highPass, hpOrder, hpFrequency, sampleRate);
        const auto lp = cutline::dsp::designButterworth<double> (
            cutline::dsp::FilterType::lowPass, lpOrder, lpFrequency, sampleRate);

        const auto responseDb = [&] (double frequency)
        {
            auto magnitude = 1.0;
            if (hpEnabled && ! filtersBypassed)
                magnitude *= cutline::dsp::magnitudeAt (hp, frequency, sampleRate);
            if (lpEnabled && ! filtersBypassed)
                magnitude *= cutline::dsp::magnitudeAt (lp, frequency, sampleRate);
            return 20.0 * std::log10 (std::max (magnitude, 1.0e-12));
        };

        juce::Path response;
        constexpr int pointCount = 420;
        for (int point = 0; point < pointCount; ++point)
        {
            const auto proportion = static_cast<double> (point) / (pointCount - 1);
            const auto frequency = 20.0 * std::pow (1000.0, proportion);
            const auto x = frequencyToX (frequency);
            const auto y = responseDecibelsToY (responseDb (frequency));
            if (point == 0)
                response.startNewSubPath (x, y);
            else
                response.lineTo (x, y);
        }

        {
            juce::Graphics::ScopedSaveState savedState (graphics);
            graphics.reduceClipRegion (plot.toNearestIntEdges());
            graphics.setColour (curveColour);
            graphics.strokePath (response, juce::PathStrokeType { 2.2f });
        }

        const auto drawCutoff = [&] (double frequency, juce::Colour colour)
        {
            const auto centre = juce::Point<float> { frequencyToX (frequency),
                                                     decibelsToY (responseDb (frequency)) };
            graphics.setColour (colour);
            graphics.fillEllipse (juce::Rectangle<float> { 8.0f, 8.0f }.withCentre (centre));
            graphics.setColour (background);
            graphics.drawEllipse (juce::Rectangle<float> { 8.0f, 8.0f }.withCentre (centre), 1.0f);
        };

        if (hpEnabled && ! filtersBypassed)
            drawCutoff (hpFrequency, highPassColour);
        if (lpEnabled && ! filtersBypassed)
            drawCutoff (lpFrequency, lowPassColour);
    }

private:
    void timerCallback() override { repaint(); }
    CutlineAudioProcessor& processor;
};

CutlineAudioProcessorEditor::CutlineAudioProcessorEditor (CutlineAudioProcessor& owner)
    : AudioProcessorEditor (&owner), pluginProcessor (owner),
      lookAndFeel (std::make_unique<CutlineLookAndFeel>()),
      responseGraph (std::make_unique<ResponseGraph> (owner))
{
    setLookAndFeel (lookAndFeel.get());
    setResizable (false, false);
    setSize (720, 420);

    configureFrequencySlider (highPassFrequency);
    configureFrequencySlider (lowPassFrequency);
    configureGainSlider (outputGain);
    configurePoleSelector (highPassPoles);
    configurePoleSelector (lowPassPoles);

    for (auto* component : std::array<juce::Component*, 11> {
             responseGraph.get(), &highPassEnabled, &highPassPoles, &highPassFrequency,
             &lowPassEnabled, &lowPassPoles, &lowPassFrequency, &outputGain,
             &leftRightSwap, &filterBypass, &mono })
        addAndMakeVisible (*component);

    auto& state = pluginProcessor.getParameters();
    highPassEnabledAttachment = std::make_unique<ButtonAttachment> (
        state, cutline::parameters::highPassEnabled, highPassEnabled);
    highPassPolesAttachment = std::make_unique<ComboBoxAttachment> (
        state, cutline::parameters::highPassPoles, highPassPoles);
    highPassFrequencyAttachment = std::make_unique<SliderAttachment> (
        state, cutline::parameters::highPassFrequency, highPassFrequency);
    lowPassEnabledAttachment = std::make_unique<ButtonAttachment> (
        state, cutline::parameters::lowPassEnabled, lowPassEnabled);
    lowPassPolesAttachment = std::make_unique<ComboBoxAttachment> (
        state, cutline::parameters::lowPassPoles, lowPassPoles);
    lowPassFrequencyAttachment = std::make_unique<SliderAttachment> (
        state, cutline::parameters::lowPassFrequency, lowPassFrequency);
    outputGainAttachment = std::make_unique<SliderAttachment> (
        state, cutline::parameters::outputGain, outputGain);
    leftRightSwapAttachment = std::make_unique<ButtonAttachment> (
        state, cutline::parameters::leftRightSwap, leftRightSwap);
    filterBypassAttachment = std::make_unique<ButtonAttachment> (
        state, cutline::parameters::filterBypass, filterBypass);
    monoAttachment = std::make_unique<ButtonAttachment> (
        state, cutline::parameters::mono, mono);

    highPassFrequency.textFromValueFunction = formatFrequency;
    highPassFrequency.valueFromTextFunction = parseFrequency;
    lowPassFrequency.textFromValueFunction = formatFrequency;
    lowPassFrequency.valueFromTextFunction = parseFrequency;
    outputGain.textFromValueFunction = [] (double value)
    {
        if (std::abs (value) < 0.005)
            value = 0.0;
        return juce::String (value, 2) + " dB";
    };
    outputGain.valueFromTextFunction = parseGain;
}

CutlineAudioProcessorEditor::~CutlineAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void CutlineAudioProcessorEditor::configureFrequencySlider (juce::Slider& slider)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 92, 22);
    slider.setRange (20.0, 20000.0);
    slider.setSkewFactorFromMidPoint (632.455532);
    slider.textFromValueFunction = formatFrequency;
    slider.valueFromTextFunction = parseFrequency;
}

void CutlineAudioProcessorEditor::configureGainSlider (juce::Slider& slider)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 92, 22);
    slider.setRange (-12.0, 12.0, 0.01);
    slider.textFromValueFunction = [] (double value)
    {
        if (std::abs (value) < 0.005)
            value = 0.0;
        return juce::String (value, 2) + " dB";
    };
    slider.valueFromTextFunction = parseGain;
}

void CutlineAudioProcessorEditor::configurePoleSelector (juce::ComboBox& selector)
{
    selector.setTextWhenNothingSelected ("dB/Oct");
    for (int pole = 1; pole <= 8; ++pole)
        selector.addItem (juce::String (pole * 6) + " dB/Oct", pole);
}

void CutlineAudioProcessorEditor::paint (juce::Graphics& graphics)
{
    graphics.fillAll (background);
    graphics.setColour (text);
    graphics.setFont (juce::FontOptions { 17.0f, juce::Font::bold });
    graphics.drawText ("CUTLINE", 16, 8, 130, 24, juce::Justification::centredLeft);

    graphics.setFont (juce::FontOptions { 11.0f });
    graphics.setColour (mutedText);
    graphics.drawText ("MINIMUM PHASE FILTER", 139, 9, 180, 22, juce::Justification::centredLeft);
    graphics.drawText ("HIGH PASS", 16, 258, 218, 20, juce::Justification::centred);
    graphics.drawText ("LOW PASS", 251, 258, 218, 20, juce::Justification::centred);
    graphics.drawText ("OUTPUT", 486, 258, 218, 20, juce::Justification::centred);

    graphics.setColour (grid);
    graphics.drawVerticalLine (242, 267.0f, 410.0f);
    graphics.drawVerticalLine (477, 267.0f, 410.0f);
}

void CutlineAudioProcessorEditor::resized()
{
    responseGraph->setBounds (12, 38, getWidth() - 24, 210);

    highPassEnabled.setBounds (28, 283, 68, 24);
    highPassPoles.setBounds (116, 283, 102, 24);
    highPassFrequency.setBounds (61, 307, 128, 105);

    lowPassEnabled.setBounds (263, 283, 68, 24);
    lowPassPoles.setBounds (351, 283, 102, 24);
    lowPassFrequency.setBounds (296, 307, 128, 105);

    outputGain.setBounds (531, 285, 128, 127);

    leftRightSwap.setBounds (392, 7, 90, 24);
    filterBypass.setBounds (489, 7, 128, 24);
    mono.setBounds (625, 7, 76, 24);
}
