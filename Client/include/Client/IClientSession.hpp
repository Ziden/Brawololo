#pragma once

#include "GameLogic/Commands.hpp"
#include "GameLogic/Events.hpp"
#include "GameLogic/Replication.hpp"

#include <cstddef>
#include <cstdint>

namespace game::client {

class ClientRuntime;

enum class ClientConnectionState : std::uint8_t {
    Disconnected,
    Connecting,
    AwaitingSpawn,
    Connected,
};

struct ClientSessionStats {
    game::ClientId localClientId{};
    ClientConnectionState connectionState{ClientConnectionState::Disconnected};
    game::SnapshotId lastSnapshotId{};
    game::SnapshotId lastBaselineId{};
    game::CommandSequence lastAckedInputSequence{};
    game::CommandSequence lastTimeSyncSequence{};
    std::size_t snapshotsApplied{};
    std::size_t snapshotAcksSent{};
    std::size_t timeSyncRequestsSent{};
    std::size_t timeSyncResponsesApplied{};
    std::size_t reliableEventsReceived{};
    std::size_t reliableEventAcksSent{};
    std::size_t spawnAcceptedEventsReceived{};
};

class IClientSession {
public:
    virtual ~IClientSession() = default;

    [[nodiscard]] virtual bool Connect(game::ClientId localClientId, game::TimestampMs nowMs) = 0;
    virtual void SendInput(const game::ClientInputPacket& packet) = 0;
    virtual void Tick(game::TimestampMs nowMs) = 0;
    virtual void Pump(ClientRuntime& runtime) = 0;
    [[nodiscard]] virtual game::EventList DrainEvents() = 0;
    [[nodiscard]] virtual ClientSessionStats Stats() const noexcept = 0;
};

} // namespace game::client
