#include "StatePersistence.h"

#include "Parameters.h"

#include <array>
#include <cmath>
#include <string_view>

namespace cutline::state
{
namespace
{
struct ParameterRange
{
    std::string_view id;
    double minimum;
    double maximum;
    bool integral;
};

constexpr std::array ranges {
    ParameterRange { parameters::highPassEnabled, 0.0, 1.0, true },
    ParameterRange { parameters::highPassPoles, 0.0, 7.0, true },
    ParameterRange { parameters::highPassFrequency, 20.0, 20000.0, false },
    ParameterRange { parameters::lowPassEnabled, 0.0, 1.0, true },
    ParameterRange { parameters::lowPassPoles, 0.0, 7.0, true },
    ParameterRange { parameters::lowPassFrequency, 20.0, 20000.0, false },
    ParameterRange { parameters::outputGain, -12.0, 12.0, false }
};

bool validateParameters (const juce::ValueTree& candidate)
{
    if (candidate.getNumChildren() != static_cast<int> (ranges.size()))
        return false;

    for (const auto& range : ranges)
    {
        int matchingChildren = 0;

        for (int index = 0; index < candidate.getNumChildren(); ++index)
        {
            const auto child = candidate.getChild (index);
            if (child.getProperty ("id").toString() != range.id.data())
                continue;

            ++matchingChildren;
            const auto valueProperty = child.getProperty ("value");
            if (valueProperty.isVoid() || valueProperty.isUndefined())
                return false;

            const auto value = static_cast<double> (valueProperty);
            if (! std::isfinite (value) || value < range.minimum || value > range.maximum)
                return false;

            if (range.integral && value != std::round (value))
                return false;
        }

        if (matchingChildren != 1)
            return false;
    }

    return true;
}
}

bool validateAndMigrate (juce::ValueTree& candidate)
{
    if (! candidate.isValid() || ! candidate.hasType (parameters::stateTreeType))
        return false;

    auto schemaVersion = 0;
    if (candidate.hasProperty (parameters::schemaProperty))
    {
        const auto schemaProperty = candidate.getProperty (parameters::schemaProperty);
        if (! schemaProperty.isInt() && ! schemaProperty.isInt64())
            return false;
        schemaVersion = static_cast<int> (schemaProperty);
    }

    if (schemaVersion < 0 || schemaVersion > parameters::currentSchemaVersion)
        return false;

    if (! validateParameters (candidate))
        return false;

    if (schemaVersion == 0)
        candidate.setProperty (parameters::schemaProperty,
                               parameters::currentSchemaVersion, nullptr);

    return true;
}

juce::ValueTree readValidatedState (const void* data, int sizeInBytes)
{
    if (data == nullptr || sizeInBytes <= 0)
        return {};

    juce::MemoryInputStream stream (data, static_cast<std::size_t> (sizeInBytes), false);
    auto candidate = juce::ValueTree::readFromStream (stream);
    return validateAndMigrate (candidate) ? candidate : juce::ValueTree {};
}
}
