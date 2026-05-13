#include "GameLogic/Map.hpp"

#include <algorithm>
#include <cstdlib>

namespace game {

ChunkCoord ChunkForPosition(const MapConfig& config, Fixed x, Fixed y)
{
    const auto chunkSizePixels = config.tileSizePixels * config.chunkSizeTiles;
    const auto pixelX = std::clamp(FixedToPixels(x), 0, (config.widthTiles * config.tileSizePixels) - 1);
    const auto pixelY = std::clamp(FixedToPixels(y), 0, (config.heightTiles * config.tileSizePixels) - 1);
    return {pixelX / chunkSizePixels, pixelY / chunkSizePixels};
}

std::vector<ChunkCoord> ChunksInAreaOfInterest(const MapConfig& config, ChunkCoord center)
{
    std::vector<ChunkCoord> result{};
    const auto maxChunkX = (config.widthTiles + config.chunkSizeTiles - 1) / config.chunkSizeTiles;
    const auto maxChunkY = (config.heightTiles + config.chunkSizeTiles - 1) / config.chunkSizeTiles;

    for (auto y = center.y - config.interestRadiusChunks; y <= center.y + config.interestRadiusChunks; ++y) {
        for (auto x = center.x - config.interestRadiusChunks; x <= center.x + config.interestRadiusChunks; ++x) {
            if (x >= 0 && y >= 0 && x < maxChunkX && y < maxChunkY) {
                result.push_back({x, y});
            }
        }
    }

    return result;
}

bool IsChunkInAreaOfInterest(const MapConfig& config, ChunkCoord observer, ChunkCoord candidate)
{
    const auto dx = std::abs(observer.x - candidate.x);
    const auto dy = std::abs(observer.y - candidate.y);
    return dx <= config.interestRadiusChunks && dy <= config.interestRadiusChunks;
}

Fixed MapWidthFixed(const MapConfig& config)
{
    return PixelsToFixed(config.widthTiles * config.tileSizePixels);
}

Fixed MapHeightFixed(const MapConfig& config)
{
    return PixelsToFixed(config.heightTiles * config.tileSizePixels);
}

} // namespace game
