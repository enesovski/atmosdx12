#pragma once

#include "FlyCameraController.h"

#include "core/FrameTimer.h"
#include "graphics/RenderScene.h"
#include "graphics/d3d12/GraphicsDeviceOptions.h"
#include "platform/Input.h"
#include "platform/Window.h"
#include "scene/Camera.h"

#include <filesystem>
#include <memory>
#include <vector>

namespace atmos
{
    class Mesh;
    class Renderer;
}

struct AppOptions
{
    atmos::GraphicsDeviceOptions device;
    std::filesystem::path scenePath;   // Empty: built-in test scene.
    bool dumpFrame = false;            // Log the first frame's render graph.
};

class Application
{
public:
    Application(HINSTANCE instance, const AppOptions& options);
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    int Run();

private:
    void LoadGltfScene(const std::filesystem::path& path);
    void LoadDefaultScene();
    void RenderFrame();

    atmos::Input input_;
    atmos::Window window_;
    atmos::Camera camera_;
    FlyCameraController cameraController_;
    atmos::FrameTimer frameTimer_;
    atmos::RenderScene scene_;

    // Meshes are declared after the renderer so they are destroyed first.
    std::unique_ptr<atmos::Renderer> renderer_;
    std::vector<std::unique_ptr<atmos::Mesh>> meshes_;

    bool windowShown_ = false;
};
