#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace game {

inline constexpr std::uint16_t kProtocolVersion = 1;
inline constexpr std::size_t kMaxNetworkPayloadBytes = 64U * 1024U;

enum class NetworkChannel : std::uint8_t {
    MovementInput,
    Snapshots,
    CombatEvents,
    LoginSpawn,
    ChatUi,
    TimeSync,
};

enum class Reliability : std::uint8_t {
    Unreliable,
    UnreliableSequenced,
    Reliable,
};

enum class MessageClass : std::uint8_t {
    ClientInput,
    Snapshot,
    SnapshotAck,
    CombatEvent,
    LoginRequest,
    SpawnAccepted,
    InterestEvent,
    NetworkEventAck,
    ChatUi,
    TimeSyncRequest,
    TimeSyncResponse,
};

struct ChannelPolicy {
    NetworkChannel channel{};
    Reliability reliability{};
};

enum class ProtocolError : std::uint8_t {
    None,
    UnsupportedVersion,
    PayloadTooLarge,
    ChannelMismatch,
};

struct ProtocolValidationResult {
    ProtocolError error{ProtocolError::None};

    [[nodiscard]] constexpr bool Ok() const noexcept
    {
        return error == ProtocolError::None;
    }
};

constexpr ChannelPolicy PolicyFor(NetworkChannel channel) noexcept
{
    switch (channel) {
    case NetworkChannel::MovementInput:
        return {channel, Reliability::Unreliable};
    case NetworkChannel::Snapshots:
        return {channel, Reliability::UnreliableSequenced};
    case NetworkChannel::TimeSync:
        return {channel, Reliability::Unreliable};
    case NetworkChannel::CombatEvents:
    case NetworkChannel::LoginSpawn:
    case NetworkChannel::ChatUi:
        return {channel, Reliability::Reliable};
    }

    return {channel, Reliability::Reliable};
}

constexpr NetworkChannel ExpectedChannelFor(MessageClass messageClass) noexcept
{
    switch (messageClass) {
    case MessageClass::ClientInput:
        return NetworkChannel::MovementInput;
    case MessageClass::Snapshot:
    case MessageClass::SnapshotAck:
        return NetworkChannel::Snapshots;
    case MessageClass::CombatEvent:
        return NetworkChannel::CombatEvents;
    case MessageClass::LoginRequest:
    case MessageClass::SpawnAccepted:
    case MessageClass::InterestEvent:
    case MessageClass::NetworkEventAck:
        return NetworkChannel::LoginSpawn;
    case MessageClass::ChatUi:
        return NetworkChannel::ChatUi;
    case MessageClass::TimeSyncRequest:
    case MessageClass::TimeSyncResponse:
        return NetworkChannel::TimeSync;
    }

    return NetworkChannel::ChatUi;
}

struct NetworkEnvelope {
    std::uint16_t protocolVersion{kProtocolVersion};
    NetworkChannel channel{};
    MessageClass messageClass{};
    std::uint32_t sequence{};
    std::vector<std::byte> payload{};
};

inline ProtocolValidationResult ValidateEnvelope(const NetworkEnvelope& envelope) noexcept
{
    if (envelope.protocolVersion != kProtocolVersion) {
        return {ProtocolError::UnsupportedVersion};
    }

    if (envelope.payload.size() > kMaxNetworkPayloadBytes) {
        return {ProtocolError::PayloadTooLarge};
    }

    if (envelope.channel != ExpectedChannelFor(envelope.messageClass)) {
        return {ProtocolError::ChannelMismatch};
    }

    return {};
}

} // namespace game
