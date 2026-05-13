#include "RaylibClientHost.hpp"

#include "Client/ClientViewModel.hpp"

#include <chrono>

namespace game::client {

RaylibClientHost::RaylibClientHost(RaylibClientHostConfig config) : config_(config) {}

int RaylibClientHost::Run(ClientApplication& app) {
    if (!app.Connect(NowMs())) {
        return 1;
    }

    RaylibGameView view{config_.width, config_.height, config_.title};

    while (!view.ShouldClose()) {
        const auto frameSeconds = view.FrameSeconds();
        fixedStep_.BeginFrame(frameSeconds);
        const auto input = view.SampleInput();
        game::EventList frameEvents{};

        while (fixedStep_.ShouldTick()) {
            const auto nowMs = NowMs();
            (void)app.SubmitInput(input, nowMs);
            app.TickFixed(nowMs);

            auto tickEvents = app.DrainEvents();
            frameEvents.insert(frameEvents.end(), tickEvents.begin(), tickEvents.end());
            fixedStep_.ConsumeTick();
        }

        fixedStep_.EndFrame();
        eventLog_.Update(frameSeconds);
        visualEffectLog_.Update(frameSeconds);
        visualEffectLog_.PushFromEvents(frameEvents, app.Runtime());

        auto frame = BuildClientViewFrame(app.Runtime(), frameEvents, app.Stats());
        eventLog_.PushMany(frame.eventLines);
        frame.eventLines = eventLog_.Lines();
        frame.effects = visualEffectLog_.Effects();
        view.Render(frame);
    }

    return 0;
}

game::TimestampMs RaylibClientHost::NowMs() const {
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return static_cast<game::TimestampMs>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
}

} // namespace game::client
