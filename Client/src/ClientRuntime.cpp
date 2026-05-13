#include "Client/ClientRuntime.hpp"

#include <utility>

namespace game::client {

void InterpolationBuffer::Push(game::SnapshotDTO snapshot)
{
    snapshots_.push_back(std::move(snapshot));
    while (snapshots_.size() > maxSnapshots_) {
        snapshots_.pop_front();
    }
}

std::optional<game::SnapshotDTO> InterpolationBuffer::SampleAt(game::TimestampMs serverRenderTimeMs) const
{
    std::optional<game::SnapshotDTO> result{};
    for (const auto& snapshot : snapshots_) {
        if (snapshot.serverTimeMs <= serverRenderTimeMs) {
            result = snapshot;
        }
    }
    return result;
}

std::size_t InterpolationBuffer::Size() const noexcept
{
    return snapshots_.size();
}

ClientRuntime::ClientRuntime(game::ClientId localClientId, game::SimulationConfig config)
    : localClientId_(localClientId)
    , simulation_([&config] {
        config.mode = game::SimulationMode::ClientPrediction;
        return config;
    }())
{
}

bool ClientRuntime::ConnectLocal(game::TimestampMs localTimeMs)
{
    game::LoginCommand login{};
    login.header.clientId = localClientId_;
    login.header.sequence = 0;
    login.header.clientTimestampMs = localTimeMs;
    return simulation_.Submit(login);
}

game::ClientInputPacket ClientRuntime::QueueInput(game::InputFrame input, game::TimestampMs localTimeMs)
{
    game::ClientInputPacket packet{};
    packet.header.clientId = localClientId_;
    packet.header.sequence = nextSequence_++;
    packet.header.clientTimestampMs = localTimeMs;
    packet.input = input;

    pendingInputs_.Record(packet);
    (void)simulation_.Submit(packet);
    return packet;
}

void ClientRuntime::TickSimulation()
{
    simulation_.TickFixed();
}

void ClientRuntime::ApplyServerSnapshot(const game::SnapshotDTO& snapshot)
{
    interpolation_.Push(snapshot);
    presentationInterpolator_.PushSnapshot(snapshot);
    simulation_.ApplySnapshot(snapshot, localClientId_);

    pendingInputs_.AcknowledgeThrough(snapshot.ackedInputSequence);
    simulation_.ReplayLocalInputsForPrediction(pendingInputs_.UnackedInputs());
}

void ClientRuntime::RecordTimeSyncSample(const game::TimeSyncSample& sample)
{
    clock_.RecordSample(sample);
}

game::EventList ClientRuntime::DrainEvents()
{
    return simulation_.DrainEvents();
}

game::ClientId ClientRuntime::LocalClientId() const noexcept
{
    return localClientId_;
}

const game::NetworkClock& ClientRuntime::Clock() const noexcept
{
    return clock_;
}

const game::GameSimulation& ClientRuntime::Simulation() const noexcept
{
    return simulation_;
}

game::GameSimulation& ClientRuntime::Simulation() noexcept
{
    return simulation_;
}

const std::vector<game::ClientInputPacket>& ClientRuntime::UnackedInputs() const noexcept
{
    return pendingInputs_.UnackedInputs();
}

const InterpolationBuffer& ClientRuntime::Interpolation() const noexcept
{
    return interpolation_;
}

const PresentationInterpolator& ClientRuntime::Presentation() const noexcept
{
    return presentationInterpolator_;
}

} // namespace game::client
