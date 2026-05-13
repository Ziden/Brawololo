#pragma once

#include "Networking/ITransport.hpp"

#include <cstdint>
#include <memory>
#include <queue>
#include <utility>

namespace game::net {

struct LoopbackNetworkConditions {
    std::uint32_t dropEveryNthOutgoingEnvelope{};
    std::uint32_t delayEveryNthOutgoingEnvelope{};
};

class LoopbackTransport final : public ITransport {
public:
    static std::pair<LoopbackTransport, LoopbackTransport> CreatePair();
    static std::pair<LoopbackTransport, LoopbackTransport> CreatePair(
        LoopbackNetworkConditions conditions);

    LoopbackTransport() = default;

    void SetNetworkConditions(LoopbackNetworkConditions conditions) noexcept;
    void FlushDelayed();

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
        A,
        B,
    };

    struct SharedState {
        std::queue<game::NetworkEnvelope> aToB{};
        std::queue<game::NetworkEnvelope> bToA{};
        std::queue<game::NetworkEnvelope> delayedAToB{};
        std::queue<game::NetworkEnvelope> delayedBToA{};
    };

    LoopbackTransport(
        std::shared_ptr<SharedState> state,
        Endpoint endpoint,
        LoopbackNetworkConditions conditions = {});

    [[nodiscard]] std::queue<game::NetworkEnvelope>& IncomingQueue();
    [[nodiscard]] std::queue<game::NetworkEnvelope>& OutgoingQueue();
    [[nodiscard]] std::queue<game::NetworkEnvelope>& DelayedOutgoingQueue();
    [[nodiscard]] bool ShouldDropOutgoing() const noexcept;
    [[nodiscard]] bool ShouldDelayOutgoing() const noexcept;
    void PushOutgoing(game::NetworkEnvelope envelope);
    void ReleaseOneDelayed();

    std::shared_ptr<SharedState> state_{};
    Endpoint endpoint_{Endpoint::A};
    LoopbackNetworkConditions conditions_{};
    std::uint32_t outgoingEnvelopeOrdinal_{};
    TransportState stateValue_{TransportState::Disconnected};
    TransportStats stats_{};
    TransportError lastError_{TransportError::None};
};

} // namespace game::net
