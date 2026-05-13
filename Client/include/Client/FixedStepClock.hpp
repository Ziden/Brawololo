#pragma once

namespace game::client {

struct FixedStepClockConfig {
    float fixedStepSeconds{16.0F / 1000.0F};
    float maxFrameSeconds{100.0F / 1000.0F};
    int maxTicksPerFrame{8};
};

class FixedStepClock {
public:
    explicit FixedStepClock(FixedStepClockConfig config = {});

    void BeginFrame(float frameSeconds);
    [[nodiscard]] bool ShouldTick() const noexcept;
    void ConsumeTick();
    void EndFrame();

    [[nodiscard]] float RenderAlpha() const noexcept;
    [[nodiscard]] int TicksConsumedThisFrame() const noexcept;
    [[nodiscard]] float FixedStepSeconds() const noexcept;

private:
    FixedStepClockConfig config_{};
    float accumulatorSeconds_{};
    int ticksConsumedThisFrame_{};
};

} // namespace game::client
