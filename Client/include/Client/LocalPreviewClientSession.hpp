#pragma once

#include "Client/IClientSession.hpp"

namespace game::client {

class LocalPreviewClientSession final : public IClientSession {
public:
    [[nodiscard]] bool Connect(game::ClientId localClientId, game::TimestampMs nowMs) override;
    void SendInput(const game::ClientInputPacket& packet) override;
    void Tick(game::TimestampMs nowMs) override;
    void Pump(ClientRuntime& runtime) override;
    [[nodiscard]] game::EventList DrainEvents() override;
    [[nodiscard]] ClientSessionStats Stats() const noexcept override;

private:
    ClientSessionStats stats_{};
};

} // namespace game::client
