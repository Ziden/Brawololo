#pragma once

#include "GameLogic/FixedPoint.hpp"
#include "GameLogic/Types.hpp"
#include "GameLogic/Weapons.hpp"

#include <variant>
#include <vector>

namespace game {

struct PlayerSpawned {
    NetworkEntityId entityId{};
    ClientId clientId{};
    Fixed x{};
    Fixed y{};
};

struct PlayerMoved {
    NetworkEntityId entityId{};
    Fixed fromX{};
    Fixed fromY{};
    Fixed toX{};
    Fixed toY{};
};

struct WeaponWarmupStarted {
    NetworkEntityId entityId{};
    TimestampMs startedAtMs{};
    TimestampMs completesAtMs{};
    WeaponType weaponType{WeaponType::None};
};

struct WeaponFired {
    NetworkEntityId entityId{};
    TimestampMs firedAtMs{};
    WeaponType weaponType{WeaponType::None};
};

using BowFired = WeaponFired;

struct ProjectileSpawned {
    NetworkEntityId projectileId{};
    NetworkEntityId ownerId{};
    Fixed x{};
    Fixed y{};
    WeaponType sourceWeapon{WeaponType::None};
};

struct HitConfirmed {
    NetworkEntityId attackerId{};
    NetworkEntityId targetId{};
    NetworkEntityId projectileId{};
    TimestampMs serverTimeMs{};
};

struct PlayerDamaged {
    NetworkEntityId entityId{};
    NetworkEntityId attackerId{};
    std::int32_t damage{};
    std::int32_t healthAfter{};
};

struct PlayerDied {
    NetworkEntityId entityId{};
    NetworkEntityId attackerId{};
    TimestampMs respawnAtMs{};
};

struct PlayerRespawned {
    NetworkEntityId entityId{};
    Fixed x{};
    Fixed y{};
};

struct EntityEnteredInterest {
    NetworkEntityId entityId{};
    ClientId observerClientId{};
};

struct EntityLeftInterest {
    NetworkEntityId entityId{};
    ClientId observerClientId{};
};

struct SnapshotApplied {
    SnapshotId snapshotId{};
    SnapshotId baselineId{};
    CommandSequence ackedInputSequence{};
};

struct LocalPredictionCorrected {
    NetworkEntityId entityId{};
    Fixed predictedX{};
    Fixed predictedY{};
    Fixed authoritativeX{};
    Fixed authoritativeY{};
};

using DomainEvent = std::variant<PlayerSpawned,
                                 PlayerMoved,
                                 WeaponWarmupStarted,
                                 WeaponFired,
                                 ProjectileSpawned,
                                 HitConfirmed,
                                 PlayerDamaged,
                                 PlayerDied,
                                 PlayerRespawned,
                                 EntityEnteredInterest,
                                 EntityLeftInterest,
                                 SnapshotApplied,
                                 LocalPredictionCorrected>;

using EventList = std::vector<DomainEvent>;

} // namespace game
