#include "Networking/KcpRtcPumpedTransport.hpp"

#include "Networking/KcpRtcDataChannelPump.hpp"
#include "Networking/KcpRtcSignalingPump.hpp"

#include <utility>

namespace game::net {
namespace {

[[nodiscard]] TransportError MapSignalingError(RtcSignalingClientError error) noexcept {
    switch (error) {
        case RtcSignalingClientError::None:
            return TransportError::None;
        case RtcSignalingClientError::InvalidConfiguration:
            return TransportError::InvalidConfiguration;
        case RtcSignalingClientError::NotConnected:
        case RtcSignalingClientError::RouteUnavailable:
            return TransportError::NotConnected;
        case RtcSignalingClientError::NotImplemented:
            return TransportError::NotImplemented;
        case RtcSignalingClientError::ProtocolRejected:
            return TransportError::ProtocolRejected;
    }

    return TransportError::InvalidConfiguration;
}

[[nodiscard]] TransportError MapDataChannelError(RtcDataChannelError error) noexcept {
    switch (error) {
        case RtcDataChannelError::None:
            return TransportError::None;
        case RtcDataChannelError::NotOpen:
            return TransportError::NotConnected;
        case RtcDataChannelError::NotImplemented:
            return TransportError::NotImplemented;
        case RtcDataChannelError::BufferFull:
        case RtcDataChannelError::InvalidFrame:
            return TransportError::ProtocolRejected;
    }

    return TransportError::InvalidConfiguration;
}

} // namespace

KcpRtcPumpedTransport::KcpRtcPumpedTransport(
    KcpRtcTransportConfig transportConfig,
    std::unique_ptr<IRtcSignalingClient> signalingClient,
    std::unique_ptr<IRtcDataChannel> dataChannel)
    : transport_(std::move(transportConfig)), signalingClient_(std::move(signalingClient)),
      dataChannel_(std::move(dataChannel)) {}

bool KcpRtcPumpedTransport::Connect() {
    if (!signalingClient_ || !dataChannel_) {
        lastError_ = TransportError::InvalidConfiguration;
        return false;
    }

    if (signalingClient_->State() != RtcSignalingConnectionState::Connected &&
        !signalingClient_->Connect()) {
        lastError_ = MapSignalingError(signalingClient_->LastError());
        return false;
    }

    const auto connected = transport_.Connect();
    lastError_ = connected ? TransportError::None : transport_.LastError();
    return connected;
}

void KcpRtcPumpedTransport::Update() {
    if (!signalingClient_ || !dataChannel_) {
        lastError_ = TransportError::InvalidConfiguration;
        return;
    }

    (void)PumpKcpRtcSignaling(transport_, *signalingClient_);
    NotifyReadyIfPossible();
    (void)PumpKcpRtcSignaling(transport_, *signalingClient_);
    (void)PumpKcpRtcDataChannel(transport_, *dataChannel_);
    if (dataChannel_->State() == RtcDataChannelState::Failed) {
        lastError_ = MapDataChannelError(dataChannel_->LastError());
    } else {
        lastError_ = transport_.LastError();
    }
}

void KcpRtcPumpedTransport::Close() {
    transport_.Close();
    if (signalingClient_) {
        signalingClient_->Close();
    }

    readyNotified_ = false;
    lastError_ = TransportError::None;
}

bool KcpRtcPumpedTransport::Send(const game::NetworkEnvelope& envelope) {
    Update();
    const auto sent = transport_.Send(envelope);
    if (sent) {
        (void)PumpKcpRtcDataChannel(transport_, *dataChannel_);
    }
    lastError_ = sent ? TransportError::None : transport_.LastError();
    return sent;
}

std::optional<game::NetworkEnvelope> KcpRtcPumpedTransport::Poll() {
    Update();
    return transport_.Poll();
}

TransportState KcpRtcPumpedTransport::State() const noexcept {
    return transport_.State();
}

TransportStats KcpRtcPumpedTransport::Stats() const noexcept {
    return transport_.Stats();
}

TransportError KcpRtcPumpedTransport::LastError() const noexcept {
    return lastError_ == TransportError::None ? transport_.LastError() : lastError_;
}

const KcpRtcTransport& KcpRtcPumpedTransport::InnerTransport() const noexcept {
    return transport_;
}

KcpRtcTransport& KcpRtcPumpedTransport::InnerTransport() noexcept {
    return transport_;
}

void KcpRtcPumpedTransport::NotifyReadyIfPossible() {
    if (readyNotified_ || transport_.Phase() != KcpRtcConnectionPhase::DataChannelConnecting ||
        !dataChannel_ || dataChannel_->State() != RtcDataChannelState::Open) {
        return;
    }

    readyNotified_ = transport_.NotifyDataChannelReady();
}

std::pair<std::unique_ptr<ITransport>, std::unique_ptr<ITransport>>
CreateInMemoryKcpRtcTransportPair(InMemoryKcpRtcTransportPairConfig config) {
    const auto signalingHub = InMemoryRtcSignalingClient::CreateHub();
    auto dataChannels = InMemoryRtcDataChannel::CreatePair();

    auto clientSignaling = std::make_unique<InMemoryRtcSignalingClient>(
        signalingHub, RtcSignalingClientConfig{config.clientPeerId, config.sessionId});
    auto serverSignaling = std::make_unique<InMemoryRtcSignalingClient>(
        signalingHub, RtcSignalingClientConfig{config.serverPeerId, config.sessionId});

    KcpRtcTransportConfig clientConfig{};
    clientConfig.role = KcpRtcRole::Client;
    clientConfig.localPeerId = config.clientPeerId;
    clientConfig.peerName = "client";
    clientConfig.signalingUrl = "memory://signaling";
    clientConfig.dataChannelLabel = config.dataChannelLabel;
    clientConfig.maxBufferedAmountBytes = config.maxBufferedAmountBytes;
    clientConfig.sessionId = config.sessionId;

    KcpRtcTransportConfig serverConfig{};
    serverConfig.role = KcpRtcRole::Server;
    serverConfig.localPeerId = config.serverPeerId;
    serverConfig.peerName = "server";
    serverConfig.signalingUrl = "memory://signaling";
    serverConfig.dataChannelLabel = config.dataChannelLabel;
    serverConfig.maxBufferedAmountBytes = config.maxBufferedAmountBytes;
    serverConfig.sessionId = config.sessionId;

    return {
        std::make_unique<KcpRtcPumpedTransport>(std::move(clientConfig),
                                                std::move(clientSignaling),
                                                std::make_unique<InMemoryRtcDataChannel>(
                                                    std::move(dataChannels.first))),
        std::make_unique<KcpRtcPumpedTransport>(std::move(serverConfig),
                                                std::move(serverSignaling),
                                                std::make_unique<InMemoryRtcDataChannel>(
                                                    std::move(dataChannels.second))),
    };
}

} // namespace game::net
