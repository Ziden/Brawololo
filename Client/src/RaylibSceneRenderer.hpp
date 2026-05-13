#pragma once

#include "Client/ClientViewModel.hpp"

namespace game::client {

struct RaylibSceneRenderConfig {
    float originX{460.0F};
    float originY{260.0F};
    float scale{0.08F};
};

void DrawRaylibScene(const ClientViewFrame& frame, RaylibSceneRenderConfig config = {});

} // namespace game::client
