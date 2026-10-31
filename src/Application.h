
#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

class Application
{
    public:
        Application(HINSTANCE instance, int width, int height);
        ~Application();

        Application(const Application&) = delete;
        Application& operator=(const Application&) = delete;

        int Run();

        HWND GetWindowHandle() const { return window_; }

    private:
        static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

        HINSTANCE instance_;
        HWND window_ = nullptr;
};
