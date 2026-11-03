#pragma once

#include "core/FrameClock.h"
#include "input/Input.h"
#include "platform/Window.h"
#include "scene/Camera.h"
#include "scene/CelestialManager.h"
#include "scene/SkySettings.h"
#include "scene/TimeManager.h"
#include "ui/UI.h"

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
    TimeManager timeManager_;
    CelestialManager celestialManager_;
    SkySettings skySettings_;
    UI ui_;
    std::unique_ptr<Renderer> renderer_;
    FrameClock clock_;
    bool windowShown_ = false;
};