#include "Client/FixedStepClock.hpp"

#include <algorithm>

namespace game::client {

FixedStepClock::FixedStepClock(FixedStepClockConfig config)
    : config_(config)
{
}

void FixedStepClock::BeginFrame(float frameSeconds)
{
    ticksConsumedThisFrame_ = 0;
    accumulatorSeconds_ += std::clamp(frameSeconds, 0.0F, config_.maxFrameSeconds);
}

bool FixedStepClock::ShouldTick() const noexcept
{
    return accumulatorSeconds_ >= config_.fixedStepSeconds &&
        ticksConsumedThisFrame_ < config_.maxTicksPerFrame;
}

void FixedStepClock::ConsumeTick()
{
    accumulatorSeconds_ -= config_.fixedStepSeconds;
    ++ticksConsumedThisFrame_;
}

void FixedStepClock::EndFrame()
{
    if (ticksConsumedThisFrame_ >= config_.maxTicksPerFrame &&
        accumulatorSeconds_ >= config_.fixedStepSeconds) {
        accumulatorSeconds_ = 0.0F;
    }
}

float FixedStepClock::RenderAlpha() const noexcept
{
    if (config_.fixedStepSeconds <= 0.0F) {
        return 0.0F;
    }

    return std::clamp(accumulatorSeconds_ / config_.fixedStepSeconds, 0.0F, 1.0F);
}

int FixedStepClock::TicksConsumedThisFrame() const noexcept
{
    return ticksConsumedThisFrame_;
}

float FixedStepClock::FixedStepSeconds() const noexcept
{
    return config_.fixedStepSeconds;
}

} // namespace game::client

