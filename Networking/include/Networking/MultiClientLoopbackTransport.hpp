#pragma once

#include "Networking/ITransport.hpp"

#include <memory>
#include <queue>
#include <unordered_map>

namespace game::net {

class MultiClientLoopbackTransport final : public ITransport {
public:
    struct Hub {
        std::queue<game::NetworkEnvelope> toServer{};
        std::unordered_map<game::ClientId, std::queue<game::NetworkEnvelope>> toClients{};
    };

    static std::shared_ptr<Hub> CreateHub();
    static MultiClientLoopbackTransport CreateServer(std::shared_ptr<Hub> hub);
    static MultiClientLoopbackTransport CreateClient(std::shared_ptr<Hub> hub, game::ClientId clientId);

    MultiClientLoopbackTransport() = default;

    [[nodiscard]] bool Connect() override;
    void Update() override;
    void Close() override;
    [[nodiscard]] bool Send(const game::NetworkEnvelope& envelope) override;
    [[nodiscard]] std::optional<game::NetworkEnvelope> Poll() override;
    [[nodiscard]] TransportState State() const noexcept override;
    [[nodiscard]] TransportStats Stats() const noexcept override;
    [[nodiscard]] TransportError LastError() const noexcept override;

private:
    enum class Endpoint {
        Server,
        Client,
    };

    MultiClientLoopbackTransport(std::shared_ptr<Hub> hub, Endpoint endpoint, game::ClientId clientId = 0);

    std::shared_ptr<Hub> hub_{};
    Endpoint endpoint_{Endpoint::Server};
    game::ClientId clientId_{};
    TransportState state_{TransportState::Disconnected};
    TransportStats stats_{};
    TransportError lastError_{TransportError::None};
};

} // namespace game::net
