#include "RaylibSceneRenderer.hpp"

#include <raylib.h>

#include <algorithm>
#include <cmath>

namespace game::client {
namespace {

float ToPixels(game::Fixed value)
{
    return static_cast<float>(value) / static_cast<float>(game::kFixedOne);
}

Vector2 PositionFor(const ViewEntity& entity)
{
    return {ToPixels(entity.x), ToPixels(entity.y)};
}

Color ColorForRole(ViewAuthorityRole role)
{
    switch (role) {
    case ViewAuthorityRole::Predicted:
        return Color{83, 209, 255, 255};
    case ViewAuthorityRole::Authoritative:
        return Color{255, 132, 85, 255};
    case ViewAuthorityRole::Interpolated:
        return Color{185, 205, 156, 255};
    }

    return RAYWHITE;
}

Vector2 CameraTargetFor(const ClientViewFrame& frame, const RaylibSceneRenderConfig& config)
{
    const auto localPlayer = std::find_if(
        frame.entities.begin(),
        frame.entities.end(),
        [&frame](const ViewEntity& entity) {
            return entity.kind == ViewEntityKind::Player && entity.ownerClientId == frame.localClientId;
        });

    if (localPlayer != frame.entities.end()) {
        return PositionFor(*localPlayer);
    }

    const auto firstPlayer = std::find_if(
        frame.entities.begin(),
        frame.entities.end(),
        [](const ViewEntity& entity) {
            return entity.kind == ViewEntityKind::Player;
        });

    if (firstPlayer != frame.entities.end()) {
        return PositionFor(*firstPlayer);
    }

    return {
        static_cast<float>(config.mapWidthTiles * config.tileSizePixels) * 0.5F,
        static_cast<float>(config.mapHeightTiles * config.tileSizePixels) * 0.5F};
}

void DrawTileMap(const Camera2D& camera, const RaylibSceneRenderConfig& config)
{
    const auto worldWidth = config.mapWidthTiles * config.tileSizePixels;
    const auto worldHeight = config.mapHeightTiles * config.tileSizePixels;
    const auto halfWidth = (static_cast<float>(config.viewportWidth) * 0.5F) / camera.zoom;
    const auto halfHeight = (static_cast<float>(config.viewportHeight) * 0.5F) / camera.zoom;

    const auto minWorldX = camera.target.x - halfWidth - static_cast<float>(config.tileSizePixels);
    const auto minWorldY = camera.target.y - halfHeight - static_cast<float>(config.tileSizePixels);
    const auto maxWorldX = camera.target.x + halfWidth + static_cast<float>(config.tileSizePixels);
    const auto maxWorldY = camera.target.y + halfHeight + static_cast<float>(config.tileSizePixels);

    const auto startTileX = std::clamp(
        static_cast<int>(std::floor(minWorldX / static_cast<float>(config.tileSizePixels))),
        0,
        config.mapWidthTiles - 1);
    const auto startTileY = std::clamp(
        static_cast<int>(std::floor(minWorldY / static_cast<float>(config.tileSizePixels))),
        0,
        config.mapHeightTiles - 1);
    const auto endTileX = std::clamp(
        static_cast<int>(std::ceil(maxWorldX / static_cast<float>(config.tileSizePixels))),
        0,
        config.mapWidthTiles - 1);
    const auto endTileY = std::clamp(
        static_cast<int>(std::ceil(maxWorldY / static_cast<float>(config.tileSizePixels))),
        0,
        config.mapHeightTiles - 1);

    for (auto tileY = startTileY; tileY <= endTileY; ++tileY) {
        for (auto tileX = startTileX; tileX <= endTileX; ++tileX) {
            const auto x = tileX * config.tileSizePixels;
            const auto y = tileY * config.tileSizePixels;
            const auto fill = ((tileX + tileY) % 2) == 0
                ? Color{42, 53, 45, 255}
                : Color{37, 47, 41, 255};
            DrawRectangle(x, y, config.tileSizePixels, config.tileSizePixels, fill);
            DrawRectangleLinesEx(
                Rectangle{
                    static_cast<float>(x),
                    static_cast<float>(y),
                    static_cast<float>(config.tileSizePixels),
                    static_cast<float>(config.tileSizePixels)},
                1.0F,
                Color{57, 68, 58, 135});
        }
    }

    DrawRectangleLinesEx(
        Rectangle{0.0F, 0.0F, static_cast<float>(worldWidth), static_cast<float>(worldHeight)},
        4.0F,
        Color{96, 118, 86, 255});
}

void DrawHealthBar(const ViewEntity& entity, Vector2 position)
{
    if (entity.maxHealth <= 0 || entity.kind != ViewEntityKind::Player) {
        return;
    }

    constexpr auto barWidth = 58.0F;
    constexpr auto barHeight = 7.0F;
    const auto healthRatio = std::clamp(
        static_cast<float>(entity.health) / static_cast<float>(entity.maxHealth),
        0.0F,
        1.0F);
    const auto x = position.x - (barWidth * 0.5F);
    const auto y = position.y - 48.0F;
    DrawRectangleRounded(Rectangle{x, y, barWidth, barHeight}, 0.45F, 4, Color{15, 19, 21, 220});
    DrawRectangleRounded(
        Rectangle{x, y, barWidth * healthRatio, barHeight},
        0.45F,
        4,
        healthRatio > 0.5F ? Color{109, 220, 128, 255} : Color{255, 159, 83, 255});
}

void DrawPlayer(const ViewEntity& entity, Vector2 position)
{
    auto color = ColorForRole(entity.role);
    if (entity.defeated) {
        color = Color{93, 98, 103, 190};
    }

    DrawCircleV(position, 24.0F, Color{12, 16, 18, 155});
    DrawCircleV(position, 20.0F, color);

    if (entity.weaponWarming && !entity.defeated) {
        DrawCircleLines(static_cast<int>(position.x), static_cast<int>(position.y), 28.0F, Color{255, 212, 101, 255});
    }

    if (!entity.defeated) {
        DrawLineEx(
            position,
            {
                position.x + (static_cast<float>(entity.aimX) * 0.055F),
                position.y + (static_cast<float>(entity.aimY) * 0.055F)},
            4.0F,
            RAYWHITE);
    } else {
        DrawLineEx({position.x - 12.0F, position.y - 12.0F}, {position.x + 12.0F, position.y + 12.0F}, 4.0F, RAYWHITE);
        DrawLineEx({position.x + 12.0F, position.y - 12.0F}, {position.x - 12.0F, position.y + 12.0F}, 4.0F, RAYWHITE);
    }

    DrawHealthBar(entity, position);
    DrawText(TextFormat("P%u", entity.ownerClientId), static_cast<int>(position.x - 17.0F), static_cast<int>(position.y - 72.0F), 18, RAYWHITE);
}

} // namespace

void DrawRaylibScene(const ClientViewFrame& frame, RaylibSceneRenderConfig config)
{
    const auto target = CameraTargetFor(frame, config);
    const Camera2D camera{
        {static_cast<float>(config.viewportWidth) * 0.5F, static_cast<float>(config.viewportHeight) * 0.5F},
        target,
        0.0F,
        config.zoom};

    BeginMode2D(camera);
    DrawTileMap(camera, config);

    for (const auto& entity : frame.entities) {
        const auto position = PositionFor(entity);

        if (entity.kind == ViewEntityKind::Projectile) {
            DrawCircleV(position, 7.0F, Color{255, 208, 85, 255});
            DrawCircleLines(static_cast<int>(position.x), static_cast<int>(position.y), 10.0F, Color{92, 60, 24, 255});
            continue;
        }

        DrawPlayer(entity, position);
    }

    EndMode2D();
}

} // namespace game::client
