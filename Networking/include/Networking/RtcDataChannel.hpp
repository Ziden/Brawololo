#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <utility>
#include <queue>
#include <span>
#include <vector>

namespace game::net {

enum class RtcDataChannelState {
    Closed,
    Open,
    Failed,
};

enum class RtcDataChannelError {
    None,
    NotOpen,
    NotImplemented,
    BufferFull,
    InvalidFrame,
};

struct RtcDataChannelStats {
    std::size_t framesSent{};
    std::size_t framesReceived{};
    std::size_t sendFailures{};
    std::size_t receiveFailures{};
    std::size_t bytesSent{};
    std::size_t bytesReceived{};
};

class IRtcDataChannel {
public:
    virtual ~IRtcDataChannel() = default;

    [[nodiscard]] virtual bool SendFrame(std::span<const std::byte> bytes) = 0;
    [[nodiscard]] virtual std::optional<std::vector<std::byte>> PollFrame() = 0;
    [[nodiscard]] virtual RtcDataChannelState State() const noexcept = 0;
    [[nodiscard]] virtual RtcDataChannelStats Stats() const noexcept = 0;
    [[nodiscard]] virtual RtcDataChannelError LastError() const noexcept = 0;
};

class InMemoryRtcDataChannel final : public IRtcDataChannel {
public:
    struct SharedState {
        std::queue<std::vector<std::byte>> aToB{};
        std::queue<std::vector<std::byte>> bToA{};
    };

    static std::pair<InMemoryRtcDataChannel, InMemoryRtcDataChannel> CreatePair();

    InMemoryRtcDataChannel() = default;

    [[nodiscard]] bool SendFrame(std::span<const std::byte> bytes) override;
    [[nodiscard]] std::optional<std::vector<std::byte>> PollFrame() override;
    [[nodiscard]] RtcDataChannelState State() const noexcept override;
    [[nodiscard]] RtcDataChannelStats Stats() const noexcept override;
    [[nodiscard]] RtcDataChannelError LastError() const noexcept override;
    void Close() noexcept;

private:
    enum class Endpoint {
        A,
        B,
    };

    InMemoryRtcDataChannel(std::shared_ptr<SharedState> state, Endpoint endpoint);

    [[nodiscard]] std::queue<std::vector<std::byte>>& IncomingQueue();
    [[nodiscard]] std::queue<std::vector<std::byte>>& OutgoingQueue();

    std::shared_ptr<SharedState> state_{};
    Endpoint endpoint_{Endpoint::A};
    RtcDataChannelState stateValue_{RtcDataChannelState::Closed};
    RtcDataChannelStats stats_{};
    RtcDataChannelError lastError_{RtcDataChannelError::None};
};

class UnsupportedRtcDataChannel final : public IRtcDataChannel {
public:
    [[nodiscard]] bool SendFrame(std::span<const std::byte> bytes) override;
    [[nodiscard]] std::optional<std::vector<std::byte>> PollFrame() override;
    [[nodiscard]] RtcDataChannelState State() const noexcept override;
    [[nodiscard]] RtcDataChannelStats Stats() const noexcept override;
    [[nodiscard]] RtcDataChannelError LastError() const noexcept override;

private:
    RtcDataChannelStats stats_{};
    RtcDataChannelError lastError_{RtcDataChannelError::NotImplemented};
};

} // namespace game::net
