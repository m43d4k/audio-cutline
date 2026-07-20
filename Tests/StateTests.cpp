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
    add (cutline::parameters::leftRightSwap, 1.0);
    add (cutline::parameters::filterBypass, 0.0);
    add (cutline::parameters::mono, 1.0);
    return state;
}

void testValidation()
{
    auto state = makeValidState();
    expect (cutline::state::validateAndMigrate (state), "valid current-schema state must be accepted");

    auto legacy = makeValidState();
    legacy.setProperty (cutline::parameters::schemaProperty, 1, nullptr);
    legacy.removeChild (legacy.getChildWithProperty ("id", cutline::parameters::leftRightSwap), nullptr);
    legacy.removeChild (legacy.getChildWithProperty ("id", cutline::parameters::filterBypass), nullptr);
    legacy.removeChild (legacy.getChildWithProperty ("id", cutline::parameters::mono), nullptr);
    expect (cutline::state::validateAndMigrate (legacy), "schema v1 state must migrate");
    expect (static_cast<int> (legacy[cutline::parameters::schemaProperty])
                == cutline::parameters::currentSchemaVersion,
            "schema v1 migration must update the schema version");
    expect (legacy.getChildWithProperty ("id", cutline::parameters::leftRightSwap)
                  .getProperty ("value") == juce::var { 0.0 }
            && legacy.getChildWithProperty ("id", cutline::parameters::filterBypass)
                   .getProperty ("value") == juce::var { 0.0 }
            && legacy.getChildWithProperty ("id", cutline::parameters::mono)
                   .getProperty ("value") == juce::var { 0.0 },
            "schema v1 migration must add disabled utility parameters");

    auto unversioned = legacy.createCopy();
    unversioned.removeProperty (cutline::parameters::schemaProperty, nullptr);
    unversioned.removeChild (unversioned.getChildWithProperty ("id", cutline::parameters::leftRightSwap), nullptr);
    unversioned.removeChild (unversioned.getChildWithProperty ("id", cutline::parameters::filterBypass), nullptr);
    unversioned.removeChild (unversioned.getChildWithProperty ("id", cutline::parameters::mono), nullptr);
    expect (cutline::state::validateAndMigrate (unversioned), "unversioned state must migrate");

    auto future = makeValidState();
    future.setProperty (cutline::parameters::schemaProperty,
                        cutline::parameters::currentSchemaVersion + 1, nullptr);
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
