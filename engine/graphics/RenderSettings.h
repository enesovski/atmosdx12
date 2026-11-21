#pragma once

#include <cstdint>

namespace atmos
{
    // Mirrors the DebugView values in shaders/Lighting/LightingCommon.hlsli.
    enum class DebugView : uint32_t
    {
        Lit,
        BaseColor,
        Normals,
        Roughness,
        Metallic,
        AmbientOcclusion,
        ShadowCascades,
        LightComplexity,
        Count,
    };

    inline constexpr const char* DebugViewNames[] = {
        "Lit", "Base color", "Normals", "Roughness", "Metallic", "Ambient occlusion", "Shadow cascades", "Light complexity",
    };

    // Renderer quality and debug switches; owned by the app, not saved with the scene.
    struct RenderSettings
    {
        DebugView debugView = DebugView::Lit;

        bool sunShadows = true;
        float shadowDistance = 60.0f;     // meters covered by the cascades
        float cascadeSplitLambda = 0.75f; // 0 linear, 1 logarithmic
        float shadowNormalOffset = 1.5f;  // in shadow-map texels
        bool localShadows = true;

        bool imageBasedLighting = true;
        float ambientIntensity = 1.0f;
    };
}
