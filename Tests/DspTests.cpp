#include "DSP/Butterworth.h"
#include "DSP/CutlineEngine.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string_view>
#include <vector>

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

void expectNear (double actual, double expected, double tolerance, std::string_view message)
{
    if (! std::isfinite (actual) || std::abs (actual - expected) > tolerance)
    {
        std::cerr << "FAIL: " << message << " (actual=" << actual
                  << ", expected=" << expected << ")\n";
        ++failures;
    }
}

double decibels (double magnitude)
{
    return 20.0 * std::log10 (std::max (magnitude, 1.0e-300));
}

void testButterworthCutoffAndStability()
{
    constexpr std::array sampleRates { 44100.0, 48000.0, 88200.0, 96000.0,
                                       176400.0, 192000.0 };
    constexpr std::array cutoffFrequencies { 20.0, 1000.0, 20000.0 };

    for (const auto sampleRate : sampleRates)
    {
        for (const auto cutoff : cutoffFrequencies)
        {
            for (int order = 1; order <= 8; ++order)
            {
                for (const auto type : { cutline::dsp::FilterType::highPass,
                                         cutline::dsp::FilterType::lowPass })
                {
                    const auto coefficients = cutline::dsp::designButterworth<double> (
                        type, order, cutoff, sampleRate);
                    const auto floatCoefficients = cutline::dsp::designButterworth<float> (
                        type, order, cutoff, sampleRate);

                    expectNear (decibels (cutline::dsp::magnitudeAt (
                                    coefficients, cutoff, sampleRate)),
                                -3.0102999566, 0.001,
                                "double cutoff must be the Butterworth -3.0103 dB point");
                    expectNear (decibels (cutline::dsp::magnitudeAt (
                                    floatCoefficients, cutoff, sampleRate)),
                                -3.0102999566, 0.1,
                                "float cutoff must remain within the 0.1 dB requirement");
                    expect (cutline::dsp::allPolesInsideUnitCircle (coefficients),
                            "all double poles must remain inside the unit circle");
                    expect (cutline::dsp::allPolesInsideUnitCircle (floatCoefficients),
                            "all float poles must remain inside the unit circle");
                }
            }
        }
    }
}

void testAsymptoticSlope()
{
    constexpr auto sampleRate = 192000.0;
    constexpr auto cutoff = 1000.0;

    for (int order = 1; order <= 8; ++order)
    {
        const auto hp = cutline::dsp::designButterworth<double> (
            cutline::dsp::FilterType::highPass, order, cutoff, sampleRate);
        const auto lp = cutline::dsp::designButterworth<double> (
            cutline::dsp::FilterType::lowPass, order, cutoff, sampleRate);

        const auto hpSlope = decibels (cutline::dsp::magnitudeAt (hp, 250.0, sampleRate))
                           - decibels (cutline::dsp::magnitudeAt (hp, 125.0, sampleRate));
        const auto lpSlope = decibels (cutline::dsp::magnitudeAt (lp, 4000.0, sampleRate))
                           - decibels (cutline::dsp::magnitudeAt (lp, 8000.0, sampleRate));

        expectNear (hpSlope, 6.0206 * order, 0.35,
                    "HP stop-band slope must approach 6 dB/oct per pole");
        expectNear (lpSlope, 6.0206 * order, 0.35,
                    "LP stop-band slope must approach 6 dB/oct per pole");
    }
}

template <typename Sample>
void testEngineSafetyAndGain()
{
    cutline::dsp::CutlineEngine<Sample> engine;
    engine.prepare (48000.0, 512);

    auto parameters = cutline::dsp::Parameters {};
    parameters.outputGainDb = 6.0f;
    engine.setTargets (parameters);

    std::vector<Sample> left (2048, static_cast<Sample> (0.25));
    std::vector<Sample> right (2048, static_cast<Sample> (-0.25));
    Sample* channels[] { left.data(), right.data() };

    engine.process (channels, 2, 0);
    engine.process (channels, 2, static_cast<int> (left.size()));

    for (std::size_t i = 0; i < left.size(); ++i)
    {
        expect (std::isfinite (left[i]) && std::isfinite (right[i]),
                "engine output must always be finite");
    }

    expectNear (static_cast<double> (left.back()), 0.25 * std::pow (10.0, 6.0 / 20.0),
                1.0e-4, "output gain must converge in linear amplitude");

    parameters.highPassEnabled = true;
    parameters.highPassPoles = 8;
    parameters.highPassFrequency = 20000.0f;
    parameters.lowPassEnabled = true;
    parameters.lowPassPoles = 8;
    parameters.lowPassFrequency = 20.0f;
    engine.setTargets (parameters);

    for (int pass = 0; pass < 20; ++pass)
    {
        std::fill (left.begin(), left.end(), static_cast<Sample> (pass == 0 ? 1.0 : 0.0));
        std::fill (right.begin(), right.end(), static_cast<Sample> (pass == 0 ? -1.0 : 0.0));
        engine.process (channels, 2, static_cast<int> (left.size()));
        for (const auto value : left)
            expect (std::isfinite (value), "boundary processing must not produce NaN or Inf");
    }
}

template <typename Sample>
void testUtilityRoutingAndFilterBypass()
{
    constexpr auto sampleRate = 1000.0;
    constexpr auto sampleCount = 64;
    cutline::dsp::CutlineEngine<Sample> engine;
    engine.prepare (sampleRate, sampleCount);

    auto parameters = cutline::dsp::Parameters {};
    parameters.highPassEnabled = true;
    parameters.highPassFrequency = 200.0f;
    engine.setTargets (parameters);

    std::array<Sample, sampleCount> left {};
    std::array<Sample, sampleCount> right {};
    Sample* channels[] { left.data(), right.data() };

    for (int pass = 0; pass < 4; ++pass)
    {
        left.fill (static_cast<Sample> (1.0));
        right.fill (static_cast<Sample> (1.0));
        engine.process (channels, 2, sampleCount);
    }
    expectNear (static_cast<double> (left.back()), 0.0, 1.0e-4,
                "enabled high-pass must reject settled DC");

    parameters.filterBypass = true;
    engine.setTargets (parameters);
    left.fill (static_cast<Sample> (1.0));
    right.fill (static_cast<Sample> (1.0));
    engine.process (channels, 2, sampleCount);
    expectNear (static_cast<double> (left.back()), 1.0, 1.0e-4,
                "filter bypass must converge to the unfiltered signal");

    parameters.leftRightSwap = true;
    engine.setTargets (parameters);
    left.fill (static_cast<Sample> (0.25));
    right.fill (static_cast<Sample> (-0.75));
    engine.process (channels, 2, sampleCount);
    expectNear (static_cast<double> (left.back()), -0.75, 1.0e-4,
                "LR swap must route right input to left output");
    expectNear (static_cast<double> (right.back()), 0.25, 1.0e-4,
                "LR swap must route left input to right output");

    parameters.mono = true;
    engine.setTargets (parameters);
    left.fill (static_cast<Sample> (0.25));
    right.fill (static_cast<Sample> (-0.75));
    engine.process (channels, 2, sampleCount);
    expectNear (static_cast<double> (left.back()), -0.25, 1.0e-4,
                "mono must output the arithmetic mean on the left");
    expectNear (static_cast<double> (right.back()), -0.25, 1.0e-4,
                "mono must output the arithmetic mean on the right");
}

void testInvalidSampleRateIsDry()
{
    cutline::dsp::CutlineEngine<float> engine;
    engine.prepare (0.0, 64);
    auto parameters = cutline::dsp::Parameters {};
    parameters.highPassEnabled = true;
    parameters.outputGainDb = 12.0f;
    engine.setTargets (parameters);

    std::array left { 0.25f, -0.5f, 1.0f };
    std::array right { -0.25f, 0.5f, -1.0f };
    const auto expectedLeft = left;
    const auto expectedRight = right;
    float* channels[] { left.data(), right.data() };
    engine.process (channels, 2, static_cast<int> (left.size()));

    expect (left == expectedLeft && right == expectedRight,
            "invalid sample rate must produce a dry pass-through");
}
}

int main()
{
    testButterworthCutoffAndStability();
    testAsymptoticSlope();
    testEngineSafetyAndGain<float>();
    testEngineSafetyAndGain<double>();
    testUtilityRoutingAndFilterBypass<float>();
    testUtilityRoutingAndFilterBypass<double>();
    testInvalidSampleRateIsDry();

    if (failures != 0)
        std::cerr << failures << " test assertion(s) failed\n";

    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
