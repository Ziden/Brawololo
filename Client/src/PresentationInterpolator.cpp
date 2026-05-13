#include "Client/PresentationInterpolator.hpp"

#include <algorithm>
#include <utility>

namespace game::client {
namespace {

const game::EntityStateDTO* FindEntity(
    const game::SnapshotDTO& snapshot,
    game::NetworkEntityId entityId)
{
    const auto found = std::find_if(
        snapshot.entities.begin(),
        snapshot.entities.end(),
        [entityId](const game::EntityStateDTO& entity) {
            return entity.replication.entityId == entityId;
        });

    return found == snapshot.entities.end() ? nullptr : &(*found);
}

game::Fixed LerpFixed(game::Fixed from, game::Fixed to, std::uint16_t alphaPermille)
{
    const auto delta = static_cast<std::int64_t>(to) - static_cast<std::int64_t>(from);
    return static_cast<game::Fixed>(
        static_cast<std::int64_t>(from) + ((delta * alphaPermille) / 1000));
}

std::int16_t LerpAim(std::int16_t from, std::int16_t to, std::uint16_t alphaPermille)
{
    const auto delta = static_cast<std::int32_t>(to) - static_cast<std::int32_t>(from);
    return static_cast<std::int16_t>(
        static_cast<std::int32_t>(from) + ((delta * alphaPermille) / 1000));
}

InterpolatedEntityState ToInterpolatedEntity(
    const game::EntityStateDTO& target,
    const game::EntityStateDTO* source,
    std::uint16_t alphaPermille)
{
    InterpolatedEntityState entity{};
    entity.entityId = target.replication.entityId;
    entity.ownerClientId = target.replication.ownerClientId;
    entity.kind = target.replication.kind;
    entity.priority = target.replication.priority;
    entity.x = target.movement.x;
    entity.y = target.movement.y;
    entity.aimX = target.movement.aimX;
    entity.aimY = target.movement.aimY;

    if (source != nullptr) {
        entity.x = LerpFixed(source->movement.x, target.movement.x, alphaPermille);
        entity.y = LerpFixed(source->movement.y, target.movement.y, alphaPermille);
        entity.aimX = LerpAim(source->movement.aimX, target.movement.aimX, alphaPermille);
        entity.aimY = LerpAim(source->movement.aimY, target.movement.aimY, alphaPermille);
    }

    if (target.combat.has_value()) {
        entity.health = target.combat->health;
        entity.maxHealth = target.combat->maxHealth;
        entity.weaponType = target.combat->weaponType;
        entity.weaponWarming = target.combat->weaponWarming;
        entity.defeated = target.combat->defeated;
        entity.serverOwnedProjectile = target.combat->serverOwnedProjectile;
    }

    return entity;
}

} // namespace

PresentationInterpolator::PresentationInterpolator(PresentationInterpolationConfig config)
    : config_(config)
{
}

void PresentationInterpolator::PushSnapshot(game::SnapshotDTO snapshot)
{
    snapshots_.push_back(std::move(snapshot));
    while (snapshots_.size() > config_.maxSnapshots) {
        snapshots_.pop_front();
    }
}

PresentationFrame PresentationInterpolator::Sample(game::TimestampMs estimatedServerNowMs) const
{
    PresentationFrame frame{};
    if (snapshots_.empty()) {
        return frame;
    }

    frame.renderServerTimeMs = estimatedServerNowMs > config_.interpolationDelayMs
        ? estimatedServerNowMs - config_.interpolationDelayMs
        : game::TimestampMs{};

    const game::SnapshotDTO* from = nullptr;
    const game::SnapshotDTO* to = nullptr;

    for (const auto& snapshot : snapshots_) {
        if (snapshot.serverTimeMs <= frame.renderServerTimeMs) {
            from = &snapshot;
            continue;
        }

        to = &snapshot;
        break;
    }

    if (from == nullptr) {
        from = &snapshots_.front();
    }

    if (to == nullptr) {
        to = from;
    }

    frame.fromSnapshotId = from->snapshotId;
    frame.toSnapshotId = to->snapshotId;

    if (to != from && to->serverTimeMs > from->serverTimeMs) {
        const auto elapsedMs = frame.renderServerTimeMs > from->serverTimeMs
            ? frame.renderServerTimeMs - from->serverTimeMs
            : game::TimestampMs{};
        const auto durationMs = to->serverTimeMs - from->serverTimeMs;
        frame.alphaPermille = static_cast<std::uint16_t>(
            std::min<game::TimestampMs>((elapsedMs * 1000) / durationMs, 1000));
    }

    frame.entities.reserve(to->entities.size());
    for (const auto& target : to->entities) {
        frame.entities.push_back(ToInterpolatedEntity(
            target,
            from == to ? nullptr : FindEntity(*from, target.replication.entityId),
            frame.alphaPermille));
    }

    return frame;
}

std::size_t PresentationInterpolator::SnapshotCount() const noexcept
{
    return snapshots_.size();
}

void PresentationInterpolator::Clear()
{
    snapshots_.clear();
}

} // namespace game::client
