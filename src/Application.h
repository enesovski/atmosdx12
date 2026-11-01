#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "input/Input.h"
#include "scene/Camera.h"

class D3D12Renderer;

class Application
{
public:
    Application(HINSTANCE instance, int width, int height);
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    int Run(D3D12Renderer& renderer);

    HWND GetWindowHandle() const { return window_; }

private:
    static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

    LRESULT HandleMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    void UpdateCamera();

    HINSTANCE instance_;
    HWND window_ = nullptr;

    Input input_;
    Camera camera_;
};