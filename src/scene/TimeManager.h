#pragma once

class TimeManager
{
public:
    void Update(float deltaSeconds);

    void SetTimeOfDay(float hours);
    void SetDayDuration(float seconds);
    void SetPaused(bool paused) { paused_ = paused; }

    float GetTimeOfDay() const { return timeOfDay_; }
    float GetDayDuration() const { return dayDuration_; }
    bool IsPaused() const { return paused_; }

private:
    float timeOfDay_ = 8.0f;
    float dayDuration_ = 180.0f;
    bool paused_ = false;
};