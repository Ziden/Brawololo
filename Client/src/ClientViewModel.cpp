#include "Client/ClientViewModel.hpp"

#include "GameLogic/Components.hpp"

#include <algorithm>
#include <string>
#include <variant>

namespace game::client {
namespace {

ViewAuthorityRole RoleFor(const entt::registry& registry, entt::entity entity) {
    if (registry.all_of<game::PredictedTag>(entity)) {
        return ViewAuthorityRole::Predicted;
    }

    if (registry.all_of<game::AuthoritativeTag>(entity)) {
        return ViewAuthorityRole::Authoritative;
    }

    return ViewAuthorityRole::Interpolated;
}

ViewEntityKind KindFor(game::ReplicatedEntityKind kind) {
    switch (kind) {
        case game::ReplicatedEntityKind::Player:
            return ViewEntityKind::Player;
        case game::ReplicatedEntityKind::Projectile:
            return ViewEntityKind::Projectile;
    }

    return ViewEntityKind::Player;
}

void ApplyPresentationFrame(ClientViewFrame& frame) {
    for (const auto& interpolated : frame.presentation.entities) {
        if (interpolated.kind == game::ReplicatedEntityKind::Player &&
            interpolated.ownerClientId == frame.localClientId) {
            continue;
        }

        const auto found = std::find_if(frame.entities.begin(),
                                        frame.entities.end(),
                                        [&interpolated](const ViewEntity& entity) {
                                            return entity.entityId == interpolated.entityId;
                                        });

        if (found != frame.entities.end()) {
            found->role = ViewAuthorityRole::Interpolated;
            found->x = interpolated.x;
            found->y = interpolated.y;
            found->aimX = interpolated.aimX;
            found->aimY = interpolated.aimY;
            found->health = interpolated.health;
            found->maxHealth = interpolated.maxHealth;
            found->weaponType = interpolated.weaponType;
            found->weaponWarming = interpolated.weaponWarming;
            found->defeated = interpolated.defeated;
            continue;
        }

        frame.entities.push_back(ViewEntity{interpolated.entityId,
                                            interpolated.ownerClientId,
                                            KindFor(interpolated.kind),
                                            ViewAuthorityRole::Interpolated,
                                            interpolated.x,
                                            interpolated.y,
                                            interpolated.aimX,
                                            interpolated.aimY,
                                            interpolated.health,
                                            interpolated.maxHealth,
                                            interpolated.weaponType,
                                            interpolated.weaponWarming,
                                            interpolated.defeated});
    }
}

std::string EventLine(const game::DomainEvent& event) {
    if (const auto* spawned = std::get_if<game::PlayerSpawned>(&event); spawned != nullptr) {
        return "Player spawned: client " + std::to_string(spawned->clientId);
    }

    if (const auto* warmup = std::get_if<game::WeaponWarmupStarted>(&event); warmup != nullptr) {
        return "Bow warmup started: entity " + std::to_string(warmup->entityId.value);
    }

    if (const auto* fired = std::get_if<game::BowFired>(&event); fired != nullptr) {
        return "Bow fired: entity " + std::to_string(fired->entityId.value);
    }

    if (const auto* projectile = std::get_if<game::ProjectileSpawned>(&event);
        projectile != nullptr) {
        return "Projectile spawned: entity " + std::to_string(projectile->projectileId.value);
    }

    if (const auto* hit = std::get_if<game::HitConfirmed>(&event); hit != nullptr) {
        return "Hit confirmed: " + std::to_string(hit->attackerId.value) + " -> " +
               std::to_string(hit->targetId.value);
    }

    if (const auto* damaged = std::get_if<game::PlayerDamaged>(&event); damaged != nullptr) {
        return "Player damaged: entity " + std::to_string(damaged->entityId.value) + " for " +
               std::to_string(damaged->damage) + " hp, remaining " +
               std::to_string(damaged->healthAfter);
    }

    if (const auto* died = std::get_if<game::PlayerDied>(&event); died != nullptr) {
        return "Player died: entity " + std::to_string(died->entityId.value) + ", respawn at " +
               std::to_string(died->respawnAtMs) + "ms";
    }

    if (const auto* respawned = std::get_if<game::PlayerRespawned>(&event); respawned != nullptr) {
        return "Player respawned: entity " + std::to_string(respawned->entityId.value);
    }

    if (const auto* corrected = std::get_if<game::LocalPredictionCorrected>(&event);
        corrected != nullptr) {
        return "Prediction corrected: entity " + std::to_string(corrected->entityId.value);
    }

    if (const auto* entered = std::get_if<game::EntityEnteredInterest>(&event);
        entered != nullptr) {
        return "Entity entered interest: " + std::to_string(entered->entityId.value);
    }

    if (const auto* left = std::get_if<game::EntityLeftInterest>(&event); left != nullptr) {
        return "Entity left interest: " + std::to_string(left->entityId.value);
    }

    if (const auto* snapshot = std::get_if<game::SnapshotApplied>(&event); snapshot != nullptr) {
        return "Snapshot applied: " + std::to_string(snapshot->snapshotId);
    }

    return "Simulation event";
}

} // namespace

ClientViewFrame BuildClientViewFrame(const ClientRuntime& runtime,
                                     const game::EventList& events,
                                     const ClientApplicationStats& stats) {
    ClientViewFrame frame{};
    frame.localClientId = runtime.LocalClientId();
    frame.stats = stats;
    frame.presentation = runtime.Presentation().Sample(runtime.Simulation().ServerTimeMs());

    const auto& registry = runtime.Simulation().Registry();
    const auto players = registry.view<game::NetworkIdentityComponent,
                                       game::TransformComponent,
                                       game::PlayerComponent,
                                       game::AimComponent,
                                       game::WeaponStateComponent>();

    for (const auto entity : players) {
        const auto& identity = players.get<game::NetworkIdentityComponent>(entity);
        const auto& transform = players.get<game::TransformComponent>(entity);
        const auto& player = players.get<game::PlayerComponent>(entity);
        const auto& aim = players.get<game::AimComponent>(entity);
        const auto& weapon = players.get<game::WeaponStateComponent>(entity);

        frame.entities.push_back(ViewEntity{identity.id,
                                            player.clientId,
                                            ViewEntityKind::Player,
                                            RoleFor(registry, entity),
                                            transform.x,
                                            transform.y,
                                            aim.x,
                                            aim.y,
                                            player.health,
                                            player.maxHealth,
                                            weapon.type,
                                            weapon.warming,
                                            player.defeated});
    }

    const auto projectiles = registry.view<game::NetworkIdentityComponent,
                                           game::TransformComponent,
                                           game::ProjectileComponent>();

    for (const auto entity : projectiles) {
        const auto& identity = projectiles.get<game::NetworkIdentityComponent>(entity);
        const auto& transform = projectiles.get<game::TransformComponent>(entity);
        const auto& projectile = projectiles.get<game::ProjectileComponent>(entity);

        frame.entities.push_back(ViewEntity{identity.id,
                                            projectile.ownerId.value,
                                            ViewEntityKind::Projectile,
                                            RoleFor(registry, entity),
                                            transform.x,
                                            transform.y,
                                            1000,
                                            0,
                                            0,
                                            0,
                                            game::WeaponType::None,
                                            false,
                                            false});
    }

    for (const auto& event : events) {
        frame.eventLines.push_back(EventLine(event));
    }

    ApplyPresentationFrame(frame);
    return frame;
}

} // namespace game::client
