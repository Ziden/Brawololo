#include "GameLogic/NetworkEvents.hpp"

namespace game {

NetworkEventKind KindForNetworkEvent(const NetworkEventDTO& event) noexcept
{
    return static_cast<NetworkEventKind>(event.payload.index());
}

std::optional<NetworkEventDTO> ToNetworkEventDTO(
    NetworkEventId eventId,
    Tick serverTick,
    TimestampMs serverTimeMs,
    const DomainEvent& event)
{
    NetworkEventDTO dto{};
    dto.eventId = eventId;
    dto.serverTick = serverTick;
    dto.serverTimeMs = serverTimeMs;

    if (const auto* spawned = std::get_if<PlayerSpawned>(&event); spawned != nullptr) {
        dto.payload = PlayerSpawnedEventDTO{
            spawned->entityId,
            spawned->clientId,
            spawned->x,
            spawned->y};
        return dto;
    }

    if (const auto* warmup = std::get_if<WeaponWarmupStarted>(&event); warmup != nullptr) {
        dto.payload = WeaponWarmupStartedEventDTO{
            warmup->entityId,
            warmup->startedAtMs,
            warmup->completesAtMs,
            warmup->weaponType};
        return dto;
    }

    if (const auto* fired = std::get_if<BowFired>(&event); fired != nullptr) {
        dto.payload = BowFiredEventDTO{fired->entityId, fired->firedAtMs, fired->weaponType};
        return dto;
    }

    if (const auto* projectile = std::get_if<ProjectileSpawned>(&event); projectile != nullptr) {
        dto.payload = ProjectileSpawnedEventDTO{
            projectile->projectileId,
            projectile->ownerId,
            projectile->x,
            projectile->y,
            projectile->sourceWeapon};
        return dto;
    }

    if (const auto* hit = std::get_if<HitConfirmed>(&event); hit != nullptr) {
        dto.payload = HitConfirmedEventDTO{
            hit->attackerId,
            hit->targetId,
            hit->projectileId,
            hit->serverTimeMs};
        return dto;
    }

    if (const auto* damaged = std::get_if<PlayerDamaged>(&event); damaged != nullptr) {
        dto.payload = PlayerDamagedEventDTO{
            damaged->entityId,
            damaged->attackerId,
            damaged->damage,
            damaged->healthAfter};
        return dto;
    }

    if (const auto* died = std::get_if<PlayerDied>(&event); died != nullptr) {
        dto.payload = PlayerDiedEventDTO{died->entityId, died->attackerId, died->respawnAtMs};
        return dto;
    }

    if (const auto* respawned = std::get_if<PlayerRespawned>(&event); respawned != nullptr) {
        dto.payload = PlayerRespawnedEventDTO{
            respawned->entityId,
            respawned->x,
            respawned->y};
        return dto;
    }

    if (const auto* entered = std::get_if<EntityEnteredInterest>(&event); entered != nullptr) {
        dto.payload = EntityEnteredInterestEventDTO{
            entered->entityId,
            entered->observerClientId};
        return dto;
    }

    if (const auto* left = std::get_if<EntityLeftInterest>(&event); left != nullptr) {
        dto.payload = EntityLeftInterestEventDTO{
            left->entityId,
            left->observerClientId};
        return dto;
    }

    return std::nullopt;
}

std::optional<DomainEvent> ToDomainEvent(const NetworkEventDTO& event)
{
    if (const auto* spawned = std::get_if<PlayerSpawnedEventDTO>(&event.payload);
        spawned != nullptr) {
        return PlayerSpawned{spawned->entityId, spawned->clientId, spawned->x, spawned->y};
    }

    if (const auto* warmup = std::get_if<WeaponWarmupStartedEventDTO>(&event.payload);
        warmup != nullptr) {
        return WeaponWarmupStarted{
            warmup->entityId,
            warmup->startedAtMs,
            warmup->completesAtMs,
            warmup->weaponType};
    }

    if (const auto* fired = std::get_if<BowFiredEventDTO>(&event.payload); fired != nullptr) {
        return BowFired{fired->entityId, fired->firedAtMs, fired->weaponType};
    }

    if (const auto* projectile = std::get_if<ProjectileSpawnedEventDTO>(&event.payload);
        projectile != nullptr) {
        return ProjectileSpawned{
            projectile->projectileId,
            projectile->ownerId,
            projectile->x,
            projectile->y,
            projectile->sourceWeapon};
    }

    if (const auto* hit = std::get_if<HitConfirmedEventDTO>(&event.payload); hit != nullptr) {
        return HitConfirmed{
            hit->attackerId,
            hit->targetId,
            hit->projectileId,
            hit->serverTimeMs};
    }

    if (const auto* damaged = std::get_if<PlayerDamagedEventDTO>(&event.payload);
        damaged != nullptr) {
        return PlayerDamaged{
            damaged->entityId,
            damaged->attackerId,
            damaged->damage,
            damaged->healthAfter};
    }

    if (const auto* died = std::get_if<PlayerDiedEventDTO>(&event.payload); died != nullptr) {
        return PlayerDied{died->entityId, died->attackerId, died->respawnAtMs};
    }

    if (const auto* respawned = std::get_if<PlayerRespawnedEventDTO>(&event.payload);
        respawned != nullptr) {
        return PlayerRespawned{respawned->entityId, respawned->x, respawned->y};
    }

    if (const auto* entered = std::get_if<EntityEnteredInterestEventDTO>(&event.payload);
        entered != nullptr) {
        return EntityEnteredInterest{entered->entityId, entered->observerClientId};
    }

    if (const auto* left = std::get_if<EntityLeftInterestEventDTO>(&event.payload);
        left != nullptr) {
        return EntityLeftInterest{left->entityId, left->observerClientId};
    }

    return std::nullopt;
}

} // namespace game
