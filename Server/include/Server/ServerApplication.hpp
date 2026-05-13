#pragma once

#include "Networking/ITransport.hpp"
#include "Server/ServerNetworkHost.hpp"

#include <cstddef>
#include <memory>

namespace game::server {

struct ServerApplicationConfig {
    game::TimestampMs tickDurationMs{16};
    ServerNetworkHostConfig network{};
};

struct ServerApplicationStats {
    game::Tick ticksRun{};
    bool running{};
    ServerNetworkStats network{};
};

class ServerApplication {
public:
    ServerApplication(std::unique_ptr<game::net::ITransport> transport,
                      ServerApplicationConfig config = {});
    ~ServerApplication();

    ServerApplication(const ServerApplication&) = delete;
    ServerApplication& operator=(const ServerApplication&) = delete;

    [[nodiscard]] bool Start();
    void RequestStop();
    void TickOnce(game::TimestampMs nowMs);
    void RunForTicks(std::size_t tickCount, game::TimestampMs startTimeMs = 0);

    [[nodiscard]] bool IsRunning() const noexcept;
    [[nodiscard]] ServerApplicationStats Stats() const noexcept;
    [[nodiscard]] ServerNetworkHost* NetworkHost() noexcept;
    [[nodiscard]] const ServerNetworkHost* NetworkHost() const noexcept;

private:
    std::unique_ptr<game::net::ITransport> transport_{};
    ServerApplicationConfig config_{};
    std::unique_ptr<ServerNetworkHost> networkHost_{};
    game::Tick ticksRun_{};
    bool running_{};
};

} // namespace game::server
