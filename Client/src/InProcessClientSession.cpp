#include "Client/InProcessClientSession.hpp"

#include "Client/ClientProtocolPump.hpp"
#include "Client/ClientRuntime.hpp"

#include <memory>
#include <utility>

namespace game::client {

InProcessClientSession::InProcessClientSession(InProcessClientSessionConfig config)
    : config_(std::move(config))
{
}

bool InProcessClientSession::Connect(game::ClientId localClientId, game::TimestampMs nowMs)
{
    localClientId_ = localClientId;
    stats_.localClientId = localClientId_;
    stats_.connectionState = ClientConnectionState::Connecting;
    auto transports = game::net::LoopbackTransport::CreatePair();
    clientTransport_ = std::move(transports.first);
    serverTransport_ = std::move(transports.second);
    serverHost_ = std::make_unique<game::server::ServerNetworkHost>(serverTransport_);

    game::LoginCommand login{};
    login.header.clientId = localClientId_;
    login.header.sequence = 0;
    login.header.clientTimestampMs = nowMs;

    ClientProtocolPump clientPump{clientTransport_};
    connected_ = clientPump.SendLogin(login);
    stats_.connectionState = connected_
        ? ClientConnectionState::AwaitingSpawn
        : ClientConnectionState::Disconnected;
    serverHost_->PumpClientMessages();

    for (const auto clientId : config_.extraServerClients) {
        if (clientId != localClientId_) {
            connected_ = serverHost_->ConnectSimulationOnlyClient(clientId, nowMs) && connected_;
        }
    }

    return connected_;
}

void InProcessClientSession::SendInput(const game::ClientInputPacket& packet)
{
    if (!connected_) {
        return;
    }

    ClientProtocolPump pump{clientTransport_};
    (void)pump.SendInput(packet);
}

void InProcessClientSession::Tick(game::TimestampMs nowMs)
{
    if (!connected_ || !serverHost_) {
        return;
    }

    lastTickTimeMs_ = nowMs;

    if (nowMs >= nextTimeSyncAtMs_) {
        game::TimeSyncRequest request{};
        request.clientId = localClientId_;
        request.sequence = nextTimeSyncSequence_++;
        request.clientSentAtMs = nowMs;

        ClientProtocolPump clientPump{clientTransport_};
        if (clientPump.SendTimeSyncRequest(request)) {
            ++stats_.timeSyncRequestsSent;
        }
        nextTimeSyncAtMs_ = nowMs + 1000;
    }

    serverHost_->PumpClientMessages();
    serverHost_->TickAndSendSnapshots(nowMs);
}

void InProcessClientSession::Pump(ClientRuntime& runtime)
{
    if (!connected_) {
        return;
    }

    ClientProtocolPump pump{clientTransport_};
    (void)pump.PumpIncoming(runtime, stats_, reliableEvents_, lastTickTimeMs_);
}

game::EventList InProcessClientSession::DrainEvents()
{
    game::EventList drained{};
    drained.swap(reliableEvents_);
    return drained;
}

ClientSessionStats InProcessClientSession::Stats() const noexcept
{
    return stats_;
}

} // namespace game::client
