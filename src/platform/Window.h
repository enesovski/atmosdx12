#pragma once

#include <windows.h>

class Input;

class Window
{
public:
    Window(HINSTANCE instance, int width, int height, Input& input);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    HWND GetHandle() const { return window_; }
    bool IsMinimized() const { return IsIconic(window_) != FALSE; }
    bool ProcessMessages(int& exitCode);

private:
    static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

    HINSTANCE instance_;
    HWND window_ = nullptr;
    Input& input_;
};
