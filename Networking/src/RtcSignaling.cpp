#include "Networking/RtcSignaling.hpp"

#include <algorithm>
#include <cstdint>
#include <type_traits>

namespace game::net {
namespace {

template <typename T> void WritePod(std::vector<std::byte>& bytes, const T& value) {
    static_assert(std::is_trivially_copyable_v<T>);
    const auto* source = reinterpret_cast<const std::byte*>(&value);
    bytes.insert(bytes.end(), source, source + sizeof(T));
}

void WriteString(std::vector<std::byte>& bytes, const std::string& value) {
    WritePod(bytes, static_cast<std::uint32_t>(value.size()));
    const auto* source = reinterpret_cast<const std::byte*>(value.data());
    bytes.insert(bytes.end(), source, source + value.size());
}

class SignalReader {
public:
    explicit SignalReader(std::span<const std::byte> bytes) : bytes_(bytes) {}

    template <typename T> std::optional<T> ReadPod() {
        static_assert(std::is_trivially_copyable_v<T>);
        if (offset_ + sizeof(T) > bytes_.size()) {
            Fail(RtcSignalingDecodeError::UnexpectedEof);
            return std::nullopt;
        }

        T value{};
        const auto* source = bytes_.data() + offset_;
        auto* destination = reinterpret_cast<std::byte*>(&value);
        std::copy(source, source + sizeof(T), destination);
        offset_ += sizeof(T);
        return value;
    }

    [[nodiscard]] std::optional<std::string> ReadString(std::size_t maxBytes) {
        const auto size = ReadPod<std::uint32_t>();
        if (!size.has_value()) {
            return std::nullopt;
        }

        if (*size > maxBytes || offset_ + *size > bytes_.size()) {
            Fail(RtcSignalingDecodeError::InvalidCount);
            return std::nullopt;
        }

        std::string value{};
        value.resize(*size);
        auto* destination = reinterpret_cast<std::byte*>(value.data());
        std::copy(bytes_.data() + offset_, bytes_.data() + offset_ + *size, destination);
        offset_ += *size;
        return value;
    }

    [[nodiscard]] bool Finished() noexcept {
        if (error_ != RtcSignalingDecodeError::None) {
            return false;
        }

        if (offset_ != bytes_.size()) {
            Fail(RtcSignalingDecodeError::TrailingBytes);
            return false;
        }

        return true;
    }

    [[nodiscard]] RtcSignalingDecodeError Error() const noexcept {
        return error_;
    }

private:
    void Fail(RtcSignalingDecodeError error) noexcept {
        if (error_ == RtcSignalingDecodeError::None) {
            error_ = error;
        }
    }

    std::span<const std::byte> bytes_{};
    std::size_t offset_{};
    RtcSignalingDecodeError error_{RtcSignalingDecodeError::None};
};

[[nodiscard]] bool IsValidKind(std::uint8_t value) noexcept {
    return value <= static_cast<std::uint8_t>(RtcSignalingMessageKind::Error);
}

} // namespace

RtcSignalingValidationResult ValidateRtcSignalingMessage(
    const RtcSignalingMessage& message) noexcept {
    if (message.senderPeerId == 0) {
        return {RtcSignalingValidationError::MissingSender};
    }

    if (message.sessionId.empty()) {
        return {RtcSignalingValidationError::MissingSession};
    }

    if (message.payload.size() > kMaxRtcSignalingPayloadBytes) {
        return {RtcSignalingValidationError::PayloadTooLarge};
    }

    return {};
}

std::vector<std::byte> SerializeRtcSignalingMessage(const RtcSignalingMessage& message) {
    std::vector<std::byte> bytes{};
    bytes.reserve(sizeof(std::uint8_t) + sizeof(message.senderPeerId) +
                  sizeof(message.targetPeerId) + sizeof(message.sequence) +
                  sizeof(std::uint32_t) + message.sessionId.size() + sizeof(std::uint32_t) +
                  message.payload.size());

    WritePod(bytes, static_cast<std::uint8_t>(message.kind));
    WritePod(bytes, message.senderPeerId);
    WritePod(bytes, message.targetPeerId);
    WritePod(bytes, message.sequence);
    WriteString(bytes, message.sessionId);
    WriteString(bytes, message.payload);
    return bytes;
}

RtcSignalingDecodeResult DeserializeRtcSignalingMessage(std::span<const std::byte> bytes) {
    SignalReader reader{bytes};
    RtcSignalingMessage message{};

    const auto kind = reader.ReadPod<std::uint8_t>();
    const auto sender = reader.ReadPod<game::ClientId>();
    const auto target = reader.ReadPod<game::ClientId>();
    const auto sequence = reader.ReadPod<std::uint32_t>();
    const auto sessionId = reader.ReadString(256U);
    const auto payload = reader.ReadString(kMaxRtcSignalingPayloadBytes);

    if (!kind.has_value() || !sender.has_value() || !target.has_value() ||
        !sequence.has_value() || !sessionId.has_value() || !payload.has_value()) {
        return {std::nullopt, reader.Error()};
    }

    if (!IsValidKind(*kind)) {
        return {std::nullopt, RtcSignalingDecodeError::InvalidKind};
    }

    if (!reader.Finished()) {
        return {std::nullopt, reader.Error()};
    }

    message.kind = static_cast<RtcSignalingMessageKind>(*kind);
    message.senderPeerId = *sender;
    message.targetPeerId = *target;
    message.sequence = *sequence;
    message.sessionId = *sessionId;
    message.payload = *payload;

    if (!ValidateRtcSignalingMessage(message).Ok()) {
        return {std::nullopt, RtcSignalingDecodeError::ValidationFailed};
    }

    return {std::move(message), RtcSignalingDecodeError::None};
}

} // namespace game::net
