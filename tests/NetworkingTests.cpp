#include "Client/ClientProtocolPump.hpp"
#include "Client/ClientRuntime.hpp"
#include "Client/RemoteClientSession.hpp"
#include "GameLogic/Commands.hpp"
#include "GameLogic/NetworkProtocol.hpp"
#include "GameLogic/Serialization.hpp"
#include "Networking/KcpRtcTransport.hpp"
#include "Networking/KcpRtcDataChannelPump.hpp"
#include "Networking/KcpRtcPumpedTransport.hpp"
#include "Networking/KcpRtcSignalingPump.hpp"
#include "Networking/LoopbackTransport.hpp"
#include "Networking/MultiClientLoopbackTransport.hpp"
#include "Networking/RtcDataChannel.hpp"
#include "Networking/RtcSignaling.hpp"
#include "Networking/RtcSignalingClient.hpp"
#include "Networking/TransportPacketCodec.hpp"
#include "Networking/TransportFactory.hpp"
#include "Server/ServerNetworkHost.hpp"
#include "TestSupport.hpp"

#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <string>
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
    Expect(transport.Connect(), "kcp rtc transport starts async signaling when config is valid");
    Expect(transport.State() == game::net::TransportState::Connecting,
           "kcp rtc transport enters connecting state");
    Expect(transport.Phase() == game::net::KcpRtcConnectionPhase::Signaling,
           "kcp rtc transport reports signaling phase");
    Expect(transport.Diagnostics().outgoingSignalsQueued >= 2,
           "client transport queues join and offer signals");

    game::NetworkEnvelope invalidEnvelope{};
    invalidEnvelope.channel = game::NetworkChannel::Snapshots;
    invalidEnvelope.messageClass = game::MessageClass::ClientInput;
    Expect(!transport.Send(invalidEnvelope), "kcp rtc transport validates protocol envelopes");
    Expect(transport.LastError() == game::net::TransportError::ProtocolRejected,
           "kcp rtc transport reports protocol rejection before state rejection");

    game::NetworkEnvelope validEnvelope{};
    validEnvelope.channel = game::NetworkChannel::MovementInput;
    validEnvelope.messageClass = game::MessageClass::ClientInput;
    Expect(!transport.Send(validEnvelope),
           "kcp rtc transport rejects valid send before data channel connection");
    Expect(transport.LastError() == game::net::TransportError::NotConnected,
           "kcp rtc transport reports not connected for valid send before data channel opens");
}

void TestTransportPacketCodecRoundTripsEnvelope() {
    game::NetworkEnvelope envelope{};
    envelope.peerId = 42;
    envelope.channel = game::NetworkChannel::MovementInput;
    envelope.messageClass = game::MessageClass::ClientInput;
    envelope.sequence = 7;
    envelope.payload = {std::byte{0x01}, std::byte{0x02}, std::byte{0x03}};

    const auto bytes = game::net::EncodeTransportPacket(envelope);
    const auto decoded = game::net::DecodeTransportPacket(bytes);
    Expect(decoded.Ok(), "transport packet codec decodes valid frame");
    Expect(decoded.envelope->peerId == envelope.peerId, "codec preserves peer id");
    Expect(decoded.envelope->messageClass == envelope.messageClass,
           "codec preserves message class");
    Expect(decoded.envelope->payload == envelope.payload, "codec preserves payload bytes");

    auto mismatched = envelope;
    mismatched.channel = game::NetworkChannel::Snapshots;
    const auto rejected = game::net::DecodeTransportPacket(
        game::net::EncodeTransportPacket(mismatched));
    Expect(!rejected.Ok(), "transport packet codec rejects invalid channel/message pairs");
    Expect(rejected.error == game::net::TransportPacketDecodeError::ChannelMismatch,
           "codec reports channel mismatch");
}

void TestRtcSignalingSerializesAndValidatesMessages() {
    const game::net::RtcSignalingMessage offer{
        game::net::RtcSignalingMessageKind::Offer,
        1,
        2,
        10,
        "match-one",
        "sdp-offer",
    };

    Expect(game::net::ValidateRtcSignalingMessage(offer).Ok(),
           "rtc signaling validates a complete message");
    const auto decoded = game::net::DeserializeRtcSignalingMessage(
        game::net::SerializeRtcSignalingMessage(offer));
    Expect(decoded.Ok(), "rtc signaling round-trips through byte payload");
    Expect(decoded.message->kind == game::net::RtcSignalingMessageKind::Offer,
           "rtc signaling preserves kind");
    Expect(decoded.message->payload == "sdp-offer", "rtc signaling preserves payload");

    auto invalid = offer;
    invalid.senderPeerId = 0;
    Expect(!game::net::ValidateRtcSignalingMessage(invalid).Ok(),
           "rtc signaling rejects missing sender");
}

void TestInMemoryRtcSignalingClientRoutesBySessionAndTarget() {
    const auto hub = game::net::InMemoryRtcSignalingClient::CreateHub();
    game::net::InMemoryRtcSignalingClient clientOne{
        hub, game::net::RtcSignalingClientConfig{1, "match-one"}};
    game::net::InMemoryRtcSignalingClient clientTwo{
        hub, game::net::RtcSignalingClientConfig{2, "match-one"}};
    game::net::InMemoryRtcSignalingClient clientOtherSession{
        hub, game::net::RtcSignalingClientConfig{3, "match-two"}};

    Expect(clientOne.Connect(), "first signaling client connects to in-memory hub");
    Expect(clientTwo.Connect(), "second signaling client connects to in-memory hub");
    Expect(clientOtherSession.Connect(), "other-session signaling client connects");
    Expect(hub->PeerCount() == 3, "signaling hub tracks connected peers");

    Expect(clientOne.Send(game::net::RtcSignalingMessage{
               game::net::RtcSignalingMessageKind::Join,
               1,
               0,
               1,
               "match-one",
               "client-one",
           }),
           "broadcast signaling message routes to same-session peers");
    const auto broadcast = clientTwo.Poll();
    Expect(broadcast.has_value(), "same-session peer receives broadcast signal");
    Expect(broadcast->kind == game::net::RtcSignalingMessageKind::Join,
           "broadcast signal preserves message kind");
    Expect(!clientOtherSession.Poll().has_value(),
           "different session does not receive broadcast signal");

    Expect(clientTwo.Send(game::net::RtcSignalingMessage{
               game::net::RtcSignalingMessageKind::Answer,
               2,
               1,
               2,
               "match-one",
               "answer",
           }),
           "targeted signaling message routes to explicit peer");
    const auto targeted = clientOne.Poll();
    Expect(targeted.has_value(), "targeted peer receives signal");
    Expect(targeted->targetPeerId == 1, "targeted signal preserves target peer");

    Expect(!clientOne.Send(game::net::RtcSignalingMessage{
               game::net::RtcSignalingMessageKind::Offer,
               1,
               3,
               3,
               "match-one",
               "wrong-session-target",
           }),
           "signaling hub rejects targeted delivery across sessions");
    Expect(clientOne.LastError() == game::net::RtcSignalingClientError::RouteUnavailable,
           "signaling client reports missing route");
}

void TestKcpRtcTransportSignalingAndFrameBoundary() {
    game::net::KcpRtcTransport client{game::net::KcpRtcTransportConfig{
        game::net::KcpRtcRole::Client,
        1,
        "client-one",
        "memory://signaling",
        "game",
        1024U * 1024U,
        true,
        "match-one",
    }};
    game::net::KcpRtcTransport server{game::net::KcpRtcTransportConfig{
        game::net::KcpRtcRole::Server,
        2,
        "server",
        "memory://signaling",
        "game",
        1024U * 1024U,
        true,
        "match-one",
    }};

    Expect(client.Connect(), "client starts rtc signaling");
    Expect(server.Connect(), "server starts rtc signaling");

    while (auto signal = client.PollOutgoingSignal()) {
        Expect(server.ReceiveSignalingMessage(*signal), "server accepts client signal");
    }
    while (auto signal = server.PollOutgoingSignal()) {
        Expect(client.ReceiveSignalingMessage(*signal), "client accepts server signal");
    }

    Expect(client.Phase() == game::net::KcpRtcConnectionPhase::DataChannelConnecting,
           "client reaches data-channel connecting after answer");
    Expect(server.Phase() == game::net::KcpRtcConnectionPhase::DataChannelConnecting,
           "server reaches data-channel connecting after offer");

    Expect(client.ReceiveSignalingMessage(game::net::RtcSignalingMessage{
               game::net::RtcSignalingMessageKind::DataChannelReady,
               2,
               1,
               99,
               "match-one",
               "",
           }),
           "client accepts data-channel ready signal");
    Expect(server.ReceiveSignalingMessage(game::net::RtcSignalingMessage{
               game::net::RtcSignalingMessageKind::DataChannelReady,
               1,
               2,
               100,
               "match-one",
               "",
           }),
           "server accepts data-channel ready signal");
    Expect(client.State() == game::net::TransportState::Connected,
           "client transport becomes connected after data channel ready");
    Expect(server.State() == game::net::TransportState::Connected,
           "server transport becomes connected after data channel ready");

    game::NetworkEnvelope envelope{};
    envelope.peerId = 1;
    envelope.channel = game::NetworkChannel::MovementInput;
    envelope.messageClass = game::MessageClass::ClientInput;
    envelope.sequence = 4;
    envelope.payload = {std::byte{0x0A}};

    Expect(client.Send(envelope), "connected rtc transport encodes outgoing frame");
    const auto frame = client.PollOutgoingFrame();
    Expect(frame.has_value(), "rtc transport exposes encoded data-channel frame");
    Expect(server.ReceiveFrame(*frame), "server receives encoded data-channel frame");
    const auto received = server.Poll();
    Expect(received.has_value(), "server polls decoded envelope from received frame");
    Expect(received->sequence == envelope.sequence, "decoded frame preserves sequence");
    Expect(received->payload == envelope.payload, "decoded frame preserves payload");

    Expect(!server.ReceiveSignalingMessage(game::net::RtcSignalingMessage{
               game::net::RtcSignalingMessageKind::Offer,
               1,
               2,
               101,
               "wrong-match",
               "bad",
           }),
           "rtc transport rejects signaling for a different session");
}

void TestKcpRtcSignalingPumpExchangesTransportSignals() {
    const auto hub = game::net::InMemoryRtcSignalingClient::CreateHub();
    game::net::InMemoryRtcSignalingClient clientSignals{
        hub, game::net::RtcSignalingClientConfig{1, "match-one"}};
    game::net::InMemoryRtcSignalingClient serverSignals{
        hub, game::net::RtcSignalingClientConfig{2, "match-one"}};
    Expect(clientSignals.Connect(), "client signaling path connects");
    Expect(serverSignals.Connect(), "server signaling path connects");

    game::net::KcpRtcTransport client{game::net::KcpRtcTransportConfig{
        game::net::KcpRtcRole::Client,
        1,
        "client-one",
        "memory://signaling",
        "game",
        1024U * 1024U,
        true,
        "match-one",
    }};
    game::net::KcpRtcTransport server{game::net::KcpRtcTransportConfig{
        game::net::KcpRtcRole::Server,
        2,
        "server",
        "memory://signaling",
        "game",
        1024U * 1024U,
        true,
        "match-one",
    }};

    Expect(client.Connect(), "client rtc transport starts signaling");
    Expect(server.Connect(), "server rtc transport starts signaling");

    const auto clientToHub = game::net::PumpKcpRtcSignaling(client, clientSignals);
    Expect(clientToHub.outgoingSignalsSent >= 2,
           "pump sends client join/offer to signaling service");
    const auto hubToServer = game::net::PumpKcpRtcSignaling(server, serverSignals);
    Expect(hubToServer.incomingSignalsApplied >= 1,
           "pump applies client offer to server transport");
    const auto serverToHub = game::net::PumpKcpRtcSignaling(server, serverSignals);
    Expect(serverToHub.outgoingSignalsSent >= 1,
           "pump sends server answer through signaling service");
    const auto hubToClient = game::net::PumpKcpRtcSignaling(client, clientSignals);
    Expect(hubToClient.incomingSignalsApplied >= 1,
           "pump applies server answer to client transport");

    Expect(client.Phase() == game::net::KcpRtcConnectionPhase::DataChannelConnecting,
           "client reaches data-channel connecting through signaling pump");
    Expect(server.Phase() == game::net::KcpRtcConnectionPhase::DataChannelConnecting,
           "server reaches data-channel connecting through signaling pump");

    Expect(serverSignals.Send(game::net::RtcSignalingMessage{
               game::net::RtcSignalingMessageKind::DataChannelReady,
               2,
               1,
               10,
               "match-one",
               "",
           }),
           "server signaling path can carry data-channel-ready notification");
    Expect(clientSignals.Send(game::net::RtcSignalingMessage{
               game::net::RtcSignalingMessageKind::DataChannelReady,
               1,
               2,
               11,
               "match-one",
               "",
           }),
           "client signaling path can carry data-channel-ready notification");
    (void)game::net::PumpKcpRtcSignaling(client, clientSignals);
    (void)game::net::PumpKcpRtcSignaling(server, serverSignals);
    Expect(client.State() == game::net::TransportState::Connected,
           "client transport consumes ready signal through signaling pump");
    Expect(server.State() == game::net::TransportState::Connected,
           "server transport consumes ready signal through signaling pump");
}

void TestInMemoryRtcDataChannelRoutesFrames() {
    auto channels = game::net::InMemoryRtcDataChannel::CreatePair();
    auto& channelA = channels.first;
    auto& channelB = channels.second;

    const std::vector<std::byte> frame{std::byte{0x01}, std::byte{0x02}};
    Expect(channelA.SendFrame(frame), "in-memory data channel sends frame");
    const auto received = channelB.PollFrame();
    Expect(received.has_value(), "paired data channel receives frame");
    Expect(*received == frame, "paired data channel preserves frame bytes");
    Expect(channelA.Stats().framesSent == 1, "data channel records sent frames");
    Expect(channelB.Stats().framesReceived == 1, "data channel records received frames");

    Expect(!channelA.SendFrame(std::span<const std::byte>{}),
           "data channel rejects empty frames");
    Expect(channelA.LastError() == game::net::RtcDataChannelError::InvalidFrame,
           "data channel reports invalid frame");

    channelB.Close();
    Expect(!channelB.SendFrame(frame), "closed data channel rejects send");
    Expect(channelB.LastError() == game::net::RtcDataChannelError::NotOpen,
           "closed data channel reports not open");
}

void TestKcpRtcDataChannelPumpMovesFramesBetweenTransports() {
    game::net::KcpRtcTransport client{game::net::KcpRtcTransportConfig{
        game::net::KcpRtcRole::Client,
        1,
        "client-one",
        "memory://signaling",
        "game",
        1024U * 1024U,
        true,
        "match-one",
    }};
    game::net::KcpRtcTransport server{game::net::KcpRtcTransportConfig{
        game::net::KcpRtcRole::Server,
        2,
        "server",
        "memory://signaling",
        "game",
        1024U * 1024U,
        true,
        "match-one",
    }};
    Expect(client.Connect(), "client rtc transport starts before data-channel pump");
    Expect(server.Connect(), "server rtc transport starts before data-channel pump");

    Expect(client.ReceiveSignalingMessage(game::net::RtcSignalingMessage{
               game::net::RtcSignalingMessageKind::DataChannelReady,
               2,
               1,
               1,
               "match-one",
               "",
           }),
           "client accepts ready signal for data-channel pump test");
    Expect(server.ReceiveSignalingMessage(game::net::RtcSignalingMessage{
               game::net::RtcSignalingMessageKind::DataChannelReady,
               1,
               2,
               2,
               "match-one",
               "",
           }),
           "server accepts ready signal for data-channel pump test");

    auto channels = game::net::InMemoryRtcDataChannel::CreatePair();
    auto& clientChannel = channels.first;
    auto& serverChannel = channels.second;

    game::NetworkEnvelope input{};
    input.peerId = 1;
    input.channel = game::NetworkChannel::MovementInput;
    input.messageClass = game::MessageClass::ClientInput;
    input.sequence = 77;
    input.payload = {std::byte{0x0A}, std::byte{0x0B}};
    Expect(client.Send(input), "connected client transport queues encoded input frame");

    const auto clientPump = game::net::PumpKcpRtcDataChannel(client, clientChannel);
    Expect(clientPump.outgoingFramesSent == 1,
           "data-channel pump sends encoded client frame");
    const auto serverPump = game::net::PumpKcpRtcDataChannel(server, serverChannel);
    Expect(serverPump.incomingFramesApplied == 1,
           "data-channel pump applies frame to server transport");
    const auto receivedInput = server.Poll();
    Expect(receivedInput.has_value(), "server polls decoded envelope from data-channel pump");
    Expect(receivedInput->messageClass == game::MessageClass::ClientInput,
           "server receives client input message class");
    Expect(receivedInput->payload == input.payload, "server receives client input payload");

    game::NetworkEnvelope snapshot{};
    snapshot.peerId = 1;
    snapshot.channel = game::NetworkChannel::Snapshots;
    snapshot.messageClass = game::MessageClass::Snapshot;
    snapshot.sequence = 88;
    snapshot.payload = {std::byte{0xCC}};
    Expect(server.Send(snapshot), "connected server transport queues encoded snapshot frame");

    const auto serverOutbound = game::net::PumpKcpRtcDataChannel(server, serverChannel);
    Expect(serverOutbound.outgoingFramesSent == 1,
           "data-channel pump sends encoded server frame");
    const auto clientInbound = game::net::PumpKcpRtcDataChannel(client, clientChannel);
    Expect(clientInbound.incomingFramesApplied == 1,
           "data-channel pump applies frame to client transport");
    const auto receivedSnapshot = client.Poll();
    Expect(receivedSnapshot.has_value(), "client polls decoded snapshot from data-channel pump");
    Expect(receivedSnapshot->messageClass == game::MessageClass::Snapshot,
           "client receives snapshot message class");
    Expect(receivedSnapshot->sequence == snapshot.sequence, "client receives snapshot sequence");
}

void TestUnsupportedRtcBackendsReportNotImplemented() {
    game::net::UnsupportedRtcSignalingClient signaling{};
    Expect(!signaling.Connect(), "unsupported signaling client refuses to connect");
    Expect(signaling.LastError() == game::net::RtcSignalingClientError::NotImplemented,
           "unsupported signaling reports not implemented");

    game::net::UnsupportedRtcDataChannel dataChannel{};
    const std::vector<std::byte> frame{std::byte{0x01}};
    Expect(!dataChannel.SendFrame(frame), "unsupported data channel refuses to send");
    Expect(dataChannel.LastError() == game::net::RtcDataChannelError::NotImplemented,
           "unsupported data channel reports not implemented");
}

void TestPumpedKcpRtcTransportConnectsAndMovesEnvelopes() {
    auto transports = game::net::CreateInMemoryKcpRtcTransportPair(
        game::net::InMemoryKcpRtcTransportPairConfig{1, 2, "match-one"});
    auto& clientTransport = transports.first;
    auto& serverTransport = transports.second;

    Expect(serverTransport->Connect(), "pumped rtc server transport starts async connection");
    Expect(clientTransport->Connect(), "pumped rtc client transport starts async connection");

    for (int step = 0; step < 8 &&
                       (clientTransport->State() != game::net::TransportState::Connected ||
                        serverTransport->State() != game::net::TransportState::Connected);
         ++step) {
        clientTransport->Update();
        serverTransport->Update();
    }

    Expect(clientTransport->State() == game::net::TransportState::Connected,
           "pumped rtc client reaches connected state");
    Expect(serverTransport->State() == game::net::TransportState::Connected,
           "pumped rtc server reaches connected state");

    game::NetworkEnvelope input{};
    input.peerId = 1;
    input.channel = game::NetworkChannel::MovementInput;
    input.messageClass = game::MessageClass::ClientInput;
    input.sequence = 12;
    input.payload = {std::byte{0xA0}};
    Expect(clientTransport->Send(input), "pumped rtc client sends envelope");

    serverTransport->Update();
    const auto receivedInput = serverTransport->Poll();
    Expect(receivedInput.has_value(), "pumped rtc server polls envelope");
    Expect(receivedInput->sequence == input.sequence, "pumped rtc preserves input sequence");

    game::NetworkEnvelope snapshot{};
    snapshot.peerId = 1;
    snapshot.channel = game::NetworkChannel::Snapshots;
    snapshot.messageClass = game::MessageClass::Snapshot;
    snapshot.sequence = 13;
    snapshot.payload = {std::byte{0xB0}, std::byte{0xB1}};
    Expect(serverTransport->Send(snapshot), "pumped rtc server sends envelope");

    clientTransport->Update();
    const auto receivedSnapshot = clientTransport->Poll();
    Expect(receivedSnapshot.has_value(), "pumped rtc client polls envelope");
    Expect(receivedSnapshot->payload == snapshot.payload, "pumped rtc preserves snapshot payload");
}

void TestRemoteTransportFactoryCreatesUnsupportedPumpedRtcTransport() {
    const auto transport = game::net::CreateRemoteTransport(game::net::RemoteTransportConfig{
        game::net::RemoteTransportKind::KcpRtcPumped,
        game::net::KcpRtcTransportConfig{
            game::net::KcpRtcRole::Client,
            5,
            "client-five",
            "wss://signaling.example.invalid",
            "game",
            1024U * 1024U,
            true,
            "match-one",
        },
        game::net::RtcBackendKind::UnsupportedNative,
    });

    Expect(transport != nullptr, "remote factory creates pumped rtc transport");
    Expect(!transport->Connect(),
           "pumped rtc transport with unsupported native backend refuses connection");
    Expect(transport->LastError() == game::net::TransportError::NotImplemented,
           "pumped rtc transport maps unsupported backend to transport not implemented");
}

void TestRemoteTransportFactoryCreatesKcpRtcTransport() {
    const auto transport = game::net::CreateRemoteTransport(game::net::RemoteTransportConfig{
        game::net::RemoteTransportKind::KcpRtc,
        game::net::KcpRtcTransportConfig{
            game::net::KcpRtcRole::Client,
            7,
            "client-seven",
            "wss://signaling.invalid",
            "game",
            1024U * 1024U,
            true,
        },
    });

    Expect(transport != nullptr, "remote transport factory creates transport instance");
    Expect(transport->State() == game::net::TransportState::Disconnected,
           "factory-created remote transport starts disconnected");
    Expect(transport->Connect(), "factory-created kcp rtc transport starts async connection");
    Expect(transport->State() == game::net::TransportState::Connecting,
           "factory-created kcp rtc transport exposes async connecting state");
    const auto* kcpRtc = dynamic_cast<const game::net::KcpRtcTransport*>(transport.get());
    Expect(kcpRtc != nullptr, "remote transport factory preserves concrete diagnostics access");
    Expect(kcpRtc->Phase() == game::net::KcpRtcConnectionPhase::Signaling,
           "factory-created transport reaches signaling seam");
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

TEST(NetworkingTests, TransportPacketCodecRoundTripsEnvelope) {
    TestTransportPacketCodecRoundTripsEnvelope();
}

TEST(NetworkingTests, RtcSignalingSerializesAndValidatesMessages) {
    TestRtcSignalingSerializesAndValidatesMessages();
}

TEST(NetworkingTests, InMemoryRtcSignalingClientRoutesBySessionAndTarget) {
    TestInMemoryRtcSignalingClientRoutesBySessionAndTarget();
}

TEST(NetworkingTests, KcpRtcTransportSignalingAndFrameBoundary) {
    TestKcpRtcTransportSignalingAndFrameBoundary();
}

TEST(NetworkingTests, KcpRtcSignalingPumpExchangesTransportSignals) {
    TestKcpRtcSignalingPumpExchangesTransportSignals();
}

TEST(NetworkingTests, InMemoryRtcDataChannelRoutesFrames) {
    TestInMemoryRtcDataChannelRoutesFrames();
}

TEST(NetworkingTests, KcpRtcDataChannelPumpMovesFramesBetweenTransports) {
    TestKcpRtcDataChannelPumpMovesFramesBetweenTransports();
}

TEST(NetworkingTests, UnsupportedRtcBackendsReportNotImplemented) {
    TestUnsupportedRtcBackendsReportNotImplemented();
}

TEST(NetworkingTests, PumpedKcpRtcTransportConnectsAndMovesEnvelopes) {
    TestPumpedKcpRtcTransportConnectsAndMovesEnvelopes();
}

TEST(NetworkingTests, RemoteTransportFactoryCreatesUnsupportedPumpedRtcTransport) {
    TestRemoteTransportFactoryCreatesUnsupportedPumpedRtcTransport();
}

TEST(NetworkingTests, RemoteTransportFactoryCreatesKcpRtcTransport) {
    TestRemoteTransportFactoryCreatesKcpRtcTransport();
}
