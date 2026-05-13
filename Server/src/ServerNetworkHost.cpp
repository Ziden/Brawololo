#include "Server/ServerNetworkHost.hpp"

#include "GameLogic/Serialization.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <span>
#include <utility>
#include <variant>

namespace game::server {
namespace {

std::span<const std::byte> PayloadSpan(const game::NetworkEnvelope& envelope) {
    return {envelope.payload.data(), envelope.payload.size()};
}

} // namespace

ServerNetworkHost::ServerNetworkHost(game::net::ITransport& transport,
                                     ServerNetworkHostConfig config)
    : transport_(transport), config_(config), baselineCache_(config_.baselines) {}

bool ServerNetworkHost::ConnectSimulationOnlyClient(game::ClientId clientId,
                                                    game::TimestampMs nowMs) {
    return runtime_.ConnectClient(clientId, nowMs);
}

void ServerNetworkHost::PumpClientMessages() {
    while (auto envelope = transport_.Poll()) {
        if (!game::ValidateEnvelope(*envelope).Ok()) {
            continue;
        }

        switch (envelope->messageClass) {
            case game::MessageClass::LoginRequest:
                HandleLoginEnvelope(*envelope);
                break;
            case game::MessageClass::ClientInput:
                HandleInputEnvelope(*envelope);
                break;
            case game::MessageClass::SnapshotAck:
                HandleSnapshotAckEnvelope(*envelope);
                break;
            case game::MessageClass::NetworkEventAck:
                HandleNetworkEventAckEnvelope(*envelope);
                break;
            case game::MessageClass::TimeSyncRequest:
                HandleTimeSyncEnvelope(*envelope);
                break;
            default:
                break;
        }
    }
}

void ServerNetworkHost::TickAndSendSnapshots(game::TimestampMs nowMs) {
    runtime_.Tick();
    auto tickEvents = runtime_.DrainEvents();

    for (const auto clientId : clients_) {
        if (!ShouldSendSnapshot(clientId, nowMs)) {
            ++stats_.snapshotsSkippedBySchedule;
            continue;
        }

        if (SendSnapshot(clientId, nowMs)) {
            MarkSnapshotSent(clientId, nowMs);
        }
    }

    BroadcastReliableEvents(tickEvents, nowMs);
    ResendPendingReliableEvents(nowMs);
    drainedEvents_.insert(drainedEvents_.end(), tickEvents.begin(), tickEvents.end());
}

game::EventList ServerNetworkHost::DrainEvents() {
    game::EventList drained{};
    drained.swap(drainedEvents_);
    return drained;
}

ServerNetworkStats ServerNetworkHost::Stats() const noexcept {
    return stats_;
}

const ServerRuntime& ServerNetworkHost::Runtime() const noexcept {
    return runtime_;
}

ServerRuntime& ServerNetworkHost::Runtime() noexcept {
    return runtime_;
}

void ServerNetworkHost::HandleLoginEnvelope(const game::NetworkEnvelope& envelope) {
    const auto login = game::DeserializeLoginCommand(PayloadSpan(envelope));
    if (!login.has_value()) {
        return;
    }

    ++stats_.loginRequests;
    if (!runtime_.ConnectClient(login->header.clientId, login->header.clientTimestampMs)) {
        return;
    }

    if (std::find(clients_.begin(), clients_.end(), login->header.clientId) == clients_.end()) {
        clients_.push_back(login->header.clientId);
        clientStateById_.emplace(login->header.clientId,
                                 ServerClientReplicationState{login->header.clientId});
        stats_.connectedClients = clients_.size();
    }
}

void ServerNetworkHost::HandleInputEnvelope(const game::NetworkEnvelope& envelope) {
    const auto packet = game::DeserializeClientInputPacket(PayloadSpan(envelope));
    if (!packet.has_value()) {
        return;
    }

    if (runtime_.SubmitInput(*packet)) {
        ++stats_.inputsReceived;
    }
}

void ServerNetworkHost::HandleSnapshotAckEnvelope(const game::NetworkEnvelope& envelope) {
    const auto ack = game::DeserializeSnapshotAck(PayloadSpan(envelope));
    if (!ack.has_value()) {
        return;
    }

    const auto state = clientStateById_.find(ack->clientId);
    if (state == clientStateById_.end()) {
        ++stats_.staleSnapshotAcksRejected;
        return;
    }

    if (!baselineCache_.Find(ack->clientId, ack->snapshotId).has_value() ||
        !state->second.AcknowledgeSnapshot(ack->snapshotId)) {
        ++stats_.staleSnapshotAcksRejected;
        return;
    }

    ++stats_.snapshotAcksReceived;
}

void ServerNetworkHost::HandleNetworkEventAckEnvelope(const game::NetworkEnvelope& envelope) {
    const auto ack = game::DeserializeNetworkEventAck(PayloadSpan(envelope));
    if (!ack.has_value()) {
        return;
    }

    const auto state = clientStateById_.find(ack->clientId);
    if (state == clientStateById_.end() || !state->second.AcknowledgeReliableEvent(ack->eventId)) {
        ++stats_.staleReliableEventAcksRejected;
        return;
    }

    ++stats_.reliableEventAcksReceived;
}

void ServerNetworkHost::HandleTimeSyncEnvelope(const game::NetworkEnvelope& envelope) {
    const auto request = game::DeserializeTimeSyncRequest(PayloadSpan(envelope));
    if (!request.has_value()) {
        return;
    }

    ++stats_.timeSyncRequestsReceived;

    const auto serverNowMs = runtime_.Simulation().ServerTimeMs();
    game::TimeSyncResponse response{};
    response.clientId = request->clientId;
    response.sequence = request->sequence;
    response.clientSentAtMs = request->clientSentAtMs;
    response.serverReceivedAtMs = serverNowMs;
    response.serverSentAtMs = serverNowMs;

    game::NetworkEnvelope responseEnvelope{};
    responseEnvelope.peerId = request->clientId;
    responseEnvelope.channel = game::NetworkChannel::TimeSync;
    responseEnvelope.messageClass = game::MessageClass::TimeSyncResponse;
    responseEnvelope.sequence = response.sequence;
    responseEnvelope.payload = game::SerializeTimeSyncResponse(response);
    if (transport_.Send(responseEnvelope)) {
        ++stats_.timeSyncResponsesSent;
    }
}

bool ServerNetworkHost::ShouldSendSnapshot(game::ClientId clientId, game::TimestampMs nowMs) const {
    const auto state = clientStateById_.find(clientId);
    return state == clientStateById_.end() || state->second.ShouldSendSnapshot(nowMs);
}

void ServerNetworkHost::MarkSnapshotSent(game::ClientId clientId, game::TimestampMs nowMs) {
    auto state = clientStateById_.find(clientId);
    if (state != clientStateById_.end()) {
        state->second.MarkSnapshotSent(nowMs, config_.snapshotSendIntervalMs);
    }
}

void ServerNetworkHost::BroadcastReliableEvents(const game::EventList& events, game::TimestampMs nowMs) {
    for (const auto& event : events) {
        const auto networkEvent = game::ToNetworkEventDTO(nextNetworkEventId_++,
                                                          runtime_.Simulation().CurrentTick(),
                                                          runtime_.Simulation().ServerTimeMs(),
                                                          event);
        if (!networkEvent.has_value()) {
            continue;
        }

        std::optional<game::ClientId> observerOnly{};
        if (const auto* entered =
                std::get_if<game::EntityEnteredInterestEventDTO>(&networkEvent->payload);
            entered != nullptr) {
            observerOnly = entered->observerClientId;
        } else if (const auto* left =
                       std::get_if<game::EntityLeftInterestEventDTO>(&networkEvent->payload);
                   left != nullptr) {
            observerOnly = left->observerClientId;
        }

        for (const auto clientId : clients_) {
            if (observerOnly.has_value() && *observerOnly != clientId) {
                continue;
            }
            SendReliableEvent(clientId, *networkEvent, nowMs);
        }
    }
}

void ServerNetworkHost::ResendPendingReliableEvents(game::TimestampMs nowMs) {
    for (auto& [clientId, state] : clientStateById_) {
        const auto dueEvents = state.ReliableEventsDueForResend(
            nowMs,
            config_.reliableEventResendBaseIntervalMs,
            config_.reliableEventMaxSendCount);
        for (const auto& event : dueEvents) {
            SendReliableEvent(clientId, event, nowMs, true);
        }
    }
}

void ServerNetworkHost::SendInterestEvents(
    game::ClientId clientId,
    const game::InterestFrame& interestFrame,
    game::TimestampMs nowMs) {
    for (const auto entityId : interestFrame.entered) {
        const auto networkEvent = game::ToNetworkEventDTO(
            nextNetworkEventId_++,
            runtime_.Simulation().CurrentTick(),
            runtime_.Simulation().ServerTimeMs(),
            game::EntityEnteredInterest{entityId, clientId});
        if (networkEvent.has_value()) {
            SendReliableEvent(clientId, *networkEvent, nowMs);
        }
    }

    for (const auto entityId : interestFrame.exited) {
        const auto networkEvent = game::ToNetworkEventDTO(
            nextNetworkEventId_++,
            runtime_.Simulation().CurrentTick(),
            runtime_.Simulation().ServerTimeMs(),
            game::EntityLeftInterest{entityId, clientId});
        if (networkEvent.has_value()) {
            SendReliableEvent(clientId, *networkEvent, nowMs);
        }
    }
}

void ServerNetworkHost::SendReliableEvent(game::ClientId clientId,
                                          const game::NetworkEventDTO& event,
                                          game::TimestampMs nowMs,
                                          bool isResend) {
    const auto kind = game::KindForNetworkEvent(event);
    game::NetworkEnvelope envelope{};
    envelope.peerId = clientId;
    envelope.sequence = event.eventId;
    envelope.payload = game::SerializeNetworkEvent(event);

    switch (kind) {
        case game::NetworkEventKind::PlayerSpawned:
            envelope.channel = game::NetworkChannel::LoginSpawn;
            envelope.messageClass = game::MessageClass::SpawnAccepted;
            break;
        case game::NetworkEventKind::EntityEnteredInterest:
        case game::NetworkEventKind::EntityLeftInterest:
            envelope.channel = game::NetworkChannel::LoginSpawn;
            envelope.messageClass = game::MessageClass::InterestEvent;
            break;
        case game::NetworkEventKind::WeaponWarmupStarted:
        case game::NetworkEventKind::BowFired:
        case game::NetworkEventKind::ProjectileSpawned:
        case game::NetworkEventKind::HitConfirmed:
        case game::NetworkEventKind::PlayerDamaged:
        case game::NetworkEventKind::PlayerDied:
        case game::NetworkEventKind::PlayerRespawned:
            envelope.channel = game::NetworkChannel::CombatEvents;
            envelope.messageClass = game::MessageClass::CombatEvent;
            break;
    }

    if (transport_.Send(envelope)) {
        auto state = clientStateById_.find(clientId);
        if (state != clientStateById_.end()) {
            state->second.TrackReliableEvent(event, nowMs);
        }
        ++stats_.reliableEventsSent;
        if (isResend) {
            ++stats_.reliableEventsResent;
        }
    }
}

bool ServerNetworkHost::SendSnapshot(game::ClientId clientId, game::TimestampMs nowMs) {
    auto state = clientStateById_.find(clientId);
    const auto requestedBaselineId =
        state == clientStateById_.end() ? game::SnapshotId{} : state->second.AckedSnapshot();
    const auto baselineDecision = baselineCache_.Decide(clientId, requestedBaselineId);
    auto snapshot = runtime_.BuildSnapshotFor(clientId, baselineDecision.effectiveBaselineId);
    snapshot.deliveryKind = baselineDecision.deliveryKind;

    game::ReplicationPlanner planner{config_.replication};
    auto planned = planner.Plan(std::move(snapshot), clientId);

    if (planned.snapshot.deliveryKind == game::SnapshotDeliveryKind::DeltaEligible) {
        const auto baseline = baselineCache_.Find(clientId, planned.snapshot.baselineId);
        if (baseline.has_value()) {
            const auto deltaPlan = game::PlanSnapshotDelta(planned.snapshot, *baseline);
            if (deltaPlan.baselineMatched) {
                ++stats_.deltaPlaceholderSnapshotsPlanned;
                stats_.deltaPlaceholderAddedEntities += deltaPlan.addedCount;
                stats_.deltaPlaceholderChangedEntities += deltaPlan.changedCount;
                stats_.deltaPlaceholderRemovedEntities += deltaPlan.removedCount;
                stats_.deltaPlaceholderUnchangedEntities += deltaPlan.unchangedCount;
            }
        }
    }

    game::NetworkEnvelope envelope{};
    envelope.peerId = clientId;
    envelope.channel = game::NetworkChannel::Snapshots;
    envelope.messageClass = game::MessageClass::Snapshot;
    envelope.sequence = planned.snapshot.snapshotId;
    envelope.payload = game::SerializeSnapshot(planned.snapshot);
    if (!transport_.Send(envelope)) {
        return false;
    }

    if (state != clientStateById_.end()) {
        const auto interestFrame = state->second.Interest().Update(planned.snapshot);
        stats_.interestEnterEvents += interestFrame.entered.size();
        stats_.interestStayEvents += interestFrame.stayed.size();
        stats_.interestExitEvents += interestFrame.exited.size();
        SendInterestEvents(clientId, interestFrame, nowMs);
    }

    baselineCache_.Store(clientId, planned.snapshot);

    ++stats_.snapshotsSent;
    if (planned.snapshot.deliveryKind == game::SnapshotDeliveryKind::DeltaEligible) {
        ++stats_.deltaEligibleSnapshotsSent;
    } else {
        ++stats_.fullSnapshotsSent;
    }
    if (requestedBaselineId != 0 && !baselineDecision.baselineAvailable) {
        ++stats_.baselineMisses;
    }
    stats_.snapshotEntitiesSent += planned.stats.outputEntityCount;
    stats_.snapshotEntitiesDropped += planned.stats.droppedEntityCount;
    return true;
}

} // namespace game::server
