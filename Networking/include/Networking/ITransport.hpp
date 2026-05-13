#pragma once

#include "GameLogic/NetworkProtocol.hpp"

#include <cstddef>
#include <optional>

namespace game::net {

enum class TransportState {
    Disconnected,
    Connecting,
    Connected,
    Closing,
    Failed,
};

enum class TransportError {
    None,
    NotConnected,
    ProtocolRejected,
    NotImplemented,
    InvalidConfiguration,
    PeerClosed,
};

struct TransportStats {
    std::size_t envelopesSent{};
    std::size_t envelopesReceived{};
    std::size_t sendFailures{};
    std::size_t receiveFailures{};
    std::size_t bytesSent{};
    std::size_t bytesReceived{};
    std::size_t envelopesDroppedByCondition{};
    std::size_t envelopesDelayedByCondition{};
};

class ITransport {
public:
    virtual ~ITransport() = default;

    [[nodiscard]] virtual bool Connect() = 0;
    virtual void Update() = 0;
    virtual void Close() = 0;
    [[nodiscard]] virtual bool Send(const game::NetworkEnvelope& envelope) = 0;
    [[nodiscard]] virtual std::optional<game::NetworkEnvelope> Poll() = 0;
    [[nodiscard]] virtual TransportState State() const noexcept = 0;
    [[nodiscard]] virtual TransportStats Stats() const noexcept = 0;
    [[nodiscard]] virtual TransportError LastError() const noexcept = 0;
};

} // namespace game::net
