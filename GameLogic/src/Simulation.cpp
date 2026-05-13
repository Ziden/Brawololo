#include "GameLogic/Simulation.hpp"

#include <algorithm>
#include <unordered_set>
#include <utility>

namespace game {
namespace {

SnapshotPriority BetterPriority(SnapshotPriority left, SnapshotPriority right) {
    return static_cast<std::uint8_t>(left) >= static_cast<std::uint8_t>(right) ? left : right;
}

} // namespace

GameSimulation::GameSimulation(SimulationConfig config) : config_(config) {}

bool GameSimulation::Submit(const LoginCommand& command) {
    if (playerByClient_.contains(command.header.clientId)) {
        return true;
    }

    if (playerByClient_.size() >= config_.maxPlayers) {
        return false;
    }

    lastAcceptedSequence_[command.header.clientId] = command.header.sequence;

    const auto spawn = SpawnTransformForClient(command.header.clientId);
    [[maybe_unused]] const auto createdPlayerId =
        CreatePlayer(command.header.clientId, spawn.x, spawn.y);
    return true;
}

bool GameSimulation::Submit(const ClientInputPacket& packet) {
    const auto playerIt = playerByClient_.find(packet.header.clientId);
    if (playerIt == playerByClient_.end()) {
        return false;
    }

    const auto lastIt = lastAcceptedSequence_.find(packet.header.clientId);
    if (lastIt != lastAcceptedSequence_.end() && packet.header.sequence <= lastIt->second) {
        return false;
    }

    lastAcceptedSequence_[packet.header.clientId] = packet.header.sequence;
    pendingInputs_.push_back(packet);
    return true;
}

void GameSimulation::TickFixed() {
    ++tick_;
    timeMs_ += config_.tickDurationMs;

    ProcessInputs();
    ProcessWeaponWarmups();
    ProcessProjectiles();
    ProcessRespawns();
}

void GameSimulation::ReplayLocalInputsForPrediction(const std::vector<ClientInputPacket>& inputs) {
    for (const auto& input : inputs) {
        const auto player = PlayerEntityForClient(input.header.clientId);
        if (!player.has_value()) {
            continue;
        }

        const auto entity = EntityFor(*player);
        if (entity == entt::null) {
            continue;
        }

        ApplyInputToEntity(entity, input);
        IntegrateMovement(entity);
        if (registry_.all_of<InputIntentComponent>(entity)) {
            registry_.remove<InputIntentComponent>(entity);
        }
    }
}

SnapshotDTO GameSimulation::BuildSnapshot(ClientId observerClientId,
                                          SnapshotId snapshotId,
                                          SnapshotId baselineId) const {
    SnapshotDTO snapshot{};
    snapshot.snapshotId = snapshotId;
    snapshot.baselineId = baselineId;
    snapshot.serverTick = tick_;
    snapshot.serverTimeMs = timeMs_;

    if (const auto last = lastAcceptedSequence_.find(observerClientId);
        last != lastAcceptedSequence_.end()) {
        snapshot.ackedInputSequence = last->second;
    }

    std::optional<ChunkCoord> observerChunk{};
    if (const auto observerEntityId = PlayerEntityForClient(observerClientId);
        observerEntityId.has_value()) {
        if (const auto movement = MovementForEntity(*observerEntityId); movement.has_value()) {
            observerChunk = ChunkForPosition(config_.map, movement->x, movement->y);
        }
    }

    const auto view =
        registry_.view<NetworkIdentityComponent, TransformComponent, VelocityComponent>();
    for (const auto entity : view) {
        const auto& identity = view.get<NetworkIdentityComponent>(entity);
        const auto& transform = view.get<TransformComponent>(entity);
        const auto& velocity = view.get<VelocityComponent>(entity);

        const auto* owner = registry_.try_get<OwnedByPlayerComponent>(entity);
        const auto ownedByObserver = owner != nullptr && owner->clientId == observerClientId;
        if (observerChunk.has_value() && !ownedByObserver) {
            const auto entityChunk = ChunkForPosition(config_.map, transform.x, transform.y);
            if (!IsChunkInAreaOfInterest(config_.map, *observerChunk, entityChunk)) {
                continue;
            }
        }

        EntityStateDTO state{};
        state.movement.entityId = identity.id;
        state.movement.x = transform.x;
        state.movement.y = transform.y;
        state.movement.vx = velocity.vx;
        state.movement.vy = velocity.vy;

        if (const auto* aim = registry_.try_get<AimComponent>(entity); aim != nullptr) {
            state.movement.aimX = aim->x;
            state.movement.aimY = aim->y;
        }

        state.replication.entityId = identity.id;
        state.replication.ownerClientId = owner != nullptr ? owner->clientId : ClientId{};
        state.replication.priority = PriorityForEntity(observerClientId, entity);

        if (const auto* player = registry_.try_get<PlayerComponent>(entity); player != nullptr) {
            CombatStateDTO combat{};
            combat.entityId = identity.id;
            combat.health = player->health;
            combat.maxHealth = player->maxHealth;
            combat.defeated = player->defeated;
            combat.respawnAtMs = player->respawnAtMs;
            if (const auto* weapon = registry_.try_get<WeaponStateComponent>(entity);
                weapon != nullptr) {
                combat.weaponType = weapon->type;
                combat.weaponWarming = weapon->warming;
                combat.weaponWarmupCompletesAtMs = weapon->warmupCompletesAtMs;
            }
            state.combat = combat;
            state.replication.kind = ReplicatedEntityKind::Player;
        } else if (const auto* projectile = registry_.try_get<ProjectileComponent>(entity);
                   projectile != nullptr) {
            CombatStateDTO combat{};
            combat.entityId = identity.id;
            combat.weaponType = projectile->sourceWeapon;
            combat.serverOwnedProjectile = true;
            combat.projectileOwnerId = projectile->ownerId;
            state.combat = combat;
            state.replication.kind = ReplicatedEntityKind::Projectile;
            state.replication.priority =
                BetterPriority(state.replication.priority, SnapshotPriority::High);
        }

        snapshot.entities.push_back(std::move(state));
    }

    return snapshot;
}

void GameSimulation::ApplySnapshot(const SnapshotDTO& snapshot, ClientId localClientId) {
    std::unordered_set<NetworkEntityId> seenEntities{};

    for (const auto& state : snapshot.entities) {
        seenEntities.insert(state.movement.entityId);
        auto entity = EntityFor(state.movement.entityId);
        const auto isNewEntity = entity == entt::null;
        if (isNewEntity) {
            entity = registry_.create();
            registry_.emplace<NetworkIdentityComponent>(entity, state.movement.entityId);
            entityByNetworkId_[state.movement.entityId] = entity;
            QueueEvent(EntityEnteredInterest{state.movement.entityId, localClientId});
        }

        auto& transform = registry_.get_or_emplace<TransformComponent>(entity);
        auto& velocity = registry_.get_or_emplace<VelocityComponent>(entity);
        auto& aim = registry_.get_or_emplace<AimComponent>(entity);

        const auto oldX = transform.x;
        const auto oldY = transform.y;
        const auto ownedByLocalPlayer = state.replication.ownerClientId == localClientId &&
                                        state.replication.kind == ReplicatedEntityKind::Player;

        if (!isNewEntity && ownedByLocalPlayer && registry_.all_of<PredictedTag>(entity) &&
            (transform.x != state.movement.x || transform.y != state.movement.y)) {
            QueueEvent(LocalPredictionCorrected{state.movement.entityId,
                                                transform.x,
                                                transform.y,
                                                state.movement.x,
                                                state.movement.y});
        }

        transform.x = state.movement.x;
        transform.y = state.movement.y;
        velocity.vx = state.movement.vx;
        velocity.vy = state.movement.vy;
        aim.x = state.movement.aimX;
        aim.y = state.movement.aimY;

        registry_.emplace_or_replace<OwnedByPlayerComponent>(entity,
                                                             state.replication.ownerClientId);
        registry_.emplace_or_replace<NetworkReplicationComponent>(
            entity, NetworkReplicationMode::AreaOfInterest);

        if (state.replication.kind == ReplicatedEntityKind::Player) {
            auto& player = registry_.get_or_emplace<PlayerComponent>(entity);
            player.clientId = state.replication.ownerClientId;
            if (state.combat.has_value()) {
                player.health = state.combat->health;
                player.maxHealth = state.combat->maxHealth;
                player.defeated = state.combat->defeated;
                player.respawnAtMs = state.combat->respawnAtMs;
            }
            auto& weapon = registry_.get_or_emplace<WeaponStateComponent>(entity);
            if (state.combat.has_value()) {
                weapon.type = state.combat->weaponType;
                weapon.warming = state.combat->weaponWarming;
                weapon.warmupCompletesAtMs = state.combat->weaponWarmupCompletesAtMs;
            }
            if (state.replication.ownerClientId != 0) {
                playerByClient_[state.replication.ownerClientId] = state.movement.entityId;
            }
        } else {
            auto& projectile = registry_.get_or_emplace<ProjectileComponent>(entity);
            projectile.ownerId =
                state.combat.has_value() ? state.combat->projectileOwnerId : kInvalidNetworkEntity;
            projectile.spawnedAtMs = snapshot.serverTimeMs;
            projectile.sourceWeapon =
                state.combat.has_value() ? state.combat->weaponType : WeaponType::None;
            const auto* weapon = FindWeaponDefinition(config_.weapons, projectile.sourceWeapon);
            projectile.expiresAtMs =
                snapshot.serverTimeMs +
                (weapon != nullptr ? weapon->projectileLifetimeMs : TimestampMs{});
        }

        if (ownedByLocalPlayer) {
            registry_.remove<InterpolatedTag>(entity);
            registry_.emplace_or_replace<PredictedTag>(entity);
        } else {
            registry_.remove<PredictedTag>(entity);
            registry_.emplace_or_replace<InterpolatedTag>(entity);
        }

        if (config_.mode == SimulationMode::ServerAuthoritative) {
            registry_.emplace_or_replace<AuthoritativeTag>(entity);
        }

        if (!isNewEntity && (oldX != transform.x || oldY != transform.y) &&
            registry_.all_of<PlayerComponent>(entity)) {
            QueueEvent(PlayerMoved{state.movement.entityId, oldX, oldY, transform.x, transform.y});
        }
    }

    std::vector<NetworkEntityId> entitiesLeavingInterest{};
    for (const auto& [entityId, entity] : entityByNetworkId_) {
        if (seenEntities.contains(entityId)) {
            continue;
        }

        const auto* owner = registry_.try_get<OwnedByPlayerComponent>(entity);
        if (owner != nullptr && owner->clientId == localClientId) {
            continue;
        }

        entitiesLeavingInterest.push_back(entityId);
    }

    for (const auto entityId : entitiesLeavingInterest) {
        const auto entity = EntityFor(entityId);
        if (entity == entt::null) {
            continue;
        }

        if (const auto* player = registry_.try_get<PlayerComponent>(entity); player != nullptr) {
            playerByClient_.erase(player->clientId);
        }

        QueueEvent(EntityLeftInterest{entityId, localClientId});
        registry_.destroy(entity);
        entityByNetworkId_.erase(entityId);
    }

    QueueEvent(
        SnapshotApplied{snapshot.snapshotId, snapshot.baselineId, snapshot.ackedInputSequence});
}

EventList GameSimulation::DrainEvents() {
    EventList drained{};
    drained.swap(events_);
    return drained;
}

Tick GameSimulation::CurrentTick() const noexcept {
    return tick_;
}

TimestampMs GameSimulation::ServerTimeMs() const noexcept {
    return timeMs_;
}

std::optional<NetworkEntityId> GameSimulation::PlayerEntityForClient(ClientId clientId) const {
    const auto found = playerByClient_.find(clientId);
    if (found == playerByClient_.end()) {
        return std::nullopt;
    }

    return found->second;
}

std::optional<MovementStateDTO> GameSimulation::MovementForEntity(NetworkEntityId entityId) const {
    const auto entity = EntityFor(entityId);
    if (entity == entt::null ||
        !registry_.all_of<NetworkIdentityComponent, TransformComponent, VelocityComponent>(
            entity)) {
        return std::nullopt;
    }

    const auto& identity = registry_.get<NetworkIdentityComponent>(entity);
    const auto& transform = registry_.get<TransformComponent>(entity);
    const auto& velocity = registry_.get<VelocityComponent>(entity);
    MovementStateDTO state{};
    state.entityId = identity.id;
    state.x = transform.x;
    state.y = transform.y;
    state.vx = velocity.vx;
    state.vy = velocity.vy;

    if (const auto* aim = registry_.try_get<AimComponent>(entity); aim != nullptr) {
        state.aimX = aim->x;
        state.aimY = aim->y;
    }

    return state;
}

std::size_t GameSimulation::EntityCount() const noexcept {
    return entityByNetworkId_.size();
}

entt::registry& GameSimulation::Registry() noexcept {
    return registry_;
}

const entt::registry& GameSimulation::Registry() const noexcept {
    return registry_;
}

NetworkEntityId GameSimulation::AllocateNetworkId() {
    const auto allocated = nextNetworkId_;
    ++nextNetworkId_.value;
    return allocated;
}

TransformComponent GameSimulation::SpawnTransformForClient(ClientId clientId) const {
    const auto tileX = 10 + static_cast<std::int32_t>(clientId % 20U);
    const auto tileY = 10 + static_cast<std::int32_t>((clientId / 20U) % 20U);
    return TransformComponent{PixelsToFixed(tileX * config_.map.tileSizePixels),
                              PixelsToFixed(tileY * config_.map.tileSizePixels)};
}

NetworkEntityId GameSimulation::CreatePlayer(ClientId clientId, Fixed x, Fixed y) {
    const auto entity = registry_.create();
    const auto networkId = AllocateNetworkId();

    registry_.emplace<NetworkIdentityComponent>(entity, networkId);
    registry_.emplace<TransformComponent>(entity, x, y);
    registry_.emplace<VelocityComponent>(entity);
    registry_.emplace<AimComponent>(entity);
    registry_.emplace<PlayerComponent>(
        entity, PlayerComponent{clientId, config_.maxPlayerHealth, config_.maxPlayerHealth});
    registry_.emplace<WeaponStateComponent>(entity, WeaponStateComponent{config_.defaultWeapon});
    registry_.emplace<OwnedByPlayerComponent>(entity, clientId);
    registry_.emplace<NetworkReplicationComponent>(entity, NetworkReplicationMode::AreaOfInterest);

    if (config_.mode == SimulationMode::ServerAuthoritative) {
        registry_.emplace<AuthoritativeTag>(entity);
    } else {
        registry_.emplace<PredictedTag>(entity);
    }

    playerByClient_[clientId] = networkId;
    entityByNetworkId_[networkId] = entity;

    QueueEvent(PlayerSpawned{networkId, clientId, x, y});
    return networkId;
}

entt::entity GameSimulation::EntityFor(NetworkEntityId entityId) const {
    const auto found = entityByNetworkId_.find(entityId);
    return found == entityByNetworkId_.end() ? entt::null : found->second;
}

void GameSimulation::QueueEvent(DomainEvent event) {
    events_.push_back(std::move(event));
}

} // namespace game
