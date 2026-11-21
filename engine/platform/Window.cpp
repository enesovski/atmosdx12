#include "platform/Window.h"
#include "platform/Input.h"

#include <stdexcept>

namespace atmos
{
    namespace
    {
        constexpr wchar_t WindowClassName[] = L"AtmosDX12Window";

        void RegisterRawMouse(HWND window)
        {
            RAWINPUTDEVICE device{};
            device.usUsagePage = 0x01;  // generic desktop
            device.usUsage = 0x02;      // mouse
            device.hwndTarget = window;

            if (!RegisterRawInputDevices(&device, 1, sizeof(device)))
                throw std::runtime_error("Failed to register raw mouse input");
        }

        std::optional<MouseButton> ToMouseButton(UINT message)
        {
            switch (message)
            {
                case WM_LBUTTONDOWN: case WM_LBUTTONUP: return MouseButton::Left;
                case WM_RBUTTONDOWN: case WM_RBUTTONUP: return MouseButton::Right;
                case WM_MBUTTONDOWN: case WM_MBUTTONUP: return MouseButton::Middle;
                default:                                return std::nullopt;
            }
        }
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

        RegisterRawMouse(window_);
    }

    Window::~Window()
    {
        SetCursorLocked(false);

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

    void Window::SetCursorLocked(bool locked)
    {
        if (locked == cursorLocked_)
            return;

        cursorLocked_ = locked;

        if (locked)
        {
            GetCursorPos(&lockedCursorPosition_);
            const RECT pin{ lockedCursorPosition_.x, lockedCursorPosition_.y,
                            lockedCursorPosition_.x + 1, lockedCursorPosition_.y + 1 };
            ClipCursor(&pin);
            SetCapture(window_);
            ShowCursor(FALSE);
        }
        else
        {
            ShowCursor(TRUE);
            ClipCursor(nullptr);

            if (GetCapture() == window_)
                ReleaseCapture();

            SetCursorPos(lockedCursorPosition_.x, lockedCursorPosition_.y);
        }
    }

    void Window::HandleRawInput(LPARAM lParam)
    {
        RAWINPUT raw{};
        UINT size = sizeof(raw);

        if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, &raw, &size, sizeof(RAWINPUTHEADER)) ==
            static_cast<UINT>(-1))
            return;

        // Absolute devices (remote desktop, tablets) report positions, not motion.
        if (raw.header.dwType == RIM_TYPEMOUSE && !(raw.data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE))
            input_.OnMouseMove(static_cast<float>(raw.data.mouse.lLastX), static_cast<float>(raw.data.mouse.lLastY));
    }

    LRESULT CALLBACK Window::WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
    {
        if (message == WM_NCCREATE)
        {
            auto* createInfo = reinterpret_cast<CREATESTRUCTW*>(lParam);
            SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(createInfo->lpCreateParams));
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

        if (const std::optional<MouseButton> button = ToMouseButton(message))
        {
            const bool down = message == WM_LBUTTONDOWN || message == WM_RBUTTONDOWN || message == WM_MBUTTONDOWN;
            input_.OnMouseButton(*button, down);
            return 0;
        }

        switch (message)
        {
            case WM_INPUT:
                HandleRawInput(lParam);
                return DefWindowProcW(window, message, wParam, lParam);

            case WM_MOUSEWHEEL:
                input_.OnWheel(static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) / WHEEL_DELTA);
                return 0;

            case WM_KEYDOWN:
            case WM_KEYUP:
                input_.OnKey(static_cast<uint32_t>(wParam), message == WM_KEYDOWN);
                return 0;

            case WM_KILLFOCUS:
                SetCursorLocked(false);
                input_.Reset();
                return 0;

            case WM_SIZE:
                // Minimizing reports 0x0; keep the last real size.
                if (wParam != SIZE_MINIMIZED)
                {
                    clientSize_ = { LOWORD(lParam), HIWORD(lParam) };
                    resizePending_ = true;
                }
                return 0;

            case WM_DESTROY:
                PostQuitMessage(0);
                return 0;

            case WM_NCDESTROY:
                window_ = nullptr;
                SetWindowLongPtrW(window, GWLP_USERDATA, 0);
                return DefWindowProcW(window, message, wParam, lParam);

            default:
                return DefWindowProcW(window, message, wParam, lParam);
        }
    }
}
