#pragma once

#include "Client/ClientRuntime.hpp"
#include "Client/IClientSession.hpp"

#include <memory>

namespace game::client {

struct ClientApplicationConfig {
    game::ClientId localClientId{1};
};

struct ClientApplicationStats {
    game::Tick fixedTick{};
    std::size_t entityCount{};
    std::size_t unackedInputCount{};
    ClientSessionStats session{};
};

class ClientApplication {
public:
    ClientApplication(std::unique_ptr<IClientSession> session, ClientApplicationConfig config = {});

    [[nodiscard]] bool Connect(game::TimestampMs nowMs);
    [[nodiscard]] game::ClientInputPacket SubmitInput(game::InputFrame input,
                                                      game::TimestampMs nowMs);
    void TickFixed(game::TimestampMs nowMs);
    [[nodiscard]] game::EventList DrainEvents();
    [[nodiscard]] ClientApplicationStats Stats() const;
    [[nodiscard]] ClientRuntime& Runtime() noexcept;
    [[nodiscard]] const ClientRuntime& Runtime() const noexcept;

private:
    ClientApplicationConfig config_{};
    std::unique_ptr<IClientSession> session_{};
    ClientRuntime runtime_;
    game::Tick fixedTick_{};
};

} // namespace game::client
