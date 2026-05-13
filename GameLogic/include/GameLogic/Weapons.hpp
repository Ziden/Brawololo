#pragma once

#include "GameLogic/FixedPoint.hpp"
#include "GameLogic/Types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace game {

enum class WeaponType : std::uint8_t {
    None,
    Bow,
};

inline constexpr std::size_t kWeaponTypeCount = 2;

struct WeaponDefinition {
    WeaponType type{WeaponType::None};
    TimestampMs warmupMs{};
    Fixed projectileSpeedPerSecond{};
    TimestampMs projectileLifetimeMs{};
    std::int32_t damage{};
    bool serverSpawnedProjectile{};
};

using WeaponDefinitionTable = std::array<WeaponDefinition, kWeaponTypeCount>;

[[nodiscard]] constexpr WeaponDefinition DefaultWeaponDefinition(WeaponType type) noexcept {
    switch (type) {
        case WeaponType::Bow:
            return WeaponDefinition{WeaponType::Bow, 300, PixelsToFixed(900), 2000, 35, true};
        case WeaponType::None:
            return WeaponDefinition{};
    }

    return WeaponDefinition{};
}

[[nodiscard]] constexpr WeaponDefinitionTable DefaultWeaponDefinitions() noexcept {
    return {
        DefaultWeaponDefinition(WeaponType::None),
        DefaultWeaponDefinition(WeaponType::Bow),
    };
}

[[nodiscard]] const WeaponDefinition* FindWeaponDefinition(const WeaponDefinitionTable& definitions,
                                                           WeaponType type) noexcept;

} // namespace game
