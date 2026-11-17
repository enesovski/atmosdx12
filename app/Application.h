#pragma once

#include "graphics/RenderScene.h"
#include "platform/Input.h"
#include "platform/Window.h"
#include "scene/Camera.h"

#include <memory>

namespace atmos
{
    class Renderer;
    struct GraphicsDeviceOptions;
}

class Application
{
public:
    Application(HINSTANCE instance, const atmos::GraphicsDeviceOptions& deviceOptions);
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    int Run();

private:
    void UpdateCamera();
    void RenderFrame();

    atmos::Input input_;
    atmos::Window window_;
    atmos::Camera camera_;
    atmos::RenderScene scene_;
    std::unique_ptr<atmos::Renderer> renderer_;
    bool windowShown_ = false;
};
