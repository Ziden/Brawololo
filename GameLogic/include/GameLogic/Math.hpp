#pragma once

#include "GameLogic/FixedPoint.hpp"

#include <cstdint>

namespace game {

struct Vec2Fixed {
    Fixed x{};
    Fixed y{};
};

struct Vec2Input {
    std::int16_t x{};
    std::int16_t y{};
};

constexpr Vec2Input NormalizeDigitalInput(std::int8_t moveX, std::int8_t moveY) noexcept {
    const auto clampedX = static_cast<std::int8_t>((moveX > 0) - (moveX < 0));
    const auto clampedY = static_cast<std::int8_t>((moveY > 0) - (moveY < 0));

    if (clampedX != 0 && clampedY != 0) {
        return {static_cast<std::int16_t>(clampedX * 707),
                static_cast<std::int16_t>(clampedY * 707)};
    }

    return {static_cast<std::int16_t>(clampedX * 1000), static_cast<std::int16_t>(clampedY * 1000)};
}

} // namespace game
