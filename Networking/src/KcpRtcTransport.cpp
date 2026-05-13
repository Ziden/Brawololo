#include "Networking/KcpRtcTransport.hpp"

#include <utility>

namespace game::net {

KcpRtcTransport::KcpRtcTransport(KcpRtcTransportConfig config) : config_(std::move(config)) {}

bool KcpRtcTransport::Connect() {
    if (config_.peerName.empty() || config_.signalingUrl.empty() ||
        config_.dataChannelLabel.empty() || config_.maxBufferedAmountBytes == 0) {
        state_ = TransportState::Failed;
        lastError_ = TransportError::InvalidConfiguration;
        return false;
    }

    // Scaffold seam: wire libdatachannel signaling/data channels and KCP session updates here.
    state_ = TransportState::Failed;
    lastError_ = TransportError::NotImplemented;
    return false;
}

void KcpRtcTransport::Update() {}

void KcpRtcTransport::Close() {
    state_ = TransportState::Disconnected;
    lastError_ = TransportError::None;
}

bool KcpRtcTransport::Send(const game::NetworkEnvelope& envelope) {
    if (!game::ValidateEnvelope(envelope).Ok()) {
        ++stats_.sendFailures;
        lastError_ = TransportError::ProtocolRejected;
        return false;
    }

    if (state_ != TransportState::Connected) {
        ++stats_.sendFailures;
        lastError_ = TransportError::NotConnected;
        return false;
    }

    ++stats_.sendFailures;
    lastError_ = TransportError::NotImplemented;
    return false;
}

std::optional<game::NetworkEnvelope> KcpRtcTransport::Poll() {
    return std::nullopt;
}

TransportState KcpRtcTransport::State() const noexcept {
    return state_;
}

TransportStats KcpRtcTransport::Stats() const noexcept {
    return stats_;
}

TransportError KcpRtcTransport::LastError() const noexcept {
    return lastError_;
}

} // namespace game::net
