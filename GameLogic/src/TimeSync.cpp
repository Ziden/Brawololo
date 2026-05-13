#include "GameLogic/TimeSync.hpp"

#include <algorithm>

namespace game {

void NetworkClock::RecordSample(const TimeSyncSample& sample)
{
    const auto measuredRtt = sample.clientReceivedAtMs >= sample.clientSentAtMs
        ? sample.clientReceivedAtMs - sample.clientSentAtMs
        : TimestampMs{};
    const auto clientMidpoint = sample.clientSentAtMs + (measuredRtt / 2);
    const auto measuredOffset = static_cast<std::int64_t>(sample.serverReceivedAtMs) -
        static_cast<std::int64_t>(clientMidpoint);

    if (!hasSample_) {
        serverOffsetMs_ = measuredOffset;
        rttMs_ = measuredRtt;
        hasSample_ = true;
    } else {
        serverOffsetMs_ = ((serverOffsetMs_ * 7) + measuredOffset) / 8;
        rttMs_ = ((rttMs_ * 7) + measuredRtt) / 8;
    }

    interpolationDelayMs_ = std::clamp<TimestampMs>((rttMs_ / 2) + 50, 50, 250);
}

std::int64_t NetworkClock::ServerOffsetMs() const noexcept
{
    return serverOffsetMs_;
}

TimestampMs NetworkClock::RttMs() const noexcept
{
    return rttMs_;
}

TimestampMs NetworkClock::InterpolationDelayMs() const noexcept
{
    return interpolationDelayMs_;
}

TimestampMs NetworkClock::EstimateServerTime(TimestampMs localClientTimeMs) const noexcept
{
    const auto estimated = static_cast<std::int64_t>(localClientTimeMs) + serverOffsetMs_;
    if (estimated <= 0) {
        return 0;
    }

    return static_cast<TimestampMs>(estimated);
}

} // namespace game
