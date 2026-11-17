#pragma once

#include <algorithm>
#include <chrono>

namespace atmos
{
    // Measures time between frames. Clamped so a breakpoint or a window drag
    // does not produce one huge step.
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
