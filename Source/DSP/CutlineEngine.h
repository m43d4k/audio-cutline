#pragma once

#include "Butterworth.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>

namespace cutline::dsp
{
struct Parameters
{
    bool highPassEnabled {};
    int highPassPoles { 1 };
    float highPassFrequency { 20.0f };
    bool lowPassEnabled {};
    int lowPassPoles { 1 };
    float lowPassFrequency { 20000.0f };
    float outputGainDb {};
    bool leftRightSwap {};
    bool filterBypass {};
    bool mono {};
};

namespace detail
{
class LinearRamp
{
public:
    void reset (double newSampleRate, double seconds, double initialValue)
    {
        sampleRate = newSampleRate;
        durationSeconds = seconds;
        current = target = initialValue;
        increment = 0.0;
        remaining = 0;
    }

    void setTarget (double newTarget)
    {
        if (std::abs (newTarget - target) <= std::numeric_limits<double>::epsilon())
            return;

        target = newTarget;
        remaining = std::max (1, static_cast<int> (std::round (sampleRate * durationSeconds)));
        increment = (target - current) / static_cast<double> (remaining);
    }

    double next()
    {
        const auto value = current;

        if (remaining > 0)
        {
            current += increment;
            if (--remaining == 0)
                current = target;
        }

        return value;
    }

    double value() const noexcept { return current; }

private:
    double sampleRate { 1.0 };
    double durationSeconds {};
    double current {};
    double target {};
    double increment {};
    int remaining {};
};

template <typename Sample>
struct FilterBank
{
    struct State
    {
        double z1 {};
        double z2 {};
    };

    ButterworthCoefficients<Sample> coefficients;
    std::array<std::array<State, 4>, 2> states {};

    void reset() noexcept { states = {}; }

    Sample process (int channel, Sample input) noexcept
    {
        auto output = static_cast<double> (input);
        auto& channelStates = states[static_cast<std::size_t> (channel)];

        for (int index = 0; index < coefficients.sectionCount; ++index)
        {
            const auto& coefficient = coefficients.sections[static_cast<std::size_t> (index)];
            auto& state = channelStates[static_cast<std::size_t> (index)];
            const auto next = coefficient.b0 * output + state.z1;
            state.z1 = coefficient.b1 * output - coefficient.a1 * next + state.z2;
            state.z2 = coefficient.b2 * output - coefficient.a2 * next;
            output = next;
        }

        return static_cast<Sample> (output);
    }
};

template <typename Sample>
class CutFilter
{
public:
    void prepare (FilterType newType, double newSampleRate, double initialFrequency)
    {
        type = newType;
        sampleRate = newSampleRate;
        activeOrder = transitionOrder = 1;
        pendingOrder = 0;
        transitionPosition = transitionLength = 0;
        frequencyLog.reset (sampleRate, 0.020, std::log (initialFrequency));
        wetMix.reset (sampleRate, 0.005, 0.0);
        activeBank.reset();
        transitionBank.reset();
        updateCoefficients();
    }

    void setTargets (bool enabled, int order, double frequency)
    {
        wetMix.setTarget (enabled ? 1.0 : 0.0);
        frequency = std::clamp (frequency, 20.0, 20000.0);
        frequencyLog.setTarget (std::log (frequency));
        order = std::clamp (order, 1, 8);

        if (transitionLength > 0)
        {
            pendingOrder = order == transitionOrder ? 0 : order;
        }
        else if (order != activeOrder)
        {
            beginOrderTransition (order);
        }
    }

    void updateCoefficients()
    {
        const auto cutoff = std::exp (frequencyLog.value());
        activeBank.coefficients = designButterworth<Sample> (type, activeOrder, cutoff, sampleRate);

        if (transitionLength > 0)
            transitionBank.coefficients = designButterworth<Sample> (
                type, transitionOrder, cutoff, sampleRate);
    }

    void processFrame (Sample& left, Sample& right) noexcept
    {
        const auto dryLeft = left;
        const auto dryRight = right;
        const auto activeLeft = activeBank.process (0, dryLeft);
        const auto activeRight = activeBank.process (1, dryRight);
        auto filteredLeft = activeLeft;
        auto filteredRight = activeRight;

        if (transitionLength > 0)
        {
            const auto nextLeft = transitionBank.process (0, dryLeft);
            const auto nextRight = transitionBank.process (1, dryRight);
            const auto mix = static_cast<Sample> (
                static_cast<double> (transitionPosition) / static_cast<double> (transitionLength));
            filteredLeft = activeLeft + (nextLeft - activeLeft) * mix;
            filteredRight = activeRight + (nextRight - activeRight) * mix;

            if (++transitionPosition >= transitionLength)
                finishOrderTransition();
        }

        const auto mix = static_cast<Sample> (wetMix.next());
        left = dryLeft + (filteredLeft - dryLeft) * mix;
        right = dryRight + (filteredRight - dryRight) * mix;
        static_cast<void> (frequencyLog.next());
    }

private:
    void beginOrderTransition (int newOrder) noexcept
    {
        transitionOrder = newOrder;
        transitionBank.reset();
        transitionBank.coefficients = designButterworth<Sample> (
            type, transitionOrder, std::exp (frequencyLog.value()), sampleRate);
        transitionPosition = 0;
        transitionLength = std::max (1, static_cast<int> (std::round (sampleRate * 0.010)));
    }

    void finishOrderTransition() noexcept
    {
        activeBank = transitionBank;
        activeOrder = transitionOrder;
        transitionLength = transitionPosition = 0;

        const auto nextOrder = pendingOrder;
        pendingOrder = 0;
        if (nextOrder != 0 && nextOrder != activeOrder)
            beginOrderTransition (nextOrder);
    }

    FilterType type { FilterType::lowPass };
    double sampleRate { 48000.0 };
    int activeOrder { 1 };
    int transitionOrder { 1 };
    int pendingOrder {};
    int transitionPosition {};
    int transitionLength {};
    LinearRamp frequencyLog;
    LinearRamp wetMix;
    FilterBank<Sample> activeBank;
    FilterBank<Sample> transitionBank;
};
}

template <typename Sample>
class CutlineEngine
{
public:
    void prepare (double newSampleRate, int maximumBlockSize)
    {
        static_cast<void> (maximumBlockSize);
        sampleRate = newSampleRate;
        validSampleRate = std::isfinite (sampleRate) && sampleRate > 0.0;

        if (! validSampleRate)
            return;

        highPass.prepare (FilterType::highPass, sampleRate, parameters.highPassFrequency);
        lowPass.prepare (FilterType::lowPass, sampleRate, parameters.lowPassFrequency);
        outputGain.reset (sampleRate, 0.010, decibelsToGain (parameters.outputGainDb));
        filterMix.reset (sampleRate, 0.005, 1.0);
        swapMix.reset (sampleRate, 0.005, 0.0);
        monoMix.reset (sampleRate, 0.005, 0.0);
    }

    void setTargets (const Parameters& newParameters)
    {
        parameters = newParameters;

        if (! validSampleRate)
            return;

        highPass.setTargets (parameters.highPassEnabled,
                             parameters.highPassPoles,
                             parameters.highPassFrequency);
        lowPass.setTargets (parameters.lowPassEnabled,
                            parameters.lowPassPoles,
                            parameters.lowPassFrequency);
        outputGain.setTarget (decibelsToGain (parameters.outputGainDb));
        filterMix.setTarget (parameters.filterBypass ? 0.0 : 1.0);
        swapMix.setTarget (parameters.leftRightSwap ? 1.0 : 0.0);
        monoMix.setTarget (parameters.mono ? 1.0 : 0.0);
    }

    void process (Sample* const* channels, int channelCount, int sampleCount) noexcept
    {
        if (! validSampleRate || channels == nullptr || channelCount < 2 || sampleCount <= 0)
            return;

        constexpr int coefficientInterval = 16;

        for (int blockOffset = 0; blockOffset < sampleCount; blockOffset += coefficientInterval)
        {
            highPass.updateCoefficients();
            lowPass.updateCoefficients();
            const auto blockEnd = std::min (sampleCount, blockOffset + coefficientInterval);

            for (int sample = blockOffset; sample < blockEnd; ++sample)
            {
                auto left = channels[0][sample];
                auto right = channels[1][sample];
                const auto dryLeft = left;
                const auto dryRight = right;
                highPass.processFrame (left, right);
                lowPass.processFrame (left, right);
                const auto filters = static_cast<Sample> (filterMix.next());
                left = dryLeft + (left - dryLeft) * filters;
                right = dryRight + (right - dryRight) * filters;

                const auto gain = static_cast<Sample> (outputGain.next());
                left *= gain;
                right *= gain;

                const auto swap = static_cast<Sample> (swapMix.next());
                const auto unswappedLeft = left;
                left += (right - left) * swap;
                right += (unswappedLeft - right) * swap;

                const auto mono = static_cast<Sample> (monoMix.next());
                const auto monoSignal = static_cast<Sample> (0.5) * (left + right);
                channels[0][sample] = left + (monoSignal - left) * mono;
                channels[1][sample] = right + (monoSignal - right) * mono;
            }
        }
    }

private:
    static double decibelsToGain (double decibels) noexcept
    {
        return std::pow (10.0, decibels / 20.0);
    }

    double sampleRate {};
    bool validSampleRate {};
    Parameters parameters;
    detail::CutFilter<Sample> highPass;
    detail::CutFilter<Sample> lowPass;
    detail::LinearRamp outputGain;
    detail::LinearRamp filterMix;
    detail::LinearRamp swapMix;
    detail::LinearRamp monoMix;
};
}
