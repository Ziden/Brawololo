#pragma once

#include "GameLogic/Replication.hpp"

#include <cstddef>
#include <optional>
#include <unordered_map>
#include <vector>

namespace game {

struct SnapshotBaselineCacheConfig {
    std::size_t maxBaselinesPerClient{32};
};

struct SnapshotBaselineDecision {
    SnapshotId requestedBaselineId{};
    SnapshotId effectiveBaselineId{};
    SnapshotDeliveryKind deliveryKind{SnapshotDeliveryKind::Full};
    bool baselineAvailable{};
};

class SnapshotBaselineCache {
public:
    explicit SnapshotBaselineCache(SnapshotBaselineCacheConfig config = {});

    void Store(ClientId clientId, SnapshotDTO snapshot);
    [[nodiscard]] std::optional<SnapshotDTO> Find(ClientId clientId, SnapshotId snapshotId) const;
    [[nodiscard]] std::optional<SnapshotId> LatestBaselineId(ClientId clientId) const;
    [[nodiscard]] SnapshotBaselineDecision Decide(ClientId clientId, SnapshotId requestedBaselineId) const;
    [[nodiscard]] std::size_t BaselineCount(ClientId clientId) const;
    void Clear(ClientId clientId);

private:
    SnapshotBaselineCacheConfig config_{};
    std::unordered_map<ClientId, std::vector<SnapshotDTO>> baselinesByClient_{};
};

} // namespace game

