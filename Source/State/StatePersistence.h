#pragma once

#include <juce_data_structures/juce_data_structures.h>

namespace cutline::state
{
bool validateAndMigrate (juce::ValueTree& candidate);
juce::ValueTree readValidatedState (const void* data, int sizeInBytes);
}
