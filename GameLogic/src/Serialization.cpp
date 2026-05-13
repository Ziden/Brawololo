#include "GameLogic/Serialization.hpp"

#include <utility>

namespace game {
namespace {

constexpr std::size_t kMinSerializedEntityBytes =
    sizeof(std::uint32_t) + (sizeof(Fixed) * 4U) + (sizeof(std::int16_t) * 2U) +
    sizeof(std::uint8_t) + sizeof(std::uint32_t) + sizeof(ClientId) + (sizeof(std::uint8_t) * 2U);

template <typename T> std::optional<T> FinishDecode(BinaryReader& reader, T value) {
    if (!reader.Finish()) {
        return std::nullopt;
    }

    return std::move(value);
}

std::optional<WeaponType> ReadWeaponType(BinaryReader& reader) {
    return reader.ReadEnum<WeaponType>(
        static_cast<std::underlying_type_t<WeaponType>>(kWeaponTypeCount - 1U));
}

std::optional<ReplicatedEntityKind> ReadReplicatedEntityKind(BinaryReader& reader) {
    return reader.ReadEnum<ReplicatedEntityKind>(
        static_cast<std::underlying_type_t<ReplicatedEntityKind>>(
            ReplicatedEntityKind::Projectile));
}

std::optional<SnapshotPriority> ReadSnapshotPriority(BinaryReader& reader) {
    return reader.ReadEnum<SnapshotPriority>(
        static_cast<std::underlying_type_t<SnapshotPriority>>(SnapshotPriority::High));
}

std::optional<SnapshotDeliveryKind> ReadSnapshotDeliveryKind(BinaryReader& reader) {
    return reader.ReadEnum<SnapshotDeliveryKind>(
        static_cast<std::underlying_type_t<SnapshotDeliveryKind>>(
            SnapshotDeliveryKind::DeltaEligible));
}

std::optional<NetworkEventKind> ReadNetworkEventKind(BinaryReader& reader) {
    return reader.ReadEnum<NetworkEventKind>(static_cast<std::underlying_type_t<NetworkEventKind>>(
        NetworkEventKind::EntityLeftInterest));
}

void WriteEntityId(BinaryWriter& writer, NetworkEntityId id) {
    writer.WritePod(id.value);
}

std::optional<NetworkEntityId> ReadEntityId(BinaryReader& reader) {
    const auto value = reader.ReadPod<std::uint32_t>();
    if (!value.has_value()) {
        return std::nullopt;
    }
    return NetworkEntityId{*value};
}

void WriteMovement(BinaryWriter& writer, const MovementStateDTO& movement) {
    WriteEntityId(writer, movement.entityId);
    writer.WritePod(movement.x);
    writer.WritePod(movement.y);
    writer.WritePod(movement.vx);
    writer.WritePod(movement.vy);
    writer.WritePod(movement.aimX);
    writer.WritePod(movement.aimY);
}

std::optional<MovementStateDTO> ReadMovement(BinaryReader& reader) {
    MovementStateDTO movement{};
    const auto entityId = ReadEntityId(reader);
    const auto x = reader.ReadPod<Fixed>();
    const auto y = reader.ReadPod<Fixed>();
    const auto vx = reader.ReadPod<Fixed>();
    const auto vy = reader.ReadPod<Fixed>();
    const auto aimX = reader.ReadPod<std::int16_t>();
    const auto aimY = reader.ReadPod<std::int16_t>();

    if (!entityId.has_value() || !x.has_value() || !y.has_value() || !vx.has_value() ||
        !vy.has_value() || !aimX.has_value() || !aimY.has_value()) {
        return std::nullopt;
    }

    movement.entityId = *entityId;
    movement.x = *x;
    movement.y = *y;
    movement.vx = *vx;
    movement.vy = *vy;
    movement.aimX = *aimX;
    movement.aimY = *aimY;
    return movement;
}

void WriteCombat(BinaryWriter& writer, const CombatStateDTO& combat) {
    WriteEntityId(writer, combat.entityId);
    writer.WritePod(combat.health);
    writer.WritePod(combat.maxHealth);
    writer.WritePod(static_cast<std::uint8_t>(combat.weaponType));
    writer.WritePod(static_cast<std::uint8_t>(combat.weaponWarming ? 1 : 0));
    writer.WritePod(combat.weaponWarmupCompletesAtMs);
    writer.WritePod(static_cast<std::uint8_t>(combat.defeated ? 1 : 0));
    writer.WritePod(combat.respawnAtMs);
    writer.WritePod(static_cast<std::uint8_t>(combat.serverOwnedProjectile ? 1 : 0));
    WriteEntityId(writer, combat.projectileOwnerId);
}

std::optional<CombatStateDTO> ReadCombat(BinaryReader& reader) {
    CombatStateDTO combat{};
    const auto entityId = ReadEntityId(reader);
    const auto health = reader.ReadPod<std::int32_t>();
    const auto maxHealth = reader.ReadPod<std::int32_t>();
    const auto weaponType = ReadWeaponType(reader);
    const auto weaponWarming = reader.ReadBool();
    const auto weaponWarmupCompletesAtMs = reader.ReadPod<TimestampMs>();
    const auto defeated = reader.ReadBool();
    const auto respawnAtMs = reader.ReadPod<TimestampMs>();
    const auto serverOwnedProjectile = reader.ReadBool();
    const auto projectileOwnerId = ReadEntityId(reader);

    if (!entityId.has_value() || !health.has_value() || !maxHealth.has_value() ||
        !weaponType.has_value() || !weaponWarming.has_value() ||
        !weaponWarmupCompletesAtMs.has_value() || !defeated.has_value() ||
        !respawnAtMs.has_value() || !serverOwnedProjectile.has_value() ||
        !projectileOwnerId.has_value()) {
        return std::nullopt;
    }

    combat.entityId = *entityId;
    combat.health = *health;
    combat.maxHealth = *maxHealth;
    combat.weaponType = *weaponType;
    combat.weaponWarming = *weaponWarming;
    combat.weaponWarmupCompletesAtMs = *weaponWarmupCompletesAtMs;
    combat.defeated = *defeated;
    combat.respawnAtMs = *respawnAtMs;
    combat.serverOwnedProjectile = *serverOwnedProjectile;
    combat.projectileOwnerId = *projectileOwnerId;
    return combat;
}

void WriteReplication(BinaryWriter& writer, const ReplicationStateDTO& replication) {
    WriteEntityId(writer, replication.entityId);
    writer.WritePod(replication.ownerClientId);
    writer.WritePod(static_cast<std::uint8_t>(replication.kind));
    writer.WritePod(static_cast<std::uint8_t>(replication.priority));
}

std::optional<ReplicationStateDTO> ReadReplication(BinaryReader& reader) {
    ReplicationStateDTO replication{};
    const auto entityId = ReadEntityId(reader);
    const auto ownerClientId = reader.ReadPod<ClientId>();
    const auto kind = ReadReplicatedEntityKind(reader);
    const auto priority = ReadSnapshotPriority(reader);

    if (!entityId.has_value() || !ownerClientId.has_value() || !kind.has_value() ||
        !priority.has_value()) {
        return std::nullopt;
    }

    replication.entityId = *entityId;
    replication.ownerClientId = *ownerClientId;
    replication.kind = *kind;
    replication.priority = *priority;
    return replication;
}

} // namespace

const std::vector<std::byte>& BinaryWriter::Bytes() const noexcept {
    return bytes_;
}

BinaryReader::BinaryReader(std::span<const std::byte> bytes) : bytes_(bytes) {}

std::vector<std::byte> SerializeMovementState(const MovementStateDTO& dto) {
    BinaryWriter writer{};
    WriteMovement(writer, dto);
    return writer.Bytes();
}

std::optional<MovementStateDTO> DeserializeMovementState(std::span<const std::byte> bytes) {
    BinaryReader reader{bytes};
    const auto movement = ReadMovement(reader);
    if (!movement.has_value()) {
        return std::nullopt;
    }

    return FinishDecode(reader, *movement);
}

std::vector<std::byte> SerializeLoginCommand(const LoginCommand& command) {
    BinaryWriter writer{};
    writer.WritePod(command.header.clientId);
    writer.WritePod(command.header.sequence);
    writer.WritePod(command.header.clientTimestampMs);
    return writer.Bytes();
}

std::optional<LoginCommand> DeserializeLoginCommand(std::span<const std::byte> bytes) {
    BinaryReader reader{bytes};
    LoginCommand command{};
    const auto clientId = reader.ReadPod<ClientId>();
    const auto sequence = reader.ReadPod<CommandSequence>();
    const auto clientTimestampMs = reader.ReadPod<TimestampMs>();

    if (!clientId.has_value() || !sequence.has_value() || !clientTimestampMs.has_value()) {
        return std::nullopt;
    }

    command.header.clientId = *clientId;
    command.header.sequence = *sequence;
    command.header.clientTimestampMs = *clientTimestampMs;
    return FinishDecode(reader, command);
}

std::vector<std::byte> SerializeClientInputPacket(const ClientInputPacket& packet) {
    BinaryWriter writer{};
    writer.WritePod(packet.header.clientId);
    writer.WritePod(packet.header.sequence);
    writer.WritePod(packet.header.clientTimestampMs);
    writer.WritePod(packet.input.moveX);
    writer.WritePod(packet.input.moveY);
    writer.WritePod(packet.input.aimX);
    writer.WritePod(packet.input.aimY);
    writer.WritePod(static_cast<std::uint8_t>(packet.input.fire ? 1 : 0));
    return writer.Bytes();
}

std::optional<ClientInputPacket> DeserializeClientInputPacket(std::span<const std::byte> bytes) {
    BinaryReader reader{bytes};
    ClientInputPacket packet{};
    const auto clientId = reader.ReadPod<ClientId>();
    const auto sequence = reader.ReadPod<CommandSequence>();
    const auto clientTimestampMs = reader.ReadPod<TimestampMs>();
    const auto moveX = reader.ReadPod<std::int8_t>();
    const auto moveY = reader.ReadPod<std::int8_t>();
    const auto aimX = reader.ReadPod<std::int16_t>();
    const auto aimY = reader.ReadPod<std::int16_t>();
    const auto fire = reader.ReadBool();

    if (!clientId.has_value() || !sequence.has_value() || !clientTimestampMs.has_value() ||
        !moveX.has_value() || !moveY.has_value() || !aimX.has_value() || !aimY.has_value() ||
        !fire.has_value()) {
        return std::nullopt;
    }

    packet.header.clientId = *clientId;
    packet.header.sequence = *sequence;
    packet.header.clientTimestampMs = *clientTimestampMs;
    packet.input.moveX = *moveX;
    packet.input.moveY = *moveY;
    packet.input.aimX = *aimX;
    packet.input.aimY = *aimY;
    packet.input.fire = *fire;
    return FinishDecode(reader, packet);
}

std::vector<std::byte> SerializeTimeSyncRequest(const TimeSyncRequest& request) {
    BinaryWriter writer{};
    writer.WritePod(request.clientId);
    writer.WritePod(request.sequence);
    writer.WritePod(request.clientSentAtMs);
    return writer.Bytes();
}

std::optional<TimeSyncRequest> DeserializeTimeSyncRequest(std::span<const std::byte> bytes) {
    BinaryReader reader{bytes};
    TimeSyncRequest request{};
    const auto clientId = reader.ReadPod<ClientId>();
    const auto sequence = reader.ReadPod<CommandSequence>();
    const auto clientSentAtMs = reader.ReadPod<TimestampMs>();

    if (!clientId.has_value() || !sequence.has_value() || !clientSentAtMs.has_value()) {
        return std::nullopt;
    }

    request.clientId = *clientId;
    request.sequence = *sequence;
    request.clientSentAtMs = *clientSentAtMs;
    return FinishDecode(reader, request);
}

std::vector<std::byte> SerializeTimeSyncResponse(const TimeSyncResponse& response) {
    BinaryWriter writer{};
    writer.WritePod(response.clientId);
    writer.WritePod(response.sequence);
    writer.WritePod(response.clientSentAtMs);
    writer.WritePod(response.serverReceivedAtMs);
    writer.WritePod(response.serverSentAtMs);
    return writer.Bytes();
}

std::optional<TimeSyncResponse> DeserializeTimeSyncResponse(std::span<const std::byte> bytes) {
    BinaryReader reader{bytes};
    TimeSyncResponse response{};
    const auto clientId = reader.ReadPod<ClientId>();
    const auto sequence = reader.ReadPod<CommandSequence>();
    const auto clientSentAtMs = reader.ReadPod<TimestampMs>();
    const auto serverReceivedAtMs = reader.ReadPod<TimestampMs>();
    const auto serverSentAtMs = reader.ReadPod<TimestampMs>();

    if (!clientId.has_value() || !sequence.has_value() || !clientSentAtMs.has_value() ||
        !serverReceivedAtMs.has_value() || !serverSentAtMs.has_value()) {
        return std::nullopt;
    }

    response.clientId = *clientId;
    response.sequence = *sequence;
    response.clientSentAtMs = *clientSentAtMs;
    response.serverReceivedAtMs = *serverReceivedAtMs;
    response.serverSentAtMs = *serverSentAtMs;
    return FinishDecode(reader, response);
}

std::vector<std::byte> SerializeNetworkEvent(const NetworkEventDTO& event) {
    BinaryWriter writer{};
    writer.WritePod(event.eventId);
    writer.WritePod(event.serverTick);
    writer.WritePod(event.serverTimeMs);
    writer.WritePod(static_cast<std::uint8_t>(KindForNetworkEvent(event)));

    if (const auto* spawned = std::get_if<PlayerSpawnedEventDTO>(&event.payload);
        spawned != nullptr) {
        WriteEntityId(writer, spawned->entityId);
        writer.WritePod(spawned->clientId);
        writer.WritePod(spawned->x);
        writer.WritePod(spawned->y);
    } else if (const auto* warmup = std::get_if<WeaponWarmupStartedEventDTO>(&event.payload);
               warmup != nullptr) {
        WriteEntityId(writer, warmup->entityId);
        writer.WritePod(warmup->startedAtMs);
        writer.WritePod(warmup->completesAtMs);
        writer.WritePod(static_cast<std::uint8_t>(warmup->weaponType));
    } else if (const auto* fired = std::get_if<BowFiredEventDTO>(&event.payload);
               fired != nullptr) {
        WriteEntityId(writer, fired->entityId);
        writer.WritePod(fired->firedAtMs);
        writer.WritePod(static_cast<std::uint8_t>(fired->weaponType));
    } else if (const auto* projectile = std::get_if<ProjectileSpawnedEventDTO>(&event.payload);
               projectile != nullptr) {
        WriteEntityId(writer, projectile->projectileId);
        WriteEntityId(writer, projectile->ownerId);
        writer.WritePod(projectile->x);
        writer.WritePod(projectile->y);
        writer.WritePod(static_cast<std::uint8_t>(projectile->sourceWeapon));
    } else if (const auto* hit = std::get_if<HitConfirmedEventDTO>(&event.payload);
               hit != nullptr) {
        WriteEntityId(writer, hit->attackerId);
        WriteEntityId(writer, hit->targetId);
        WriteEntityId(writer, hit->projectileId);
        writer.WritePod(hit->serverTimeMs);
    } else if (const auto* damaged = std::get_if<PlayerDamagedEventDTO>(&event.payload);
               damaged != nullptr) {
        WriteEntityId(writer, damaged->entityId);
        WriteEntityId(writer, damaged->attackerId);
        writer.WritePod(damaged->damage);
        writer.WritePod(damaged->healthAfter);
    } else if (const auto* died = std::get_if<PlayerDiedEventDTO>(&event.payload);
               died != nullptr) {
        WriteEntityId(writer, died->entityId);
        WriteEntityId(writer, died->attackerId);
        writer.WritePod(died->respawnAtMs);
    } else if (const auto* respawned = std::get_if<PlayerRespawnedEventDTO>(&event.payload);
               respawned != nullptr) {
        WriteEntityId(writer, respawned->entityId);
        writer.WritePod(respawned->x);
        writer.WritePod(respawned->y);
    } else if (const auto* entered = std::get_if<EntityEnteredInterestEventDTO>(&event.payload);
               entered != nullptr) {
        WriteEntityId(writer, entered->entityId);
        writer.WritePod(entered->observerClientId);
    } else if (const auto* left = std::get_if<EntityLeftInterestEventDTO>(&event.payload);
               left != nullptr) {
        WriteEntityId(writer, left->entityId);
        writer.WritePod(left->observerClientId);
    }

    return writer.Bytes();
}

std::optional<NetworkEventDTO> DeserializeNetworkEvent(std::span<const std::byte> bytes) {
    BinaryReader reader{bytes};
    NetworkEventDTO event{};
    const auto eventId = reader.ReadPod<NetworkEventId>();
    const auto serverTick = reader.ReadPod<Tick>();
    const auto serverTimeMs = reader.ReadPod<TimestampMs>();
    const auto kind = ReadNetworkEventKind(reader);

    if (!eventId.has_value() || !serverTick.has_value() || !serverTimeMs.has_value() ||
        !kind.has_value()) {
        return std::nullopt;
    }

    event.eventId = *eventId;
    event.serverTick = *serverTick;
    event.serverTimeMs = *serverTimeMs;

    switch (*kind) {
        case NetworkEventKind::PlayerSpawned: {
            const auto entityId = ReadEntityId(reader);
            const auto clientId = reader.ReadPod<ClientId>();
            const auto x = reader.ReadPod<Fixed>();
            const auto y = reader.ReadPod<Fixed>();
            if (!entityId.has_value() || !clientId.has_value() || !x.has_value() ||
                !y.has_value()) {
                return std::nullopt;
            }
            event.payload = PlayerSpawnedEventDTO{*entityId, *clientId, *x, *y};
            break;
        }
        case NetworkEventKind::WeaponWarmupStarted: {
            const auto entityId = ReadEntityId(reader);
            const auto startedAtMs = reader.ReadPod<TimestampMs>();
            const auto completesAtMs = reader.ReadPod<TimestampMs>();
            const auto weaponType = ReadWeaponType(reader);
            if (!entityId.has_value() || !startedAtMs.has_value() || !completesAtMs.has_value() ||
                !weaponType.has_value()) {
                return std::nullopt;
            }
            event.payload =
                WeaponWarmupStartedEventDTO{*entityId, *startedAtMs, *completesAtMs, *weaponType};
            break;
        }
        case NetworkEventKind::BowFired: {
            const auto entityId = ReadEntityId(reader);
            const auto firedAtMs = reader.ReadPod<TimestampMs>();
            const auto weaponType = ReadWeaponType(reader);
            if (!entityId.has_value() || !firedAtMs.has_value() || !weaponType.has_value()) {
                return std::nullopt;
            }
            event.payload = BowFiredEventDTO{*entityId, *firedAtMs, *weaponType};
            break;
        }
        case NetworkEventKind::ProjectileSpawned: {
            const auto projectileId = ReadEntityId(reader);
            const auto ownerId = ReadEntityId(reader);
            const auto x = reader.ReadPod<Fixed>();
            const auto y = reader.ReadPod<Fixed>();
            const auto sourceWeapon = ReadWeaponType(reader);
            if (!projectileId.has_value() || !ownerId.has_value() || !x.has_value() ||
                !y.has_value() || !sourceWeapon.has_value()) {
                return std::nullopt;
            }
            event.payload =
                ProjectileSpawnedEventDTO{*projectileId, *ownerId, *x, *y, *sourceWeapon};
            break;
        }
        case NetworkEventKind::HitConfirmed: {
            const auto attackerId = ReadEntityId(reader);
            const auto targetId = ReadEntityId(reader);
            const auto projectileId = ReadEntityId(reader);
            const auto hitServerTimeMs = reader.ReadPod<TimestampMs>();
            if (!attackerId.has_value() || !targetId.has_value() || !projectileId.has_value() ||
                !hitServerTimeMs.has_value()) {
                return std::nullopt;
            }
            event.payload =
                HitConfirmedEventDTO{*attackerId, *targetId, *projectileId, *hitServerTimeMs};
            break;
        }
        case NetworkEventKind::PlayerDamaged: {
            const auto entityId = ReadEntityId(reader);
            const auto attackerId = ReadEntityId(reader);
            const auto damage = reader.ReadPod<std::int32_t>();
            const auto healthAfter = reader.ReadPod<std::int32_t>();
            if (!entityId.has_value() || !attackerId.has_value() || !damage.has_value() ||
                !healthAfter.has_value()) {
                return std::nullopt;
            }
            event.payload = PlayerDamagedEventDTO{*entityId, *attackerId, *damage, *healthAfter};
            break;
        }
        case NetworkEventKind::PlayerDied: {
            const auto entityId = ReadEntityId(reader);
            const auto attackerId = ReadEntityId(reader);
            const auto respawnAtMs = reader.ReadPod<TimestampMs>();
            if (!entityId.has_value() || !attackerId.has_value() || !respawnAtMs.has_value()) {
                return std::nullopt;
            }
            event.payload = PlayerDiedEventDTO{*entityId, *attackerId, *respawnAtMs};
            break;
        }
        case NetworkEventKind::PlayerRespawned: {
            const auto entityId = ReadEntityId(reader);
            const auto x = reader.ReadPod<Fixed>();
            const auto y = reader.ReadPod<Fixed>();
            if (!entityId.has_value() || !x.has_value() || !y.has_value()) {
                return std::nullopt;
            }
            event.payload = PlayerRespawnedEventDTO{*entityId, *x, *y};
            break;
        }
        case NetworkEventKind::EntityEnteredInterest: {
            const auto entityId = ReadEntityId(reader);
            const auto observerClientId = reader.ReadPod<ClientId>();
            if (!entityId.has_value() || !observerClientId.has_value()) {
                return std::nullopt;
            }
            event.payload = EntityEnteredInterestEventDTO{*entityId, *observerClientId};
            break;
        }
        case NetworkEventKind::EntityLeftInterest: {
            const auto entityId = ReadEntityId(reader);
            const auto observerClientId = reader.ReadPod<ClientId>();
            if (!entityId.has_value() || !observerClientId.has_value()) {
                return std::nullopt;
            }
            event.payload = EntityLeftInterestEventDTO{*entityId, *observerClientId};
            break;
        }
    }

    return FinishDecode(reader, event);
}

std::vector<std::byte> SerializeNetworkEventAck(const NetworkEventAckDTO& ack) {
    BinaryWriter writer{};
    writer.WritePod(ack.clientId);
    writer.WritePod(ack.eventId);
    writer.WritePod(ack.clientReceivedAtMs);
    return writer.Bytes();
}

std::optional<NetworkEventAckDTO> DeserializeNetworkEventAck(std::span<const std::byte> bytes) {
    BinaryReader reader{bytes};
    NetworkEventAckDTO ack{};
    const auto clientId = reader.ReadPod<ClientId>();
    const auto eventId = reader.ReadPod<NetworkEventId>();
    const auto clientReceivedAtMs = reader.ReadPod<TimestampMs>();

    if (!clientId.has_value() || !eventId.has_value() || !clientReceivedAtMs.has_value()) {
        return std::nullopt;
    }

    ack.clientId = *clientId;
    ack.eventId = *eventId;
    ack.clientReceivedAtMs = *clientReceivedAtMs;
    return FinishDecode(reader, ack);
}

std::vector<std::byte> SerializeSnapshotAck(const SnapshotAckDTO& ack) {
    BinaryWriter writer{};
    writer.WritePod(ack.clientId);
    writer.WritePod(ack.snapshotId);
    writer.WritePod(ack.baselineId);
    writer.WritePod(ack.ackedInputSequence);
    writer.WritePod(ack.clientReceivedAtMs);
    return writer.Bytes();
}

std::optional<SnapshotAckDTO> DeserializeSnapshotAck(std::span<const std::byte> bytes) {
    BinaryReader reader{bytes};
    SnapshotAckDTO ack{};
    const auto clientId = reader.ReadPod<ClientId>();
    const auto snapshotId = reader.ReadPod<SnapshotId>();
    const auto baselineId = reader.ReadPod<SnapshotId>();
    const auto ackedInputSequence = reader.ReadPod<CommandSequence>();
    const auto clientReceivedAtMs = reader.ReadPod<TimestampMs>();

    if (!clientId.has_value() || !snapshotId.has_value() || !baselineId.has_value() ||
        !ackedInputSequence.has_value() || !clientReceivedAtMs.has_value()) {
        return std::nullopt;
    }

    ack.clientId = *clientId;
    ack.snapshotId = *snapshotId;
    ack.baselineId = *baselineId;
    ack.ackedInputSequence = *ackedInputSequence;
    ack.clientReceivedAtMs = *clientReceivedAtMs;
    return FinishDecode(reader, ack);
}

std::vector<std::byte> SerializeSnapshot(const SnapshotDTO& snapshot) {
    BinaryWriter writer{};
    writer.WritePod(snapshot.snapshotId);
    writer.WritePod(snapshot.baselineId);
    writer.WritePod(static_cast<std::uint8_t>(snapshot.deliveryKind));
    writer.WritePod(snapshot.serverTick);
    writer.WritePod(snapshot.serverTimeMs);
    writer.WritePod(snapshot.ackedInputSequence);
    writer.WritePod(static_cast<std::uint32_t>(snapshot.entities.size()));

    for (const auto& entity : snapshot.entities) {
        WriteMovement(writer, entity.movement);
        writer.WritePod(static_cast<std::uint8_t>(entity.combat.has_value() ? 1 : 0));
        if (entity.combat.has_value()) {
            WriteCombat(writer, *entity.combat);
        }
        WriteReplication(writer, entity.replication);
    }

    return writer.Bytes();
}

std::optional<SnapshotDTO> DeserializeSnapshot(std::span<const std::byte> bytes) {
    BinaryReader reader{bytes};
    SnapshotDTO snapshot{};
    const auto snapshotId = reader.ReadPod<SnapshotId>();
    const auto baselineId = reader.ReadPod<SnapshotId>();
    const auto deliveryKind = ReadSnapshotDeliveryKind(reader);
    const auto serverTick = reader.ReadPod<Tick>();
    const auto serverTimeMs = reader.ReadPod<TimestampMs>();
    const auto ackedInputSequence = reader.ReadPod<CommandSequence>();
    const auto entityCount = reader.ReadCount(reader.RemainingBytes() / kMinSerializedEntityBytes);

    if (!snapshotId.has_value() || !baselineId.has_value() || !deliveryKind.has_value() ||
        !serverTick.has_value() || !serverTimeMs.has_value() || !ackedInputSequence.has_value() ||
        !entityCount.has_value()) {
        return std::nullopt;
    }

    snapshot.snapshotId = *snapshotId;
    snapshot.baselineId = *baselineId;
    snapshot.deliveryKind = *deliveryKind;
    snapshot.serverTick = *serverTick;
    snapshot.serverTimeMs = *serverTimeMs;
    snapshot.ackedInputSequence = *ackedInputSequence;
    snapshot.entities.reserve(*entityCount);

    for (std::uint32_t index = 0; index < *entityCount; ++index) {
        EntityStateDTO entity{};
        const auto movement = ReadMovement(reader);
        const auto hasCombat = reader.ReadBool();
        if (!movement.has_value() || !hasCombat.has_value()) {
            return std::nullopt;
        }

        entity.movement = *movement;
        if (*hasCombat) {
            const auto combat = ReadCombat(reader);
            if (!combat.has_value()) {
                return std::nullopt;
            }
            entity.combat = *combat;
        }

        const auto replication = ReadReplication(reader);
        if (!replication.has_value()) {
            return std::nullopt;
        }
        entity.replication = *replication;
        snapshot.entities.push_back(entity);
    }

    return FinishDecode(reader, std::move(snapshot));
}

} // namespace game
