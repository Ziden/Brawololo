#pragma once

#include "GameLogic/FixedPoint.hpp"

#include <cstdint>
#include <vector>

namespace game {

struct MapConfig {
    std::int32_t tileSizePixels{128};
    std::int32_t widthTiles{80};
    std::int32_t heightTiles{80};
    std::int32_t chunkSizeTiles{8};
    std::int32_t interestRadiusChunks{1};
};

struct ChunkCoord {
    std::int32_t x{};
    std::int32_t y{};

    friend constexpr bool operator==(ChunkCoord left, ChunkCoord right) noexcept
    {
        return left.x == right.x && left.y == right.y;
    }
};

ChunkCoord ChunkForPosition(const MapConfig& config, Fixed x, Fixed y);
std::vector<ChunkCoord> ChunksInAreaOfInterest(const MapConfig& config, ChunkCoord center);
bool IsChunkInAreaOfInterest(const MapConfig& config, ChunkCoord observer, ChunkCoord candidate);
Fixed MapWidthFixed(const MapConfig& config);
Fixed MapHeightFixed(const MapConfig& config);

} // namespace game

