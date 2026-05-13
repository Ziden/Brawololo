#pragma once

#include "GameLogic/Events.hpp"

#include <cstdint>
#include <optional>
#include <variant>

namespace game {

enum class NetworkEventKind : std::uint8_t {
    PlayerSpawned,
    WeaponWarmupStarted,
    BowFired,
    ProjectileSpawned,
    HitConfirmed,
    EntityEnteredInterest,
    EntityLeftInterest,
};

struct PlayerSpawnedEventDTO {
    NetworkEntityId entityId{};
    ClientId clientId{};
    Fixed x{};
    Fixed y{};
};

struct WeaponWarmupStartedEventDTO {
    NetworkEntityId entityId{};
    TimestampMs startedAtMs{};
    TimestampMs completesAtMs{};
    WeaponType weaponType{WeaponType::None};
};

struct WeaponFiredEventDTO {
    NetworkEntityId entityId{};
    TimestampMs firedAtMs{};
    WeaponType weaponType{WeaponType::None};
};

using BowFiredEventDTO = WeaponFiredEventDTO;

struct ProjectileSpawnedEventDTO {
    NetworkEntityId projectileId{};
    NetworkEntityId ownerId{};
    Fixed x{};
    Fixed y{};
    WeaponType sourceWeapon{WeaponType::None};
};

struct HitConfirmedEventDTO {
    NetworkEntityId attackerId{};
    NetworkEntityId targetId{};
    NetworkEntityId projectileId{};
    TimestampMs serverTimeMs{};
};

struct EntityEnteredInterestEventDTO {
    NetworkEntityId entityId{};
    ClientId observerClientId{};
};

struct EntityLeftInterestEventDTO {
    NetworkEntityId entityId{};
    ClientId observerClientId{};
};

using NetworkEventPayloadDTO = std::variant<
    PlayerSpawnedEventDTO,
    WeaponWarmupStartedEventDTO,
    WeaponFiredEventDTO,
    ProjectileSpawnedEventDTO,
    HitConfirmedEventDTO,
    EntityEnteredInterestEventDTO,
    EntityLeftInterestEventDTO>;

struct NetworkEventDTO {
    NetworkEventId eventId{};
    Tick serverTick{};
    TimestampMs serverTimeMs{};
    NetworkEventPayloadDTO payload{PlayerSpawnedEventDTO{}};
};

struct NetworkEventAckDTO {
    ClientId clientId{};
    NetworkEventId eventId{};
    TimestampMs clientReceivedAtMs{};
};

[[nodiscard]] NetworkEventKind KindForNetworkEvent(const NetworkEventDTO& event) noexcept;
[[nodiscard]] std::optional<NetworkEventDTO> ToNetworkEventDTO(
    NetworkEventId eventId,
    Tick serverTick,
    TimestampMs serverTimeMs,
    const DomainEvent& event);
[[nodiscard]] std::optional<DomainEvent> ToDomainEvent(const NetworkEventDTO& event);

} // namespace game
