#pragma once

#include "GameLogic/Types.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace game::net {

inline constexpr std::size_t kMaxRtcSignalingPayloadBytes = 32U * 1024U;

enum class RtcSignalingMessageKind : std::uint8_t {
    Join,
    Offer,
    Answer,
    IceCandidate,
    DataChannelReady,
    Leave,
    Error,
};

struct RtcSignalingMessage {
    RtcSignalingMessageKind kind{RtcSignalingMessageKind::Join};
    game::ClientId senderPeerId{};
    game::ClientId targetPeerId{};
    std::uint32_t sequence{};
    std::string sessionId{};
    std::string payload{};
};

enum class RtcSignalingValidationError : std::uint8_t {
    None,
    MissingSender,
    MissingSession,
    PayloadTooLarge,
};

enum class RtcSignalingDecodeError : std::uint8_t {
    None,
    UnexpectedEof,
    InvalidKind,
    InvalidCount,
    ValidationFailed,
    TrailingBytes,
};

struct RtcSignalingValidationResult {
    RtcSignalingValidationError error{RtcSignalingValidationError::None};

    [[nodiscard]] bool Ok() const noexcept {
        return error == RtcSignalingValidationError::None;
    }
};

struct RtcSignalingDecodeResult {
    std::optional<RtcSignalingMessage> message{};
    RtcSignalingDecodeError error{RtcSignalingDecodeError::None};

    [[nodiscard]] bool Ok() const noexcept {
        return message.has_value() && error == RtcSignalingDecodeError::None;
    }
};

[[nodiscard]] RtcSignalingValidationResult ValidateRtcSignalingMessage(
    const RtcSignalingMessage& message) noexcept;
[[nodiscard]] std::vector<std::byte> SerializeRtcSignalingMessage(
    const RtcSignalingMessage& message);
[[nodiscard]] RtcSignalingDecodeResult DeserializeRtcSignalingMessage(
    std::span<const std::byte> bytes);

} // namespace game::net
