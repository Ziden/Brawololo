#pragma once

#include "Client/ClientApplication.hpp"
#include "Client/PresentationInterpolator.hpp"
#include "GameLogic/FixedPoint.hpp"
#include "GameLogic/Types.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace game::client {

enum class ViewEntityKind : std::uint8_t {
    Player,
    Projectile,
};

enum class ViewAuthorityRole : std::uint8_t {
    Predicted,
    Interpolated,
    Authoritative,
};

struct ViewEntity {
    game::NetworkEntityId entityId{};
    game::ClientId ownerClientId{};
    ViewEntityKind kind{ViewEntityKind::Player};
    ViewAuthorityRole role{ViewAuthorityRole::Interpolated};
    game::Fixed x{};
    game::Fixed y{};
    std::int16_t aimX{1000};
    std::int16_t aimY{};
    std::int32_t health{100};
    game::WeaponType weaponType{game::WeaponType::None};
    bool weaponWarming{};
};

struct ClientViewFrame {
    game::ClientId localClientId{};
    ClientApplicationStats stats{};
    PresentationFrame presentation{};
    std::vector<ViewEntity> entities{};
    std::vector<std::string> eventLines{};
};

[[nodiscard]] ClientViewFrame BuildClientViewFrame(
    const ClientRuntime& runtime,
    const game::EventList& events,
    const ClientApplicationStats& stats);

} // namespace game::client
