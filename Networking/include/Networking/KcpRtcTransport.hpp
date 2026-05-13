#pragma once

#include "Networking/ITransport.hpp"

#include <string>

namespace game::net {

struct KcpRtcTransportConfig {
    std::string peerName{};
    std::string signalingUrl{};
    bool unorderedDataChannel{true};
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

private:
    KcpRtcTransportConfig config_{};
    TransportState state_{TransportState::Disconnected};
    TransportStats stats_{};
    TransportError lastError_{TransportError::None};
};

} // namespace game::net

