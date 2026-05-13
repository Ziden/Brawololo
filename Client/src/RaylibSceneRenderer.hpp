#pragma once

#include "Client/ClientViewModel.hpp"

namespace game::client {

struct RaylibSceneRenderConfig {
    int viewportWidth{1280};
    int viewportHeight{720};
    int tileSizePixels{128};
    int mapWidthTiles{80};
    int mapHeightTiles{80};
    float zoom{1.0F};
};

void DrawRaylibScene(const ClientViewFrame& frame, RaylibSceneRenderConfig config = {});

} // namespace game::client
