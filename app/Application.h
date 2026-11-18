#pragma once

#include "FlyCameraController.h"

#include "core/FrameTimer.h"
#include "graphics/RenderScene.h"
#include "graphics/d3d12/GraphicsDeviceOptions.h"
#include "platform/Input.h"
#include "platform/Window.h"
#include "scene/Camera.h"
#include "scene/Scene.h"

#include <filesystem>
#include <memory>

namespace atmos
{
    class AssetManager;
    class DebugUi;
    class Renderer;
}

struct AppOptions
{
    atmos::GraphicsDeviceOptions device;
    std::filesystem::path scenePath;  // .json scene or a single .gltf/.glb; empty: scenes/sponza.json
    bool dumpFrame = false;           // Log the first frame's render graph.
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
    void LoadScene(const std::filesystem::path& path);
    void SaveScene();
    void UpdateCamera(float deltaSeconds);
    void DrawDebugWindows(float deltaSeconds);

    atmos::Input input_;
    atmos::Window window_;
    atmos::Camera camera_;
    FlyCameraController cameraController_;
    atmos::FrameTimer frameTimer_;

    atmos::Scene scene_;
    std::filesystem::path scenePath_;  // where F5 saves; empty for a directly loaded model
    atmos::RenderScene renderScene_;

    // Declared after the renderer so they are destroyed first.
    std::unique_ptr<atmos::Renderer> renderer_;
    std::unique_ptr<atmos::AssetManager> assets_;
    std::unique_ptr<atmos::DebugUi> debugUi_;

    bool showDebugUi_ = true;
    bool windowShown_ = false;
    float smoothedFrameMs_ = 0.0f;
};
