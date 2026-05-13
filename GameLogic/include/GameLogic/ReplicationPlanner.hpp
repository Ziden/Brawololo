#pragma once

#include "GameLogic/Replication.hpp"

#include <cstddef>

namespace game {

struct ReplicationPlannerConfig {
    std::size_t maxEntitiesPerSnapshot{};
    bool prioritizeHigherFirst{true};
    bool keepOwnerEntityFirst{true};
};

struct ReplicationPlannerStats {
    std::size_t inputEntityCount{};
    std::size_t outputEntityCount{};
    std::size_t highPriorityCount{};
    std::size_t mediumPriorityCount{};
    std::size_t lowPriorityCount{};
    std::size_t droppedEntityCount{};
};

struct PlannedSnapshot {
    SnapshotDTO snapshot{};
    ReplicationPlannerStats stats{};
};

class ReplicationPlanner {
public:
    explicit ReplicationPlanner(ReplicationPlannerConfig config = {});

    [[nodiscard]] PlannedSnapshot Plan(SnapshotDTO snapshot, ClientId observerClientId) const;

private:
    ReplicationPlannerConfig config_{};
};

} // namespace game

