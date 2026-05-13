#pragma once

#include "GameLogic/FixedPoint.hpp"
#include "GameLogic/Types.hpp"
#include "GameLogic/Weapons.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace game {

enum class SnapshotPriority : std::uint8_t {
    Low,
    Medium,
    High,
};

enum class ReplicatedEntityKind : std::uint8_t {
    Player,
    Projectile,
};

enum class SnapshotDeliveryKind : std::uint8_t {
    Full,
    DeltaEligible,
};

struct MovementStateDTO {
    NetworkEntityId entityId{};
    Fixed x{};
    Fixed y{};
    Fixed vx{};
    Fixed vy{};
    std::int16_t aimX{1000};
    std::int16_t aimY{};
};

struct CombatStateDTO {
    NetworkEntityId entityId{};
    std::int32_t health{100};
    WeaponType weaponType{WeaponType::None};
    bool weaponWarming{};
    TimestampMs weaponWarmupCompletesAtMs{};
    bool serverOwnedProjectile{};
    NetworkEntityId projectileOwnerId{};
};

struct ReplicationStateDTO {
    NetworkEntityId entityId{};
    ClientId ownerClientId{};
    ReplicatedEntityKind kind{ReplicatedEntityKind::Player};
    SnapshotPriority priority{SnapshotPriority::Medium};
};

struct EntityStateDTO {
    MovementStateDTO movement{};
    std::optional<CombatStateDTO> combat{};
    ReplicationStateDTO replication{};
};

struct SnapshotDTO {
    SnapshotId snapshotId{};
    SnapshotId baselineId{};
    SnapshotDeliveryKind deliveryKind{SnapshotDeliveryKind::Full};
    Tick serverTick{};
    TimestampMs serverTimeMs{};
    CommandSequence ackedInputSequence{};
    std::vector<EntityStateDTO> entities{};
};

struct SnapshotAckDTO {
    ClientId clientId{};
    SnapshotId snapshotId{};
    SnapshotId baselineId{};
    CommandSequence ackedInputSequence{};
    TimestampMs clientReceivedAtMs{};
};

} // namespace game
