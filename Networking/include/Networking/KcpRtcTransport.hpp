#pragma once

#include "Networking/ITransport.hpp"
#include "Networking/RtcSignaling.hpp"

#include <cstddef>
#include <optional>
#include <queue>
#include <span>
#include <string>
#include <vector>

namespace game::net {

enum class KcpRtcRole {
    Client,
    Server,
};

struct KcpRtcTransportConfig {
    KcpRtcRole role{KcpRtcRole::Client};
    game::ClientId localPeerId{};
    std::string peerName{};
    std::string signalingUrl{};
    std::string dataChannelLabel{"game"};
    std::size_t maxBufferedAmountBytes{1024U * 1024U};
    bool unorderedDataChannel{true};
    std::string sessionId{"default"};
};

enum class KcpRtcConnectionPhase {
    Disconnected,
    Signaling,
    DataChannelConnecting,
    Connected,
    Failed,
};

struct KcpRtcTransportDiagnostics {
    KcpRtcConnectionPhase phase{KcpRtcConnectionPhase::Disconnected};
    std::size_t outgoingSignalsQueued{};
    std::size_t outgoingFramesQueued{};
    std::size_t incomingFramesQueued{};
};

class KcpRtcTransport final : public ITransport {
public:
    explicit KcpRtcTransport(KcpRtcTransportConfig config);

    [[nodiscard]] bool Connect() override;
    void Update() override;
    void Close() override;
    [[nodiscard]] bool Send(const game::NetworkEnvelope& envelope) override;
    [[nodiscard]] std::optional<game::NetworkEnvelope> Poll() override;
    [[nodiscard]] TransportState State() const noexcept override;
    [[nodiscard]] TransportStats Stats() const noexcept override;
    [[nodiscard]] TransportError LastError() const noexcept override;

    [[nodiscard]] KcpRtcConnectionPhase Phase() const noexcept;
    [[nodiscard]] KcpRtcTransportDiagnostics Diagnostics() const noexcept;
    [[nodiscard]] std::optional<RtcSignalingMessage> PollOutgoingSignal();
    [[nodiscard]] bool ReceiveSignalingMessage(const RtcSignalingMessage& message);
    [[nodiscard]] std::optional<std::vector<std::byte>> PollOutgoingFrame();
    [[nodiscard]] bool ReceiveFrame(std::span<const std::byte> bytes);

private:
    [[nodiscard]] bool HasValidConfig() const noexcept;
    void QueueSignal(RtcSignalingMessageKind kind, game::ClientId targetPeerId, std::string payload);
    void TransitionToConnected();

    KcpRtcTransportConfig config_{};
    TransportState state_{TransportState::Disconnected};
    KcpRtcConnectionPhase phase_{KcpRtcConnectionPhase::Disconnected};
    TransportStats stats_{};
    TransportError lastError_{TransportError::None};
    std::uint32_t nextSignalSequence_{1};
    std::queue<RtcSignalingMessage> outgoingSignals_{};
    std::queue<std::vector<std::byte>> outgoingFrames_{};
    std::queue<game::NetworkEnvelope> incomingEnvelopes_{};
    std::size_t bufferedOutgoingFrameBytes_{};
};

} // namespace game::net
