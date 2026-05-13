#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>

namespace game {

using ClientId = std::uint32_t;
using CommandSequence = std::uint32_t;
using NetworkEventId = std::uint32_t;
using SnapshotId = std::uint32_t;
using Tick = std::uint64_t;
using TimestampMs = std::uint64_t;

struct NetworkEntityId {
    std::uint32_t value{};

    friend constexpr bool operator==(NetworkEntityId left, NetworkEntityId right) noexcept {
        return left.value == right.value;
    }

    friend constexpr bool operator!=(NetworkEntityId left, NetworkEntityId right) noexcept {
        return !(left == right);
    }
};

inline constexpr NetworkEntityId kInvalidNetworkEntity{0};

} // namespace game

template <> struct std::hash<game::NetworkEntityId> {
    std::size_t operator()(game::NetworkEntityId id) const noexcept {
        return std::hash<std::uint32_t>{}(id.value);
    }
};
