#pragma once

#include <array>
#include <string_view>

namespace cutline::parameters
{
inline constexpr auto stateTreeType = "CutlineState";
inline constexpr auto schemaProperty = "schemaVersion";
inline constexpr int currentSchemaVersion = 2;

inline constexpr auto highPassEnabled = "hpEnabled";
inline constexpr auto highPassPoles = "hpPoles";
inline constexpr auto highPassFrequency = "hpFrequency";
inline constexpr auto lowPassEnabled = "lpEnabled";
inline constexpr auto lowPassPoles = "lpPoles";
inline constexpr auto lowPassFrequency = "lpFrequency";
inline constexpr auto outputGain = "outputGain";
inline constexpr auto leftRightSwap = "lrSwap";
inline constexpr auto filterBypass = "filterBypass";
inline constexpr auto mono = "mono";

inline constexpr std::array<std::string_view, 10> ids {
    highPassEnabled,
    highPassPoles,
    highPassFrequency,
    lowPassEnabled,
    lowPassPoles,
    lowPassFrequency,
    outputGain,
    leftRightSwap,
    filterBypass,
    mono
};
}
