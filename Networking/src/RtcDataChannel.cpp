#include "Networking/RtcDataChannel.hpp"

#include <utility>

namespace game::net {

std::pair<InMemoryRtcDataChannel, InMemoryRtcDataChannel> InMemoryRtcDataChannel::CreatePair() {
    auto state = std::make_shared<SharedState>();
    return {
        InMemoryRtcDataChannel{state, Endpoint::A},
        InMemoryRtcDataChannel{state, Endpoint::B},
    };
}

InMemoryRtcDataChannel::InMemoryRtcDataChannel(std::shared_ptr<SharedState> state,
                                               Endpoint endpoint)
    : state_(std::move(state)), endpoint_(endpoint), stateValue_(RtcDataChannelState::Open) {}

bool InMemoryRtcDataChannel::SendFrame(std::span<const std::byte> bytes) {
    if (!state_ || stateValue_ != RtcDataChannelState::Open) {
        ++stats_.sendFailures;
        lastError_ = RtcDataChannelError::NotOpen;
        return false;
    }

    if (bytes.empty()) {
        ++stats_.sendFailures;
        lastError_ = RtcDataChannelError::InvalidFrame;
        return false;
    }

    OutgoingQueue().push(std::vector<std::byte>{bytes.begin(), bytes.end()});
    ++stats_.framesSent;
    stats_.bytesSent += bytes.size();
    lastError_ = RtcDataChannelError::None;
    return true;
}

std::optional<std::vector<std::byte>> InMemoryRtcDataChannel::PollFrame() {
    if (!state_ || stateValue_ != RtcDataChannelState::Open || IncomingQueue().empty()) {
        return std::nullopt;
    }

    auto bytes = std::move(IncomingQueue().front());
    IncomingQueue().pop();
    ++stats_.framesReceived;
    stats_.bytesReceived += bytes.size();
    lastError_ = RtcDataChannelError::None;
    return bytes;
}

RtcDataChannelState InMemoryRtcDataChannel::State() const noexcept {
    return stateValue_;
}

RtcDataChannelStats InMemoryRtcDataChannel::Stats() const noexcept {
    return stats_;
}

RtcDataChannelError InMemoryRtcDataChannel::LastError() const noexcept {
    return lastError_;
}

void InMemoryRtcDataChannel::Close() noexcept {
    stateValue_ = RtcDataChannelState::Closed;
    lastError_ = RtcDataChannelError::None;
}

std::queue<std::vector<std::byte>>& InMemoryRtcDataChannel::IncomingQueue() {
    return endpoint_ == Endpoint::A ? state_->bToA : state_->aToB;
}

std::queue<std::vector<std::byte>>& InMemoryRtcDataChannel::OutgoingQueue() {
    return endpoint_ == Endpoint::A ? state_->aToB : state_->bToA;
}

bool UnsupportedRtcDataChannel::SendFrame(std::span<const std::byte>) {
    ++stats_.sendFailures;
    lastError_ = RtcDataChannelError::NotImplemented;
    return false;
}

std::optional<std::vector<std::byte>> UnsupportedRtcDataChannel::PollFrame() {
    return std::nullopt;
}

RtcDataChannelState UnsupportedRtcDataChannel::State() const noexcept {
    return RtcDataChannelState::Failed;
}

RtcDataChannelStats UnsupportedRtcDataChannel::Stats() const noexcept {
    return stats_;
}

RtcDataChannelError UnsupportedRtcDataChannel::LastError() const noexcept {
    return lastError_;
}

} // namespace game::net
