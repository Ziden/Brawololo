#pragma once

#include "GameLogic/FixedPoint.hpp"
#include "GameLogic/Types.hpp"
#include "GameLogic/Weapons.hpp"

#include <cstdint>

namespace game {

enum class NetworkReplicationMode : std::uint8_t {
    NotReplicated,
    OwnerOnly,
    AreaOfInterest,
    GlobalReliable,
};

struct NetworkIdentityComponent {
    NetworkEntityId id{};
};

struct TransformComponent {
    Fixed x{};
    Fixed y{};
};

struct VelocityComponent {
    Fixed vx{};
    Fixed vy{};
};

struct AimComponent {
    std::int16_t x{1000};
    std::int16_t y{};
};

struct InputIntentComponent {
    std::int8_t moveX{};
    std::int8_t moveY{};
    std::int16_t aimX{1000};
    std::int16_t aimY{};
    bool fire{};
};

struct PlayerComponent {
    ClientId clientId{};
    std::int32_t health{100};
    std::int32_t maxHealth{100};
    bool defeated{};
    TimestampMs respawnAtMs{};
};

struct WeaponStateComponent {
    WeaponType type{WeaponType::Bow};
    bool warming{};
    TimestampMs warmupStartedAtMs{};
    TimestampMs warmupCompletesAtMs{};
};

using BowComponent = WeaponStateComponent;

struct ProjectileComponent {
    NetworkEntityId ownerId{};
    TimestampMs spawnedAtMs{};
    TimestampMs expiresAtMs{};
    WeaponType sourceWeapon{WeaponType::None};
};

struct OwnedByPlayerComponent {
    ClientId clientId{};
};

struct NetworkReplicationComponent {
    NetworkReplicationMode mode{NetworkReplicationMode::AreaOfInterest};
};

struct PredictedTag {};
struct InterpolatedTag {};
struct AuthoritativeTag {};

} // namespace game
