#include "platform/Window.h"
#include "platform/Input.h"

#include <windowsx.h>

#include <stdexcept>

namespace atmos
{
    namespace
    {
        constexpr wchar_t WindowClassName[] = L"AtmosDX12Window";
    }

    Window::Window(HINSTANCE instance, const WindowDesc& desc, Input& input)
        : instance_(instance), input_(input), clientSize_{ desc.width, desc.height }
    {
        WNDCLASSEXW windowClass{};
        windowClass.cbSize = sizeof(WNDCLASSEXW);
        windowClass.lpfnWndProc = WindowProc;
        windowClass.hInstance = instance_;
        windowClass.lpszClassName = WindowClassName;
        windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);

        if (!RegisterClassExW(&windowClass))
            throw std::runtime_error("Failed to register window class");

        constexpr DWORD style = WS_OVERLAPPEDWINDOW;

        RECT rect{ 0, 0, static_cast<LONG>(desc.width), static_cast<LONG>(desc.height) };
        AdjustWindowRect(&rect, style, FALSE);

        window_ = CreateWindowExW(
            0, WindowClassName, desc.title.c_str(), style, CW_USEDEFAULT, CW_USEDEFAULT,
            rect.right - rect.left, rect.bottom - rect.top, nullptr, nullptr, instance_, this);

        if (!window_)
        {
            UnregisterClassW(WindowClassName, instance_);
            throw std::runtime_error("Failed to create window");
        }
    }

    Window::~Window()
    {
        if (window_ && IsWindow(window_))
            DestroyWindow(window_);

        UnregisterClassW(WindowClassName, instance_);
    }

    void Window::Show()
    {
        ShowWindow(window_, SW_SHOW);
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

    std::optional<WindowSize> Window::ConsumeResize()
    {
        if (!resizePending_)
            return std::nullopt;

        resizePending_ = false;
        return clientSize_;
    }

    LRESULT CALLBACK Window::WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
    {
        if (message == WM_NCCREATE)
        {
            auto* createInfo = reinterpret_cast<CREATESTRUCTW*>(lParam);
            SetWindowLongPtrW(window, GWLP_USERDATA,
                              reinterpret_cast<LONG_PTR>(createInfo->lpCreateParams));
            return TRUE;
        }

        auto* self = reinterpret_cast<Window*>(GetWindowLongPtrW(window, GWLP_USERDATA));

        if (self)
            return self->HandleMessage(window, message, wParam, lParam);

        return DefWindowProcW(window, message, wParam, lParam);
    }

    LRESULT Window::HandleMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
    {
        if (messageHandler_ && messageHandler_(window, message, wParam, lParam))
            return TRUE;

        switch (message)
        {
            case WM_SIZE:
            {
                // Minimizing reports 0x0; keep the last real size.
                if (wParam != SIZE_MINIMIZED)
                {
                    clientSize_ = { LOWORD(lParam), HIWORD(lParam) };
                    resizePending_ = true;
                }
                return 0;
            }

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
                const float steps = static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) / WHEEL_DELTA;
                input_.AddWheelSteps(steps);
                return 0;
            }

            case WM_CAPTURECHANGED:
            {
                input_.EndLook();
                return 0;
            }

            case WM_KEYDOWN:
            case WM_KEYUP:
            {
                input_.SetKeyDown(static_cast<uint32_t>(wParam), message == WM_KEYDOWN);
                return 0;
            }

            case WM_KILLFOCUS:
            {
                input_.ReleaseAllKeys();

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
}
