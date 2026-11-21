#pragma once

namespace atmos
{
    class Camera;
    struct AssetStats;
    struct RenderFrameStats;
    struct SceneEnvironment;
}

// What the debug panels show and edit for one frame.
struct DebugPanelContext
{
    float deltaSeconds = 0.0f;
    const atmos::Camera& camera;
    float cameraMoveSpeed = 0.0f;
    atmos::SceneEnvironment& environment;
    const atmos::RenderFrameStats& renderStats;
    const atmos::AssetStats& assetStats;
    bool canSaveScene = false;
};

// The app's ImGui windows: frame stats, environment controls, render graph and asset progress.
class DebugPanels
{
public:
    // Returns true when the user asked to save the scene.
    bool Draw(const DebugPanelContext& context);

private:
    float smoothedFrameMs_ = 0.0f;
};
