#include "Application.h"
#include "graphics/D3D12Renderer.h"

#include <windowsx.h>
#include <stdexcept>

namespace
{
    constexpr wchar_t WindowClassName[] = L"AtmosDX12Window";

    constexpr float MouseSensitivity = 0.15f;
    constexpr float ZoomSensitivity = 3.0f;
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
        this
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

        UpdateCamera();
        renderer.Render(camera_);
    }
}

void Application::UpdateCamera()
{
    MouseDelta look = input_.ConsumeLookDelta();

    camera_.Rotate(
        look.x * MouseSensitivity,
        -look.y * MouseSensitivity
    );

    float wheelSteps = input_.ConsumeWheelSteps();

    camera_.AdjustFov(-wheelSteps * ZoomSensitivity);
}

LRESULT CALLBACK Application::WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_NCCREATE)
    {
        auto* createInfo = reinterpret_cast<CREATESTRUCTW*>(lParam);
        auto* app = static_cast<Application*>(createInfo->lpCreateParams);

        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));

        return TRUE;
    }

    auto* app = reinterpret_cast<Application*>(
        GetWindowLongPtrW(window, GWLP_USERDATA)
    );

    if (app)
        return app->HandleMessage(window, message, wParam, lParam);

    return DefWindowProcW(window, message, wParam, lParam);
}

LRESULT Application::HandleMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
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
            SetWindowLongPtrW(window, GWLP_USERDATA, 0);
            return DefWindowProcW(window, message, wParam, lParam);
        }

        default:
            return DefWindowProcW(window, message, wParam, lParam);
    }
}