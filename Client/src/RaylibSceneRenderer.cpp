#include "RaylibSceneRenderer.hpp"

#include <raylib.h>

namespace game::client {
namespace {

float ToPixels(game::Fixed value)
{
    return static_cast<float>(game::FixedToPixels(value));
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

} // namespace

void DrawRaylibScene(const ClientViewFrame& frame, RaylibSceneRenderConfig config)
{
    for (const auto& entity : frame.entities) {
        const auto x = config.originX + (ToPixels(entity.x) * config.scale);
        const auto y = config.originY + (ToPixels(entity.y) * config.scale);
        const auto color = ColorForRole(entity.role);

        if (entity.kind == ViewEntityKind::Projectile) {
            DrawCircleV({x, y}, 5.0F, Color{255, 208, 85, 255});
            continue;
        }

        DrawCircleV({x, y}, 18.0F, color);
        DrawLineEx(
            {x, y},
            {x + (static_cast<float>(entity.aimX) * 0.04F), y + (static_cast<float>(entity.aimY) * 0.04F)},
            3.0F,
            RAYWHITE);
        DrawText(TextFormat("P%u", entity.ownerClientId), static_cast<int>(x - 16.0F), static_cast<int>(y - 38.0F), 16, RAYWHITE);
    }
}

} // namespace game::client
