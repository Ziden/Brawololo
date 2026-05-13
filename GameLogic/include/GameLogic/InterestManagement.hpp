#pragma once

#include "GameLogic/Replication.hpp"

#include <cstddef>
#include <unordered_set>
#include <vector>

namespace game {

struct InterestFrame {
    std::vector<NetworkEntityId> entered{};
    std::vector<NetworkEntityId> stayed{};
    std::vector<NetworkEntityId> exited{};
};

class InterestTracker {
public:
    [[nodiscard]] InterestFrame Update(const SnapshotDTO& snapshot);
    [[nodiscard]] bool Contains(NetworkEntityId entityId) const;
    [[nodiscard]] std::size_t KnownEntityCount() const noexcept;
    void Clear();

private:
    std::unordered_set<NetworkEntityId> knownEntities_{};
};

} // namespace game
