#pragma once

#include "ConsoleLog.h"
#include "EditorContext.h"
#include "EditorPrefs.h"
#include "ProjectPaths.h"
#include "SceneDocument.h"
#include "panels/ConsolePanel.h"
#include "panels/ContentBrowserPanel.h"
#include "panels/HierarchyPanel.h"
#include "panels/InspectorPanel.h"
#include "panels/PreferencesWindow.h"
#include "panels/ProfilerPanel.h"
#include "panels/RenderGraphPanel.h"
#include "panels/RenderSettingsPanel.h"
#include "panels/ViewportPanel.h"
#include "tools/CameraController.h"

#include "core/FrameTimer.h"
#include "graphics/RenderScene.h"
#include "graphics/d3d12/GraphicsDeviceOptions.h"
#include "platform/Input.h"
#include "platform/Window.h"
#include "scene/Camera.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace atmos
{
    class AssetManager;
    class Renderer;
}

namespace atmos::editor
{
    class ImGuiLayer;

    struct EditorOptions
    {
        GraphicsDeviceOptions device;
        std::filesystem::path scenePath;  // .json scene or a .gltf/.glb; empty: last scene, then scenes/sponza.json
        bool dumpFrame = false;
        uint32_t spawnLights = 0;         // extra random point lights, for stress tests
    };

    // The editor application: window, renderer, assets, the open scene and the editor shell
    // (menu, toolbar, dock space, panels, status bar). One Tick per frame:
    //   input -> camera -> assets -> UI (panels edit the scene) -> commit undo -> render scene -> present
    class EditorApp
    {
    public:
        EditorApp(HINSTANCE instance, const EditorOptions& options);
        ~EditorApp();

        EditorApp(const EditorApp&) = delete;
        EditorApp& operator=(const EditorApp&) = delete;

        int Run();

    private:
        enum class PendingAction
        {
            None,
            NewScene,
            OpenScene,
            Exit,
        };

        void Tick(float deltaSeconds);
        void UpdateAppearance();
        void UpdateCamera(float deltaSeconds);
        void HandleShortcuts();
        void DrawUi();
        void DrawDockSpace();
        void DrawFullscreenHint();
        void DrawUnsavedChangesPrompt();
        void ProcessRequests();
        void BuildFrameScene();
        void Render();

        // Document actions; New/Open/Exit ask about unsaved changes first.
        void RequestAction(PendingAction action, std::filesystem::path path = {});
        void PerformAction(PendingAction action, const std::filesystem::path& path);
        void OpenScene(const std::filesystem::path& path);
        bool SaveScene();
        bool SaveSceneAs();
        void ApplySceneCamera();
        void FocusSelection();
        void UpdateWindowTitle();
        void Autosave(float deltaSeconds);

        std::filesystem::path FindStartupScene(const EditorOptions& options) const;

        // Declaration order is construction order; the console is first so it captures startup logs.
        ConsoleLog console_;
        ProjectPaths paths_;
        EditorPrefs prefs_;
        Input input_;
        Window window_;
        std::unique_ptr<Renderer> renderer_;
        std::unique_ptr<AssetManager> assets_;
        std::unique_ptr<ImGuiLayer> imgui_;
        std::unique_ptr<SceneDocument> document_;

        Camera camera_;
        CameraController cameraController_;
        FrameTimer frameTimer_;
        RenderScene renderScene_;
        std::unique_ptr<EditorContext> context_;

        // Panels hold GPU resources (the viewport texture), so they are destroyed before the renderer.
        ViewportPanel viewportPanel_;
        HierarchyPanel hierarchyPanel_;
        InspectorPanel inspectorPanel_;
        RenderSettingsPanel renderSettingsPanel_;
        ContentBrowserPanel contentBrowserPanel_;
        ConsolePanel consolePanel_;
        ProfilerPanel profilerPanel_;
        RenderGraphPanel renderGraphPanel_;
        PreferencesWindow preferencesWindow_;
        std::vector<Panel*> panels_;

        PendingAction pendingAction_ = PendingAction::None;
        std::filesystem::path pendingPath_;
        bool openUnsavedPrompt_ = false;
        std::optional<LayoutPreset> pendingLayout_;
        float fullscreenHintSeconds_ = 0.0f;
        float autosaveSeconds_ = 0.0f;
        std::wstring windowTitle_;
        bool windowShown_ = false;
    };
}
