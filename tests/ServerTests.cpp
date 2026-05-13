#include "Client/ClientProtocolPump.hpp"
#include "Client/ClientRuntime.hpp"
#include "GameLogic/Commands.hpp"
#include "GameLogic/Serialization.hpp"
#include "Networking/LoopbackTransport.hpp"
#include "Server/ServerApplication.hpp"
#include "Server/ServerNetworkHost.hpp"
#include "Server/ServerRuntime.hpp"
#include "TestSupport.hpp"

#include <cstddef>
#include <memory>
#include <optional>
#include <span>

namespace {

using test_support::Expect;

void TestServerApplicationShell()
{
    auto transports = game::net::LoopbackTransport::CreatePair();
    auto serverTransport = std::make_unique<game::net::LoopbackTransport>(std::move(transports.second));
    game::client::ClientRuntime runtime{11};
    Expect(runtime.ConnectLocal(0), "server app test runtime connects locally");
    game::client::ClientProtocolPump clientPump{transports.first};

    game::server::ServerApplication app{std::move(serverTransport)};
    Expect(app.Start(), "server application starts");

    game::LoginCommand login{};
    login.header.clientId = 11;
    Expect(clientPump.SendLogin(login), "server app test sends login");
    app.RunForTicks(1);

    game::client::ClientSessionStats sessionStats{};
    const auto applied = clientPump.PumpSnapshots(runtime, sessionStats);
    const auto appStats = app.Stats();
    Expect(appStats.ticksRun == 1, "server application runs fixed tick");
    Expect(appStats.network.connectedClients == 1, "server application network host accepts client");
    Expect(applied == 1, "server application sends snapshot to client");

    app.RequestStop();
    Expect(!app.IsRunning(), "server application stops gracefully");
}

void TestServerNetworkHostUsesReplicationPlanner()
{
    auto [clientTransport, serverTransport] = game::net::LoopbackTransport::CreatePair();
    game::client::ClientProtocolPump clientPump{clientTransport};

    game::server::ServerNetworkHostConfig config{};
    config.replication.maxEntitiesPerSnapshot = 1;
    game::server::ServerNetworkHost serverHost{serverTransport, config};

    game::LoginCommand login{};
    login.header.clientId = 1;
    Expect(clientPump.SendLogin(login), "replication host test sends login");
    serverHost.PumpClientMessages();
    Expect(serverHost.ConnectSimulationOnlyClient(2), "replication host test connects simulation-only client");

    serverHost.TickAndSendSnapshots(16);
    const auto envelope = clientTransport.Poll();
    Expect(envelope.has_value(), "replication host sends planned snapshot");
    const auto snapshot = envelope.has_value()
        ? game::DeserializeSnapshot(std::span<const std::byte>{envelope->payload.data(), envelope->payload.size()})
        : std::optional<game::SnapshotDTO>{};
    Expect(snapshot.has_value(), "planned snapshot decodes");
    Expect(snapshot->entities.size() == 1, "server network host applies replication entity budget");
    Expect(serverHost.Stats().snapshotEntitiesDropped >= 1, "server network host records dropped snapshot entities");
}

void TestServerRejectsStaleAndCapsPlayers()
{
    using namespace game;

    game::server::ServerRuntime server{};
    Expect(server.ConnectClient(1), "server connects first client");

    ClientInputPacket input{};
    input.header.clientId = 1;
    input.header.sequence = 1;
    Expect(server.SubmitInput(input), "server accepts fresh sequence");
    Expect(!server.SubmitInput(input), "server rejects stale duplicate sequence");

    game::server::ServerRuntime capacityServer{};
    for (ClientId client = 1; client <= 256; ++client) {
        Expect(capacityServer.ConnectClient(client), "server accepts player within capacity");
    }
    Expect(!capacityServer.ConnectClient(257), "server rejects player beyond capacity");
}

} // namespace

TEST(ServerTests, ServerApplicationShell)
{
    TestServerApplicationShell();
}

TEST(ServerTests, ServerNetworkHostUsesReplicationPlanner)
{
    TestServerNetworkHostUsesReplicationPlanner();
}

TEST(ServerTests, ServerRejectsStaleAndCapsPlayers)
{
    TestServerRejectsStaleAndCapsPlayers();
}