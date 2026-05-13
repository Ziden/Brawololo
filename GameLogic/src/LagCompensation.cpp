#include "GameLogic/LagCompensation.hpp"

namespace game {

LagCompensationService::LagCompensationService(LagCompensationConfig config) : config_(config) {}

LagCompensationResult LagCompensationService::Query(const LagCompensationHistory& history,
                                                    const LagCompensationQuery& query) const {
    LagCompensationResult result{};
    result.targetServerTimeMs = MapClientTimeToServerTime(query);

    if (result.targetServerTimeMs > query.serverNowMs + config_.futureToleranceMs) {
        result.rejectReason = LagCompensationRejectReason::FromFuture;
        return result;
    }

    if (query.serverNowMs > result.targetServerTimeMs &&
        query.serverNowMs - result.targetServerTimeMs > config_.maxRewindMs) {
        result.rejectReason = LagCompensationRejectReason::TooOld;
        return result;
    }

    result.historicalSnapshot = history.ClosestAtOrBefore(result.targetServerTimeMs);
    if (!result.historicalSnapshot.has_value()) {
        result.rejectReason = LagCompensationRejectReason::HistoryUnavailable;
        return result;
    }

    result.accepted = true;
    result.rejectReason = LagCompensationRejectReason::None;
    return result;
}

TimestampMs
LagCompensationService::MapClientTimeToServerTime(const LagCompensationQuery& query) const {
    const auto mapped =
        static_cast<std::int64_t>(query.clientTimestampMs) + query.clientToServerOffsetMs;
    return mapped <= 0 ? TimestampMs{} : static_cast<TimestampMs>(mapped);
}

} // namespace game
