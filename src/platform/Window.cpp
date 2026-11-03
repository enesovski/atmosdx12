#include "Window.h"
#include "../input/Input.h"

#include <windowsx.h>

#include <stdexcept>

namespace
{
    constexpr wchar_t WindowClassName[] = L"AtmosDX12Window";
}

Window::Window(HINSTANCE instance, int width, int height, Input& input)
    : instance_(instance), input_(input)
{
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(WNDCLASSEXW);
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = instance_;
    windowClass.lpszClassName = WindowClassName;
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);

    if (!RegisterClassExW(&windowClass))
        throw std::runtime_error("Failed to register window class");

    constexpr DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;

    RECT rect{ 0, 0, width, height };
    AdjustWindowRect(&rect, style, FALSE);

    window_ = CreateWindowExW(
        0, WindowClassName, L"AtmosDX12", style, CW_USEDEFAULT, CW_USEDEFAULT,
        rect.right - rect.left, rect.bottom - rect.top, nullptr, nullptr, instance_, this
    );

    if (!window_)
    {
        UnregisterClassW(WindowClassName, instance_);
        throw std::runtime_error("Failed to create window");
    }
}

void Window::Show()
{
    ShowWindow(window_, SW_SHOW);
}

Window::~Window()
{
    if (window_ && IsWindow(window_))
        DestroyWindow(window_);

    UnregisterClassW(WindowClassName, instance_);
}

bool Window::ProcessMessages(int& exitCode)
{
    MSG message{};

    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
    {
        if (message.message == WM_QUIT)
        {
            exitCode = static_cast<int>(message.wParam);
            return false;
        }

        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    return true;
}

LRESULT CALLBACK Window::WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_NCCREATE)
    {
        auto* createInfo = reinterpret_cast<CREATESTRUCTW*>(lParam);
        auto* self = static_cast<Window*>(createInfo->lpCreateParams);

        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));

        return TRUE;
    }

    auto* self = reinterpret_cast<Window*>(GetWindowLongPtrW(window, GWLP_USERDATA));

    if (self)
        return self->HandleMessage(window, message, wParam, lParam);

    return DefWindowProcW(window, message, wParam, lParam);
}

LRESULT Window::HandleMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
        case WM_RBUTTONDOWN:
        {
            input_.BeginLook(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            SetCapture(window);
            return 0;
        }

        case WM_MOUSEMOVE:
        {
            input_.OnMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;
        }

        case WM_RBUTTONUP:
        {
            input_.EndLook();

            if (GetCapture() == window)
                ReleaseCapture();

            return 0;
        }

        case WM_MOUSEWHEEL:
        {
            float steps = static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) / WHEEL_DELTA;
            input_.AddWheelSteps(steps);
            return 0;
        }

        case WM_CAPTURECHANGED:
        {
            input_.EndLook();
            return 0;
        }

        case WM_KILLFOCUS:
        {
            input_.EndLook();

            if (GetCapture() == window)
                ReleaseCapture();

            return 0;
        }

        case WM_DESTROY:
        {
            PostQuitMessage(0);
            return 0;
        }

        case WM_NCDESTROY:
        {
            window_ = nullptr;
            SetWindowLongPtrW(window, GWLP_USERDATA, 0);
            return DefWindowProcW(window, message, wParam, lParam);
        }

        default:
            return DefWindowProcW(window, message, wParam, lParam);
    }
}
