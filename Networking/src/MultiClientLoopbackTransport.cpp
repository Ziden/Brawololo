#include "Networking/MultiClientLoopbackTransport.hpp"

#include <utility>

namespace game::net {

std::shared_ptr<MultiClientLoopbackTransport::Hub> MultiClientLoopbackTransport::CreateHub() {
    return std::make_shared<Hub>();
}

MultiClientLoopbackTransport MultiClientLoopbackTransport::CreateServer(std::shared_ptr<Hub> hub) {
    return MultiClientLoopbackTransport{std::move(hub), Endpoint::Server};
}

MultiClientLoopbackTransport MultiClientLoopbackTransport::CreateClient(std::shared_ptr<Hub> hub,
                                                                        game::ClientId clientId) {
    return MultiClientLoopbackTransport{std::move(hub), Endpoint::Client, clientId};
}

MultiClientLoopbackTransport::MultiClientLoopbackTransport(std::shared_ptr<Hub> hub,
                                                           Endpoint endpoint,
                                                           game::ClientId clientId)
    : hub_(std::move(hub)), endpoint_(endpoint), clientId_(clientId),
      state_(TransportState::Connected) {}

bool MultiClientLoopbackTransport::Connect() {
    if (!hub_) {
        state_ = TransportState::Failed;
        lastError_ = TransportError::NotConnected;
        return false;
    }

    state_ = TransportState::Connected;
    lastError_ = TransportError::None;
    return true;
}

void MultiClientLoopbackTransport::Update() {}

void MultiClientLoopbackTransport::Close() {
    state_ = TransportState::Disconnected;
}

bool MultiClientLoopbackTransport::Send(const game::NetworkEnvelope& envelope) {
    if (!hub_ || state_ != TransportState::Connected) {
        lastError_ = TransportError::NotConnected;
        ++stats_.sendFailures;
        return false;
    }

    if (!game::ValidateEnvelope(envelope).Ok()) {
        lastError_ = TransportError::ProtocolRejected;
        ++stats_.sendFailures;
        return false;
    }

    auto routed = envelope;
    if (endpoint_ == Endpoint::Client) {
        routed.peerId = clientId_;
        hub_->toServer.push(std::move(routed));
    } else {
        if (routed.peerId == 0) {
            lastError_ = TransportError::ProtocolRejected;
            ++stats_.sendFailures;
            return false;
        }
        hub_->toClients[routed.peerId].push(std::move(routed));
    }

    ++stats_.envelopesSent;
    stats_.bytesSent += envelope.payload.size();
    lastError_ = TransportError::None;
    return true;
}

std::optional<game::NetworkEnvelope> MultiClientLoopbackTransport::Poll() {
    if (!hub_ || state_ != TransportState::Connected) {
        return std::nullopt;
    }

    auto& queue = endpoint_ == Endpoint::Server ? hub_->toServer : hub_->toClients[clientId_];
    if (queue.empty()) {
        return std::nullopt;
    }

    auto envelope = queue.front();
    queue.pop();
    if (!game::ValidateEnvelope(envelope).Ok()) {
        lastError_ = TransportError::ProtocolRejected;
        ++stats_.receiveFailures;
        return std::nullopt;
    }

    ++stats_.envelopesReceived;
    stats_.bytesReceived += envelope.payload.size();
    lastError_ = TransportError::None;
    return envelope;
}

TransportState MultiClientLoopbackTransport::State() const noexcept {
    return state_;
}

TransportStats MultiClientLoopbackTransport::Stats() const noexcept {
    return stats_;
}

TransportError MultiClientLoopbackTransport::LastError() const noexcept {
    return lastError_;
}

} // namespace game::net
