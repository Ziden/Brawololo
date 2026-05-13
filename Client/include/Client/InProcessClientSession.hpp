#pragma once

#include "Client/IClientSession.hpp"
#include "Networking/LoopbackTransport.hpp"
#include "Server/ServerNetworkHost.hpp"

#include <memory>
#include <vector>

namespace game::client {

struct InProcessClientSessionConfig {
    std::vector<game::ClientId> extraServerClients{2};
};

class InProcessClientSession final : public IClientSession {
public:
    explicit InProcessClientSession(InProcessClientSessionConfig config = {});

    [[nodiscard]] bool Connect(game::ClientId localClientId, game::TimestampMs nowMs) override;
    void SendInput(const game::ClientInputPacket& packet) override;
    void Tick(game::TimestampMs nowMs) override;
    void Pump(ClientRuntime& runtime) override;
    [[nodiscard]] game::EventList DrainEvents() override;
    [[nodiscard]] ClientSessionStats Stats() const noexcept override;

private:
    InProcessClientSessionConfig config_{};
    game::net::LoopbackTransport clientTransport_{};
    game::net::LoopbackTransport serverTransport_{};
    std::unique_ptr<game::server::ServerNetworkHost> serverHost_{};
    game::ClientId localClientId_{};
    game::TimestampMs nextTimeSyncAtMs_{};
    game::TimestampMs lastTickTimeMs_{};
    game::CommandSequence nextTimeSyncSequence_{1};
    game::EventList reliableEvents_{};
    ClientSessionStats stats_{};
    bool connected_{};
};

} // namespace game::client
