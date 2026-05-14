#include "RaylibClientHost.hpp"

#include "Client/ClientViewModel.hpp"

#include <chrono>
#include <memory>

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#endif

namespace game::client {
namespace {

game::TimestampMs NowMs()
{
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return static_cast<game::TimestampMs>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
}

void RunFrame(
    ClientApplication& app,
    RaylibGameView& view,
    FixedStepClock& fixedStep,
    ClientEventLog& eventLog,
    ClientVisualEffectLog& visualEffectLog)
{
    const auto frameSeconds = view.FrameSeconds();
    fixedStep.BeginFrame(frameSeconds);
    const auto input = view.SampleInput();
    game::EventList frameEvents{};

    while (fixedStep.ShouldTick()) {
        const auto nowMs = NowMs();
        (void)app.SubmitInput(input, nowMs);
        app.TickFixed(nowMs);

        auto tickEvents = app.DrainEvents();
        frameEvents.insert(frameEvents.end(), tickEvents.begin(), tickEvents.end());
        fixedStep.ConsumeTick();
    }

    fixedStep.EndFrame();
    eventLog.Update(frameSeconds);
    visualEffectLog.Update(frameSeconds);
    visualEffectLog.PushFromEvents(frameEvents, app.Runtime());

    auto frame = BuildClientViewFrame(app.Runtime(), frameEvents, app.Stats());
    eventLog.PushMany(frame.eventLines);
    frame.eventLines = eventLog.Lines();
    frame.effects = visualEffectLog.Effects();
    view.Render(frame);
}

#if defined(__EMSCRIPTEN__)
struct BrowserLoopState {
    BrowserLoopState(ClientApplication& application, const RaylibClientHostConfig& config)
        : app(&application)
        , view(config.width, config.height, config.title)
    {
    }

    ClientApplication* app{};
    RaylibGameView view;
    FixedStepClock fixedStep{};
    ClientEventLog eventLog{};
    ClientVisualEffectLog visualEffectLog{};
};

void RunBrowserFrame(void* userData)
{
    auto* state = static_cast<BrowserLoopState*>(userData);
    RunFrame(
        *state->app,
        state->view,
        state->fixedStep,
        state->eventLog,
        state->visualEffectLog);
}
#endif

} // namespace

RaylibClientHost::RaylibClientHost(RaylibClientHostConfig config) : config_(config) {}

int RaylibClientHost::Run(ClientApplication& app) {
    if (!app.Connect(NowMs())) {
        return 1;
    }

#if defined(__EMSCRIPTEN__)
    auto browserState = std::make_unique<BrowserLoopState>(app, config_);
    emscripten_set_main_loop_arg(RunBrowserFrame, browserState.release(), 0, 1);
    return 0;
#else
    RaylibGameView view{config_.width, config_.height, config_.title};
    while (!view.ShouldClose()) {
        RunFrame(app, view, fixedStep_, eventLog_, visualEffectLog_);
    }

    return 0;
#endif
}

} // namespace game::client
