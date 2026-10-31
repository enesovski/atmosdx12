
#include "Application.h"

#include <stdexcept>

namespace
{
    constexpr wchar_t WindowClassName[] = L"AtmosDX12Window";
}

Application::Application(HINSTANCE instance, int width, int height)
    : instance_(instance)
{
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(WNDCLASSEXW);
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = instance_;
    windowClass.lpszClassName = WindowClassName;
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);

    if (!RegisterClassExW(&windowClass))
        throw std::runtime_error("Failed to register window class");

    RECT rect{ 0, 0, width, height };
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

    window_ = CreateWindowExW(
        0,
        WindowClassName,
        L"AtmosDX12",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        rect.right - rect.left,
        rect.bottom - rect.top,
        nullptr,
        nullptr,
        instance_,
        nullptr
    );

    if (!window_)
    {
        UnregisterClassW(WindowClassName, instance_);
        throw std::runtime_error("Failed to create window");
    }

    ShowWindow(window_, SW_SHOW);
}

Application::~Application()
{
    if (window_ && IsWindow(window_))
        DestroyWindow(window_);

    UnregisterClassW(WindowClassName, instance_);
}

int Application::Run()
{
    MSG message{};

    while (true)
    {
        BOOL result = GetMessageW(&message, nullptr, 0, 0);

        if (result == -1)
            throw std::runtime_error("Failed to retrieve window message");

        if (result == 0)
            break;

        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    return static_cast<int>(message.wParam);
}

LRESULT CALLBACK Application::WindowProc(
    HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        default:
            return DefWindowProcW(window, message, wParam, lParam);
    }
}