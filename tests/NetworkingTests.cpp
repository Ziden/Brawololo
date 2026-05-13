#include "Client/ClientProtocolPump.hpp"
#include "Client/ClientRuntime.hpp"
#include "GameLogic/Commands.hpp"
#include "GameLogic/NetworkProtocol.hpp"
#include "Networking/LoopbackTransport.hpp"
#include "Server/ServerNetworkHost.hpp"
#include "TestSupport.hpp"

namespace {

using test_support::Expect;

void TestProtocolPumpsAndServerNetworkHost()
{
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
    Expect(stats.lastAckedInputSequence == packet.header.sequence, "snapshot ack reaches client stats");
}

void TestTransportLifecycleAndStats()
{
    auto [clientTransport, serverTransport] = game::net::LoopbackTransport::CreatePair();
    Expect(clientTransport.State() == game::net::TransportState::Connected, "loopback pair starts connected");

    game::NetworkEnvelope invalid{};
    invalid.channel = game::NetworkChannel::Snapshots;
    invalid.messageClass = game::MessageClass::ClientInput;
    Expect(!clientTransport.Send(invalid), "transport rejects invalid protocol envelope");
    Expect(clientTransport.LastError() == game::net::TransportError::ProtocolRejected, "transport reports protocol rejection");
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
    Expect(clientTransport.State() == game::net::TransportState::Disconnected, "transport closes cleanly");
    Expect(!clientTransport.Send(valid), "closed transport rejects send");
    Expect(clientTransport.LastError() == game::net::TransportError::NotConnected, "closed transport reports not connected");
}

} // namespace

TEST(NetworkingTests, ProtocolPumpsAndServerNetworkHost)
{
    TestProtocolPumpsAndServerNetworkHost();
}

TEST(NetworkingTests, TransportLifecycleAndStats)
{
    TestTransportLifecycleAndStats();
}