#pragma once

#include "GameLogic/Types.hpp"

#include <cstdint>
#include <type_traits>

namespace game {

struct TimeSyncRequest {
    ClientId clientId{};
    CommandSequence sequence{};
    TimestampMs clientSentAtMs{};
};

struct TimeSyncResponse {
    ClientId clientId{};
    CommandSequence sequence{};
    TimestampMs clientSentAtMs{};
    TimestampMs serverReceivedAtMs{};
    TimestampMs serverSentAtMs{};
};

struct TimeSyncSample {
    TimestampMs clientSentAtMs{};
    TimestampMs serverReceivedAtMs{};
    TimestampMs clientReceivedAtMs{};
};

class NetworkClock {
public:
    void RecordSample(const TimeSyncSample& sample);

    [[nodiscard]] std::int64_t ServerOffsetMs() const noexcept;
    [[nodiscard]] TimestampMs RttMs() const noexcept;
    [[nodiscard]] TimestampMs InterpolationDelayMs() const noexcept;
    [[nodiscard]] TimestampMs EstimateServerTime(TimestampMs localClientTimeMs) const noexcept;

private:
    std::int64_t serverOffsetMs_{};
    TimestampMs rttMs_{};
    TimestampMs interpolationDelayMs_{100};
    bool hasSample_{};
};

static_assert(std::is_trivially_copyable_v<TimeSyncRequest>);
static_assert(std::is_trivially_copyable_v<TimeSyncResponse>);
static_assert(std::is_trivially_copyable_v<TimeSyncSample>);

} // namespace game
