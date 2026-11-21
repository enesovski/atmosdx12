#include "EditorApp.h"
#include "EditorLayout.h"
#include "ImGuiLayer.h"
#include "ViewModes.h"
#include "shell/EditorBars.h"
#include "shell/MainMenuBar.h"
#include "style/EditorFonts.h"
#include "style/EditorTheme.h"
#include "tools/FileDialogs.h"
#include "tools/Picking.h"
#include "widgets/Widgets.h"

#include "core/Log.h"
#include "core/StringUtils.h"
#include "graphics/RenderView.h"
#include "graphics/Renderer.h"
#include "platform/Paths.h"
#include "runtime/AssetManager.h"
#include "runtime/RenderSceneBuilder.h"
#include "scene/SceneSerializer.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <ImGuizmo.h>

#include <algorithm>

namespace atmos::editor
{
    namespace
    {
        const WindowDesc MainWindow{ L"AtmosDX12 Editor", 1600, 900 };
        constexpr const char* UnsavedPrompt = "Unsaved Changes";
        constexpr float FullscreenHintDuration = 3.0f;
    }

    EditorApp::EditorApp(HINSTANCE instance, const EditorOptions& options)
        : paths_(ProjectPaths::Find()),
          window_(instance, MainWindow, input_),
          renderer_(std::make_unique<Renderer>(window_.GetHandle(), MainWindow.width, MainWindow.height, options.device)),
          viewportPanel_(renderer_->GetDevice())
    {
        prefs_.Load(paths_.prefsFile);

        assets_ = std::make_unique<AssetManager>(
            renderer_->GetDevice(), AssetManager::Options{ .cacheDirectory = GetExecutableDirectory() / "AssetCache" });
        imgui_ = std::make_unique<ImGuiLayer>(window_.GetHandle(), renderer_->GetDevice(), paths_.layoutFile);
        document_ = std::make_unique<SceneDocument>(*assets_);

        context_ = std::unique_ptr<EditorContext>(new EditorContext{
            .document = *document_,
            .renderer = *renderer_,
            .assets = *assets_,
            .console = console_,
            .prefs = prefs_,
            .paths = paths_,
            .camera = camera_,
            .cameraController = cameraController_,
            .renderScene = renderScene_,
        });

        context_->viewport.showStats = prefs_.showStatsByDefault;
        cameraController_.lookSensitivity = prefs_.lookSensitivity;
        cameraController_.invertY = prefs_.invertY;
        cameraController_.SetMoveSpeed(prefs_.defaultMoveSpeed);

        // Submission order: in a freshly built layout, each dock node shows its first panel here.
        panels_ = { &viewportPanel_, &hierarchyPanel_, &inspectorPanel_, &renderSettingsPanel_,
                    &contentBrowserPanel_, &consolePanel_, &profilerPanel_, &renderGraphPanel_ };

        window_.SetMessageHandler([this](HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
            // Closing goes through the unsaved-changes prompt.
            if (message == WM_CLOSE)
            {
                context_->requests.exit = true;
                return true;
            }

            return imgui_->HandleMessage(window, message, wParam, lParam);
        });

        const std::filesystem::path scene = FindStartupScene(options);

        if (scene.empty())
            document_->New();
        else
            OpenScene(scene);

        document_->SpawnTestLights(options.spawnLights);

        if (options.dumpFrame)
            renderer_->RequestFrameDump();
    }

    EditorApp::~EditorApp()
    {
        window_.SetMessageHandler(nullptr);
        renderer_->WaitIdle();
        prefs_.Save(paths_.prefsFile);
    }

    int EditorApp::Run()
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
                window_.Show(true);
                windowShown_ = true;
            }
        }

        return exitCode;
    }

    void EditorApp::Tick(float deltaSeconds)
    {
        console_.Drain();
        UpdateAppearance();
        imgui_->BeginFrame();
        context_->deltaSeconds = deltaSeconds;

        UpdateCamera(deltaSeconds);
        assets_->Update();
        HandleShortcuts();
        DrawUi();
        ProcessRequests();

        // A drag (slider, gizmo) becomes one undo step when it ends.
        if (!ImGui::IsAnyItemActive() && !ImGuizmo::IsUsing())
            document_->CommitPendingEdit();

        imgui_->EndFrame();

        BuildFrameScene();
        Render();

        context_->history.Push(deltaSeconds * 1000.0f, renderer_->GetFrameStats().gpuMilliseconds);
        UpdateWindowTitle();
        Autosave(deltaSeconds);
    }

    void EditorApp::UpdateAppearance()
    {
        // Rebuilding fonts waits for the GPU, so never while a slider (e.g. UI scale) is being dragged.
        if (ImGui::GetCurrentContext()->FrameCount > 0 && ImGui::IsAnyItemActive())
            return;

        imgui_->SetAppearance(window_.GetDpiScale() * prefs_.uiScale, prefs_.theme, prefs_.fontSize,
                              [this] { renderer_->WaitIdle(); });
    }

    void EditorApp::UpdateCamera(float deltaSeconds)
    {
        ImGuiIO& io = ImGui::GetIO();
        const MouseDelta delta = input_.ConsumeMouseDelta();
        input_.ConsumeWheelSteps();  // ImGui's wheel is used instead, so scrolling panels does not move the camera

        const bool hovered = context_->fullscreenViewport ? !io.WantCaptureMouse : viewportPanel_.IsHovered();
        const CameraInput input{
            .hovered = hovered,
            .keyboard = !io.WantTextInput,
            .rawDeltaX = delta.x,
            .rawDeltaY = delta.y,
            .wheel = hovered ? io.MouseWheel : 0.0f,
        };

        const bool dragging = cameraController_.Update(camera_, input, deltaSeconds);
        window_.SetCursorLocked(dragging);

        // Keep the hidden cursor hidden while flying.
        if (dragging)
            io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
        else
            io.ConfigFlags &= ~ImGuiConfigFlags_NoMouseCursorChange;
    }

    void EditorApp::HandleShortcuts()
    {
        const ImGuiIO& io = ImGui::GetIO();

        if (io.WantTextInput)
            return;

        auto pressed = [](ImGuiKey key) { return ImGui::IsKeyPressed(key, false); };
        EditorRequests& requests = context_->requests;

        if (io.KeyCtrl)
        {
            if (pressed(ImGuiKey_N))
                requests.newScene = true;
            else if (pressed(ImGuiKey_O))
                requests.openSceneDialog = true;
            else if (pressed(ImGuiKey_S))
                (io.KeyShift ? requests.saveSceneAs : requests.saveScene) = true;
            else if (pressed(ImGuiKey_Z))
                io.KeyShift ? document_->Redo() : document_->Undo();
            else if (pressed(ImGuiKey_Y))
                document_->Redo();
            else if (pressed(ImGuiKey_D))
                document_->DuplicateSelection();

            return;
        }

        if (pressed(ImGuiKey_F11) || pressed(ImGuiKey_F1))
            requests.toggleFullscreen = true;

        if (pressed(ImGuiKey_F5))
            requests.saveScene = true;

        if (io.KeyAlt)
        {
            const auto modes = GetViewModes();

            for (size_t i = 0; i < std::min<size_t>(modes.size(), 9); ++i)
            {
                if (pressed(static_cast<ImGuiKey>(ImGuiKey_1 + i)))
                    context_->viewport.viewMode = modes[i].debugView;
            }

            return;
        }

        // Letter shortcuts are flight controls while the camera flies.
        if (cameraController_.IsDragging())
            return;

        if (pressed(ImGuiKey_Delete))
            document_->DeleteSelection();

        if (pressed(ImGuiKey_F))
            requests.focusSelection = true;

        if (pressed(ImGuiKey_Q))
            context_->gizmo.tool = GizmoTool::Select;
        else if (pressed(ImGuiKey_W))
            context_->gizmo.tool = GizmoTool::Move;
        else if (pressed(ImGuiKey_E))
            context_->gizmo.tool = GizmoTool::Rotate;
        else if (pressed(ImGuiKey_R))
            context_->gizmo.tool = GizmoTool::Scale;
    }

    void EditorApp::DrawUi()
    {
        if (context_->fullscreenViewport)
        {
            DrawFullscreenHint();
        }
        else
        {
            DrawMainMenuBar(*context_, panels_);
            DrawToolbar(*context_);

            if (DrawStatusBar(*context_))
                consolePanel_.Focus();

            DrawDockSpace();

            for (Panel* panel : panels_)
                panel->Draw(*context_);
        }

        preferencesWindow_.Draw(*context_);
        DrawUnsavedChangesPrompt();
    }

    void EditorApp::DrawDockSpace()
    {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        const ImGuiID dockspace = ImHashStr("EditorDockSpace");

        // First run (no saved layout) or a layout request: build the dock tree before submitting it.
        // Docked sizes are absolute, so the first build waits until the window has its real (maximized) size.
        if (pendingLayout_ || !ImGui::DockBuilderGetNode(dockspace))
        {
            if (!windowShown_)
                return;

            BuildLayout(dockspace, viewport->WorkSize, pendingLayout_.value_or(LayoutPreset::Default));

            for (Panel* panel : panels_)
                panel->SetOpen(true);

            pendingLayout_.reset();
        }

        ImGui::PushStyleColor(ImGuiCol_WindowBg, GetThemeColors().background);
        ImGui::DockSpaceOverViewport(dockspace, viewport, ImGuiDockNodeFlags_None);
        ImGui::PopStyleColor();
    }

    void EditorApp::DrawFullscreenHint()
    {
        fullscreenHintSeconds_ = std::max(0.0f, fullscreenHintSeconds_ - context_->deltaSeconds);

        if (fullscreenHintSeconds_ <= 0.0f)
            return;

        const ThemeColors& colors = GetThemeColors();
        const char* text = "Press F11 to leave fullscreen";
        const ImVec2 size = ImGui::CalcTextSize(text);
        const ImVec2 center(ImGui::GetMainViewport()->Size.x * 0.5f, ImGui::GetFontSize() * 2.0f);
        const float alpha = std::min(fullscreenHintSeconds_, 1.0f);
        const ImVec2 padding(ImGui::GetFontSize(), ImGui::GetFontSize() * 0.5f);

        ImDrawList* draw = ImGui::GetForegroundDrawList();
        draw->AddRectFilled(ImVec2(center.x - size.x * 0.5f - padding.x, center.y - padding.y),
                            ImVec2(center.x + size.x * 0.5f + padding.x, center.y + size.y + padding.y),
                            ToU32(colors.panel, 0.85f * alpha), ImGui::GetStyle().WindowRounding);
        draw->AddText(ImVec2(center.x - size.x * 0.5f, center.y), ToU32(colors.text, alpha), text);
    }

    void EditorApp::DrawUnsavedChangesPrompt()
    {
        if (std::exchange(openUnsavedPrompt_, false))
            ImGui::OpenPopup(UnsavedPrompt);

        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        if (!ImGui::BeginPopupModal(UnsavedPrompt, nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
            return;

        {
            const ScopedFont bold(FontRole::Bold);
            ImGui::Text("%s  Save changes to %s?", Icon::Warning.utf8, document_->GetDisplayName().c_str());
        }

        widgets::TextDim("Your changes will be lost if you don't save them.");
        ImGui::Spacing();

        const float buttonWidth = ImGui::GetFontSize() * 7.0f;
        const ThemeColors& colors = GetThemeColors();
        ImGui::PushStyleColor(ImGuiCol_Button, colors.accentDim);

        if (ImGui::Button("Save", ImVec2(buttonWidth, 0)))
        {
            ImGui::CloseCurrentPopup();

            if (SaveScene())
                PerformAction(pendingAction_, pendingPath_);

            pendingAction_ = PendingAction::None;
        }

        ImGui::PopStyleColor();
        ImGui::SameLine();

        if (ImGui::Button("Don't Save", ImVec2(buttonWidth, 0)))
        {
            ImGui::CloseCurrentPopup();
            PerformAction(pendingAction_, pendingPath_);
            pendingAction_ = PendingAction::None;
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(buttonWidth, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape))
        {
            ImGui::CloseCurrentPopup();
            pendingAction_ = PendingAction::None;
        }

        ImGui::EndPopup();
    }

    void EditorApp::ProcessRequests()
    {
        EditorRequests requests = std::exchange(context_->requests, {});

        if (requests.openSceneDialog)
        {
            if (const auto path = ShowOpenDialog(window_.GetHandle(), FileKind::Scene, paths_.scenes))
                requests.openScene = *path;
        }

        if (requests.openScene)
            RequestAction(PendingAction::OpenScene, *requests.openScene);

        if (requests.newScene)
            RequestAction(PendingAction::NewScene);

        if (requests.saveScene)
            SaveScene();

        if (requests.saveSceneAs)
            SaveSceneAs();

        if (requests.focusSelection)
            FocusSelection();

        if (requests.showPreferences)
            preferencesWindow_.Open();

        if (requests.layout)
            pendingLayout_ = requests.layout;

        if (requests.toggleFullscreen)
        {
            context_->fullscreenViewport = !context_->fullscreenViewport;
            fullscreenHintSeconds_ = context_->fullscreenViewport ? FullscreenHintDuration : 0.0f;
        }

        if (requests.exit)
            RequestAction(PendingAction::Exit);
    }

    void EditorApp::BuildFrameScene()
    {
        const Scene& scene = document_->GetScene();
        BuildRenderScene(scene, *assets_, renderScene_);

        const ViewportSettings& viewport = context_->viewport;
        renderScene_.settings.debugView = viewport.viewMode;
        renderScene_.overlays.wireframe = viewport.wireframe;
        renderScene_.overlays.selectedEntities.clear();

        // Children of a selected entity are outlined with it.
        if (viewport.showSelectionOutline && !context_->fullscreenViewport)
        {
            const EditorSelection& selection = document_->GetSelection();

            for (EntityId id = 0; id < scene.GetEntityCount(); ++id)
            {
                for (EntityId selected : selection.GetEntities())
                {
                    if (scene.IsValid(selected) && scene.IsDescendantOf(id, selected))
                    {
                        renderScene_.overlays.selectedEntities.push_back(id);
                        break;
                    }
                }
            }
        }

        const std::array<float, 4>& outline = prefs_.outlineColor;
        renderScene_.overlays.outlineColor = { outline[0], outline[1], outline[2], outline[3] };
    }

    void EditorApp::Render()
    {
        Texture* target = context_->fullscreenViewport ? nullptr : viewportPanel_.GetTarget();
        const WindowSize windowSize = window_.GetClientSize();
        const uint32_t width = target ? target->GetWidth() : windowSize.width;
        const uint32_t height = target ? target->GetHeight() : windowSize.height;

        renderer_->Render(renderScene_, RenderView::FromCamera(camera_, width, height), imgui_.get(), target);
    }

    void EditorApp::RequestAction(PendingAction action, std::filesystem::path path)
    {
        if (!document_->IsDirty())
        {
            PerformAction(action, path);
            return;
        }

        pendingAction_ = action;
        pendingPath_ = std::move(path);
        openUnsavedPrompt_ = true;
    }

    void EditorApp::PerformAction(PendingAction action, const std::filesystem::path& path)
    {
        switch (action)
        {
            case PendingAction::NewScene:
                document_->New();
                ApplySceneCamera();
                break;
            case PendingAction::OpenScene:
                OpenScene(path);
                break;
            case PendingAction::Exit:
                window_.Close();
                break;
            case PendingAction::None:
                break;
        }
    }

    void EditorApp::OpenScene(const std::filesystem::path& path)
    {
        if (!document_->Open(path))
            return;

        ApplySceneCamera();

        if (path.extension() == ".json")
        {
            prefs_.AddRecentScene(path);
            prefs_.Save(paths_.prefsFile);
        }
    }

    bool EditorApp::SaveScene()
    {
        if (document_->GetPath().empty())
            return SaveSceneAs();

        // The current view becomes the scene's start camera, as F5 always did.
        document_->GetScene().camera = { camera_.GetPosition(), camera_.GetYawDegrees(), camera_.GetPitchDegrees(),
                                         camera_.GetFovDegrees() };
        return document_->Save();
    }

    bool EditorApp::SaveSceneAs()
    {
        const std::wstring name = ToWide(document_->GetDisplayName()) + L".json";
        const auto path = ShowSaveDialog(window_.GetHandle(), FileKind::Scene, paths_.scenes, name);

        if (!path)
            return false;

        document_->GetScene().camera = { camera_.GetPosition(), camera_.GetYawDegrees(), camera_.GetPitchDegrees(),
                                         camera_.GetFovDegrees() };

        if (!document_->SaveAs(*path))
            return false;

        prefs_.AddRecentScene(*path);
        prefs_.Save(paths_.prefsFile);
        return true;
    }

    void EditorApp::ApplySceneCamera()
    {
        const SceneCamera& start = document_->GetScene().camera;
        camera_.SetPosition(start.position);
        camera_.SetRotation(start.yawDegrees, start.pitchDegrees);
        camera_.SetFov(start.fovDegrees);
    }

    void EditorApp::FocusSelection()
    {
        const EditorSelection& selection = document_->GetSelection();
        const std::vector<EntityId> entities(selection.GetEntities().begin(), selection.GetEntities().end());

        if (const auto bounds = ComputeBounds(document_->GetScene(), renderScene_, entities))
            cameraController_.Focus(camera_, bounds->Center, bounds->Radius);
    }

    void EditorApp::UpdateWindowTitle()
    {
        std::wstring title = L"AtmosDX12 Editor - " + ToWide(document_->GetDisplayName());

        if (document_->IsDirty())
            title += L"*";

        if (title != windowTitle_)
        {
            window_.SetTitle(title);
            windowTitle_ = std::move(title);
        }
    }

    void EditorApp::Autosave(float deltaSeconds)
    {
        if (prefs_.autosaveMinutes <= 0 || !document_->IsDirty() || document_->GetPath().empty())
        {
            autosaveSeconds_ = 0.0f;
            return;
        }

        autosaveSeconds_ += deltaSeconds;

        if (autosaveSeconds_ < prefs_.autosaveMinutes * 60.0f)
            return;

        autosaveSeconds_ = 0.0f;
        std::filesystem::path path = document_->GetPath();
        path.replace_extension(".autosave.json");

        try
        {
            atmos::SaveScene(document_->GetScene(), path);
            log::Info("Autosaved to {}", ToUtf8(path.filename().wstring()));
        }
        catch (const std::exception& e)
        {
            log::Warning("Autosave failed: {}", e.what());
        }
    }

    std::filesystem::path EditorApp::FindStartupScene(const EditorOptions& options) const
    {
        if (!options.scenePath.empty())
            return options.scenePath;

        for (const std::filesystem::path& recent : prefs_.recentScenes)
        {
            if (std::filesystem::exists(recent))
                return recent;
        }

        const std::filesystem::path sponza = paths_.scenes / "sponza.json";
        return std::filesystem::exists(sponza) ? sponza : std::filesystem::path();
    }
}
