#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <limits>

namespace cutline::dsp
{
enum class FilterType
{
    highPass,
    lowPass
};

template <typename Sample>
struct BiquadCoefficients
{
    using HostSample = Sample;
    double b0 { 1.0 };
    double b1 {};
    double b2 {};
    double a1 {};
    double a2 {};
};

template <typename Sample>
struct ButterworthCoefficients
{
    std::array<BiquadCoefficients<Sample>, 4> sections {};
    int sectionCount {};
};

namespace detail
{
template <typename Sample>
BiquadCoefficients<Sample> makeFirstOrder (FilterType type, double k)
{
    const auto normalisation = 1.0 / (1.0 + k);
    BiquadCoefficients<Sample> result;

    if (type == FilterType::lowPass)
    {
        result.b0 = result.b1 = k * normalisation;
    }
    else
    {
        result.b0 = normalisation;
        result.b1 = -normalisation;
    }

    result.a1 = (k - 1.0) * normalisation;
    return result;
}

template <typename Sample>
BiquadCoefficients<Sample> makeSecondOrder (FilterType type, double k, double q)
{
    const auto kSquared = k * k;
    const auto normalisation = 1.0 / (1.0 + k / q + kSquared);
    BiquadCoefficients<Sample> result;

    if (type == FilterType::lowPass)
    {
        result.b0 = kSquared * normalisation;
        result.b1 = 2.0 * kSquared * normalisation;
        result.b2 = result.b0;
    }
    else
    {
        result.b0 = normalisation;
        result.b1 = -2.0 * normalisation;
        result.b2 = result.b0;
    }

    result.a1 = 2.0 * (kSquared - 1.0) * normalisation;
    result.a2 = (1.0 - k / q + kSquared) * normalisation;
    return result;
}
}

template <typename Sample>
ButterworthCoefficients<Sample> designButterworth (FilterType type,
                                                   int order,
                                                   double cutoff,
                                                   double sampleRate)
{
    ButterworthCoefficients<Sample> result;

    if (! std::isfinite (sampleRate) || sampleRate <= 0.0)
        return result;

    order = std::clamp (order, 1, 8);
    cutoff = std::clamp (cutoff, 1.0e-3, sampleRate * 0.499);
    const auto pi = std::acos (-1.0);
    const auto k = std::tan (pi * cutoff / sampleRate);

    if ((order & 1) != 0)
        result.sections[static_cast<std::size_t> (result.sectionCount++)]
            = detail::makeFirstOrder<Sample> (type, k);

    std::array<double, 4> qualityFactors {};
    int qualityFactorCount = 0;

    for (int poleIndex = 0; poleIndex < order / 2; ++poleIndex)
    {
        const auto angle = pi * (0.5 + (2.0 * poleIndex + 1.0) / (2.0 * order));
        qualityFactors[static_cast<std::size_t> (qualityFactorCount++)]
            = -1.0 / (2.0 * std::cos (angle));
    }

    std::sort (qualityFactors.begin(), qualityFactors.begin() + qualityFactorCount);

    for (int index = 0; index < qualityFactorCount; ++index)
        result.sections[static_cast<std::size_t> (result.sectionCount++)]
            = detail::makeSecondOrder<Sample> (
                type, k, qualityFactors[static_cast<std::size_t> (index)]);

    return result;
}

template <typename Sample>
double magnitudeAt (const ButterworthCoefficients<Sample>& coefficients,
                    double frequency,
                    double sampleRate)
{
    if (! std::isfinite (sampleRate) || sampleRate <= 0.0)
        return 1.0;

    const auto pi = std::acos (-1.0);
    const auto zInverse = std::polar (1.0, -2.0 * pi * frequency / sampleRate);
    const auto zInverseSquared = zInverse * zInverse;
    auto response = std::complex<double> { 1.0, 0.0 };

    for (int index = 0; index < coefficients.sectionCount; ++index)
    {
        const auto& section = coefficients.sections[static_cast<std::size_t> (index)];
        const auto numerator = static_cast<double> (section.b0)
                             + static_cast<double> (section.b1) * zInverse
                             + static_cast<double> (section.b2) * zInverseSquared;
        const auto denominator = 1.0
                               + static_cast<double> (section.a1) * zInverse
                               + static_cast<double> (section.a2) * zInverseSquared;
        response *= numerator / denominator;
    }

    return std::abs (response);
}

template <typename Sample>
bool allPolesInsideUnitCircle (const ButterworthCoefficients<Sample>& coefficients)
{
    for (int index = 0; index < coefficients.sectionCount; ++index)
    {
        const auto& section = coefficients.sections[static_cast<std::size_t> (index)];
        const auto a1 = static_cast<double> (section.a1);
        const auto a2 = static_cast<double> (section.a2);
        const auto discriminant = std::complex<double> { a1 * a1 - 4.0 * a2, 0.0 };
        const auto root = std::sqrt (discriminant);
        const auto pole1 = (-a1 + root) * 0.5;
        const auto pole2 = (-a1 - root) * 0.5;

        if (! std::isfinite (std::abs (pole1)) || ! std::isfinite (std::abs (pole2))
            || std::abs (pole1) >= 1.0 || std::abs (pole2) >= 1.0)
            return false;
    }

    return true;
}
}
