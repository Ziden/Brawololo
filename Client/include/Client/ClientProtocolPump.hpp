#pragma once

#include "Client/IClientSession.hpp"
#include "GameLogic/NetworkEvents.hpp"
#include "GameLogic/TimeSync.hpp"
#include "Networking/ITransport.hpp"

namespace game::client {

class ClientRuntime;

class ClientProtocolPump {
public:
    explicit ClientProtocolPump(game::net::ITransport& transport);

    [[nodiscard]] bool SendLogin(const game::LoginCommand& command);
    [[nodiscard]] bool SendInput(const game::ClientInputPacket& packet);
    [[nodiscard]] bool SendSnapshotAck(const game::SnapshotAckDTO& ack);
    [[nodiscard]] bool SendNetworkEventAck(const game::NetworkEventAckDTO& ack);
    [[nodiscard]] bool SendTimeSyncRequest(const game::TimeSyncRequest& request);
    int PumpIncoming(
        ClientRuntime& runtime,
        ClientSessionStats& stats,
        game::EventList& reliableEvents,
        game::TimestampMs localReceiveTimeMs = 0);
    int PumpSnapshots(
        ClientRuntime& runtime,
        ClientSessionStats& stats,
        game::TimestampMs localReceiveTimeMs = 0);

private:
    game::net::ITransport& transport_;
};

} // namespace game::client
