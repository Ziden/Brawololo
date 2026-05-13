#include "Networking/KcpRtcTransport.hpp"

#include <utility>

namespace game::net {

KcpRtcTransport::KcpRtcTransport(KcpRtcTransportConfig config)
    : config_(std::move(config))
{
}

bool KcpRtcTransport::Connect()
{
    // Scaffold seam: wire libdatachannel signaling/data channels and KCP session updates here.
    state_ = TransportState::Failed;
    lastError_ = TransportError::NotImplemented;
    return false;
}

void KcpRtcTransport::Update()
{
}

void KcpRtcTransport::Close()
{
    state_ = TransportState::Disconnected;
    lastError_ = TransportError::None;
}

bool KcpRtcTransport::Send(const game::NetworkEnvelope& envelope)
{
    (void)envelope;
    ++stats_.sendFailures;
    lastError_ = TransportError::NotImplemented;
    return false;
}

std::optional<game::NetworkEnvelope> KcpRtcTransport::Poll()
{
    return std::nullopt;
}

TransportState KcpRtcTransport::State() const noexcept
{
    return state_;
}

TransportStats KcpRtcTransport::Stats() const noexcept
{
    return stats_;
}

TransportError KcpRtcTransport::LastError() const noexcept
{
    return lastError_;
}

} // namespace game::net

