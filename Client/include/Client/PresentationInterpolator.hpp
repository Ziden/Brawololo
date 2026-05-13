#pragma once

#include "GameLogic/Replication.hpp"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>

namespace game::client {

struct PresentationInterpolationConfig {
    game::TimestampMs interpolationDelayMs{100};
    std::size_t maxSnapshots{32};
};

struct InterpolatedEntityState {
    game::NetworkEntityId entityId{};
    game::ClientId ownerClientId{};
    game::ReplicatedEntityKind kind{game::ReplicatedEntityKind::Player};
    game::SnapshotPriority priority{game::SnapshotPriority::Medium};
    game::Fixed x{};
    game::Fixed y{};
    std::int16_t aimX{1000};
    std::int16_t aimY{};
    std::int32_t health{100};
    std::int32_t maxHealth{100};
    game::WeaponType weaponType{game::WeaponType::None};
    bool weaponWarming{};
    bool defeated{};
    bool serverOwnedProjectile{};
};

struct PresentationFrame {
    game::TimestampMs renderServerTimeMs{};
    game::SnapshotId fromSnapshotId{};
    game::SnapshotId toSnapshotId{};
    std::uint16_t alphaPermille{};
    std::vector<InterpolatedEntityState> entities{};
};

class PresentationInterpolator {
public:
    explicit PresentationInterpolator(PresentationInterpolationConfig config = {});

    void PushSnapshot(game::SnapshotDTO snapshot);
    [[nodiscard]] PresentationFrame Sample(game::TimestampMs estimatedServerNowMs) const;
    [[nodiscard]] std::size_t SnapshotCount() const noexcept;
    void Clear();

private:
    PresentationInterpolationConfig config_{};
    std::deque<game::SnapshotDTO> snapshots_{};
};

} // namespace game::client
