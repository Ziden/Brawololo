#include "Client/ClientApplication.hpp"

#include <utility>

namespace game::client {

ClientApplication::ClientApplication(std::unique_ptr<IClientSession> session, ClientApplicationConfig config)
    : config_(config)
    , session_(std::move(session))
    , runtime_(config_.localClientId)
{
}

bool ClientApplication::Connect(game::TimestampMs nowMs)
{
    if (!session_) {
        return false;
    }

    const auto runtimeConnected = runtime_.ConnectLocal(nowMs);
    const auto sessionConnected = session_->Connect(config_.localClientId, nowMs);
    return runtimeConnected && sessionConnected;
}

game::ClientInputPacket ClientApplication::SubmitInput(game::InputFrame input, game::TimestampMs nowMs)
{
    auto packet = runtime_.QueueInput(input, nowMs);
    if (session_) {
        session_->SendInput(packet);
    }
    return packet;
}

void ClientApplication::TickFixed(game::TimestampMs nowMs)
{
    runtime_.TickSimulation();

    if (session_) {
        session_->Tick(nowMs);
        session_->Pump(runtime_);
    }

    ++fixedTick_;
}

game::EventList ClientApplication::DrainEvents()
{
    auto events = runtime_.DrainEvents();
    if (!session_) {
        return events;
    }

    auto sessionEvents = session_->DrainEvents();
    events.insert(events.end(), sessionEvents.begin(), sessionEvents.end());
    return events;
}

ClientApplicationStats ClientApplication::Stats() const
{
    ClientApplicationStats stats{};
    stats.fixedTick = fixedTick_;
    stats.entityCount = runtime_.Simulation().EntityCount();
    stats.unackedInputCount = runtime_.UnackedInputs().size();
    if (session_) {
        stats.session = session_->Stats();
    }
    return stats;
}

ClientRuntime& ClientApplication::Runtime() noexcept
{
    return runtime_;
}

const ClientRuntime& ClientApplication::Runtime() const noexcept
{
    return runtime_;
}

} // namespace game::client

