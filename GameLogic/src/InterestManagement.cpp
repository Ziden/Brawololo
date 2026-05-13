#include "GameLogic/InterestManagement.hpp"

#include <utility>

namespace game {

InterestFrame InterestTracker::Update(const SnapshotDTO& snapshot) {
    InterestFrame frame{};
    std::unordered_set<NetworkEntityId> nextKnown{};

    for (const auto& entity : snapshot.entities) {
        const auto entityId = entity.replication.entityId;
        nextKnown.insert(entityId);

        if (knownEntities_.contains(entityId)) {
            frame.stayed.push_back(entityId);
        } else {
            frame.entered.push_back(entityId);
        }
    }

    for (const auto entityId : knownEntities_) {
        if (!nextKnown.contains(entityId)) {
            frame.exited.push_back(entityId);
        }
    }

    knownEntities_ = std::move(nextKnown);
    return frame;
}

bool InterestTracker::Contains(NetworkEntityId entityId) const {
    return knownEntities_.contains(entityId);
}

std::size_t InterestTracker::KnownEntityCount() const noexcept {
    return knownEntities_.size();
}

void InterestTracker::Clear() {
    knownEntities_.clear();
}

} // namespace game
