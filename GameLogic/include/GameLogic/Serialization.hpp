#pragma once

#include "GameLogic/Commands.hpp"
#include "GameLogic/NetworkEvents.hpp"
#include "GameLogic/Replication.hpp"
#include "GameLogic/TimeSync.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <type_traits>
#include <vector>

namespace game {

enum class DecodeError : std::uint8_t {
    None,
    UnexpectedEof,
    InvalidBoolean,
    InvalidEnum,
    InvalidCount,
    TrailingBytes,
};

class BinaryWriter {
public:
    template <typename T>
    void WritePod(const T& value)
    {
        static_assert(std::is_trivially_copyable_v<T>);
        const auto* bytes = reinterpret_cast<const std::byte*>(&value);
        bytes_.insert(bytes_.end(), bytes, bytes + sizeof(T));
    }

    [[nodiscard]] const std::vector<std::byte>& Bytes() const noexcept;

private:
    std::vector<std::byte> bytes_{};
};

class BinaryReader {
public:
    explicit BinaryReader(std::span<const std::byte> bytes);

    [[nodiscard]] std::size_t RemainingBytes() const noexcept
    {
        return bytes_.size() - offset_;
    }

    [[nodiscard]] DecodeError Error() const noexcept
    {
        return error_;
    }

    [[nodiscard]] bool Finish() noexcept
    {
        if (error_ != DecodeError::None) {
            return false;
        }

        if (offset_ != bytes_.size()) {
            error_ = DecodeError::TrailingBytes;
            return false;
        }

        return true;
    }

    template <typename T>
    std::optional<T> ReadPod()
    {
        static_assert(std::is_trivially_copyable_v<T>);
        if (offset_ + sizeof(T) > bytes_.size()) {
            Fail(DecodeError::UnexpectedEof);
            return std::nullopt;
        }

        T value{};
        const auto* source = bytes_.data() + offset_;
        auto* destination = reinterpret_cast<std::byte*>(&value);
        std::copy(source, source + sizeof(T), destination);
        offset_ += sizeof(T);
        return value;
    }

    template <typename Enum>
    std::optional<Enum> ReadEnum(std::underlying_type_t<Enum> maxValue)
    {
        static_assert(std::is_enum_v<Enum>);
        using Underlying = std::underlying_type_t<Enum>;

        const auto value = ReadPod<Underlying>();
        if (!value.has_value()) {
            return std::nullopt;
        }

        if (*value > maxValue) {
            Fail(DecodeError::InvalidEnum);
            return std::nullopt;
        }

        return static_cast<Enum>(*value);
    }

    [[nodiscard]] std::optional<bool> ReadBool()
    {
        const auto value = ReadPod<std::uint8_t>();
        if (!value.has_value()) {
            return std::nullopt;
        }

        if (*value > 1U) {
            Fail(DecodeError::InvalidBoolean);
            return std::nullopt;
        }

        return *value != 0;
    }

    [[nodiscard]] std::optional<std::uint32_t> ReadCount(std::size_t maxValue)
    {
        const auto value = ReadPod<std::uint32_t>();
        if (!value.has_value()) {
            return std::nullopt;
        }

        if (static_cast<std::size_t>(*value) > maxValue) {
            Fail(DecodeError::InvalidCount);
            return std::nullopt;
        }

        return value;
    }

private:
    void Fail(DecodeError error) noexcept
    {
        if (error_ == DecodeError::None) {
            error_ = error;
        }
    }

    std::span<const std::byte> bytes_{};
    std::size_t offset_{};
    DecodeError error_{DecodeError::None};
};

std::vector<std::byte> SerializeMovementState(const MovementStateDTO& dto);
std::optional<MovementStateDTO> DeserializeMovementState(std::span<const std::byte> bytes);
std::vector<std::byte> SerializeLoginCommand(const LoginCommand& command);
std::optional<LoginCommand> DeserializeLoginCommand(std::span<const std::byte> bytes);
std::vector<std::byte> SerializeClientInputPacket(const ClientInputPacket& packet);
std::optional<ClientInputPacket> DeserializeClientInputPacket(std::span<const std::byte> bytes);
std::vector<std::byte> SerializeTimeSyncRequest(const TimeSyncRequest& request);
std::optional<TimeSyncRequest> DeserializeTimeSyncRequest(std::span<const std::byte> bytes);
std::vector<std::byte> SerializeTimeSyncResponse(const TimeSyncResponse& response);
std::optional<TimeSyncResponse> DeserializeTimeSyncResponse(std::span<const std::byte> bytes);
std::vector<std::byte> SerializeNetworkEvent(const NetworkEventDTO& event);
std::optional<NetworkEventDTO> DeserializeNetworkEvent(std::span<const std::byte> bytes);
std::vector<std::byte> SerializeNetworkEventAck(const NetworkEventAckDTO& ack);
std::optional<NetworkEventAckDTO> DeserializeNetworkEventAck(std::span<const std::byte> bytes);
std::vector<std::byte> SerializeSnapshotAck(const SnapshotAckDTO& ack);
std::optional<SnapshotAckDTO> DeserializeSnapshotAck(std::span<const std::byte> bytes);
std::vector<std::byte> SerializeSnapshot(const SnapshotDTO& snapshot);
std::optional<SnapshotDTO> DeserializeSnapshot(std::span<const std::byte> bytes);

} // namespace game
