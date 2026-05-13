#include "RaylibGameView.hpp"

#include "RaylibDebugOverlay.hpp"
#include "RaylibSceneRenderer.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace game::client {
namespace {

std::int16_t ClampAim(float value) {
    return static_cast<std::int16_t>(std::clamp(value, -1000.0F, 1000.0F));
}

} // namespace

RaylibGameView::RaylibGameView(int width, int height, const char* title)
    : width_(width), height_(height) {
    InitWindow(width_, height_, title);
    SetTargetFPS(144);
}

RaylibGameView::~RaylibGameView() {
    CloseWindow();
}

bool RaylibGameView::ShouldClose() const {
    return WindowShouldClose();
}

float RaylibGameView::FrameSeconds() const {
    return GetFrameTime();
}

game::InputFrame RaylibGameView::SampleInput() const {
    game::InputFrame input{};
    input.moveX = static_cast<std::int8_t>((IsKeyDown(KEY_D) ? 1 : 0) - (IsKeyDown(KEY_A) ? 1 : 0));
    input.moveY = static_cast<std::int8_t>((IsKeyDown(KEY_S) ? 1 : 0) - (IsKeyDown(KEY_W) ? 1 : 0));
    input.fire = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

    const auto mouse = GetMousePosition();
    const auto dx = mouse.x - (static_cast<float>(width_) * 0.5F);
    const auto dy = mouse.y - (static_cast<float>(height_) * 0.5F);
    const auto length = std::sqrt((dx * dx) + (dy * dy));
    if (length > 0.001F) {
        input.aimX = ClampAim((dx / length) * 1000.0F);
        input.aimY = ClampAim((dy / length) * 1000.0F);
    }

    return input;
}

void RaylibGameView::Render(const ClientViewFrame& frame) const {
    BeginDrawing();
    ClearBackground(Color{24, 28, 33, 255});

    RaylibSceneRenderConfig sceneConfig{};
    sceneConfig.viewportWidth = width_;
    sceneConfig.viewportHeight = height_;
    DrawRaylibScene(frame, sceneConfig);
    DrawRaylibDebugOverlay(frame, height_);

    EndDrawing();
}

} // namespace game::client
