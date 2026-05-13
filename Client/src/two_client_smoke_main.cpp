#include "Client/ClientApplication.hpp"
#include "Client/RemoteClientSession.hpp"
#include "GameLogic/Events.hpp"
#include "Networking/MultiClientLoopbackTransport.hpp"
#include "Server/ServerNetworkHost.hpp"

#include <iostream>
#include <memory>
#include <variant>

namespace {

struct CombatEventCounters {
    std::size_t hits{};
    std::size_t damaged{};
    std::size_t deaths{};
    std::size_t respawns{};
};

void CountCombatEvents(const game::EventList& events, CombatEventCounters& counters)
{
    for (const auto& event : events) {
        if (std::holds_alternative<game::HitConfirmed>(event)) {
            ++counters.hits;
        } else if (std::holds_alternative<game::PlayerDamaged>(event)) {
            ++counters.damaged;
        } else if (std::holds_alternative<game::PlayerDied>(event)) {
            ++counters.deaths;
        } else if (std::holds_alternative<game::PlayerRespawned>(event)) {
            ++counters.respawns;
        }
    }
}

game::InputFrame ScriptedInputFor(game::ClientId clientId, int tick)
{
    game::InputFrame input{};
    input.aimX = clientId == 1 ? 1000 : -1000;
    input.aimY = 0;
    input.fire = tick == 1 || tick == 80 || tick == 160 || tick == 240;
    return input;
}

std::unique_ptr<game::client::RemoteClientSession> MakeRemoteSession(
    const std::shared_ptr<game::net::MultiClientLoopbackTransport::Hub>& hub,
    game::ClientId clientId)
{
    auto transport = std::make_unique<game::net::MultiClientLoopbackTransport>(
        game::net::MultiClientLoopbackTransport::CreateClient(hub, clientId));
    return std::make_unique<game::client::RemoteClientSession>(std::move(transport));
}

} // namespace

int main()
{
    const auto hub = game::net::MultiClientLoopbackTransport::CreateHub();
    auto serverTransport = game::net::MultiClientLoopbackTransport::CreateServer(hub);
    game::server::ServerNetworkHost serverHost{serverTransport};

    game::client::ClientApplication clientOne{
        MakeRemoteSession(hub, 1),
        game::client::ClientApplicationConfig{1}};
    game::client::ClientApplication clientTwo{
        MakeRemoteSession(hub, 2),
        game::client::ClientApplicationConfig{2}};

    if (!clientOne.Connect(0) || !clientTwo.Connect(0)) {
        std::cerr << "Failed to connect remote client applications\n";
        return 1;
    }

    CombatEventCounters combatEvents{};

    for (int tick = 0; tick < 320; ++tick) {
        const auto nowMs = static_cast<game::TimestampMs>(tick * 16);

        (void)clientOne.SubmitInput(ScriptedInputFor(1, tick), nowMs);
        (void)clientTwo.SubmitInput(ScriptedInputFor(2, tick), nowMs);

        serverHost.PumpClientMessages();
        serverHost.TickAndSendSnapshots(nowMs);
        CountCombatEvents(serverHost.DrainEvents(), combatEvents);

        clientOne.TickFixed(nowMs);
        clientTwo.TickFixed(nowMs);
        (void)clientOne.DrainEvents();
        (void)clientTwo.DrainEvents();
    }

    const auto serverStats = serverHost.Stats();
    const auto clientOneStats = clientOne.Stats();
    const auto clientTwoStats = clientTwo.Stats();

    const auto ok = serverStats.connectedClients == 2 &&
        serverStats.inputsReceived >= 100 &&
        clientOneStats.session.snapshotsApplied > 0 &&
        clientTwoStats.session.snapshotsApplied > 0 &&
        combatEvents.hits > 0 &&
        combatEvents.damaged > 0;

    std::cout << "Two-client smoke ran. Connected clients: "
              << serverStats.connectedClients
              << ", inputs: "
              << serverStats.inputsReceived
              << ", client snapshots: "
              << clientOneStats.session.snapshotsApplied
              << "/"
              << clientTwoStats.session.snapshotsApplied
              << ", hits: "
              << combatEvents.hits
              << ", damage events: "
              << combatEvents.damaged
              << ", deaths: "
              << combatEvents.deaths
              << ", respawns: "
              << combatEvents.respawns
              << '\n';

    if (!ok) {
        std::cerr << "Two-client smoke failed expected multiplayer boundary checks\n";
        return 1;
    }

    return 0;
}
