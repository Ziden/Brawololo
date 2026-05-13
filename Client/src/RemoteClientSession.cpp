#include "Client/RemoteClientSession.hpp"

#include "Client/ClientProtocolPump.hpp"
#include "Client/ClientRuntime.hpp"

#include <utility>

namespace game::client {

RemoteClientSession::RemoteClientSession(std::unique_ptr<game::net::ITransport> transport)
    : transport_(std::move(transport)) {}

bool RemoteClientSession::Connect(game::ClientId localClientId, game::TimestampMs nowMs) {
    if (!transport_) {
        return false;
    }

    stats_.localClientId = localClientId;
    stats_.connectionState = ClientConnectionState::Connecting;

    if (!transport_->Connect()) {
        stats_.connectionState = ClientConnectionState::Disconnected;
        return false;
    }

    localClientId_ = localClientId;
    game::LoginCommand login{};
    login.header.clientId = localClientId_;
    login.header.sequence = 0;
    login.header.clientTimestampMs = nowMs;

    ClientProtocolPump pump{*transport_};
    connected_ = pump.SendLogin(login);
    stats_.connectionState =
        connected_ ? ClientConnectionState::AwaitingSpawn : ClientConnectionState::Disconnected;
    return connected_;
}

void RemoteClientSession::SendInput(const game::ClientInputPacket& packet) {
    if (!connected_ || !transport_) {
        return;
    }

    ClientProtocolPump pump{*transport_};
    (void)pump.SendInput(packet);
}

void RemoteClientSession::Tick(game::TimestampMs nowMs) {
    lastTickTimeMs_ = nowMs;

    if (!connected_ || !transport_ || nowMs < nextTimeSyncAtMs_) {
        return;
    }

    game::TimeSyncRequest request{};
    request.clientId = localClientId_;
    request.sequence = nextTimeSyncSequence_++;
    request.clientSentAtMs = nowMs;

    ClientProtocolPump pump{*transport_};
    if (pump.SendTimeSyncRequest(request)) {
        ++stats_.timeSyncRequestsSent;
    }
    nextTimeSyncAtMs_ = nowMs + 1000;
}

void RemoteClientSession::Pump(ClientRuntime& runtime) {
    if (!connected_ || !transport_) {
        return;
    }

    ClientProtocolPump pump{*transport_};
    (void)pump.PumpIncoming(runtime, stats_, reliableEvents_, lastTickTimeMs_);
}

game::EventList RemoteClientSession::DrainEvents() {
    game::EventList drained{};
    drained.swap(reliableEvents_);
    return drained;
}

ClientSessionStats RemoteClientSession::Stats() const noexcept {
    return stats_;
}

} // namespace game::client
