#pragma once

#include <algorithm>
#include <cstdint>

namespace game {

using Fixed = std::int32_t;

inline constexpr Fixed kFixedOne = 1000;

constexpr Fixed PixelsToFixed(std::int32_t pixels) noexcept
{
    return pixels * kFixedOne;
}

constexpr std::int32_t FixedToPixels(Fixed value) noexcept
{
    return value / kFixedOne;
}

constexpr Fixed FixedMulRatio(Fixed value, std::int32_t numerator, std::int32_t denominator) noexcept
{
    return static_cast<Fixed>((static_cast<std::int64_t>(value) * numerator) / denominator);
}

constexpr Fixed ClampFixed(Fixed value, Fixed minValue, Fixed maxValue) noexcept
{
    return std::clamp(value, minValue, maxValue);
}

} // namespace game

