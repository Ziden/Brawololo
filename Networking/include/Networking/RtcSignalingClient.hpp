#pragma once

#include "Networking/RtcSignaling.hpp"

#include <cstddef>
#include <memory>
#include <optional>
#include <queue>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace game::net {

enum class RtcSignalingConnectionState {
    Disconnected,
    Connected,
    Failed,
};

enum class RtcSignalingClientError {
    None,
    InvalidConfiguration,
    NotConnected,
    NotImplemented,
    ProtocolRejected,
    RouteUnavailable,
};

struct RtcSignalingClientConfig {
    game::ClientId peerId{};
    std::string sessionId{"default"};
};

struct RtcSignalingClientStats {
    std::size_t messagesSent{};
    std::size_t messagesReceived{};
    std::size_t sendFailures{};
    std::size_t receiveFailures{};
    std::size_t bytesSent{};
    std::size_t bytesReceived{};
};

class IRtcSignalingClient {
public:
    virtual ~IRtcSignalingClient() = default;

    [[nodiscard]] virtual bool Connect() = 0;
    virtual void Close() = 0;
    [[nodiscard]] virtual bool Send(const RtcSignalingMessage& message) = 0;
    [[nodiscard]] virtual std::optional<RtcSignalingMessage> Poll() = 0;
    [[nodiscard]] virtual RtcSignalingConnectionState State() const noexcept = 0;
    [[nodiscard]] virtual RtcSignalingClientStats Stats() const noexcept = 0;
    [[nodiscard]] virtual RtcSignalingClientError LastError() const noexcept = 0;
};

class InMemoryRtcSignalingHub final {
public:
    [[nodiscard]] bool RegisterPeer(game::ClientId peerId, std::string sessionId);
    void UnregisterPeer(game::ClientId peerId);
    [[nodiscard]] bool Route(std::span<const std::byte> bytes);
    [[nodiscard]] std::optional<std::vector<std::byte>> Poll(game::ClientId peerId);
    [[nodiscard]] std::size_t PeerCount() const noexcept;

private:
    struct PeerQueue {
        std::string sessionId{};
        std::queue<std::vector<std::byte>> incoming{};
    };

    std::unordered_map<game::ClientId, PeerQueue> peers_{};
};

class InMemoryRtcSignalingClient final : public IRtcSignalingClient {
public:
    static std::shared_ptr<InMemoryRtcSignalingHub> CreateHub();

    InMemoryRtcSignalingClient(std::shared_ptr<InMemoryRtcSignalingHub> hub,
                               RtcSignalingClientConfig config);

    [[nodiscard]] bool Connect() override;
    void Close() override;
    [[nodiscard]] bool Send(const RtcSignalingMessage& message) override;
    [[nodiscard]] std::optional<RtcSignalingMessage> Poll() override;
    [[nodiscard]] RtcSignalingConnectionState State() const noexcept override;
    [[nodiscard]] RtcSignalingClientStats Stats() const noexcept override;
    [[nodiscard]] RtcSignalingClientError LastError() const noexcept override;

private:
    [[nodiscard]] bool HasValidConfig() const noexcept;

    std::shared_ptr<InMemoryRtcSignalingHub> hub_{};
    RtcSignalingClientConfig config_{};
    RtcSignalingConnectionState state_{RtcSignalingConnectionState::Disconnected};
    RtcSignalingClientStats stats_{};
    RtcSignalingClientError lastError_{RtcSignalingClientError::None};
};

class UnsupportedRtcSignalingClient final : public IRtcSignalingClient {
public:
    [[nodiscard]] bool Connect() override;
    void Close() override;
    [[nodiscard]] bool Send(const RtcSignalingMessage& message) override;
    [[nodiscard]] std::optional<RtcSignalingMessage> Poll() override;
    [[nodiscard]] RtcSignalingConnectionState State() const noexcept override;
    [[nodiscard]] RtcSignalingClientStats Stats() const noexcept override;
    [[nodiscard]] RtcSignalingClientError LastError() const noexcept override;

private:
    RtcSignalingConnectionState state_{RtcSignalingConnectionState::Disconnected};
    RtcSignalingClientStats stats_{};
    RtcSignalingClientError lastError_{RtcSignalingClientError::None};
};

} // namespace game::net
