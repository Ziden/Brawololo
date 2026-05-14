#pragma once

#include "Networking/ITransport.hpp"
#include "Networking/KcpRtcTransport.hpp"
#include "Networking/RtcDataChannel.hpp"
#include "Networking/RtcSignalingClient.hpp"

#include <memory>
#include <string>
#include <utility>

namespace game::net {

class KcpRtcPumpedTransport final : public ITransport {
public:
    KcpRtcPumpedTransport(KcpRtcTransportConfig transportConfig,
                          std::unique_ptr<IRtcSignalingClient> signalingClient,
                          std::unique_ptr<IRtcDataChannel> dataChannel);

    [[nodiscard]] bool Connect() override;
    void Update() override;
    void Close() override;
    [[nodiscard]] bool Send(const game::NetworkEnvelope& envelope) override;
    [[nodiscard]] std::optional<game::NetworkEnvelope> Poll() override;
    [[nodiscard]] TransportState State() const noexcept override;
    [[nodiscard]] TransportStats Stats() const noexcept override;
    [[nodiscard]] TransportError LastError() const noexcept override;

    [[nodiscard]] const KcpRtcTransport& InnerTransport() const noexcept;
    [[nodiscard]] KcpRtcTransport& InnerTransport() noexcept;

private:
    void NotifyReadyIfPossible();

    KcpRtcTransport transport_;
    std::unique_ptr<IRtcSignalingClient> signalingClient_{};
    std::unique_ptr<IRtcDataChannel> dataChannel_{};
    TransportError lastError_{TransportError::None};
    bool readyNotified_{};
};

struct InMemoryKcpRtcTransportPairConfig {
    game::ClientId clientPeerId{1};
    game::ClientId serverPeerId{2};
    std::string sessionId{"default"};
    std::string dataChannelLabel{"game"};
    std::size_t maxBufferedAmountBytes{1024U * 1024U};
};

[[nodiscard]] std::pair<std::unique_ptr<ITransport>, std::unique_ptr<ITransport>>
CreateInMemoryKcpRtcTransportPair(InMemoryKcpRtcTransportPairConfig config = {});

} // namespace game::net
