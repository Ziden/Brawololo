#include "GameLogic/SnapshotDelta.hpp"

#include <algorithm>
#include <unordered_set>

namespace game {
namespace {

bool EquivalentMovement(const MovementStateDTO& left, const MovementStateDTO& right) {
    return left.entityId == right.entityId && left.x == right.x && left.y == right.y &&
           left.vx == right.vx && left.vy == right.vy && left.aimX == right.aimX &&
           left.aimY == right.aimY;
}

bool EquivalentCombat(const CombatStateDTO& left, const CombatStateDTO& right) {
    return left.entityId == right.entityId && left.health == right.health &&
           left.maxHealth == right.maxHealth && left.weaponType == right.weaponType &&
           left.weaponWarming == right.weaponWarming &&
           left.weaponWarmupCompletesAtMs == right.weaponWarmupCompletesAtMs &&
           left.defeated == right.defeated && left.respawnAtMs == right.respawnAtMs &&
           left.serverOwnedProjectile == right.serverOwnedProjectile &&
           left.projectileOwnerId == right.projectileOwnerId;
}

bool EquivalentReplication(const ReplicationStateDTO& left, const ReplicationStateDTO& right) {
    return left.entityId == right.entityId && left.ownerClientId == right.ownerClientId &&
           left.kind == right.kind && left.priority == right.priority;
}

const EntityStateDTO* FindEntity(const SnapshotDTO& snapshot, NetworkEntityId entityId) {
    const auto found = std::find_if(snapshot.entities.begin(),
                                    snapshot.entities.end(),
                                    [entityId](const EntityStateDTO& entity) {
                                        return entity.replication.entityId == entityId;
                                    });
    return found == snapshot.entities.end() ? nullptr : &(*found);
}

void CountDelta(SnapshotDeltaPlan& plan, SnapshotEntityDeltaKind kind) {
    switch (kind) {
        case SnapshotEntityDeltaKind::Added:
            ++plan.addedCount;
            break;
        case SnapshotEntityDeltaKind::Changed:
            ++plan.changedCount;
            break;
        case SnapshotEntityDeltaKind::Removed:
            ++plan.removedCount;
            break;
        case SnapshotEntityDeltaKind::Unchanged:
            ++plan.unchangedCount;
            break;
    }
}

} // namespace

bool EquivalentEntityState(const EntityStateDTO& left, const EntityStateDTO& right) {
    if (!EquivalentMovement(left.movement, right.movement) ||
        !EquivalentReplication(left.replication, right.replication) ||
        left.combat.has_value() != right.combat.has_value()) {
        return false;
    }

    return !left.combat.has_value() || EquivalentCombat(*left.combat, *right.combat);
}

SnapshotDeltaPlan PlanSnapshotDelta(const SnapshotDTO& current, const SnapshotDTO& baseline) {
    SnapshotDeltaPlan plan{};
    plan.snapshotId = current.snapshotId;
    plan.baselineId = current.baselineId;
    plan.baselineMatched = current.baselineId != 0 && current.baselineId == baseline.snapshotId;

    std::unordered_set<NetworkEntityId> currentIds{};
    for (const auto& currentEntity : current.entities) {
        const auto entityId = currentEntity.replication.entityId;
        currentIds.insert(entityId);

        const auto* baselineEntity = FindEntity(baseline, entityId);
        const auto kind = baselineEntity == nullptr ? SnapshotEntityDeltaKind::Added
                          : EquivalentEntityState(currentEntity, *baselineEntity)
                              ? SnapshotEntityDeltaKind::Unchanged
                              : SnapshotEntityDeltaKind::Changed;

        plan.entities.push_back(SnapshotEntityDelta{entityId, kind});
        CountDelta(plan, kind);
    }

    for (const auto& baselineEntity : baseline.entities) {
        const auto entityId = baselineEntity.replication.entityId;
        if (currentIds.contains(entityId)) {
            continue;
        }

        plan.entities.push_back(SnapshotEntityDelta{entityId, SnapshotEntityDeltaKind::Removed});
        CountDelta(plan, SnapshotEntityDeltaKind::Removed);
    }

    return plan;
}

} // namespace game
