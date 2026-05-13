#pragma once

#include "GameLogic/InterestManagement.hpp"
#include "GameLogic/NetworkEvents.hpp"
#include "GameLogic/Types.hpp"

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <utility>

namespace game::server {

struct PendingReliableEvent {
    game::NetworkEventDTO event{};
    game::TimestampMs firstSentAtMs{};
    game::TimestampMs lastSentAtMs{};
    std::uint32_t sendCount{};
};

class ServerClientReplicationState {
public:
    ServerClientReplicationState() = default;
    explicit ServerClientReplicationState(game::ClientId clientId)
        : clientId_(clientId)
    {
    }

    [[nodiscard]] game::ClientId ClientId() const noexcept
    {
        return clientId_;
    }

    [[nodiscard]] game::SnapshotId AckedSnapshot() const noexcept
    {
        return ackedSnapshot_;
    }

    [[nodiscard]] bool AcknowledgeSnapshot(game::SnapshotId snapshotId) noexcept
    {
        if (snapshotId <= ackedSnapshot_) {
            return false;
        }

        ackedSnapshot_ = snapshotId;
        return true;
    }

    [[nodiscard]] bool ShouldSendSnapshot(game::TimestampMs nowMs) const noexcept
    {
        return nowMs >= nextSnapshotSendAtMs_;
    }

    void MarkSnapshotSent(game::TimestampMs nowMs, game::TimestampMs intervalMs) noexcept
    {
        nextSnapshotSendAtMs_ = nowMs + intervalMs;
    }

    [[nodiscard]] game::InterestTracker& Interest() noexcept
    {
        return interest_;
    }

    void TrackReliableEvent(game::NetworkEventDTO event, game::TimestampMs nowMs)
    {
        auto& pending = pendingReliableEvents_[event.eventId];
        if (pending.sendCount == 0) {
            pending.firstSentAtMs = nowMs;
            pending.event = std::move(event);
        }

        pending.lastSentAtMs = nowMs;
        ++pending.sendCount;
    }

    [[nodiscard]] bool AcknowledgeReliableEvent(game::NetworkEventId eventId)
    {
        return pendingReliableEvents_.erase(eventId) > 0;
    }

    [[nodiscard]] std::size_t PendingReliableEventCount() const noexcept
    {
        return pendingReliableEvents_.size();
    }

private:
    game::ClientId clientId_{};
    game::SnapshotId ackedSnapshot_{};
    game::TimestampMs nextSnapshotSendAtMs_{};
    game::InterestTracker interest_{};
    std::unordered_map<game::NetworkEventId, PendingReliableEvent> pendingReliableEvents_{};
};

} // namespace game::server
