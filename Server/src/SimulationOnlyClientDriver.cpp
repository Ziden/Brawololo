#include "Server/SimulationOnlyClientDriver.hpp"

#include "Server/ServerRuntime.hpp"

#include <algorithm>
#include <cmath>

namespace game::server {
namespace {

std::int16_t AimComponentFromRatio(double ratio)
{
    return static_cast<std::int16_t>(std::clamp(ratio * 1000.0, -1000.0, 1000.0));
}

} // namespace

SimulationOnlyClientDriver::SimulationOnlyClientDriver(SimulationOnlyClientDriverConfig config)
    : config_(config)
    , nextFireAtMs_(config_.firstFireAtMs)
{
}

game::ClientId SimulationOnlyClientDriver::ClientId() const noexcept
{
    return config_.clientId;
}

bool SimulationOnlyClientDriver::Tick(ServerRuntime& runtime, game::TimestampMs nowMs)
{
    const auto shouldFire = nowMs >= nextFireAtMs_;
    auto input = BuildInput(runtime, shouldFire);
    if (shouldFire) {
        nextFireAtMs_ = nowMs + config_.fireIntervalMs;
    }

    game::ClientInputPacket packet{};
    packet.header.clientId = config_.clientId;
    packet.header.sequence = nextSequence_++;
    packet.header.clientTimestampMs = nowMs;
    packet.input = input;
    return runtime.SubmitInput(packet);
}

game::InputFrame SimulationOnlyClientDriver::BuildInput(const ServerRuntime& runtime, bool shouldFire) const
{
    game::InputFrame input{};
    input.aimX = 1000;
    input.fire = shouldFire;

    const auto self = runtime.Simulation().PlayerEntityForClient(config_.clientId);
    const auto target = runtime.Simulation().PlayerEntityForClient(config_.targetClientId);
    if (!self.has_value() || !target.has_value()) {
        return input;
    }

    const auto selfMovement = runtime.Simulation().MovementForEntity(*self);
    const auto targetMovement = runtime.Simulation().MovementForEntity(*target);
    if (!selfMovement.has_value() || !targetMovement.has_value()) {
        return input;
    }

    const auto dx = static_cast<double>(targetMovement->x) - static_cast<double>(selfMovement->x);
    const auto dy = static_cast<double>(targetMovement->y) - static_cast<double>(selfMovement->y);
    const auto length = std::sqrt((dx * dx) + (dy * dy));
    if (length <= 0.001) {
        return input;
    }

    input.aimX = AimComponentFromRatio(dx / length);
    input.aimY = AimComponentFromRatio(dy / length);
    return input;
}

} // namespace game::server
