#include "DebugPanels.h"

#include "core/StringUtils.h"
#include "graphics/Renderer.h"
#include "runtime/AssetManager.h"
#include "scene/Camera.h"
#include "scene/Scene.h"

#include <imgui.h>

namespace
{
    void DrawEnvironment(atmos::SceneEnvironment& environment)
    {
        ImGui::SliderFloat("Sun azimuth", &environment.sunAzimuthDegrees, 0.0f, 360.0f, "%.0f deg");
        ImGui::SliderFloat("Sun elevation", &environment.sunElevationDegrees, -10.0f, 90.0f, "%.1f deg");
        ImGui::SliderFloat("Sun intensity", &environment.sunIntensity, 0.0f, 10.0f);
        ImGui::SliderFloat("Exposure", &environment.exposure, 0.1f, 5.0f);
    }

    void DrawRenderStats(const atmos::RenderFrameStats& stats)
    {
        const ImVec4 liveColor(0.5f, 1.0f, 0.5f, 1.0f);
        const ImVec4 culledColor(0.6f, 0.6f, 0.6f, 1.0f);

        for (const atmos::RenderPassInfo& pass : stats.passes)
        {
            ImGui::TextColored(pass.culled ? culledColor : liveColor, "%s  %s", pass.culled ? "culled" : "live  ",
                               atmos::ToUtf8(pass.name).c_str());
        }

        ImGui::Separator();
        ImGui::Text("Instances %u  (culled %u)", stats.scene.instances, stats.scene.culledInstances);
        ImGui::Text("Draws %u   Materials %u", stats.scene.draws, stats.scene.materials);
        ImGui::Text("Pooled render targets %zu", stats.pooledTextures);
    }

    void DrawAssetStats(const atmos::AssetStats& stats)
    {
        ImGui::Text("Models   %u ready, %u loading, %u failed", stats.modelsReady, stats.modelsLoading, stats.modelsFailed);
        ImGui::Text("Textures %u ready, %u loading, %u failed", stats.texturesReady, stats.texturesLoading,
                    stats.texturesFailed);

        const uint32_t total = stats.texturesReady + stats.texturesLoading + stats.texturesFailed;

        if (total > 0)
            ImGui::ProgressBar(static_cast<float>(stats.texturesReady + stats.texturesFailed) / total);
    }
}

bool DebugPanels::Draw(const DebugPanelContext& context)
{
    smoothedFrameMs_ += (context.deltaSeconds * 1000.0f - smoothedFrameMs_) * 0.05f;
    bool saveRequested = false;

    ImGui::SetNextWindowPos({ 10, 10 }, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({ 330, 0 }, ImGuiCond_FirstUseEver);
    ImGui::Begin("AtmosDX12  (F1 hides)");

    ImGui::Text("%.2f ms  (%.0f FPS)", smoothedFrameMs_, smoothedFrameMs_ > 0.0f ? 1000.0f / smoothedFrameMs_ : 0.0f);
    const DirectX::XMFLOAT3& position = context.camera.GetPosition();
    ImGui::Text("Camera %.1f, %.1f, %.1f  speed %.1f m/s", position.x, position.y, position.z, context.cameraMoveSpeed);
    ImGui::TextDisabled("Hold RMB: look + WASD/QE, wheel = speed");

    if (ImGui::CollapsingHeader("Environment", ImGuiTreeNodeFlags_DefaultOpen))
    {
        DrawEnvironment(context.environment);

        ImGui::BeginDisabled(!context.canSaveScene);
        saveRequested = ImGui::Button("Save scene (F5)");
        ImGui::EndDisabled();
    }

    if (ImGui::CollapsingHeader("Render graph", ImGuiTreeNodeFlags_DefaultOpen))
        DrawRenderStats(context.renderStats);

    if (ImGui::CollapsingHeader("Assets", ImGuiTreeNodeFlags_DefaultOpen))
        DrawAssetStats(context.assetStats);

    ImGui::End();
    return saveRequested;
}
