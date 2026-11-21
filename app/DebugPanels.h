#pragma once

#include <cstdint>

namespace atmos
{
    class Camera;
    class Scene;
    struct AssetStats;
    struct RenderFrameStats;
    struct RenderSettings;
}

// What the debug panels show and edit for one frame.
struct DebugPanelContext
{
    float deltaSeconds = 0.0f;
    const atmos::Camera& camera;
    float cameraMoveSpeed = 0.0f;
    atmos::Scene& scene;
    atmos::RenderSettings& renderSettings;
    const atmos::RenderFrameStats& renderStats;
    const atmos::AssetStats& assetStats;
    bool canSaveScene = false;
};

// Requests the panels hand back to the application.
struct DebugPanelActions
{
    bool saveScene = false;
    uint32_t spawnLights = 0;
};

// The app's ImGui windows: frame stats, environment, render settings, lights, render graph, assets.
class DebugPanels
{
public:
    DebugPanelActions Draw(const DebugPanelContext& context);

private:
    float smoothedFrameMs_ = 0.0f;
};
