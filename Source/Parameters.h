#pragma once

#include <array>
#include <string_view>

namespace cutline::parameters
{
inline constexpr auto stateTreeType = "CutlineState";
inline constexpr auto schemaProperty = "schemaVersion";
inline constexpr int currentSchemaVersion = 1;

inline constexpr auto highPassEnabled = "hpEnabled";
inline constexpr auto highPassPoles = "hpPoles";
inline constexpr auto highPassFrequency = "hpFrequency";
inline constexpr auto lowPassEnabled = "lpEnabled";
inline constexpr auto lowPassPoles = "lpPoles";
inline constexpr auto lowPassFrequency = "lpFrequency";
inline constexpr auto outputGain = "outputGain";

inline constexpr std::array<std::string_view, 7> ids {
    highPassEnabled,
    highPassPoles,
    highPassFrequency,
    lowPassEnabled,
    lowPassPoles,
    lowPassFrequency,
    outputGain
};
}
