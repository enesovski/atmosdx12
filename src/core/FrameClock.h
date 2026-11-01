
#pragma once

#include <chrono>

class FrameClock
{
public:
    FrameClock() : startTime_(Clock::now()) {}

    float GetElapsedSeconds() const
    {
        return std::chrono::duration<float>(
            Clock::now() - startTime_
        ).count();
    }

    void Reset()
    {
        startTime_ = Clock::now();
    }

private:
    using Clock = std::chrono::steady_clock;

    Clock::time_point startTime_;
};
