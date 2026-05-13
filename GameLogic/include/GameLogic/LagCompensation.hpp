#pragma once

#include "GameLogic/Replication.hpp"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <utility>

namespace game {

class LagCompensationHistory {
public:
    explicit LagCompensationHistory(std::size_t maxSnapshots = 128) : maxSnapshots_(maxSnapshots) {}

    void Record(SnapshotDTO snapshot) {
        snapshots_.push_back(std::move(snapshot));
        while (snapshots_.size() > maxSnapshots_) {
            snapshots_.pop_front();
        }
    }

    [[nodiscard]] std::optional<SnapshotDTO> ClosestAtOrBefore(TimestampMs serverTimeMs) const {
        std::optional<SnapshotDTO> result{};
        for (const auto& snapshot : snapshots_) {
            if (snapshot.serverTimeMs <= serverTimeMs) {
                result = snapshot;
            }
        }
        return result;
    }

    [[nodiscard]] std::size_t SnapshotCount() const noexcept {
        return snapshots_.size();
    }

    [[nodiscard]] std::optional<TimestampMs> OldestTimeMs() const noexcept {
        if (snapshots_.empty()) {
            return std::nullopt;
        }

        return snapshots_.front().serverTimeMs;
    }

    [[nodiscard]] std::optional<TimestampMs> NewestTimeMs() const noexcept {
        if (snapshots_.empty()) {
            return std::nullopt;
        }

        return snapshots_.back().serverTimeMs;
    }

private:
    std::size_t maxSnapshots_{};
    std::deque<SnapshotDTO> snapshots_{};
};

enum class LagCompensationRejectReason : std::uint8_t {
    None,
    TooOld,
    FromFuture,
    HistoryUnavailable,
};

struct LagCompensationConfig {
    TimestampMs maxRewindMs{250};
    TimestampMs futureToleranceMs{50};
};

struct LagCompensationQuery {
    ClientId clientId{};
    CommandSequence sequence{};
    TimestampMs clientTimestampMs{};
    std::int64_t clientToServerOffsetMs{};
    TimestampMs serverNowMs{};
};

struct LagCompensationResult {
    bool accepted{};
    LagCompensationRejectReason rejectReason{LagCompensationRejectReason::HistoryUnavailable};
    TimestampMs targetServerTimeMs{};
    std::optional<SnapshotDTO> historicalSnapshot{};
};

class LagCompensationService {
public:
    explicit LagCompensationService(LagCompensationConfig config = {});

    [[nodiscard]] LagCompensationResult Query(const LagCompensationHistory& history,
                                              const LagCompensationQuery& query) const;

private:
    [[nodiscard]] TimestampMs MapClientTimeToServerTime(const LagCompensationQuery& query) const;

    LagCompensationConfig config_{};
};

} // namespace game
