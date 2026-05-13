#include "GameLogic/ReplicationPlanner.hpp"

#include <algorithm>
#include <cstdint>

namespace game {
namespace {

std::uint8_t PriorityRank(SnapshotPriority priority) {
    switch (priority) {
        case SnapshotPriority::High:
            return 3;
        case SnapshotPriority::Medium:
            return 2;
        case SnapshotPriority::Low:
            return 1;
    }

    return 0;
}

void CountPriority(ReplicationPlannerStats& stats, SnapshotPriority priority) {
    switch (priority) {
        case SnapshotPriority::High:
            ++stats.highPriorityCount;
            break;
        case SnapshotPriority::Medium:
            ++stats.mediumPriorityCount;
            break;
        case SnapshotPriority::Low:
            ++stats.lowPriorityCount;
            break;
    }
}

} // namespace

ReplicationPlanner::ReplicationPlanner(ReplicationPlannerConfig config) : config_(config) {}

PlannedSnapshot ReplicationPlanner::Plan(SnapshotDTO snapshot, ClientId observerClientId) const {
    PlannedSnapshot planned{};
    planned.stats.inputEntityCount = snapshot.entities.size();

    if (config_.prioritizeHigherFirst) {
        std::stable_sort(
            snapshot.entities.begin(),
            snapshot.entities.end(),
            [this, observerClientId](const EntityStateDTO& left, const EntityStateDTO& right) {
                if (config_.keepOwnerEntityFirst) {
                    const auto leftOwned = left.replication.ownerClientId == observerClientId;
                    const auto rightOwned = right.replication.ownerClientId == observerClientId;
                    if (leftOwned != rightOwned) {
                        return leftOwned;
                    }
                }

                const auto leftRank = PriorityRank(left.replication.priority);
                const auto rightRank = PriorityRank(right.replication.priority);
                if (leftRank != rightRank) {
                    return leftRank > rightRank;
                }

                return left.replication.entityId.value < right.replication.entityId.value;
            });
    }

    const auto maxEntities = config_.maxEntitiesPerSnapshot;
    if (maxEntities > 0 && snapshot.entities.size() > maxEntities) {
        snapshot.entities.resize(maxEntities);
    }

    planned.stats.outputEntityCount = snapshot.entities.size();
    planned.stats.droppedEntityCount =
        planned.stats.inputEntityCount - planned.stats.outputEntityCount;
    for (const auto& entity : snapshot.entities) {
        CountPriority(planned.stats, entity.replication.priority);
    }

    planned.snapshot = std::move(snapshot);
    return planned;
}

} // namespace game
