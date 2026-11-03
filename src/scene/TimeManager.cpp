#include "TimeManager.h"

#include <cmath>
#include <stdexcept>

void TimeManager::Update(float deltaSeconds)
{
    if (paused_ || !std::isfinite(deltaSeconds) || deltaSeconds <= 0.0f)
        return;

    SetTimeOfDay(timeOfDay_ + deltaSeconds * (24.0f / dayDuration_));
}

void TimeManager::SetTimeOfDay(float hours)
{
    if (!std::isfinite(hours))
        throw std::invalid_argument("Time of day must be finite");

    timeOfDay_ = std::fmod(hours, 24.0f);

    if (timeOfDay_ < 0.0f)
        timeOfDay_ += 24.0f;
}

void TimeManager::SetDayDuration(float seconds)
{
    if (!std::isfinite(seconds) || seconds <= 0.0f)
        throw std::invalid_argument("Day duration must be positive and finite");

    dayDuration_ = seconds;
}