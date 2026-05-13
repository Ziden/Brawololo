#pragma once

#include "GameLogic/Types.hpp"

#include <cstdint>
#include <type_traits>

namespace game {

enum class CommandKind : std::uint8_t {
    Login,
    ClientInput,
};

struct CommandHeader {
    ClientId clientId{};
    CommandSequence sequence{};
    TimestampMs clientTimestampMs{};
};

struct LoginCommand {
    CommandHeader header{};
};

struct InputFrame {
    std::int8_t moveX{};
    std::int8_t moveY{};
    std::int16_t aimX{1000};
    std::int16_t aimY{};
    bool fire{};
};

struct ClientInputPacket {
    CommandHeader header{};
    InputFrame input{};
};

static_assert(std::is_trivially_copyable_v<CommandHeader>);
static_assert(std::is_trivially_copyable_v<LoginCommand>);
static_assert(std::is_trivially_copyable_v<InputFrame>);
static_assert(std::is_trivially_copyable_v<ClientInputPacket>);

} // namespace game

