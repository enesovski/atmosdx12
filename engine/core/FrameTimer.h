#pragma once

#include <algorithm>
#include <chrono>

namespace atmos
{
    // Seconds between frames, clamped so a breakpoint does not cause a huge step.
    class FrameTimer
    {
    public:
        static constexpr float MaxDeltaSeconds = 0.1f;

        float Tick()
        {
            const Clock::time_point now = Clock::now();
            const float delta = std::chrono::duration<float>(now - lastTick_).count();
            lastTick_ = now;
            return std::min(delta, MaxDeltaSeconds);
        }

    private:
        using Clock = std::chrono::steady_clock;
        Clock::time_point lastTick_ = Clock::now();
    };
}
