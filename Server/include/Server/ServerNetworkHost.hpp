#pragma once

#include "Networking/ITransport.hpp"
#include "GameLogic/InterestManagement.hpp"
#include "GameLogic/NetworkEvents.hpp"
#include "GameLogic/ReplicationPlanner.hpp"
#include "GameLogic/SnapshotBaselineCache.hpp"
#include "Server/ServerClientReplicationState.hpp"
#include "Server/ServerRuntime.hpp"

#include <unordered_map>
#include <vector>

namespace game::server {

struct ServerNetworkStats {
    std::size_t connectedClients{};
    std::size_t loginRequests{};
    std::size_t inputsReceived{};
    std::size_t snapshotAcksReceived{};
    std::size_t staleSnapshotAcksRejected{};
    std::size_t timeSyncRequestsReceived{};
    std::size_t timeSyncResponsesSent{};
    std::size_t reliableEventsSent{};
    std::size_t reliableEventAcksReceived{};
    std::size_t staleReliableEventAcksRejected{};
    std::size_t snapshotsSent{};
    std::size_t snapshotsSkippedBySchedule{};
    std::size_t fullSnapshotsSent{};
    std::size_t deltaEligibleSnapshotsSent{};
    std::size_t baselineMisses{};
    std::size_t snapshotEntitiesSent{};
    std::size_t snapshotEntitiesDropped{};
    std::size_t interestEnterEvents{};
    std::size_t interestStayEvents{};
    std::size_t interestExitEvents{};
};

struct ServerNetworkHostConfig {
    game::ReplicationPlannerConfig replication{};
    game::SnapshotBaselineCacheConfig baselines{};
    game::TimestampMs snapshotSendIntervalMs{16};
};

class ServerNetworkHost {
public:
    explicit ServerNetworkHost(game::net::ITransport& transport, ServerNetworkHostConfig config = {});

    [[nodiscard]] bool ConnectSimulationOnlyClient(game::ClientId clientId, game::TimestampMs nowMs = 0);
    void PumpClientMessages();
    void TickAndSendSnapshots(game::TimestampMs nowMs);

    [[nodiscard]] game::EventList DrainEvents();
    [[nodiscard]] ServerNetworkStats Stats() const noexcept;
    [[nodiscard]] const ServerRuntime& Runtime() const noexcept;
    [[nodiscard]] ServerRuntime& Runtime() noexcept;

private:
    void HandleLoginEnvelope(const game::NetworkEnvelope& envelope);
    void HandleInputEnvelope(const game::NetworkEnvelope& envelope);
    void HandleSnapshotAckEnvelope(const game::NetworkEnvelope& envelope);
    void HandleNetworkEventAckEnvelope(const game::NetworkEnvelope& envelope);
    void HandleTimeSyncEnvelope(const game::NetworkEnvelope& envelope);
    void BroadcastReliableEvents(const game::EventList& events);
    void SendReliableEvent(game::ClientId clientId, const game::NetworkEventDTO& event);
    [[nodiscard]] bool ShouldSendSnapshot(game::ClientId clientId, game::TimestampMs nowMs) const;
    void MarkSnapshotSent(game::ClientId clientId, game::TimestampMs nowMs);
    [[nodiscard]] bool SendSnapshot(game::ClientId clientId);

    game::net::ITransport& transport_;
    ServerNetworkHostConfig config_{};
    ServerRuntime runtime_;
    game::SnapshotBaselineCache baselineCache_;
    std::vector<game::ClientId> clients_{};
    std::unordered_map<game::ClientId, ServerClientReplicationState> clientStateById_{};
    game::EventList drainedEvents_{};
    game::NetworkEventId nextNetworkEventId_{1};
    ServerNetworkStats stats_{};
};

} // namespace game::server
