#pragma once

#include <windows.h>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>

namespace atmos
{
    class Input;

    struct WindowDesc
    {
        std::wstring title = L"AtmosDX12";
        uint32_t width = 1280;
        uint32_t height = 720;
    };

    struct WindowSize
    {
        uint32_t width = 0;
        uint32_t height = 0;
    };

    // Win32 window that translates OS messages into Input state.
    class Window
    {
    public:
        Window(HINSTANCE instance, const WindowDesc& desc, Input& input);
        ~Window();

        Window(const Window&) = delete;
        Window& operator=(const Window&) = delete;

        HWND GetHandle() const { return window_; }
        WindowSize GetClientSize() const { return clientSize_; }
        bool IsMinimized() const { return IsIconic(window_) != FALSE; }

        // Returns false once WM_QUIT is received.
        bool ProcessMessages(int& exitCode);
        void Show(bool maximized = false);
        void SetTitle(const std::wstring& title);

        // Destroys the window, which ends the message loop (bypasses WM_CLOSE handling).
        void Close();

        // Pixels per logical pixel of the monitor the window is on (1.0 at 96 DPI).
        float GetDpiScale() const;

        std::optional<WindowSize> ConsumeResize();

        // Hides the cursor and pins it in place (mouse motion still arrives as raw input).
        void SetCursorLocked(bool locked);

        // Sees every message first (e.g. a UI layer); returning true marks it handled.
        using MessageHandler = std::function<bool(HWND, UINT, WPARAM, LPARAM)>;
        void SetMessageHandler(MessageHandler handler) { messageHandler_ = std::move(handler); }

    private:
        static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
        LRESULT HandleMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
        void HandleRawInput(LPARAM lParam);

        HINSTANCE instance_;
        HWND window_ = nullptr;
        Input& input_;
        WindowSize clientSize_;
        bool resizePending_ = false;
        bool cursorLocked_ = false;
        POINT lockedCursorPosition_{};
        MessageHandler messageHandler_;
    };
}
