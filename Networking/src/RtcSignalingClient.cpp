#include "Networking/RtcSignalingClient.hpp"

#include <utility>

namespace game::net {

bool InMemoryRtcSignalingHub::RegisterPeer(game::ClientId peerId, std::string sessionId) {
    if (peerId == 0 || sessionId.empty()) {
        return false;
    }

    peers_[peerId] = PeerQueue{std::move(sessionId), {}};
    return true;
}

void InMemoryRtcSignalingHub::UnregisterPeer(game::ClientId peerId) {
    peers_.erase(peerId);
}

bool InMemoryRtcSignalingHub::Route(std::span<const std::byte> bytes) {
    const auto decoded = DeserializeRtcSignalingMessage(bytes);
    if (!decoded.Ok()) {
        return false;
    }

    const auto& message = *decoded.message;
    const auto sender = peers_.find(message.senderPeerId);
    if (sender == peers_.end() || sender->second.sessionId != message.sessionId) {
        return false;
    }

    const std::vector<std::byte> ownedBytes{bytes.begin(), bytes.end()};
    if (message.targetPeerId != 0) {
        const auto target = peers_.find(message.targetPeerId);
        if (target == peers_.end() || target->second.sessionId != message.sessionId) {
            return false;
        }

        target->second.incoming.push(ownedBytes);
        return true;
    }

    bool delivered = false;
    for (auto& [peerId, peer] : peers_) {
        if (peerId == message.senderPeerId || peer.sessionId != message.sessionId) {
            continue;
        }

        peer.incoming.push(ownedBytes);
        delivered = true;
    }

    return delivered;
}

std::optional<std::vector<std::byte>> InMemoryRtcSignalingHub::Poll(game::ClientId peerId) {
    const auto peer = peers_.find(peerId);
    if (peer == peers_.end() || peer->second.incoming.empty()) {
        return std::nullopt;
    }

    auto bytes = std::move(peer->second.incoming.front());
    peer->second.incoming.pop();
    return bytes;
}

std::size_t InMemoryRtcSignalingHub::PeerCount() const noexcept {
    return peers_.size();
}

std::shared_ptr<InMemoryRtcSignalingHub> InMemoryRtcSignalingClient::CreateHub() {
    return std::make_shared<InMemoryRtcSignalingHub>();
}

InMemoryRtcSignalingClient::InMemoryRtcSignalingClient(
    std::shared_ptr<InMemoryRtcSignalingHub> hub,
    RtcSignalingClientConfig config)
    : hub_(std::move(hub)), config_(std::move(config)) {}

bool InMemoryRtcSignalingClient::Connect() {
    if (!hub_ || !HasValidConfig()) {
        state_ = RtcSignalingConnectionState::Failed;
        lastError_ = RtcSignalingClientError::InvalidConfiguration;
        return false;
    }

    if (!hub_->RegisterPeer(config_.peerId, config_.sessionId)) {
        state_ = RtcSignalingConnectionState::Failed;
        lastError_ = RtcSignalingClientError::InvalidConfiguration;
        return false;
    }

    state_ = RtcSignalingConnectionState::Connected;
    lastError_ = RtcSignalingClientError::None;
    return true;
}

void InMemoryRtcSignalingClient::Close() {
    if (hub_) {
        hub_->UnregisterPeer(config_.peerId);
    }

    state_ = RtcSignalingConnectionState::Disconnected;
    lastError_ = RtcSignalingClientError::None;
}

bool InMemoryRtcSignalingClient::Send(const RtcSignalingMessage& message) {
    if (state_ != RtcSignalingConnectionState::Connected || !hub_) {
        ++stats_.sendFailures;
        lastError_ = RtcSignalingClientError::NotConnected;
        return false;
    }

    if (!ValidateRtcSignalingMessage(message).Ok() || message.senderPeerId != config_.peerId ||
        message.sessionId != config_.sessionId) {
        ++stats_.sendFailures;
        lastError_ = RtcSignalingClientError::ProtocolRejected;
        return false;
    }

    const auto bytes = SerializeRtcSignalingMessage(message);
    if (!hub_->Route(bytes)) {
        ++stats_.sendFailures;
        lastError_ = RtcSignalingClientError::RouteUnavailable;
        return false;
    }

    ++stats_.messagesSent;
    stats_.bytesSent += bytes.size();
    lastError_ = RtcSignalingClientError::None;
    return true;
}

std::optional<RtcSignalingMessage> InMemoryRtcSignalingClient::Poll() {
    if (state_ != RtcSignalingConnectionState::Connected || !hub_) {
        return std::nullopt;
    }

    const auto bytes = hub_->Poll(config_.peerId);
    if (!bytes.has_value()) {
        return std::nullopt;
    }

    const auto decoded = DeserializeRtcSignalingMessage(*bytes);
    if (!decoded.Ok()) {
        ++stats_.receiveFailures;
        lastError_ = RtcSignalingClientError::ProtocolRejected;
        return std::nullopt;
    }

    ++stats_.messagesReceived;
    stats_.bytesReceived += bytes->size();
    lastError_ = RtcSignalingClientError::None;
    return decoded.message;
}

RtcSignalingConnectionState InMemoryRtcSignalingClient::State() const noexcept {
    return state_;
}

RtcSignalingClientStats InMemoryRtcSignalingClient::Stats() const noexcept {
    return stats_;
}

RtcSignalingClientError InMemoryRtcSignalingClient::LastError() const noexcept {
    return lastError_;
}

bool InMemoryRtcSignalingClient::HasValidConfig() const noexcept {
    return config_.peerId != 0 && !config_.sessionId.empty();
}

} // namespace game::net
