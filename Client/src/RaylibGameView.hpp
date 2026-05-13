#pragma once

#include "Client/ClientViewModel.hpp"

#include <raylib.h>

namespace game::client {

class RaylibGameView {
public:
    RaylibGameView(int width, int height, const char* title);
    ~RaylibGameView();

    RaylibGameView(const RaylibGameView&) = delete;
    RaylibGameView& operator=(const RaylibGameView&) = delete;

    [[nodiscard]] bool ShouldClose() const;
    [[nodiscard]] float FrameSeconds() const;
    [[nodiscard]] game::InputFrame SampleInput() const;
    void Render(const ClientViewFrame& frame) const;

private:
    int width_{};
    int height_{};
};

} // namespace game::client

