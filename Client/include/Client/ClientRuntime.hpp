#pragma once

#include "Client/PendingInputHistory.hpp"
#include "Client/PresentationInterpolator.hpp"
#include "GameLogic/Commands.hpp"
#include "GameLogic/Events.hpp"
#include "GameLogic/Replication.hpp"
#include "GameLogic/Simulation.hpp"
#include "GameLogic/TimeSync.hpp"

#include <deque>
#include <optional>
#include <vector>

namespace game::client {

class InterpolationBuffer {
public:
    void Push(game::SnapshotDTO snapshot);
    [[nodiscard]] std::optional<game::SnapshotDTO>
    SampleAt(game::TimestampMs serverRenderTimeMs) const;
    [[nodiscard]] std::size_t Size() const noexcept;

private:
    std::deque<game::SnapshotDTO> snapshots_{};
    std::size_t maxSnapshots_{32};
};

class ClientRuntime {
public:
    explicit ClientRuntime(game::ClientId localClientId, game::SimulationConfig config = {});

    [[nodiscard]] bool ConnectLocal(game::TimestampMs localTimeMs = 0);
    [[nodiscard]] game::ClientInputPacket QueueInput(game::InputFrame input,
                                                     game::TimestampMs localTimeMs);
    void TickSimulation();
    void ApplyServerSnapshot(const game::SnapshotDTO& snapshot);
    void RecordTimeSyncSample(const game::TimeSyncSample& sample);

    [[nodiscard]] game::EventList DrainEvents();
    [[nodiscard]] game::ClientId LocalClientId() const noexcept;
    [[nodiscard]] const game::NetworkClock& Clock() const noexcept;
    [[nodiscard]] const game::GameSimulation& Simulation() const noexcept;
    [[nodiscard]] game::GameSimulation& Simulation() noexcept;
    [[nodiscard]] const std::vector<game::ClientInputPacket>& UnackedInputs() const noexcept;
    [[nodiscard]] const InterpolationBuffer& Interpolation() const noexcept;
    [[nodiscard]] const PresentationInterpolator& Presentation() const noexcept;

private:
    game::ClientId localClientId_{};
    game::CommandSequence nextSequence_{1};
    game::GameSimulation simulation_;
    game::NetworkClock clock_{};
    PendingInputHistory pendingInputs_{};
    InterpolationBuffer interpolation_{};
    PresentationInterpolator presentationInterpolator_{};
};

} // namespace game::client
