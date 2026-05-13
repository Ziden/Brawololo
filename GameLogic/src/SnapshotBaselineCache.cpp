#include "GameLogic/SnapshotBaselineCache.hpp"

#include <algorithm>
#include <utility>

namespace game {

SnapshotBaselineCache::SnapshotBaselineCache(SnapshotBaselineCacheConfig config)
    : config_(config)
{
}

void SnapshotBaselineCache::Store(ClientId clientId, SnapshotDTO snapshot)
{
    auto& baselines = baselinesByClient_[clientId];
    baselines.push_back(std::move(snapshot));
    while (baselines.size() > config_.maxBaselinesPerClient) {
        baselines.erase(baselines.begin());
    }
}

std::optional<SnapshotDTO> SnapshotBaselineCache::Find(ClientId clientId, SnapshotId snapshotId) const
{
    const auto found = baselinesByClient_.find(clientId);
    if (found == baselinesByClient_.end()) {
        return std::nullopt;
    }

    const auto& baselines = found->second;
    const auto baseline = std::find_if(
        baselines.begin(),
        baselines.end(),
        [snapshotId](const SnapshotDTO& snapshot) {
            return snapshot.snapshotId == snapshotId;
        });

    if (baseline == baselines.end()) {
        return std::nullopt;
    }

    return *baseline;
}

std::optional<SnapshotId> SnapshotBaselineCache::LatestBaselineId(ClientId clientId) const
{
    const auto found = baselinesByClient_.find(clientId);
    if (found == baselinesByClient_.end() || found->second.empty()) {
        return std::nullopt;
    }

    return found->second.back().snapshotId;
}

SnapshotBaselineDecision SnapshotBaselineCache::Decide(ClientId clientId, SnapshotId requestedBaselineId) const
{
    SnapshotBaselineDecision decision{};
    decision.requestedBaselineId = requestedBaselineId;

    if (requestedBaselineId == 0) {
        return decision;
    }

    const auto baseline = Find(clientId, requestedBaselineId);
    if (!baseline.has_value()) {
        return decision;
    }

    decision.effectiveBaselineId = requestedBaselineId;
    decision.deliveryKind = SnapshotDeliveryKind::DeltaEligible;
    decision.baselineAvailable = true;
    return decision;
}

std::size_t SnapshotBaselineCache::BaselineCount(ClientId clientId) const
{
    const auto found = baselinesByClient_.find(clientId);
    return found == baselinesByClient_.end() ? 0 : found->second.size();
}

void SnapshotBaselineCache::Clear(ClientId clientId)
{
    baselinesByClient_.erase(clientId);
}

} // namespace game

