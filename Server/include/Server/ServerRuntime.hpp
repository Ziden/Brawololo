#pragma once

#include "GameLogic/Commands.hpp"
#include "GameLogic/Events.hpp"
#include "GameLogic/LagCompensation.hpp"
#include "GameLogic/Replication.hpp"
#include "GameLogic/Simulation.hpp"

#include <optional>

namespace game::server {

class ServerRuntime {
public:
    explicit ServerRuntime(game::SimulationConfig config = {});

    [[nodiscard]] bool ConnectClient(game::ClientId clientId,
                                     game::TimestampMs clientTimestampMs = 0);
    [[nodiscard]] bool SubmitInput(const game::ClientInputPacket& packet);
    void Tick();

    [[nodiscard]] game::SnapshotDTO BuildSnapshotFor(game::ClientId clientId,
                                                     game::SnapshotId baselineId = 0);
    [[nodiscard]] std::optional<game::SnapshotDTO>
    HistoricalSnapshotAtOrBefore(game::TimestampMs serverTimeMs) const;
    [[nodiscard]] game::LagCompensationResult
    QueryLagCompensation(const game::ClientInputPacket& packet,
                         std::int64_t clientToServerOffsetMs = 0) const;
    [[nodiscard]] game::EventList DrainEvents();
    [[nodiscard]] const game::GameSimulation& Simulation() const noexcept;
    [[nodiscard]] game::GameSimulation& Simulation() noexcept;

private:
    game::GameSimulation simulation_;
    game::LagCompensationHistory history_{128};
    game::LagCompensationService lagCompensation_{};
    game::SnapshotId nextSnapshotId_{1};
    game::SnapshotId nextHistorySnapshotId_{1};
};

} // namespace game::server
