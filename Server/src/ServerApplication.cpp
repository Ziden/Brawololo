#include "Server/ServerApplication.hpp"

#include <utility>

namespace game::server {

ServerApplication::ServerApplication(std::unique_ptr<game::net::ITransport> transport, ServerApplicationConfig config)
    : transport_(std::move(transport))
    , config_(config)
{
}

ServerApplication::~ServerApplication()
{
    RequestStop();
}

bool ServerApplication::Start()
{
    if (!transport_) {
        return false;
    }

    if (!transport_->Connect()) {
        return false;
    }

    networkHost_ = std::make_unique<ServerNetworkHost>(*transport_, config_.network);
    running_ = true;
    return true;
}

void ServerApplication::RequestStop()
{
    running_ = false;
    if (transport_) {
        transport_->Close();
    }
}

void ServerApplication::TickOnce(game::TimestampMs nowMs)
{
    if (!running_ || !transport_ || !networkHost_) {
        return;
    }

    transport_->Update();
    networkHost_->PumpClientMessages();
    networkHost_->TickAndSendSnapshots(nowMs);
    ++ticksRun_;
}

void ServerApplication::RunForTicks(std::size_t tickCount, game::TimestampMs startTimeMs)
{
    for (std::size_t index = 0; index < tickCount && running_; ++index) {
        TickOnce(startTimeMs + static_cast<game::TimestampMs>(index) * config_.tickDurationMs);
    }
}

bool ServerApplication::IsRunning() const noexcept
{
    return running_;
}

ServerApplicationStats ServerApplication::Stats() const noexcept
{
    ServerApplicationStats stats{};
    stats.ticksRun = ticksRun_;
    stats.running = running_;
    if (networkHost_) {
        stats.network = networkHost_->Stats();
    }
    return stats;
}

ServerNetworkHost* ServerApplication::NetworkHost() noexcept
{
    return networkHost_.get();
}

const ServerNetworkHost* ServerApplication::NetworkHost() const noexcept
{
    return networkHost_.get();
}

} // namespace game::server
