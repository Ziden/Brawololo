#include "Client/ClientProtocolPump.hpp"
#include "Client/ClientRuntime.hpp"
#include "GameLogic/Commands.hpp"
#include "GameLogic/Components.hpp"
#include "GameLogic/NetworkEvents.hpp"
#include "GameLogic/Serialization.hpp"
#include "Networking/LoopbackTransport.hpp"
#include "Server/ServerApplication.hpp"
#include "Server/ServerNetworkHost.hpp"
#include "Server/ServerRuntime.hpp"
#include "Server/SimulationOnlyClientDriver.hpp"
#include "TestSupport.hpp"

#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <variant>

namespace {

using test_support::ContainsEvent;
using test_support::Expect;

void TestServerApplicationShell() {
    auto transports = game::net::LoopbackTransport::CreatePair();
    auto serverTransport =
        std::make_unique<game::net::LoopbackTransport>(std::move(transports.second));
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
    Expect(appStats.network.connectedClients == 1,
           "server application network host accepts client");
    Expect(applied == 1, "server application sends snapshot to client");

    app.RequestStop();
    Expect(!app.IsRunning(), "server application stops gracefully");
}

void TestServerNetworkHostUsesReplicationPlanner() {
    auto [clientTransport, serverTransport] = game::net::LoopbackTransport::CreatePair();
    game::client::ClientProtocolPump clientPump{clientTransport};

    game::server::ServerNetworkHostConfig config{};
    config.replication.maxEntitiesPerSnapshot = 1;
    game::server::ServerNetworkHost serverHost{serverTransport, config};

    game::LoginCommand login{};
    login.header.clientId = 1;
    Expect(clientPump.SendLogin(login), "replication host test sends login");
    serverHost.PumpClientMessages();
    Expect(serverHost.ConnectSimulationOnlyClient(2),
           "replication host test connects simulation-only client");

    serverHost.TickAndSendSnapshots(16);
    const auto envelope = clientTransport.Poll();
    Expect(envelope.has_value(), "replication host sends planned snapshot");
    const auto snapshot = envelope.has_value()
                              ? game::DeserializeSnapshot(std::span<const std::byte>{
                                    envelope->payload.data(), envelope->payload.size()})
                              : std::optional<game::SnapshotDTO>{};
    Expect(snapshot.has_value(), "planned snapshot decodes");
    Expect(snapshot->entities.size() == 1, "server network host applies replication entity budget");
    Expect(serverHost.Stats().snapshotEntitiesDropped >= 1,
           "server network host records dropped snapshot entities");
}

void TestServerNetworkHostPlansDeltaPlaceholdersAfterSnapshotAck() {
    auto [clientTransport, serverTransport] = game::net::LoopbackTransport::CreatePair();
    game::client::ClientProtocolPump clientPump{clientTransport};
    game::client::ClientRuntime runtime{1};
    Expect(runtime.ConnectLocal(0), "delta host test runtime connects locally");

    game::server::ServerNetworkHost serverHost{serverTransport};
    game::LoginCommand login{};
    login.header.clientId = 1;
    Expect(clientPump.SendLogin(login), "delta host test sends login");
    serverHost.PumpClientMessages();

    serverHost.TickAndSendSnapshots(16);
    game::client::ClientSessionStats sessionStats{};
    sessionStats.localClientId = 1;
    game::EventList ignoredEvents{};
    Expect(clientPump.PumpIncoming(runtime, sessionStats, ignoredEvents, 16) == 1,
           "delta host test applies first full snapshot");
    serverHost.PumpClientMessages();

    game::InputFrame move{};
    move.moveX = 1;
    const auto packet = runtime.QueueInput(move, 32);
    Expect(clientPump.SendInput(packet), "delta host test sends movement input");
    serverHost.PumpClientMessages();
    serverHost.TickAndSendSnapshots(32);

    const auto stats = serverHost.Stats();
    Expect(stats.deltaEligibleSnapshotsSent >= 1, "server marks acked snapshot as delta eligible");
    Expect(stats.deltaPlaceholderSnapshotsPlanned >= 1, "server computes delta placeholder plan");
    Expect(stats.deltaPlaceholderChangedEntities >= 1,
           "delta placeholder records changed entity state");
}

void TestServerNetworkHostSendsReliableInterestEnterAndLeaveEvents() {
    auto [clientTransport, serverTransport] = game::net::LoopbackTransport::CreatePair();
    game::client::ClientProtocolPump clientPump{clientTransport};
    game::server::ServerNetworkHost serverHost{serverTransport};

    game::LoginCommand login{};
    login.header.clientId = 1;
    Expect(clientPump.SendLogin(login), "interest event test sends login");
    serverHost.PumpClientMessages();
    Expect(serverHost.ConnectSimulationOnlyClient(2),
           "interest event test connects nearby simulation client");

    serverHost.TickAndSendSnapshots(16);

    std::size_t enteredEvents = 0;
    while (auto envelope = clientTransport.Poll()) {
        if (envelope->messageClass != game::MessageClass::InterestEvent) {
            continue;
        }

        const auto event = game::DeserializeNetworkEvent(
            std::span<const std::byte>{envelope->payload.data(), envelope->payload.size()});
        if (event.has_value() &&
            std::holds_alternative<game::EntityEnteredInterestEventDTO>(event->payload)) {
            ++enteredEvents;
        }
    }

    Expect(enteredEvents >= 2, "server sends reliable AOI enter events");

    const auto targetId = serverHost.Runtime().Simulation().PlayerEntityForClient(2);
    Expect(targetId.has_value(), "interest event test has target entity");
    auto& registry = serverHost.Runtime().Simulation().Registry();
    auto view = registry.view<game::NetworkIdentityComponent, game::TransformComponent>();
    for (const auto entity : view) {
        const auto& identity = view.get<game::NetworkIdentityComponent>(entity);
        if (identity.id == *targetId) {
            auto& transform = view.get<game::TransformComponent>(entity);
            transform.x = game::PixelsToFixed(128 * 70);
            transform.y = game::PixelsToFixed(128 * 70);
        }
    }

    serverHost.TickAndSendSnapshots(32);

    std::size_t leftEvents = 0;
    while (auto envelope = clientTransport.Poll()) {
        if (envelope->messageClass != game::MessageClass::InterestEvent) {
            continue;
        }

        const auto event = game::DeserializeNetworkEvent(
            std::span<const std::byte>{envelope->payload.data(), envelope->payload.size()});
        if (event.has_value() &&
            std::holds_alternative<game::EntityLeftInterestEventDTO>(event->payload)) {
            ++leftEvents;
        }
    }

    Expect(leftEvents >= 1, "server sends reliable AOI leave event");
}

void TestServerRejectsStaleAndCapsPlayers() {
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

void TestSimulationOnlyClientDriverUsesPureInputPackets() {
    using namespace game;

    SimulationConfig config{};
    config.weapons[static_cast<std::size_t>(WeaponType::Bow)].damage = 100;
    game::server::ServerRuntime server{config};
    Expect(server.ConnectClient(1), "driver test connects target client");
    Expect(server.ConnectClient(2), "driver test connects simulation-only client");
    (void)server.DrainEvents();

    game::server::SimulationOnlyClientDriver driver{game::server::SimulationOnlyClientDriverConfig{
        2,
        1,
        0,
        1000,
    }};
    Expect(driver.Tick(server, 0), "driver submits sequence-numbered pure input packet");

    for (int i = 0; i < 30; ++i) {
        server.Tick();
    }

    const auto events = server.DrainEvents();
    Expect(ContainsEvent<WeaponWarmupStarted>(events), "driver input starts weapon warmup");
    Expect(ContainsEvent<ProjectileSpawned>(events), "driver input spawns server-owned projectile");
    Expect(ContainsEvent<PlayerDamaged>(events),
           "driver projectile can damage target through server rules");
}

} // namespace

TEST(ServerTests, ServerApplicationShell) {
    TestServerApplicationShell();
}

TEST(ServerTests, ServerNetworkHostUsesReplicationPlanner) {
    TestServerNetworkHostUsesReplicationPlanner();
}

TEST(ServerTests, ServerNetworkHostPlansDeltaPlaceholdersAfterSnapshotAck) {
    TestServerNetworkHostPlansDeltaPlaceholdersAfterSnapshotAck();
}

TEST(ServerTests, ServerNetworkHostSendsReliableInterestEnterAndLeaveEvents) {
    TestServerNetworkHostSendsReliableInterestEnterAndLeaveEvents();
}

TEST(ServerTests, ServerRejectsStaleAndCapsPlayers) {
    TestServerRejectsStaleAndCapsPlayers();
}

TEST(ServerTests, SimulationOnlyClientDriverUsesPureInputPackets) {
    TestSimulationOnlyClientDriverUsesPureInputPackets();
}
