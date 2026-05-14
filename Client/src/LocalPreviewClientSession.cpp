#include "Client/LocalPreviewClientSession.hpp"

namespace game::client {

bool LocalPreviewClientSession::Connect(game::ClientId localClientId, game::TimestampMs nowMs)
{
    (void)nowMs;
    stats_.localClientId = localClientId;
    stats_.connectionState = ClientConnectionState::LocalPreview;
    return true;
}

void LocalPreviewClientSession::SendInput(const game::ClientInputPacket& packet)
{
    (void)packet;
}

void LocalPreviewClientSession::Tick(game::TimestampMs nowMs)
{
    (void)nowMs;
}

void LocalPreviewClientSession::Pump(ClientRuntime& runtime)
{
    (void)runtime;
}

game::EventList LocalPreviewClientSession::DrainEvents()
{
    return {};
}

ClientSessionStats LocalPreviewClientSession::Stats() const noexcept
{
    return stats_;
}

} // namespace game::client
