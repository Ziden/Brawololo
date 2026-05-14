#pragma once

#include "Networking/KcpRtcTransport.hpp"
#include "Networking/RtcSignalingClient.hpp"

#include <cstddef>

namespace game::net {

struct KcpRtcSignalingPumpResult {
    std::size_t outgoingSignalsSent{};
    std::size_t incomingSignalsApplied{};
    std::size_t failures{};
};

[[nodiscard]] KcpRtcSignalingPumpResult PumpKcpRtcSignaling(
    KcpRtcTransport& transport,
    IRtcSignalingClient& signalingClient);

} // namespace game::net
