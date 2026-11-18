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

#include <imgui.h>

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
        renderer_->GetDevice(), atmos::AssetManager::Options{ .cacheDirectory = atmos::GetExecutableDirectory() / "AssetCache" });

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

        const float deltaSeconds = frameTimer_.Tick();

        if (input_.ConsumeKeyPress(VK_F1))
            showDebugUi_ = !showDebugUi_;

        if (input_.ConsumeKeyPress(VK_F5))
            SaveScene();

        debugUi_->BeginFrame();
        UpdateCamera(deltaSeconds);

        assets_->Update();
        atmos::BuildRenderScene(scene_, *assets_, renderScene_);

        if (showDebugUi_)
            DrawDebugWindows(deltaSeconds);

        debugUi_->EndFrame();

        const atmos::WindowSize size = window_.GetClientSize();
        renderer_->Render(renderScene_, atmos::RenderView::FromCamera(camera_, size.width, size.height),
                          showDebugUi_ ? debugUi_.get() : nullptr);

        // Show after the first frame so the window never flashes uninitialized content.
        if (!windowShown_)
        {
            window_.Show();
            windowShown_ = true;
        }
    }

    return exitCode;
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

// Writes the scene back to its file, including the current camera as the start view.
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

void Application::UpdateCamera(float deltaSeconds)
{
    // While the UI owns the mouse or keyboard, drop camera input instead of letting it build up.
    if (showDebugUi_ && (debugUi_->WantsMouse() || debugUi_->WantsKeyboard()))
    {
        input_.ConsumeLookDelta();
        input_.ConsumeWheelSteps();
        return;
    }

    cameraController_.Update(camera_, input_, deltaSeconds);
}

void Application::DrawDebugWindows(float deltaSeconds)
{
    smoothedFrameMs_ += (deltaSeconds * 1000.0f - smoothedFrameMs_) * 0.05f;

    ImGui::SetNextWindowPos({ 10, 10 }, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({ 330, 0 }, ImGuiCond_FirstUseEver);
    ImGui::Begin("AtmosDX12  (F1 hides)");

    ImGui::Text("%.2f ms  (%.0f FPS)", smoothedFrameMs_, smoothedFrameMs_ > 0.0f ? 1000.0f / smoothedFrameMs_ : 0.0f);
    const DirectX::XMFLOAT3& position = camera_.GetPosition();
    ImGui::Text("Camera %.1f, %.1f, %.1f", position.x, position.y, position.z);

    if (ImGui::CollapsingHeader("Environment", ImGuiTreeNodeFlags_DefaultOpen))
    {
        atmos::SceneEnvironment& environment = scene_.environment;
        ImGui::SliderFloat("Sun azimuth", &environment.sunAzimuthDegrees, 0.0f, 360.0f, "%.0f deg");
        ImGui::SliderFloat("Sun elevation", &environment.sunElevationDegrees, -10.0f, 90.0f, "%.1f deg");
        ImGui::SliderFloat("Sun intensity", &environment.sunIntensity, 0.0f, 10.0f);
        ImGui::SliderFloat("Exposure", &environment.exposure, 0.1f, 5.0f);

        ImGui::BeginDisabled(scenePath_.empty());
        if (ImGui::Button("Save scene (F5)"))
            SaveScene();
        ImGui::EndDisabled();
    }

    if (ImGui::CollapsingHeader("Render graph", ImGuiTreeNodeFlags_DefaultOpen))
    {
        const atmos::RenderFrameStats& stats = renderer_->GetFrameStats();

        for (const atmos::RenderPassInfo& pass : stats.passes)
        {
            ImGui::TextColored(pass.culled ? ImVec4(0.6f, 0.6f, 0.6f, 1.0f) : ImVec4(0.5f, 1.0f, 0.5f, 1.0f),
                               "%s  %s", pass.culled ? "culled" : "live  ", atmos::ToUtf8(pass.name).c_str());
        }

        ImGui::Separator();
        ImGui::Text("Instances %u  (culled %u)", stats.scene.instances, stats.scene.culledInstances);
        ImGui::Text("Draws %u   Materials %u", stats.scene.draws, stats.scene.materials);
        ImGui::Text("Pooled render targets %zu", stats.pooledTextures);
    }

    if (ImGui::CollapsingHeader("Assets", ImGuiTreeNodeFlags_DefaultOpen))
    {
        const atmos::AssetStats stats = assets_->GetStats();
        const uint32_t textureTotal = stats.texturesReady + stats.texturesLoading + stats.texturesFailed;

        ImGui::Text("Models   %u ready, %u loading, %u failed", stats.modelsReady, stats.modelsLoading, stats.modelsFailed);
        ImGui::Text("Textures %u ready, %u loading, %u failed", stats.texturesReady, stats.texturesLoading, stats.texturesFailed);

        if (textureTotal > 0)
        {
            ImGui::ProgressBar(static_cast<float>(stats.texturesReady + stats.texturesFailed) / textureTotal);
        }
    }

    ImGui::End();
}
