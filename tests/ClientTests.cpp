#include "Client/ClientApplication.hpp"
#include "Client/ClientEventLog.hpp"
#include "Client/ClientRuntime.hpp"
#include "Client/FixedStepClock.hpp"
#include "Client/InProcessClientSession.hpp"
#include "Client/RemoteClientSession.hpp"
#include "GameLogic/Commands.hpp"
#include "Networking/LoopbackTransport.hpp"
#include "Server/ServerNetworkHost.hpp"
#include "Server/ServerRuntime.hpp"
#include "TestSupport.hpp"

#include <memory>

namespace {

using test_support::ContainsEvent;
using test_support::Expect;

void TestLocalPredictionReconciliation()
{
    using namespace game;

    game::server::ServerRuntime server{};
    Expect(server.ConnectClient(1), "server connects client for reconciliation");
    server.Tick();
    auto snapshot = server.BuildSnapshotFor(1);

    game::client::ClientRuntime client{1};
    Expect(client.ConnectLocal(), "client creates local predicted player");
    client.ApplyServerSnapshot(snapshot);
    (void)client.DrainEvents();

    InputFrame frame{};
    frame.moveX = 1;
    (void)client.QueueInput(frame, 100);
    client.TickSimulation();

    snapshot.ackedInputSequence = 0;
    client.ApplyServerSnapshot(snapshot);
    Expect(ContainsEvent<LocalPredictionCorrected>(client.DrainEvents()), "authoritative snapshot corrects local prediction");
    Expect(!client.UnackedInputs().empty(), "unacked input remains for replay");
}

void TestClientApplicationWithInProcessSession()
{
    auto session = std::make_unique<game::client::InProcessClientSession>();
    game::client::ClientApplication app{std::move(session), game::client::ClientApplicationConfig{1}};
    Expect(app.Connect(0), "client application connects through in-process session");

    game::InputFrame input{};
    input.moveX = 1;
    const auto packet = app.SubmitInput(input, 16);
    Expect(packet.header.sequence == 1, "client application sends sequence-numbered input");

    app.TickFixed(16);
    const auto stats = app.Stats();
    Expect(stats.entityCount >= 1, "client application has replicated entities");
    Expect(stats.session.snapshotsApplied >= 1, "in-process session applies snapshots through boundary");
    Expect(stats.unackedInputCount == 0, "authoritative snapshot acks in-process input");
}

void TestRemoteClientSessionScaffold()
{
    auto transports = game::net::LoopbackTransport::CreatePair();
    auto remoteTransport = std::make_unique<game::net::LoopbackTransport>(std::move(transports.first));
    game::server::ServerNetworkHost serverHost{transports.second};
    game::client::RemoteClientSession session{std::move(remoteTransport)};
    game::client::ClientRuntime runtime{9};

    Expect(runtime.ConnectLocal(0), "remote session test runtime connects locally");
    Expect(session.Connect(9, 0), "remote session sends login over transport");
    serverHost.PumpClientMessages();
    Expect(serverHost.Stats().connectedClients == 1, "server receives remote session login");

    game::InputFrame input{};
    input.moveX = 1;
    const auto packet = runtime.QueueInput(input, 16);
    session.SendInput(packet);
    serverHost.PumpClientMessages();
    serverHost.TickAndSendSnapshots(16);
    session.Pump(runtime);

    Expect(session.Stats().snapshotsApplied == 1, "remote session applies snapshot via shared pump");
}

void TestFixedStepClockAndEventLog()
{
    game::client::FixedStepClock clock{game::client::FixedStepClockConfig{
        0.016F,
        0.100F,
        4,
    }};

    clock.BeginFrame(0.050F);
    int ticks = 0;
    while (clock.ShouldTick()) {
        clock.ConsumeTick();
        ++ticks;
    }
    clock.EndFrame();
    Expect(ticks == 3, "fixed-step clock consumes expected ticks");
    Expect(clock.RenderAlpha() > 0.0F, "fixed-step clock preserves render alpha");

    clock.BeginFrame(1.0F);
    ticks = 0;
    while (clock.ShouldTick()) {
        clock.ConsumeTick();
        ++ticks;
    }
    clock.EndFrame();
    Expect(ticks == 4, "fixed-step clock caps ticks per frame");

    game::client::ClientEventLog log{2, 1.0F};
    log.Push("one");
    log.Push("two");
    log.Push("three");
    Expect(log.Size() == 2, "event log keeps max entries");
    auto lines = log.Lines();
    Expect(lines.front() == "two" && lines.back() == "three", "event log drops oldest entries");
    log.Update(1.1F);
    Expect(log.Size() == 0, "event log expires old entries");
}

} // namespace

TEST(ClientTests, LocalPredictionReconciliation)
{
    TestLocalPredictionReconciliation();
}

TEST(ClientTests, ClientApplicationWithInProcessSession)
{
    TestClientApplicationWithInProcessSession();
}

TEST(ClientTests, RemoteClientSessionScaffold)
{
    TestRemoteClientSessionScaffold();
}

TEST(ClientTests, FixedStepClockAndEventLog)
{
    TestFixedStepClockAndEventLog();
}