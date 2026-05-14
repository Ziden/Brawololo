#include "Networking/KcpRtcSignalingPump.hpp"

namespace game::net {

KcpRtcSignalingPumpResult PumpKcpRtcSignaling(KcpRtcTransport& transport,
                                              IRtcSignalingClient& signalingClient) {
    KcpRtcSignalingPumpResult result{};

    while (auto signal = transport.PollOutgoingSignal()) {
        if (signalingClient.Send(*signal)) {
            ++result.outgoingSignalsSent;
        } else {
            ++result.failures;
        }
    }

    while (auto signal = signalingClient.Poll()) {
        if (transport.ReceiveSignalingMessage(*signal)) {
            ++result.incomingSignalsApplied;
        } else {
            ++result.failures;
        }
    }

    return result;
}

} // namespace game::net
