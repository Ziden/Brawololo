#pragma once

#include "GameLogic/Replication.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace game {

enum class SnapshotEntityDeltaKind : std::uint8_t {
    Added,
    Changed,
    Removed,
    Unchanged,
};

struct SnapshotEntityDelta {
    NetworkEntityId entityId{};
    SnapshotEntityDeltaKind kind{SnapshotEntityDeltaKind::Unchanged};
};

struct SnapshotDeltaPlan {
    SnapshotId snapshotId{};
    SnapshotId baselineId{};
    bool baselineMatched{};
    std::vector<SnapshotEntityDelta> entities{};
    std::size_t addedCount{};
    std::size_t changedCount{};
    std::size_t removedCount{};
    std::size_t unchangedCount{};
};

[[nodiscard]] bool EquivalentEntityState(const EntityStateDTO& left, const EntityStateDTO& right);
[[nodiscard]] SnapshotDeltaPlan PlanSnapshotDelta(
    const SnapshotDTO& current,
    const SnapshotDTO& baseline);

} // namespace game
