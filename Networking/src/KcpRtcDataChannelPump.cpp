#include "Networking/KcpRtcDataChannelPump.hpp"

namespace game::net {

KcpRtcDataChannelPumpResult PumpKcpRtcDataChannel(KcpRtcTransport& transport,
                                                  IRtcDataChannel& dataChannel) {
    KcpRtcDataChannelPumpResult result{};

    while (auto frame = transport.PollOutgoingFrame()) {
        if (dataChannel.SendFrame(*frame)) {
            ++result.outgoingFramesSent;
        } else {
            ++result.failures;
        }
    }

    while (auto frame = dataChannel.PollFrame()) {
        if (transport.ReceiveFrame(*frame)) {
            ++result.incomingFramesApplied;
        } else {
            ++result.failures;
        }
    }

    return result;
}

} // namespace game::net
