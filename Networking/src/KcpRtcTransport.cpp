#include "Networking/KcpRtcTransport.hpp"

#include "Networking/TransportPacketCodec.hpp"

#include <utility>

namespace game::net {

KcpRtcTransport::KcpRtcTransport(KcpRtcTransportConfig config) : config_(std::move(config)) {}

bool KcpRtcTransport::Connect() {
    if (!HasValidConfig()) {
        state_ = TransportState::Failed;
        phase_ = KcpRtcConnectionPhase::Failed;
        lastError_ = TransportError::InvalidConfiguration;
        return false;
    }

    state_ = TransportState::Connecting;
    phase_ = KcpRtcConnectionPhase::Signaling;
    lastError_ = TransportError::None;
    QueueSignal(RtcSignalingMessageKind::Join, 0, config_.peerName);
    if (config_.role == KcpRtcRole::Client) {
        QueueSignal(RtcSignalingMessageKind::Offer, 0, config_.dataChannelLabel);
    }
    return true;
}

void KcpRtcTransport::Update() {}

void KcpRtcTransport::Close() {
    state_ = TransportState::Disconnected;
    phase_ = KcpRtcConnectionPhase::Disconnected;
    lastError_ = TransportError::None;
    outgoingSignals_ = {};
    outgoingFrames_ = {};
    incomingEnvelopes_ = {};
    bufferedOutgoingFrameBytes_ = 0;
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

    auto frame = EncodeTransportPacket(envelope);
    if (bufferedOutgoingFrameBytes_ + frame.size() > config_.maxBufferedAmountBytes) {
        ++stats_.sendFailures;
        lastError_ = TransportError::ProtocolRejected;
        return false;
    }

    stats_.bytesSent += frame.size();
    ++stats_.envelopesSent;
    bufferedOutgoingFrameBytes_ += frame.size();
    outgoingFrames_.push(std::move(frame));
    lastError_ = TransportError::None;
    return true;
}

std::optional<game::NetworkEnvelope> KcpRtcTransport::Poll() {
    if (incomingEnvelopes_.empty()) {
        return std::nullopt;
    }

    auto envelope = incomingEnvelopes_.front();
    incomingEnvelopes_.pop();
    ++stats_.envelopesReceived;
    stats_.bytesReceived += envelope.payload.size();
    lastError_ = TransportError::None;
    return envelope;
}

KcpRtcConnectionPhase KcpRtcTransport::Phase() const noexcept {
    return phase_;
}

KcpRtcTransportDiagnostics KcpRtcTransport::Diagnostics() const noexcept {
    return KcpRtcTransportDiagnostics{
        phase_,
        outgoingSignals_.size(),
        outgoingFrames_.size(),
        incomingEnvelopes_.size(),
    };
}

std::optional<RtcSignalingMessage> KcpRtcTransport::PollOutgoingSignal() {
    if (outgoingSignals_.empty()) {
        return std::nullopt;
    }

    auto signal = outgoingSignals_.front();
    outgoingSignals_.pop();
    return signal;
}

bool KcpRtcTransport::ReceiveSignalingMessage(const RtcSignalingMessage& message) {
    if (!ValidateRtcSignalingMessage(message).Ok() || message.sessionId != config_.sessionId ||
        (message.targetPeerId != 0 && message.targetPeerId != config_.localPeerId)) {
        lastError_ = TransportError::ProtocolRejected;
        return false;
    }

    if (message.senderPeerId == config_.localPeerId) {
        lastError_ = TransportError::ProtocolRejected;
        return false;
    }

    switch (message.kind) {
        case RtcSignalingMessageKind::Offer:
            if (config_.role != KcpRtcRole::Server) {
                lastError_ = TransportError::ProtocolRejected;
                return false;
            }

            phase_ = KcpRtcConnectionPhase::DataChannelConnecting;
            state_ = TransportState::Connecting;
            QueueSignal(RtcSignalingMessageKind::Answer, message.senderPeerId,
                        config_.dataChannelLabel);
            break;

        case RtcSignalingMessageKind::Answer:
            if (config_.role != KcpRtcRole::Client) {
                lastError_ = TransportError::ProtocolRejected;
                return false;
            }

            phase_ = KcpRtcConnectionPhase::DataChannelConnecting;
            state_ = TransportState::Connecting;
            break;

        case RtcSignalingMessageKind::DataChannelReady:
            TransitionToConnected();
            break;

        case RtcSignalingMessageKind::Leave:
            state_ = TransportState::Disconnected;
            phase_ = KcpRtcConnectionPhase::Disconnected;
            break;

        case RtcSignalingMessageKind::Error:
            state_ = TransportState::Failed;
            phase_ = KcpRtcConnectionPhase::Failed;
            lastError_ = TransportError::PeerClosed;
            return false;

        case RtcSignalingMessageKind::Join:
        case RtcSignalingMessageKind::IceCandidate:
            break;
    }

    lastError_ = TransportError::None;
    return true;
}

std::optional<std::vector<std::byte>> KcpRtcTransport::PollOutgoingFrame() {
    if (outgoingFrames_.empty()) {
        return std::nullopt;
    }

    auto frame = std::move(outgoingFrames_.front());
    outgoingFrames_.pop();
    bufferedOutgoingFrameBytes_ -= frame.size();
    return frame;
}

bool KcpRtcTransport::ReceiveFrame(std::span<const std::byte> bytes) {
    if (state_ != TransportState::Connected) {
        lastError_ = TransportError::NotConnected;
        ++stats_.receiveFailures;
        return false;
    }

    const auto decoded = DecodeTransportPacket(bytes);
    if (!decoded.Ok()) {
        lastError_ = TransportError::ProtocolRejected;
        ++stats_.receiveFailures;
        return false;
    }

    incomingEnvelopes_.push(*decoded.envelope);
    lastError_ = TransportError::None;
    return true;
}

bool KcpRtcTransport::HasValidConfig() const noexcept {
    return config_.localPeerId != 0 && !config_.peerName.empty() && !config_.signalingUrl.empty() &&
           !config_.dataChannelLabel.empty() && config_.maxBufferedAmountBytes != 0 &&
           !config_.sessionId.empty();
}

void KcpRtcTransport::QueueSignal(RtcSignalingMessageKind kind,
                                  game::ClientId targetPeerId,
                                  std::string payload) {
    outgoingSignals_.push(RtcSignalingMessage{
        kind,
        config_.localPeerId,
        targetPeerId,
        nextSignalSequence_++,
        config_.sessionId,
        std::move(payload),
    });
}

void KcpRtcTransport::TransitionToConnected() {
    state_ = TransportState::Connected;
    phase_ = KcpRtcConnectionPhase::Connected;
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
