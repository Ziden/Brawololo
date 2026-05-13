#include "Client/ClientProtocolPump.hpp"

#include "Client/ClientRuntime.hpp"
#include "GameLogic/Serialization.hpp"

#include <cstddef>
#include <span>
#include <variant>

namespace game::client {
namespace {

std::span<const std::byte> PayloadSpan(const game::NetworkEnvelope& envelope)
{
    return {envelope.payload.data(), envelope.payload.size()};
}

} // namespace

ClientProtocolPump::ClientProtocolPump(game::net::ITransport& transport)
    : transport_(transport)
{
}

bool ClientProtocolPump::SendLogin(const game::LoginCommand& command)
{
    game::NetworkEnvelope envelope{};
    envelope.channel = game::NetworkChannel::LoginSpawn;
    envelope.messageClass = game::MessageClass::LoginRequest;
    envelope.sequence = command.header.sequence;
    envelope.payload = game::SerializeLoginCommand(command);
    return transport_.Send(envelope);
}

bool ClientProtocolPump::SendInput(const game::ClientInputPacket& packet)
{
    game::NetworkEnvelope envelope{};
    envelope.channel = game::NetworkChannel::MovementInput;
    envelope.messageClass = game::MessageClass::ClientInput;
    envelope.sequence = packet.header.sequence;
    envelope.payload = game::SerializeClientInputPacket(packet);
    return transport_.Send(envelope);
}

bool ClientProtocolPump::SendSnapshotAck(const game::SnapshotAckDTO& ack)
{
    game::NetworkEnvelope envelope{};
    envelope.channel = game::NetworkChannel::Snapshots;
    envelope.messageClass = game::MessageClass::SnapshotAck;
    envelope.sequence = ack.snapshotId;
    envelope.payload = game::SerializeSnapshotAck(ack);
    return transport_.Send(envelope);
}

bool ClientProtocolPump::SendNetworkEventAck(const game::NetworkEventAckDTO& ack)
{
    game::NetworkEnvelope envelope{};
    envelope.channel = game::NetworkChannel::LoginSpawn;
    envelope.messageClass = game::MessageClass::NetworkEventAck;
    envelope.sequence = ack.eventId;
    envelope.payload = game::SerializeNetworkEventAck(ack);
    return transport_.Send(envelope);
}

bool ClientProtocolPump::SendTimeSyncRequest(const game::TimeSyncRequest& request)
{
    game::NetworkEnvelope envelope{};
    envelope.channel = game::NetworkChannel::TimeSync;
    envelope.messageClass = game::MessageClass::TimeSyncRequest;
    envelope.sequence = request.sequence;
    envelope.payload = game::SerializeTimeSyncRequest(request);
    return transport_.Send(envelope);
}

int ClientProtocolPump::PumpSnapshots(
    ClientRuntime& runtime,
    ClientSessionStats& stats,
    game::TimestampMs localReceiveTimeMs)
{
    game::EventList ignoredEvents{};
    return PumpIncoming(runtime, stats, ignoredEvents, localReceiveTimeMs);
}

int ClientProtocolPump::PumpIncoming(
    ClientRuntime& runtime,
    ClientSessionStats& stats,
    game::EventList& reliableEvents,
    game::TimestampMs localReceiveTimeMs)
{
    int applied = 0;
    while (auto envelope = transport_.Poll()) {
        if (!game::ValidateEnvelope(*envelope).Ok()) {
            continue;
        }

        if (envelope->messageClass == game::MessageClass::TimeSyncResponse) {
            const auto response = game::DeserializeTimeSyncResponse(PayloadSpan(*envelope));
            if (!response.has_value()) {
                continue;
            }

            runtime.RecordTimeSyncSample(game::TimeSyncSample{
                response->clientSentAtMs,
                response->serverReceivedAtMs,
                localReceiveTimeMs != 0 ? localReceiveTimeMs : response->clientSentAtMs});
            stats.lastTimeSyncSequence = response->sequence;
            ++stats.timeSyncResponsesApplied;
            continue;
        }

        if (envelope->messageClass == game::MessageClass::CombatEvent ||
            envelope->messageClass == game::MessageClass::SpawnAccepted ||
            envelope->messageClass == game::MessageClass::InterestEvent) {
            const auto networkEvent = game::DeserializeNetworkEvent(PayloadSpan(*envelope));
            if (!networkEvent.has_value()) {
                continue;
            }

            const auto domainEvent = game::ToDomainEvent(*networkEvent);
            if (!domainEvent.has_value()) {
                continue;
            }

            if (SendNetworkEventAck(game::NetworkEventAckDTO{
                    stats.localClientId,
                    networkEvent->eventId,
                    localReceiveTimeMs})) {
                ++stats.reliableEventAcksSent;
            }

            if (const auto* spawned = std::get_if<game::PlayerSpawnedEventDTO>(&networkEvent->payload);
                spawned != nullptr && spawned->clientId == stats.localClientId) {
                stats.connectionState = ClientConnectionState::Connected;
                ++stats.spawnAcceptedEventsReceived;
            }

            reliableEvents.push_back(*domainEvent);
            ++stats.reliableEventsReceived;
            continue;
        }

        if (envelope->messageClass != game::MessageClass::Snapshot) {
            continue;
        }

        const auto snapshot = game::DeserializeSnapshot(PayloadSpan(*envelope));
        if (!snapshot.has_value()) {
            continue;
        }

        runtime.ApplyServerSnapshot(*snapshot);
        if (SendSnapshotAck(game::SnapshotAckDTO{
                stats.localClientId,
                snapshot->snapshotId,
                snapshot->baselineId,
                snapshot->ackedInputSequence,
                localReceiveTimeMs})) {
            ++stats.snapshotAcksSent;
        }
        stats.lastSnapshotId = snapshot->snapshotId;
        stats.lastBaselineId = snapshot->baselineId;
        stats.lastAckedInputSequence = snapshot->ackedInputSequence;
        ++stats.snapshotsApplied;
        ++applied;
    }

    return applied;
}

} // namespace game::client
