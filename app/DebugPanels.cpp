#include "DebugPanels.h"

#include "core/StringUtils.h"
#include "graphics/RenderSettings.h"
#include "graphics/Renderer.h"
#include "runtime/AssetManager.h"
#include "scene/Camera.h"
#include "scene/Scene.h"

#include <imgui.h>

namespace
{
    constexpr int MaxListedLights = 64;

    void DrawEnvironment(atmos::SceneEnvironment& environment)
    {
        ImGui::SliderFloat("Sun azimuth", &environment.sunAzimuthDegrees, 0.0f, 360.0f, "%.0f deg");
        ImGui::SliderFloat("Sun elevation", &environment.sunElevationDegrees, -10.0f, 90.0f, "%.1f deg");
        ImGui::SliderFloat("Sun intensity", &environment.sunIntensity, 0.0f, 30.0f);
        ImGui::SliderFloat("Exposure", &environment.exposure, 0.1f, 5.0f);
    }

    void DrawRenderSettings(atmos::RenderSettings& settings)
    {
        int debugView = static_cast<int>(settings.debugView);

        if (ImGui::Combo("Debug view", &debugView, atmos::DebugViewNames, static_cast<int>(atmos::DebugView::Count)))
            settings.debugView = static_cast<atmos::DebugView>(debugView);

        ImGui::Checkbox("Sun shadows", &settings.sunShadows);
        ImGui::SliderFloat("Shadow distance", &settings.shadowDistance, 10.0f, 200.0f, "%.0f m");
        ImGui::SliderFloat("Cascade split", &settings.cascadeSplitLambda, 0.0f, 1.0f);
        ImGui::SliderFloat("Normal offset", &settings.shadowNormalOffset, 0.0f, 5.0f, "%.1f texels");
        ImGui::Checkbox("Local light shadows", &settings.localShadows);
        ImGui::Checkbox("Sky lighting (IBL)", &settings.imageBasedLighting);
        ImGui::SliderFloat("Ambient intensity", &settings.ambientIntensity, 0.0f, 3.0f);
    }

    void DrawLightEditor(atmos::LightComponent& light, atmos::Transform& transform)
    {
        ImGui::DragFloat3("Position", &transform.position.x, 0.05f);
        ImGui::ColorEdit3("Color", &light.color.x);
        ImGui::DragFloat("Intensity", &light.intensity, 0.1f, 0.0f, 1000.0f);
        ImGui::DragFloat("Range", &light.range, 0.05f, 0.1f, 100.0f, "%.1f m");

        if (light.type == atmos::LightType::Spot)
        {
            ImGui::DragFloat3("Rotation", &transform.rotationDegrees.x, 0.5f);
            ImGui::SliderFloat("Inner cone", &light.innerConeDegrees, 0.0f, light.outerConeDegrees, "%.0f deg");
            ImGui::SliderFloat("Outer cone", &light.outerConeDegrees, 1.0f, 80.0f, "%.0f deg");
        }

        ImGui::Checkbox("Casts shadows", &light.castsShadows);
    }

    uint32_t DrawLights(atmos::Scene& scene, const atmos::RenderFrameStats& stats)
    {
        ImGui::Text("%u lights, %u with shadows", stats.scene.lights, stats.scene.shadowedLights);
        const uint32_t spawn = ImGui::Button("Spawn 64 lights") ? 64u : 0u;

        int listed = 0;

        for (atmos::Entity& entity : scene.GetEntities())
        {
            if (!entity.light || listed++ >= MaxListedLights)
                continue;

            ImGui::PushID(&entity);

            if (ImGui::TreeNode(entity.name.c_str()))
            {
                DrawLightEditor(*entity.light, entity.transform);
                ImGui::TreePop();
            }

            ImGui::PopID();
        }

        return spawn;
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
        ImGui::Text("Draws %u visible / %u total, %u materials", stats.scene.visibleDraws, stats.scene.scene.draws,
                    stats.scene.scene.materials);
        ImGui::Text("Shadow views %u, shadow draws %u", stats.scene.shadowViews, stats.scene.shadowDraws);
        ImGui::Text("Pooled: %zu textures, %zu buffers", stats.pooledTextures, stats.pooledBuffers);
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

DebugPanelActions DebugPanels::Draw(const DebugPanelContext& context)
{
    smoothedFrameMs_ += (context.deltaSeconds * 1000.0f - smoothedFrameMs_) * 0.05f;
    DebugPanelActions actions;

    ImGui::SetNextWindowPos({ 10, 10 }, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({ 360, 0 }, ImGuiCond_FirstUseEver);
    ImGui::Begin("AtmosDX12  (F1 hides)");

    ImGui::Text("CPU %.2f ms (%.0f FPS)   GPU %.2f ms", smoothedFrameMs_,
                smoothedFrameMs_ > 0.0f ? 1000.0f / smoothedFrameMs_ : 0.0f, context.renderStats.gpuMilliseconds);
    const DirectX::XMFLOAT3& position = context.camera.GetPosition();
    ImGui::Text("Camera %.1f, %.1f, %.1f  speed %.1f m/s", position.x, position.y, position.z, context.cameraMoveSpeed);
    ImGui::TextDisabled("Hold RMB: look + WASD/QE, wheel = speed");

    if (ImGui::CollapsingHeader("Environment", ImGuiTreeNodeFlags_DefaultOpen))
    {
        DrawEnvironment(context.scene.environment);

        ImGui::BeginDisabled(!context.canSaveScene);
        actions.saveScene = ImGui::Button("Save scene (F5)");
        ImGui::EndDisabled();
    }

    if (ImGui::CollapsingHeader("Rendering", ImGuiTreeNodeFlags_DefaultOpen))
        DrawRenderSettings(context.renderSettings);

    if (ImGui::CollapsingHeader("Lights"))
        actions.spawnLights = DrawLights(context.scene, context.renderStats);

    if (ImGui::CollapsingHeader("Render graph"))
        DrawRenderStats(context.renderStats);

    if (ImGui::CollapsingHeader("Assets"))
        DrawAssetStats(context.assetStats);

    ImGui::End();
    return actions;
}
