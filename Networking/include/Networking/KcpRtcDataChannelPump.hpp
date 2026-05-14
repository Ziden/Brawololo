#pragma once

#include "Networking/KcpRtcTransport.hpp"
#include "Networking/RtcDataChannel.hpp"

#include <cstddef>

namespace game::net {

struct KcpRtcDataChannelPumpResult {
    std::size_t outgoingFramesSent{};
    std::size_t incomingFramesApplied{};
    std::size_t failures{};
};

[[nodiscard]] KcpRtcDataChannelPumpResult PumpKcpRtcDataChannel(
    KcpRtcTransport& transport,
    IRtcDataChannel& dataChannel);

} // namespace game::net
