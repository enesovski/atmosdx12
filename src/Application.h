#pragma once

#include "input/Input.h"
#include "graphics/RenderView.h"
#include "platform/Window.h"
#include "scene/Camera.h"
#include "scene/SkySettings.h"

#include <memory>

class Renderer;

class Application
{
public:
    Application(HINSTANCE instance, int width, int height);
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    int Run();

private:
    void UpdateCamera();

    Input input_;
    Window window_;
    Camera camera_;
    RenderView renderView_;
    SkySettings skySettings_;
    std::unique_ptr<Renderer> renderer_;
    bool windowShown_ = false;
};
