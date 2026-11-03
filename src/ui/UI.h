#pragma once

#include <windows.h>

#include <array>

class TimeManager;
struct SkySettings;

class UI
{
public:
    UI(HWND parent, TimeManager& timeManager, SkySettings& skySettings);
    ~UI();

    UI(const UI&) = delete;
    UI& operator=(const UI&) = delete;

    void Refresh();
    void Show();

private:
    enum SliderRow { TimeRow, DurationRow, SunRow, MoonRow, StarsRow, RowCount };

    static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

    void CreateRow(SliderRow row, int y, int minValue, int maxValue, int initialValue);
    void OnSliderChanged(HWND slider);
    void UpdateSettingLabels();

    HINSTANCE instance_ = nullptr;
    HWND panel_ = nullptr;
    HWND pauseButton_ = nullptr;
    std::array<HWND, RowCount> labels_{};
    std::array<HWND, RowCount> sliders_{};

    TimeManager& timeManager_;
    SkySettings& skySettings_;
    int displayedMinute_ = -1;
    bool displayedPaused_ = false;
};