#include "Application.h"

#include "core/Log.h"
#include "core/StringUtils.h"
#include "graphics/RenderView.h"
#include "graphics/Renderer.h"
#include "platform/Paths.h"
#include "runtime/AssetManager.h"
#include "runtime/RenderSceneBuilder.h"
#include "scene/SceneSerializer.h"
#include "ui/DebugUi.h"

namespace
{
    const atmos::WindowDesc MainWindow{ L"AtmosDX12", 1280, 720 };

    // scenes/sponza.json from the working directory, or from the repository root above the exe.
    std::filesystem::path FindDefaultScene()
    {
        const std::filesystem::path relative = std::filesystem::path("scenes") / "sponza.json";

        if (std::filesystem::exists(relative))
            return std::filesystem::absolute(relative);

        for (std::filesystem::path directory = atmos::GetExecutableDirectory(); directory.has_relative_path();
             directory = directory.parent_path())
        {
            if (std::filesystem::exists(directory / relative))
                return directory / relative;
        }

        throw std::runtime_error("scenes/sponza.json not found; run from the repository root or pass --scene");
    }

    bool IsModelFile(const std::filesystem::path& path)
    {
        const std::filesystem::path extension = path.extension();
        return extension == ".gltf" || extension == ".glb";
    }
}

Application::Application(HINSTANCE instance, const AppOptions& options)
    : window_(instance, MainWindow, input_),
      renderer_(std::make_unique<atmos::Renderer>(window_.GetHandle(), MainWindow.width, MainWindow.height,
                                                  options.device))
{
    assets_ = std::make_unique<atmos::AssetManager>(
        renderer_->GetDevice(),
        atmos::AssetManager::Options{ .cacheDirectory = atmos::GetExecutableDirectory() / "AssetCache" });

    debugUi_ = std::make_unique<atmos::DebugUi>(window_.GetHandle(), renderer_->GetDevice());
    window_.SetMessageHandler([this](HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
        return debugUi_->HandleMessage(window, message, wParam, lParam);
    });

    LoadScene(options.scenePath.empty() ? FindDefaultScene() : options.scenePath);

    if (options.dumpFrame)
        renderer_->RequestFrameDump();
}

Application::~Application()
{
    window_.SetMessageHandler(nullptr);
    renderer_->WaitIdle();
}

int Application::Run()
{
    int exitCode = 0;

    while (window_.ProcessMessages(exitCode))
    {
        if (window_.IsMinimized())
        {
            WaitMessage();
            continue;
        }

        if (const auto size = window_.ConsumeResize())
            renderer_->Resize(size->width, size->height);

        Tick(frameTimer_.Tick());

        // Shown after the first frame so the window never flashes uninitialized content.
        if (!windowShown_)
        {
            window_.Show();
            windowShown_ = true;
        }
    }

    return exitCode;
}

void Application::Tick(float deltaSeconds)
{
    debugUi_->BeginFrame();

    HandleShortcuts();
    UpdateCamera(deltaSeconds);

    assets_->Update();
    atmos::BuildRenderScene(scene_, *assets_, renderScene_);

    DrawUi(deltaSeconds);
    debugUi_->EndFrame();

    Render();
}

void Application::HandleShortcuts()
{
    if (input_.ConsumeKeyPress(VK_F1))
        showDebugUi_ = !showDebugUi_;

    if (input_.ConsumeKeyPress(VK_F5))
        SaveScene();
}

void Application::UpdateCamera(float deltaSeconds)
{
    const bool pointerOverUi = showDebugUi_ && debugUi_->WantsMouse() && !cameraController_.IsFlying();
    const bool flying = cameraController_.Update(camera_, input_, deltaSeconds, pointerOverUi);
    window_.SetCursorLocked(flying);
}

void Application::DrawUi(float deltaSeconds)
{
    if (!showDebugUi_)
        return;

    const bool saveRequested = debugPanels_.Draw({
        .deltaSeconds = deltaSeconds,
        .camera = camera_,
        .cameraMoveSpeed = cameraController_.GetMoveSpeed(),
        .environment = scene_.environment,
        .renderStats = renderer_->GetFrameStats(),
        .assetStats = assets_->GetStats(),
        .canSaveScene = !scenePath_.empty(),
    });

    if (saveRequested)
        SaveScene();
}

void Application::Render()
{
    const atmos::WindowSize size = window_.GetClientSize();
    renderer_->Render(renderScene_, atmos::RenderView::FromCamera(camera_, size.width, size.height),
                      showDebugUi_ ? debugUi_.get() : nullptr);
}

void Application::LoadScene(const std::filesystem::path& path)
{
    if (IsModelFile(path))
    {
        scene_ = {};
        const atmos::EntityId entity = scene_.CreateEntity(path.stem().string());
        scene_.GetEntity(entity).model = atmos::ModelComponent{ .path = std::filesystem::absolute(path) };
        scenePath_.clear();
    }
    else
    {
        scene_ = atmos::LoadScene(path);
        scenePath_ = path;
    }

    camera_.SetPosition(scene_.camera.position);
    camera_.SetRotation(scene_.camera.yawDegrees, scene_.camera.pitchDegrees);
    camera_.SetFov(scene_.camera.fovDegrees);

    atmos::RequestSceneAssets(scene_, *assets_);
    atmos::log::Info("Scene: {} ({} entities)", atmos::ToUtf8(path.filename().wstring()), scene_.GetEntities().size());
}

// Writes the scene back to its file, with the current camera as the start view.
void Application::SaveScene()
{
    if (scenePath_.empty())
    {
        atmos::log::Warning("Only scenes loaded from a .json file can be saved");
        return;
    }

    scene_.camera = { camera_.GetPosition(), camera_.GetYawDegrees(), camera_.GetPitchDegrees(), camera_.GetFovDegrees() };

    try
    {
        atmos::SaveScene(scene_, scenePath_);
        atmos::log::Info("Saved scene to {}", atmos::ToUtf8(scenePath_.wstring()));
    }
    catch (const std::exception& e)
    {
        atmos::log::Error("{}", e.what());
    }
}
