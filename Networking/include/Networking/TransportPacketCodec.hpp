#pragma once

#include "GameLogic/NetworkProtocol.hpp"

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace game::net {

enum class TransportPacketDecodeError {
    None,
    UnexpectedEof,
    UnsupportedVersion,
    InvalidChannel,
    InvalidMessageClass,
    PayloadTooLarge,
    ChannelMismatch,
    TrailingBytes,
};

struct TransportPacketDecodeResult {
    std::optional<game::NetworkEnvelope> envelope{};
    TransportPacketDecodeError error{TransportPacketDecodeError::None};

    [[nodiscard]] bool Ok() const noexcept {
        return envelope.has_value() && error == TransportPacketDecodeError::None;
    }
};

[[nodiscard]] std::vector<std::byte> EncodeTransportPacket(
    const game::NetworkEnvelope& envelope);
[[nodiscard]] TransportPacketDecodeResult DecodeTransportPacket(
    std::span<const std::byte> bytes);

} // namespace game::net
