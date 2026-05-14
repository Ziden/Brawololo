#pragma once

#include "Client/IClientSession.hpp"
#include "Networking/ITransport.hpp"

#include <memory>

namespace game::client {

class RemoteClientSession final : public IClientSession {
public:
    explicit RemoteClientSession(std::unique_ptr<game::net::ITransport> transport);

    [[nodiscard]] bool Connect(game::ClientId localClientId, game::TimestampMs nowMs) override;
    void SendInput(const game::ClientInputPacket& packet) override;
    void Tick(game::TimestampMs nowMs) override;
    void Pump(ClientRuntime& runtime) override;
    [[nodiscard]] game::EventList DrainEvents() override;
    [[nodiscard]] ClientSessionStats Stats() const noexcept override;

private:
    [[nodiscard]] bool TrySendLogin(game::TimestampMs nowMs);

    std::unique_ptr<game::net::ITransport> transport_{};
    ClientSessionStats stats_{};
    game::ClientId localClientId_{};
    game::TimestampMs nextTimeSyncAtMs_{};
    game::TimestampMs lastTickTimeMs_{};
    game::CommandSequence nextTimeSyncSequence_{1};
    game::EventList reliableEvents_{};
    bool sessionActive_{};
    bool loginSent_{};
};

} // namespace game::client
