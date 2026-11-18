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
        void Show();

        // Returns the new client size once per resize.
        std::optional<WindowSize> ConsumeResize();

        // Sees every message first (e.g. a UI layer); returning true marks it handled.
        using MessageHandler = std::function<bool(HWND, UINT, WPARAM, LPARAM)>;
        void SetMessageHandler(MessageHandler handler) { messageHandler_ = std::move(handler); }

    private:
        static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
        LRESULT HandleMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

        HINSTANCE instance_;
        HWND window_ = nullptr;
        Input& input_;
        WindowSize clientSize_;
        bool resizePending_ = false;
        MessageHandler messageHandler_;
    };
}
