#include "Networking/TransportPacketCodec.hpp"

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

class PacketReader {
public:
    explicit PacketReader(std::span<const std::byte> bytes) : bytes_(bytes) {}

    template <typename T> std::optional<T> ReadPod() {
        static_assert(std::is_trivially_copyable_v<T>);
        if (offset_ + sizeof(T) > bytes_.size()) {
            error_ = TransportPacketDecodeError::UnexpectedEof;
            return std::nullopt;
        }

        T value{};
        const auto* source = bytes_.data() + offset_;
        auto* destination = reinterpret_cast<std::byte*>(&value);
        std::copy(source, source + sizeof(T), destination);
        offset_ += sizeof(T);
        return value;
    }

    [[nodiscard]] std::optional<std::span<const std::byte>> ReadBytes(std::size_t size) {
        if (offset_ + size > bytes_.size()) {
            error_ = TransportPacketDecodeError::UnexpectedEof;
            return std::nullopt;
        }

        const auto result = bytes_.subspan(offset_, size);
        offset_ += size;
        return result;
    }

    [[nodiscard]] bool Finished() noexcept {
        if (error_ != TransportPacketDecodeError::None) {
            return false;
        }

        if (offset_ != bytes_.size()) {
            error_ = TransportPacketDecodeError::TrailingBytes;
            return false;
        }

        return true;
    }

    [[nodiscard]] TransportPacketDecodeError Error() const noexcept {
        return error_;
    }

private:
    std::span<const std::byte> bytes_{};
    std::size_t offset_{};
    TransportPacketDecodeError error_{TransportPacketDecodeError::None};
};

[[nodiscard]] bool IsValidChannel(std::uint8_t value) noexcept {
    return value <= static_cast<std::uint8_t>(game::NetworkChannel::TimeSync);
}

[[nodiscard]] bool IsValidMessageClass(std::uint8_t value) noexcept {
    return value <= static_cast<std::uint8_t>(game::MessageClass::TimeSyncResponse);
}

[[nodiscard]] TransportPacketDecodeError MapValidationError(game::ProtocolError error) noexcept {
    switch (error) {
        case game::ProtocolError::None:
            return TransportPacketDecodeError::None;
        case game::ProtocolError::UnsupportedVersion:
            return TransportPacketDecodeError::UnsupportedVersion;
        case game::ProtocolError::PayloadTooLarge:
            return TransportPacketDecodeError::PayloadTooLarge;
        case game::ProtocolError::ChannelMismatch:
            return TransportPacketDecodeError::ChannelMismatch;
    }

    return TransportPacketDecodeError::ChannelMismatch;
}

} // namespace

std::vector<std::byte> EncodeTransportPacket(const game::NetworkEnvelope& envelope) {
    std::vector<std::byte> bytes{};
    bytes.reserve(sizeof(envelope.protocolVersion) + sizeof(envelope.peerId) +
                  sizeof(std::uint8_t) + sizeof(std::uint8_t) + sizeof(envelope.sequence) +
                  sizeof(std::uint32_t) + envelope.payload.size());

    WritePod(bytes, envelope.protocolVersion);
    WritePod(bytes, envelope.peerId);
    WritePod(bytes, static_cast<std::uint8_t>(envelope.channel));
    WritePod(bytes, static_cast<std::uint8_t>(envelope.messageClass));
    WritePod(bytes, envelope.sequence);
    WritePod(bytes, static_cast<std::uint32_t>(envelope.payload.size()));
    bytes.insert(bytes.end(), envelope.payload.begin(), envelope.payload.end());
    return bytes;
}

TransportPacketDecodeResult DecodeTransportPacket(std::span<const std::byte> bytes) {
    PacketReader reader{bytes};
    game::NetworkEnvelope envelope{};

    const auto version = reader.ReadPod<std::uint16_t>();
    const auto peerId = reader.ReadPod<game::ClientId>();
    const auto channel = reader.ReadPod<std::uint8_t>();
    const auto messageClass = reader.ReadPod<std::uint8_t>();
    const auto sequence = reader.ReadPod<std::uint32_t>();
    const auto payloadSize = reader.ReadPod<std::uint32_t>();

    if (!version.has_value() || !peerId.has_value() || !channel.has_value() ||
        !messageClass.has_value() || !sequence.has_value() || !payloadSize.has_value()) {
        return {std::nullopt, reader.Error()};
    }

    if (!IsValidChannel(*channel)) {
        return {std::nullopt, TransportPacketDecodeError::InvalidChannel};
    }

    if (!IsValidMessageClass(*messageClass)) {
        return {std::nullopt, TransportPacketDecodeError::InvalidMessageClass};
    }

    if (*payloadSize > game::kMaxNetworkPayloadBytes) {
        return {std::nullopt, TransportPacketDecodeError::PayloadTooLarge};
    }

    const auto payload = reader.ReadBytes(*payloadSize);
    if (!payload.has_value()) {
        return {std::nullopt, reader.Error()};
    }

    if (!reader.Finished()) {
        return {std::nullopt, reader.Error()};
    }

    envelope.protocolVersion = *version;
    envelope.peerId = *peerId;
    envelope.channel = static_cast<game::NetworkChannel>(*channel);
    envelope.messageClass = static_cast<game::MessageClass>(*messageClass);
    envelope.sequence = *sequence;
    envelope.payload.assign(payload->begin(), payload->end());

    const auto validation = game::ValidateEnvelope(envelope);
    if (!validation.Ok()) {
        return {std::nullopt, MapValidationError(validation.error)};
    }

    return {std::move(envelope), TransportPacketDecodeError::None};
}

} // namespace game::net
