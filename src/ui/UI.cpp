#include "UI.h"
#include "../scene/SkySettings.h"
#include "../scene/TimeManager.h"

#include <commctrl.h>

#include <cmath>
#include <cwchar>
#include <stdexcept>
#include <string>

namespace
{
    constexpr wchar_t PanelClassName[] = L"AtmosDX12ControlPanel";
    constexpr int PauseButtonId = 100;
}

UI::UI(HWND parent, TimeManager& timeManager, SkySettings& skySettings)
    : instance_(GetModuleHandleW(nullptr)),
      timeManager_(timeManager), skySettings_(skySettings)
{
    INITCOMMONCONTROLSEX controls{ sizeof(INITCOMMONCONTROLSEX), ICC_BAR_CLASSES };

    if (!InitCommonControlsEx(&controls))
        throw std::runtime_error("Failed to initialize trackbar controls");

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = instance_;
    windowClass.lpszClassName = PanelClassName;
    windowClass.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);

    if (!RegisterClassExW(&windowClass))
        throw std::runtime_error("Failed to register UI window class");

    try
    {
RECT ownerRect{};
GetWindowRect(parent, &ownerRect);

constexpr DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;

RECT panelRect{ 0, 0, 320, 350 };
AdjustWindowRectEx(&panelRect, style, FALSE, WS_EX_TOOLWINDOW);

    panel_ = CreateWindowExW(
        WS_EX_TOOLWINDOW,
        PanelClassName,
        L"Sky Controls",
        style,
        ownerRect.right + 12,
        ownerRect.top,
        panelRect.right - panelRect.left,
        panelRect.bottom - panelRect.top,
        parent,
        nullptr,
        instance_,
        this
    );

    if (!panel_)
        throw std::runtime_error("Failed to create UI panel");
        if (!panel_)
            throw std::runtime_error("Failed to create UI panel");

        CreateRow(TimeRow, 12, 0, 1439,
                  static_cast<int>(timeManager_.GetTimeOfDay() * 60.0f));
        CreateRow(DurationRow, 70, 30, 600,
                  static_cast<int>(timeManager_.GetDayDuration()));
        CreateRow(SunRow, 128, 0, 200,
                  static_cast<int>(skySettings_.sunIntensity * 100.0f));
        CreateRow(MoonRow, 186, 0, 200,
                  static_cast<int>(skySettings_.moonIntensity * 100.0f));
        CreateRow(StarsRow, 244, 0, 200,
                  static_cast<int>(skySettings_.starIntensity * 100.0f));

        pauseButton_ = CreateWindowExW(
            0, L"BUTTON", L"Pause", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            12, 305, 296, 30, panel_,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(PauseButtonId)),
            instance_, nullptr
        );

        if (!pauseButton_)
            throw std::runtime_error("Failed to create pause button");

        UpdateSettingLabels();
        Refresh();
    }
    catch (...)
    {
        if (panel_ && IsWindow(panel_))
            DestroyWindow(panel_);

        UnregisterClassW(PanelClassName, instance_);
        throw;
    }
}
void UI::Show()
{
    ShowWindow(panel_, SW_SHOW);
}

UI::~UI()
{
    if (panel_ && IsWindow(panel_))
        DestroyWindow(panel_);

    UnregisterClassW(PanelClassName, instance_);
}

void UI::CreateRow(SliderRow row, int y, int minValue, int maxValue, int initialValue)
{
    labels_[row] = CreateWindowExW(
        0, L"STATIC", L"", WS_CHILD | WS_VISIBLE,
        12, y, 296, 20, panel_, nullptr, instance_, nullptr
    );

    sliders_[row] = CreateWindowExW(
        0, TRACKBAR_CLASSW, L"",
        WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS,
        12, y + 20, 296, 32, panel_, nullptr, instance_, nullptr
    );

    if (!labels_[row] || !sliders_[row])
        throw std::runtime_error("Failed to create UI slider");

    SendMessageW(sliders_[row], TBM_SETRANGE, TRUE, MAKELPARAM(minValue, maxValue));
    SendMessageW(sliders_[row], TBM_SETPOS, TRUE, initialValue);
}

void UI::Refresh()
{
    const int minute =
        static_cast<int>(std::lround(timeManager_.GetTimeOfDay() * 60.0f)) % 1440;

    if (minute != displayedMinute_)
    {
        if (GetCapture() != sliders_[TimeRow])
            SendMessageW(sliders_[TimeRow], TBM_SETPOS, TRUE, minute);

        wchar_t label[64]{};
        std::swprintf(label, 64, L"Time of Day: %02d:%02d",
                      minute / 60, minute % 60);

        SetWindowTextW(labels_[TimeRow], label);
        displayedMinute_ = minute;
    }

    if (timeManager_.IsPaused() != displayedPaused_)
    {
        SetWindowTextW(
            pauseButton_,
            timeManager_.IsPaused() ? L"Resume" : L"Pause"
        );

        displayedPaused_ = timeManager_.IsPaused();
    }
}

void UI::UpdateSettingLabels()
{
    const int duration = static_cast<int>(timeManager_.GetDayDuration());
    const int sun = static_cast<int>(std::lround(skySettings_.sunIntensity * 100.0f));
    const int moon = static_cast<int>(std::lround(skySettings_.moonIntensity * 100.0f));
    const int stars = static_cast<int>(std::lround(skySettings_.starIntensity * 100.0f));

    SetWindowTextW(
        labels_[DurationRow],
        (L"Day Duration: " + std::to_wstring(duration) + L" s").c_str()
    );

    SetWindowTextW(
        labels_[SunRow],
        (L"Sun Intensity: " + std::to_wstring(sun) + L"%").c_str()
    );

    SetWindowTextW(
        labels_[MoonRow],
        (L"Moon Intensity: " + std::to_wstring(moon) + L"%").c_str()
    );

    SetWindowTextW(
        labels_[StarsRow],
        (L"Star Intensity: " + std::to_wstring(stars) + L"%").c_str()
    );
}

void UI::OnSliderChanged(HWND slider)
{
    const int position = static_cast<int>(SendMessageW(slider, TBM_GETPOS, 0, 0));

    if (slider == sliders_[TimeRow])
    {
        timeManager_.SetTimeOfDay(static_cast<float>(position) / 60.0f);
        timeManager_.SetPaused(true); // Scrubbing keeps the selected time stable.
    }
    else if (slider == sliders_[DurationRow])
        timeManager_.SetDayDuration(static_cast<float>(position));
    else if (slider == sliders_[SunRow])
        skySettings_.sunIntensity = static_cast<float>(position) / 100.0f;
    else if (slider == sliders_[MoonRow])
        skySettings_.moonIntensity = static_cast<float>(position) / 100.0f;
    else if (slider == sliders_[StarsRow])
        skySettings_.starIntensity = static_cast<float>(position) / 100.0f;

    UpdateSettingLabels();
    Refresh();
}

LRESULT CALLBACK UI::WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_NCCREATE)
    {
        auto* createInfo = reinterpret_cast<CREATESTRUCTW*>(lParam);
        auto* self = static_cast<UI*>(createInfo->lpCreateParams);

        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        return TRUE;
    }

    auto* self = reinterpret_cast<UI*>(GetWindowLongPtrW(window, GWLP_USERDATA));

    if (self)
        return self->HandleMessage(window, message, wParam, lParam);

    return DefWindowProcW(window, message, wParam, lParam);
}

LRESULT UI::HandleMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
        case WM_HSCROLL:
            if (lParam)
                OnSliderChanged(reinterpret_cast<HWND>(lParam));
            return 0;

        case WM_COMMAND:
            if (LOWORD(wParam) == PauseButtonId && HIWORD(wParam) == BN_CLICKED)
            {
                timeManager_.SetPaused(!timeManager_.IsPaused());
                Refresh();
                return 0;
            }
            break;

        case WM_NCDESTROY:
            panel_ = nullptr;
            SetWindowLongPtrW(window, GWLP_USERDATA, 0);
            break;
    }

    return DefWindowProcW(window, message, wParam, lParam);
}