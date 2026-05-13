#include "GameLogic/Simulation.hpp"

#include "GameLogic/Math.hpp"

namespace game {
namespace {

Fixed IntegrateFixedPerSecond(Fixed valuePerSecond, TimestampMs tickDurationMs) {
    return static_cast<Fixed>(
        (static_cast<std::int64_t>(valuePerSecond) * static_cast<std::int64_t>(tickDurationMs)) /
        1000);
}

Fixed ApplyInputAcceleration(Fixed velocity,
                             Fixed accelerationDelta,
                             std::int16_t normalizedInput) {
    return velocity +
           static_cast<Fixed>((static_cast<std::int64_t>(accelerationDelta) * normalizedInput) /
                              kFixedOne);
}

Fixed ApplyFriction(Fixed velocity) {
    return FixedMulRatio(velocity, 72, 100);
}

} // namespace

void GameSimulation::ApplyInputToEntity(entt::entity entity, const ClientInputPacket& packet) {
    if (const auto* player = registry_.try_get<PlayerComponent>(entity);
        player != nullptr && player->defeated) {
        return;
    }

    registry_.emplace_or_replace<InputIntentComponent>(entity,
                                                       packet.input.moveX,
                                                       packet.input.moveY,
                                                       packet.input.aimX,
                                                       packet.input.aimY,
                                                       packet.input.fire);

    auto& aim = registry_.get_or_emplace<AimComponent>(entity);
    if (packet.input.aimX != 0 || packet.input.aimY != 0) {
        aim.x = packet.input.aimX;
        aim.y = packet.input.aimY;
    }

    if (packet.input.fire &&
        registry_.all_of<WeaponStateComponent, NetworkIdentityComponent>(entity)) {
        auto& weapon = registry_.get<WeaponStateComponent>(entity);
        const auto* definition = FindWeaponDefinition(config_.weapons, weapon.type);
        if (definition == nullptr || weapon.type == WeaponType::None) {
            return;
        }

        const auto& identity = registry_.get<NetworkIdentityComponent>(entity);
        if (!weapon.warming) {
            weapon.warming = true;
            weapon.warmupStartedAtMs = timeMs_;
            weapon.warmupCompletesAtMs = timeMs_ + definition->warmupMs;
            QueueEvent(WeaponWarmupStarted{
                identity.id, weapon.warmupStartedAtMs, weapon.warmupCompletesAtMs, weapon.type});
        }
    }
}

void GameSimulation::IntegrateMovement(entt::entity entity) {
    if (!registry_.all_of<NetworkIdentityComponent, TransformComponent, VelocityComponent>(
            entity)) {
        return;
    }

    if (const auto* player = registry_.try_get<PlayerComponent>(entity);
        player != nullptr && player->defeated) {
        auto& velocity = registry_.get<VelocityComponent>(entity);
        velocity.vx = 0;
        velocity.vy = 0;
        return;
    }

    const auto& identity = registry_.get<NetworkIdentityComponent>(entity);
    auto& transform = registry_.get<TransformComponent>(entity);
    auto& velocity = registry_.get<VelocityComponent>(entity);
    const auto oldX = transform.x;
    const auto oldY = transform.y;

    if (const auto* input = registry_.try_get<InputIntentComponent>(entity); input != nullptr) {
        const auto normalized = NormalizeDigitalInput(input->moveX, input->moveY);
        const auto accelerationDelta =
            IntegrateFixedPerSecond(config_.movementAccelerationPerSecond, config_.tickDurationMs);
        velocity.vx = ApplyInputAcceleration(velocity.vx, accelerationDelta, normalized.x);
        velocity.vy = ApplyInputAcceleration(velocity.vy, accelerationDelta, normalized.y);
    } else {
        velocity.vx = ApplyFriction(velocity.vx);
        velocity.vy = ApplyFriction(velocity.vy);
    }

    velocity.vx =
        ClampFixed(velocity.vx, -config_.maxMoveSpeedPerSecond, config_.maxMoveSpeedPerSecond);
    velocity.vy =
        ClampFixed(velocity.vy, -config_.maxMoveSpeedPerSecond, config_.maxMoveSpeedPerSecond);
    transform.x += IntegrateFixedPerSecond(velocity.vx, config_.tickDurationMs);
    transform.y += IntegrateFixedPerSecond(velocity.vy, config_.tickDurationMs);
    ClampToMap(transform);

    if ((oldX != transform.x || oldY != transform.y) && registry_.all_of<PlayerComponent>(entity)) {
        QueueEvent(PlayerMoved{identity.id, oldX, oldY, transform.x, transform.y});
    }
}

void GameSimulation::ProcessInputs() {
    for (const auto& input : pendingInputs_) {
        const auto player = PlayerEntityForClient(input.header.clientId);
        if (!player.has_value()) {
            continue;
        }

        const auto entity = EntityFor(*player);
        if (entity != entt::null) {
            ApplyInputToEntity(entity, input);
        }
    }

    pendingInputs_.clear();

    const auto view = registry_.view<PlayerComponent>();
    for (const auto entity : view) {
        IntegrateMovement(entity);
    }

    registry_.clear<InputIntentComponent>();
}

void GameSimulation::ClampToMap(TransformComponent& transform) const {
    transform.x = ClampFixed(transform.x, 0, MapWidthFixed(config_.map) - 1);
    transform.y = ClampFixed(transform.y, 0, MapHeightFixed(config_.map) - 1);
}

} // namespace game
