#include "GameLogic/Simulation.hpp"

#include <algorithm>
#include <vector>

namespace game {
namespace {

constexpr Fixed kHitRadius = PixelsToFixed(42);

Fixed IntegrateFixedPerSecond(Fixed valuePerSecond, TimestampMs tickDurationMs) {
    return static_cast<Fixed>(
        (static_cast<std::int64_t>(valuePerSecond) * static_cast<std::int64_t>(tickDurationMs)) /
        1000);
}

bool CloseEnoughForHit(const TransformComponent& left, const TransformComponent& right) {
    const auto dx = static_cast<std::int64_t>(left.x) - static_cast<std::int64_t>(right.x);
    const auto dy = static_cast<std::int64_t>(left.y) - static_cast<std::int64_t>(right.y);
    const auto radius = static_cast<std::int64_t>(kHitRadius);
    return (dx * dx) + (dy * dy) <= radius * radius;
}

} // namespace

NetworkEntityId GameSimulation::CreateProjectile(NetworkEntityId ownerId,
                                                 const TransformComponent& ownerTransform,
                                                 const AimComponent& aim,
                                                 const WeaponDefinition& weapon) {
    const auto entity = registry_.create();
    const auto networkId = AllocateNetworkId();

    registry_.emplace<NetworkIdentityComponent>(entity, networkId);
    registry_.emplace<TransformComponent>(entity, ownerTransform.x, ownerTransform.y);
    registry_.emplace<VelocityComponent>(
        entity,
        static_cast<Fixed>((static_cast<std::int64_t>(weapon.projectileSpeedPerSecond) * aim.x) /
                           kFixedOne),
        static_cast<Fixed>((static_cast<std::int64_t>(weapon.projectileSpeedPerSecond) * aim.y) /
                           kFixedOne));
    registry_.emplace<ProjectileComponent>(
        entity, ownerId, timeMs_, timeMs_ + weapon.projectileLifetimeMs, weapon.type);
    registry_.emplace<NetworkReplicationComponent>(entity, NetworkReplicationMode::AreaOfInterest);
    registry_.emplace<AuthoritativeTag>(entity);

    entityByNetworkId_[networkId] = entity;

    QueueEvent(
        ProjectileSpawned{networkId, ownerId, ownerTransform.x, ownerTransform.y, weapon.type});
    return networkId;
}

void GameSimulation::ProcessWeaponWarmups() {
    struct ProjectileRequest {
        NetworkEntityId ownerId{};
        TransformComponent transform{};
        AimComponent aim{};
        WeaponDefinition weapon{};
    };

    std::vector<ProjectileRequest> projectilesToSpawn{};
    const auto view = registry_.view<NetworkIdentityComponent,
                                     TransformComponent,
                                     AimComponent,
                                     WeaponStateComponent>();
    for (const auto entity : view) {
        if (const auto* player = registry_.try_get<PlayerComponent>(entity);
            player != nullptr && player->defeated) {
            auto& defeatedWeapon = view.get<WeaponStateComponent>(entity);
            defeatedWeapon.warming = false;
            continue;
        }

        auto& weapon = view.get<WeaponStateComponent>(entity);
        if (!weapon.warming || timeMs_ < weapon.warmupCompletesAtMs) {
            continue;
        }

        weapon.warming = false;

        if (config_.mode == SimulationMode::ServerAuthoritative) {
            const auto* definition = FindWeaponDefinition(config_.weapons, weapon.type);
            if (definition == nullptr) {
                continue;
            }

            const auto& identity = view.get<NetworkIdentityComponent>(entity);
            const auto& transform = view.get<TransformComponent>(entity);
            const auto& aim = view.get<AimComponent>(entity);
            QueueEvent(BowFired{identity.id, timeMs_, weapon.type});
            if (definition->serverSpawnedProjectile) {
                projectilesToSpawn.push_back({identity.id, transform, aim, *definition});
            }
        }
    }

    for (const auto& request : projectilesToSpawn) {
        [[maybe_unused]] const auto createdProjectileId =
            CreateProjectile(request.ownerId, request.transform, request.aim, request.weapon);
    }
}

void GameSimulation::ProcessProjectiles() {
    std::vector<entt::entity> toDestroy{};
    const auto projectiles = registry_.view<NetworkIdentityComponent,
                                            ProjectileComponent,
                                            TransformComponent,
                                            VelocityComponent>();
    const auto players =
        registry_.view<NetworkIdentityComponent, PlayerComponent, TransformComponent>();

    for (const auto projectileEntity : projectiles) {
        const auto& projectileIdentity =
            projectiles.get<NetworkIdentityComponent>(projectileEntity);
        auto& projectile = projectiles.get<ProjectileComponent>(projectileEntity);
        auto& transform = projectiles.get<TransformComponent>(projectileEntity);
        const auto& velocity = projectiles.get<VelocityComponent>(projectileEntity);

        transform.x += IntegrateFixedPerSecond(velocity.vx, config_.tickDurationMs);
        transform.y += IntegrateFixedPerSecond(velocity.vy, config_.tickDurationMs);

        if (timeMs_ >= projectile.expiresAtMs || transform.x < 0 || transform.y < 0 ||
            transform.x >= MapWidthFixed(config_.map) ||
            transform.y >= MapHeightFixed(config_.map)) {
            toDestroy.push_back(projectileEntity);
            continue;
        }

        for (const auto playerEntity : players) {
            const auto& playerIdentity = players.get<NetworkIdentityComponent>(playerEntity);
            if (playerIdentity.id == projectile.ownerId) {
                continue;
            }

            auto& player = players.get<PlayerComponent>(playerEntity);
            if (player.defeated) {
                continue;
            }

            const auto& playerTransform = players.get<TransformComponent>(playerEntity);
            if (CloseEnoughForHit(transform, playerTransform)) {
                const auto* weapon = FindWeaponDefinition(config_.weapons, projectile.sourceWeapon);
                const auto damage = weapon != nullptr ? weapon->damage : std::int32_t{};
                player.health = std::max<std::int32_t>(0, player.health - damage);
                QueueEvent(HitConfirmed{
                    projectile.ownerId, playerIdentity.id, projectileIdentity.id, timeMs_});
                QueueEvent(
                    PlayerDamaged{playerIdentity.id, projectile.ownerId, damage, player.health});
                if (player.health <= 0) {
                    player.defeated = true;
                    player.respawnAtMs = timeMs_ + config_.respawnDelayMs;
                    QueueEvent(
                        PlayerDied{playerIdentity.id, projectile.ownerId, player.respawnAtMs});
                }
                toDestroy.push_back(projectileEntity);
                break;
            }
        }
    }

    for (const auto entity : toDestroy) {
        if (registry_.valid(entity) && registry_.all_of<NetworkIdentityComponent>(entity)) {
            entityByNetworkId_.erase(registry_.get<NetworkIdentityComponent>(entity).id);
            registry_.destroy(entity);
        }
    }
}

void GameSimulation::ProcessRespawns() {
    const auto players = registry_.view<NetworkIdentityComponent,
                                        PlayerComponent,
                                        TransformComponent,
                                        VelocityComponent>();
    for (const auto entity : players) {
        auto& player = players.get<PlayerComponent>(entity);
        if (!player.defeated || timeMs_ < player.respawnAtMs) {
            continue;
        }

        const auto& identity = players.get<NetworkIdentityComponent>(entity);
        auto& transform = players.get<TransformComponent>(entity);
        auto& velocity = players.get<VelocityComponent>(entity);
        const auto spawn = SpawnTransformForClient(player.clientId);

        player.health = player.maxHealth;
        player.defeated = false;
        player.respawnAtMs = 0;
        transform = spawn;
        velocity = VelocityComponent{};

        if (auto* weapon = registry_.try_get<WeaponStateComponent>(entity); weapon != nullptr) {
            weapon->warming = false;
            weapon->warmupStartedAtMs = 0;
            weapon->warmupCompletesAtMs = 0;
        }

        QueueEvent(PlayerRespawned{identity.id, transform.x, transform.y});
    }
}

SnapshotPriority GameSimulation::PriorityForEntity(ClientId observerClientId,
                                                   entt::entity entity) const {
    if (registry_.all_of<ProjectileComponent>(entity)) {
        return SnapshotPriority::High;
    }

    const auto* owner = registry_.try_get<OwnedByPlayerComponent>(entity);
    if (owner != nullptr && owner->clientId == observerClientId) {
        return SnapshotPriority::High;
    }

    if (registry_.all_of<WeaponStateComponent>(entity)) {
        const auto& weapon = registry_.get<WeaponStateComponent>(entity);
        if (weapon.warming) {
            return SnapshotPriority::High;
        }
    }

    return SnapshotPriority::Medium;
}

} // namespace game
