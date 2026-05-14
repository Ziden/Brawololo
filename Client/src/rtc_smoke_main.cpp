#include "Client/ClientApplication.hpp"
#include "Client/RemoteClientSession.hpp"
#include "Networking/KcpRtcPumpedTransport.hpp"
#include "Server/ServerNetworkHost.hpp"

#include <iostream>
#include <memory>

namespace {

game::InputFrame ScriptedInputFor(int tick) {
    game::InputFrame input{};
    input.moveX = tick < 80 ? 1 : 0;
    input.aimX = 1000;
    input.aimY = 0;
    input.fire = tick == 40;
    return input;
}

} // namespace

int main() {
    auto transports = game::net::CreateInMemoryKcpRtcTransportPair(
        game::net::InMemoryKcpRtcTransportPairConfig{1, 2, "rtc-smoke"});
    auto clientTransport = std::move(transports.first);
    auto serverTransport = std::move(transports.second);

    if (!serverTransport->Connect()) {
        std::cerr << "Failed to start RTC smoke server transport\n";
        return 1;
    }

    game::server::ServerNetworkHost serverHost{*serverTransport};
    auto clientSession =
        std::make_unique<game::client::RemoteClientSession>(std::move(clientTransport));
    game::client::ClientApplication client{std::move(clientSession),
                                           game::client::ClientApplicationConfig{1}};

    if (!client.Connect(0)) {
        std::cerr << "Failed to start RTC smoke client transport\n";
        return 1;
    }

    for (int tick = 0; tick < 180; ++tick) {
        const auto nowMs = static_cast<game::TimestampMs>(tick * 16);

        (void)client.SubmitInput(ScriptedInputFor(tick), nowMs);

        serverTransport->Update();
        serverHost.PumpClientMessages();
        serverHost.TickAndSendSnapshots(nowMs);
        serverTransport->Update();

        client.TickFixed(nowMs);
        (void)client.DrainEvents();
    }

    const auto serverStats = serverHost.Stats();
    const auto clientStats = client.Stats();
    const auto ok = serverStats.connectedClients == 1 && serverStats.inputsReceived > 0 &&
                    serverStats.snapshotsSent > 0 && clientStats.session.snapshotsApplied > 0 &&
                    clientStats.session.connectionState ==
                        game::client::ClientConnectionState::Connected;

    std::cout << "RTC smoke ran. Connected clients: " << serverStats.connectedClients
              << ", inputs: " << serverStats.inputsReceived
              << ", snapshots sent/applied: " << serverStats.snapshotsSent << "/"
              << clientStats.session.snapshotsApplied << '\n';

    if (!ok) {
        std::cerr << "RTC smoke failed expected remote transport boundary checks\n";
        return 1;
    }

    return 0;
}
