#pragma once

#include "ConsoleLog.h"
#include "EditorPrefs.h"
#include "ProjectPaths.h"
#include "SceneDocument.h"
#include "tools/CameraController.h"

#include "graphics/RenderScene.h"
#include "scene/Camera.h"
#include "scene/RenderSettings.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>

namespace atmos
{
    class AssetManager;
    class Renderer;
}

namespace atmos::editor
{
    enum class LayoutPreset
    {
        Default,
        Lighting,
        Profiling,
    };

    enum class GizmoTool
    {
        Select,
        Move,
        Rotate,
        Scale,
    };

    enum class GizmoSpace
    {
        World,
        Local,
    };

    struct GizmoSettings
    {
        GizmoTool tool = GizmoTool::Move;
        GizmoSpace space = GizmoSpace::World;
        bool snap = false;
        float moveSnap = 0.25f;    // meters
        float rotateSnap = 15.0f;  // degrees
        float scaleSnap = 0.1f;
    };

    // How the viewport shows the scene; editor state, not saved with it.
    struct ViewportSettings
    {
        DebugView viewMode = DebugView::Lit;
        bool showStats = true;
        bool showLightIcons = true;
        bool showLightRanges = true;  // for selected lights
        bool showSelectionOutline = true;
        bool wireframe = false;
        float resolutionScale = 1.0f;
    };

    // CPU and GPU frame times for the profiler graph.
    struct FrameHistory
    {
        static constexpr size_t Size = 300;

        std::array<float, Size> cpuMs{};
        std::array<float, Size> gpuMs{};
        size_t next = 0;
        bool paused = false;

        void Push(float cpu, float gpu)
        {
            if (paused)
                return;

            cpuMs[next] = cpu;
            gpuMs[next] = gpu;
            next = (next + 1) % Size;
        }
    };

    // Actions a panel requests but the app performs, because they may need an unsaved-changes prompt,
    // a file dialog or the window.
    struct EditorRequests
    {
        std::optional<std::filesystem::path> openScene;
        bool newScene = false;
        bool openSceneDialog = false;
        bool saveScene = false;
        bool saveSceneAs = false;
        bool exit = false;
        bool focusSelection = false;
        bool toggleFullscreen = false;
        bool showPreferences = false;
        std::optional<LayoutPreset> layout;
    };

    // Everything a panel may read or edit. Owned by EditorApp; panels hold no scene state of their own.
    struct EditorContext
    {
        SceneDocument& document;
        Renderer& renderer;
        AssetManager& assets;
        ConsoleLog& console;
        EditorPrefs& prefs;
        const ProjectPaths& paths;
        Camera& camera;
        CameraController& cameraController;
        const RenderScene& renderScene;  // built this frame: instances and lights in world space

        ViewportSettings viewport;
        GizmoSettings gizmo;
        FrameHistory history;
        EditorRequests requests;
        float deltaSeconds = 0.0f;
        bool fullscreenViewport = false;
    };

    // Where new entities appear: a few meters in front of the camera.
    inline DirectX::XMFLOAT3 GetSpawnPosition(const EditorContext& context, float distance = 4.0f)
    {
        const DirectX::XMFLOAT3 position = context.camera.GetPosition();
        const DirectX::XMFLOAT3 forward = context.camera.GetForward();
        return { position.x + forward.x * distance, position.y + forward.y * distance, position.z + forward.z * distance };
    }
}
