#include "Client/ClientProtocolPump.hpp"
#include "Client/ClientRuntime.hpp"
#include "Client/RemoteClientSession.hpp"
#include "GameLogic/Commands.hpp"
#include "GameLogic/NetworkProtocol.hpp"
#include "GameLogic/Serialization.hpp"
#include "Networking/KcpRtcTransport.hpp"
#include "Networking/LoopbackTransport.hpp"
#include "Networking/MultiClientLoopbackTransport.hpp"
#include "Server/ServerNetworkHost.hpp"
#include "TestSupport.hpp"

#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace {

using test_support::Expect;

void TestProtocolPumpsAndServerNetworkHost() {
    auto [clientTransport, serverTransport] = game::net::LoopbackTransport::CreatePair();
    game::client::ClientRuntime runtime{5};
    Expect(runtime.ConnectLocal(0), "runtime connects local client for protocol pump test");

    game::client::ClientProtocolPump clientPump{clientTransport};
    game::server::ServerNetworkHost serverHost{serverTransport};

    game::LoginCommand login{};
    login.header.clientId = 5;
    login.header.clientTimestampMs = 11;
    Expect(clientPump.SendLogin(login), "client protocol pump sends login");
    serverHost.PumpClientMessages();
    Expect(serverHost.Stats().connectedClients == 1, "server network host handles login");

    game::InputFrame input{};
    input.moveX = 1;
    const auto packet = runtime.QueueInput(input, 16);
    Expect(clientPump.SendInput(packet), "client protocol pump sends input");
    serverHost.PumpClientMessages();
    serverHost.TickAndSendSnapshots(16);

    game::client::ClientSessionStats stats{};
    const auto snapshotsApplied = clientPump.PumpSnapshots(runtime, stats);
    Expect(snapshotsApplied == 1, "client protocol pump applies snapshot");
    Expect(stats.lastAckedInputSequence == packet.header.sequence,
           "snapshot ack reaches client stats");
}

void TestTransportLifecycleAndStats() {
    auto [clientTransport, serverTransport] = game::net::LoopbackTransport::CreatePair();
    Expect(clientTransport.State() == game::net::TransportState::Connected,
           "loopback pair starts connected");

    game::NetworkEnvelope invalid{};
    invalid.channel = game::NetworkChannel::Snapshots;
    invalid.messageClass = game::MessageClass::ClientInput;
    Expect(!clientTransport.Send(invalid), "transport rejects invalid protocol envelope");
    Expect(clientTransport.LastError() == game::net::TransportError::ProtocolRejected,
           "transport reports protocol rejection");
    Expect(clientTransport.Stats().sendFailures == 1, "transport records send failure");

    game::NetworkEnvelope valid{};
    valid.channel = game::NetworkChannel::MovementInput;
    valid.messageClass = game::MessageClass::ClientInput;
    valid.payload.resize(3);
    Expect(clientTransport.Send(valid), "transport sends valid envelope");
    const auto received = serverTransport.Poll();
    Expect(received.has_value(), "transport receives valid envelope");
    Expect(clientTransport.Stats().envelopesSent == 1, "transport records sent envelope");
    Expect(serverTransport.Stats().envelopesReceived == 1, "transport records received envelope");
    Expect(serverTransport.Stats().bytesReceived == 3, "transport records received payload bytes");

    clientTransport.Close();
    Expect(clientTransport.State() == game::net::TransportState::Disconnected,
           "transport closes cleanly");
    Expect(!clientTransport.Send(valid), "closed transport rejects send");
    Expect(clientTransport.LastError() == game::net::TransportError::NotConnected,
           "closed transport reports not connected");
}

void TestReliableEventsResendUntilAcknowledged() {
    auto [clientTransport, serverTransport] = game::net::LoopbackTransport::CreatePair();
    game::client::ClientProtocolPump clientPump{clientTransport};

    game::server::ServerNetworkHostConfig config{};
    config.reliableEventResendBaseIntervalMs = 1;
    config.reliableEventMaxSendCount = 4;
    game::server::ServerNetworkHost serverHost{serverTransport, config};

    game::LoginCommand login{};
    login.header.clientId = 5;
    Expect(clientPump.SendLogin(login), "reliable resend test sends login");
    serverHost.PumpClientMessages();

    serverHost.TickAndSendSnapshots(0);
    serverHost.TickAndSendSnapshots(1);

    std::size_t reliableEvents = 0;
    std::vector<game::NetworkEventId> eventIds{};
    while (auto envelope = clientTransport.Poll()) {
        if (envelope->messageClass != game::MessageClass::SpawnAccepted &&
            envelope->messageClass != game::MessageClass::InterestEvent) {
            continue;
        }

        const auto event = game::DeserializeNetworkEvent(
            std::span<const std::byte>{envelope->payload.data(), envelope->payload.size()});
        if (event.has_value()) {
            eventIds.push_back(event->eventId);
            ++reliableEvents;
        }
    }

    Expect(reliableEvents >= 2, "unacked reliable event is resent");
    Expect(serverHost.Stats().reliableEventsResent >= 1, "server records reliable event resend");
    Expect(!eventIds.empty(), "resent reliable events decode");

    for (const auto eventId : eventIds) {
        Expect(clientPump.SendNetworkEventAck(game::NetworkEventAckDTO{5, eventId, 2}),
               "client sends reliable event ack");
    }
    serverHost.PumpClientMessages();
    const auto resentBeforeAckedTick = serverHost.Stats().reliableEventsResent;

    serverHost.TickAndSendSnapshots(8);
    Expect(serverHost.Stats().reliableEventsResent == resentBeforeAckedTick,
           "acked reliable event stops resending");
}

void TestTwoRemoteClientsShareServerTransport() {
    const auto hub = game::net::MultiClientLoopbackTransport::CreateHub();
    auto serverTransport = game::net::MultiClientLoopbackTransport::CreateServer(hub);
    game::server::ServerNetworkHost serverHost{serverTransport};

    auto clientOneTransport = std::make_unique<game::net::MultiClientLoopbackTransport>(
        game::net::MultiClientLoopbackTransport::CreateClient(hub, 1));
    auto clientTwoTransport = std::make_unique<game::net::MultiClientLoopbackTransport>(
        game::net::MultiClientLoopbackTransport::CreateClient(hub, 2));
    game::client::RemoteClientSession clientOne{std::move(clientOneTransport)};
    game::client::RemoteClientSession clientTwo{std::move(clientTwoTransport)};
    game::client::ClientRuntime runtimeOne{1};
    game::client::ClientRuntime runtimeTwo{2};

    Expect(runtimeOne.ConnectLocal(0), "first remote runtime connects locally");
    Expect(runtimeTwo.ConnectLocal(0), "second remote runtime connects locally");
    Expect(clientOne.Connect(1, 0), "first remote client sends login");
    Expect(clientTwo.Connect(2, 0), "second remote client sends login");

    serverHost.PumpClientMessages();
    Expect(serverHost.Stats().connectedClients == 2,
           "peer-aware loopback routes both logins to one server host");

    serverHost.TickAndSendSnapshots(16);
    clientOne.Pump(runtimeOne);
    clientTwo.Pump(runtimeTwo);
    Expect(clientOne.Stats().snapshotsApplied == 1, "first client receives routed snapshot");
    Expect(clientTwo.Stats().snapshotsApplied == 1, "second client receives routed snapshot");

    game::InputFrame inputOne{};
    inputOne.moveX = 1;
    game::InputFrame inputTwo{};
    inputTwo.moveY = 1;
    clientOne.SendInput(runtimeOne.QueueInput(inputOne, 32));
    clientTwo.SendInput(runtimeTwo.QueueInput(inputTwo, 32));

    serverHost.PumpClientMessages();
    Expect(serverHost.Stats().inputsReceived >= 2,
           "peer-aware loopback routes both client inputs to one server host");
}

void TestKcpRtcTransportScaffoldFailsSafely() {
    game::net::KcpRtcTransport invalidTransport{game::net::KcpRtcTransportConfig{}};
    Expect(!invalidTransport.Connect(), "kcp rtc transport rejects invalid scaffold config");
    Expect(invalidTransport.LastError() == game::net::TransportError::InvalidConfiguration,
           "kcp rtc transport reports invalid configuration");

    game::net::KcpRtcTransport transport{game::net::KcpRtcTransportConfig{
        game::net::KcpRtcRole::Client,
        1,
        "client-one",
        "wss://signaling.invalid",
        "game",
        1024U * 1024U,
        true,
    }};
    Expect(!transport.Connect(), "kcp rtc transport remains explicit not-implemented seam");
    Expect(transport.LastError() == game::net::TransportError::NotImplemented,
           "kcp rtc transport reports not implemented when config is valid");

    game::NetworkEnvelope invalidEnvelope{};
    invalidEnvelope.channel = game::NetworkChannel::Snapshots;
    invalidEnvelope.messageClass = game::MessageClass::ClientInput;
    Expect(!transport.Send(invalidEnvelope), "kcp rtc transport validates protocol envelopes");
    Expect(transport.LastError() == game::net::TransportError::ProtocolRejected,
           "kcp rtc transport reports protocol rejection before state rejection");

    game::NetworkEnvelope validEnvelope{};
    validEnvelope.channel = game::NetworkChannel::MovementInput;
    validEnvelope.messageClass = game::MessageClass::ClientInput;
    Expect(!transport.Send(validEnvelope), "kcp rtc transport rejects valid send while disconnected");
    Expect(transport.LastError() == game::net::TransportError::NotConnected,
           "kcp rtc transport reports not connected for valid send before integration");
}

} // namespace

TEST(NetworkingTests, ProtocolPumpsAndServerNetworkHost) {
    TestProtocolPumpsAndServerNetworkHost();
}

TEST(NetworkingTests, TransportLifecycleAndStats) {
    TestTransportLifecycleAndStats();
}

TEST(NetworkingTests, ReliableEventsResendUntilAcknowledged) {
    TestReliableEventsResendUntilAcknowledged();
}

TEST(NetworkingTests, TwoRemoteClientsShareServerTransport) {
    TestTwoRemoteClientsShareServerTransport();
}

TEST(NetworkingTests, KcpRtcTransportScaffoldFailsSafely) {
    TestKcpRtcTransportScaffoldFailsSafely();
}
