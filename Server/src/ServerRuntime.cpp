#include "Server/ServerRuntime.hpp"

namespace game::server {

ServerRuntime::ServerRuntime(game::SimulationConfig config)
    : simulation_([&config] {
          config.mode = game::SimulationMode::ServerAuthoritative;
          return config;
      }()) {}

bool ServerRuntime::ConnectClient(game::ClientId clientId, game::TimestampMs clientTimestampMs) {
    game::LoginCommand login{};
    login.header.clientId = clientId;
    login.header.sequence = 0;
    login.header.clientTimestampMs = clientTimestampMs;
    return simulation_.Submit(login);
}

bool ServerRuntime::SubmitInput(const game::ClientInputPacket& packet) {
    return simulation_.Submit(packet);
}

void ServerRuntime::Tick() {
    simulation_.TickFixed();
    history_.Record(simulation_.BuildSnapshot(0, nextHistorySnapshotId_++, 0));
}

game::SnapshotDTO ServerRuntime::BuildSnapshotFor(game::ClientId clientId,
                                                  game::SnapshotId baselineId) {
    return simulation_.BuildSnapshot(clientId, nextSnapshotId_++, baselineId);
}

std::optional<game::SnapshotDTO>
ServerRuntime::HistoricalSnapshotAtOrBefore(game::TimestampMs serverTimeMs) const {
    return history_.ClosestAtOrBefore(serverTimeMs);
}

game::LagCompensationResult
ServerRuntime::QueryLagCompensation(const game::ClientInputPacket& packet,
                                    std::int64_t clientToServerOffsetMs) const {
    return lagCompensation_.Query(history_,
                                  game::LagCompensationQuery{packet.header.clientId,
                                                             packet.header.sequence,
                                                             packet.header.clientTimestampMs,
                                                             clientToServerOffsetMs,
                                                             simulation_.ServerTimeMs()});
}

game::EventList ServerRuntime::DrainEvents() {
    return simulation_.DrainEvents();
}

const game::GameSimulation& ServerRuntime::Simulation() const noexcept {
    return simulation_;
}

game::GameSimulation& ServerRuntime::Simulation() noexcept {
    return simulation_;
}

} // namespace game::server
