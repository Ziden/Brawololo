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
    localClientId_ = localClientId;
    lastTickTimeMs_ = nowMs;

    if (!transport_->Connect()) {
        stats_.connectionState = ClientConnectionState::Disconnected;
        return false;
    }

    sessionActive_ = true;
    if (transport_->State() != game::net::TransportState::Connected) {
        return true;
    }

    return TrySendLogin(nowMs);
}

bool RemoteClientSession::TrySendLogin(game::TimestampMs nowMs) {
    if (!transport_ || !sessionActive_) {
        return false;
    }

    if (loginSent_) {
        return true;
    }

    if (transport_->State() != game::net::TransportState::Connected) {
        stats_.connectionState = ClientConnectionState::Connecting;
        return false;
    }

    game::LoginCommand login{};
    login.header.clientId = localClientId_;
    login.header.sequence = 0;
    login.header.clientTimestampMs = nowMs;

    ClientProtocolPump pump{*transport_};
    loginSent_ = pump.SendLogin(login);
    stats_.connectionState =
        loginSent_ ? ClientConnectionState::AwaitingSpawn : ClientConnectionState::Disconnected;
    sessionActive_ = loginSent_;
    return loginSent_;
}

void RemoteClientSession::SendInput(const game::ClientInputPacket& packet) {
    if (!sessionActive_ || !loginSent_ || !transport_) {
        return;
    }

    ClientProtocolPump pump{*transport_};
    (void)pump.SendInput(packet);
}

void RemoteClientSession::Tick(game::TimestampMs nowMs) {
    lastTickTimeMs_ = nowMs;

    if (!sessionActive_ || !transport_) {
        return;
    }

    transport_->Update();
    if (!loginSent_) {
        (void)TrySendLogin(nowMs);
        return;
    }

    if (nowMs < nextTimeSyncAtMs_) {
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
    if (!sessionActive_ || !transport_) {
        return;
    }

    transport_->Update();
    if (!loginSent_) {
        (void)TrySendLogin(lastTickTimeMs_);
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
