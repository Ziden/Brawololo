#pragma once

#include "Client/ClientApplication.hpp"
#include "Client/ClientEventLog.hpp"
#include "Client/ClientVisualEffectLog.hpp"
#include "Client/FixedStepClock.hpp"
#include "RaylibGameView.hpp"

namespace game::client {

struct RaylibClientHostConfig {
    int width{1280};
    int height{720};
    const char* title{"Raylib Multiplayer Scaffold"};
};

class RaylibClientHost {
public:
    explicit RaylibClientHost(RaylibClientHostConfig config = {});

    [[nodiscard]] int Run(ClientApplication& app);

private:
    [[nodiscard]] game::TimestampMs NowMs() const;

    RaylibClientHostConfig config_{};
    FixedStepClock fixedStep_{};
    ClientEventLog eventLog_{};
    ClientVisualEffectLog visualEffectLog_{};
};

} // namespace game::client
