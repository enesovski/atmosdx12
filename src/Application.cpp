
#include "Application.h"
#include "graphics/D3D12Renderer.h"

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

    // No resizing for this milestone; swapchain resizing will be added later.
    constexpr DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;

    RECT rect{ 0, 0, width, height };
    AdjustWindowRect(&rect, style, FALSE);

    window_ = CreateWindowExW(
        0,
        WindowClassName,
        L"AtmosDX12",
        style,
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

int Application::Run(D3D12Renderer& renderer)
{
    MSG message{};

    while (true)
    {
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
        {
            if (message.message == WM_QUIT)
                return static_cast<int>(message.wParam);

            TranslateMessage(&message);
            DispatchMessageW(&message);
        }

        if (IsIconic(window_))
        {
            WaitMessage();
            continue;
        }

        renderer.Render();
    }
}

LRESULT CALLBACK Application::WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
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
