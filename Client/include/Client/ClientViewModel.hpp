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

enum class ViewEffectKind : std::uint8_t {
    PredictedFire,
    AuthoritativeHit,
    AuthoritativeDeath,
    AuthoritativeRespawn,
    PredictionCorrection,
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
    std::int32_t maxHealth{100};
    game::WeaponType weaponType{game::WeaponType::None};
    bool weaponWarming{};
    bool defeated{};
};

struct ViewEffect {
    ViewEffectKind kind{ViewEffectKind::PredictedFire};
    game::NetworkEntityId entityId{};
    game::Fixed x{};
    game::Fixed y{};
    std::int32_t amount{};
    std::uint16_t progressPermille{};
};

struct ClientViewFrame {
    game::ClientId localClientId{};
    ClientApplicationStats stats{};
    PresentationFrame presentation{};
    std::vector<ViewEntity> entities{};
    std::vector<ViewEffect> effects{};
    std::vector<std::string> eventLines{};
};

[[nodiscard]] ClientViewFrame BuildClientViewFrame(const ClientRuntime& runtime,
                                                   const game::EventList& events,
                                                   const ClientApplicationStats& stats);

} // namespace game::client
