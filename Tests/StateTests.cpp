#include "Parameters.h"
#include "State/StatePersistence.h"

#include <juce_data_structures/juce_data_structures.h>

#include <cstdlib>
#include <iostream>
#include <limits>
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

juce::ValueTree makeValidState()
{
    juce::ValueTree state (cutline::parameters::stateTreeType);
    state.setProperty (cutline::parameters::schemaProperty,
                       cutline::parameters::currentSchemaVersion, nullptr);

    const auto add = [&state] (const char* id, double value)
    {
        juce::ValueTree parameter ("PARAM");
        parameter.setProperty ("id", id, nullptr);
        parameter.setProperty ("value", value, nullptr);
        state.appendChild (parameter, nullptr);
    };

    add (cutline::parameters::highPassEnabled, 0.0);
    add (cutline::parameters::highPassPoles, 0.0);
    add (cutline::parameters::highPassFrequency, 20.0);
    add (cutline::parameters::lowPassEnabled, 1.0);
    add (cutline::parameters::lowPassPoles, 7.0);
    add (cutline::parameters::lowPassFrequency, 20000.0);
    add (cutline::parameters::outputGain, -3.0);
    return state;
}

void testValidation()
{
    auto state = makeValidState();
    expect (cutline::state::validateAndMigrate (state), "valid schema v1 state must be accepted");

    auto legacy = makeValidState();
    legacy.removeProperty (cutline::parameters::schemaProperty, nullptr);
    expect (cutline::state::validateAndMigrate (legacy), "legacy state without schema must migrate");
    expect (static_cast<int> (legacy[cutline::parameters::schemaProperty]) == 1,
            "legacy migration must add schema version 1");

    auto future = makeValidState();
    future.setProperty (cutline::parameters::schemaProperty, 2, nullptr);
    expect (! cutline::state::validateAndMigrate (future), "unknown future schema must be rejected");

    auto missing = makeValidState();
    missing.removeChild (0, nullptr);
    expect (! cutline::state::validateAndMigrate (missing), "missing parameter must be rejected");

    auto outOfRange = makeValidState();
    outOfRange.getChildWithProperty ("id", cutline::parameters::outputGain)
              .setProperty ("value", 13.0, nullptr);
    expect (! cutline::state::validateAndMigrate (outOfRange), "out-of-range value must be rejected");

    auto nonFinite = makeValidState();
    nonFinite.getChildWithProperty ("id", cutline::parameters::highPassFrequency)
             .setProperty ("value", std::numeric_limits<double>::infinity(), nullptr);
    expect (! cutline::state::validateAndMigrate (nonFinite), "non-finite value must be rejected");

    juce::ValueTree wrongType ("OtherState");
    expect (! cutline::state::validateAndMigrate (wrongType), "wrong root type must be rejected");
}
}

int main()
{
    testValidation();
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
