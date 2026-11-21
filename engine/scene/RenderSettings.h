#pragma once

#include <cstdint>

namespace atmos
{
    // Mirrors the DebugView values in shaders/Lighting/LightingCommon.hlsli.
    enum class DebugView : uint32_t
    {
        Lit,
        LightingOnly,  // grey albedo, normal maps kept
        BaseColor,
        Normals,
        Roughness,
        Metallic,
        AmbientOcclusion,
        ShadowCascades,
        LightComplexity,
        Count,
    };

    // Mirrors the ShadowFilter values in shaders/Lighting/Shadows.hlsli.
    enum class ShadowFilter : uint32_t
    {
        Hard,
        Pcf3x3,
    };

    // Renderer quality switches, saved with the scene (Unity's Lighting settings / an Unreal
    // post-process volume). Plain data, so the scene stays graphics-free.
    struct RenderSettings
    {
        DebugView debugView = DebugView::Lit;  // viewport state, not saved

        bool sunShadows = true;
        float shadowDistance = 60.0f;      // meters covered by the cascades
        float cascadeSplitLambda = 0.75f;  // 0 linear, 1 logarithmic
        uint32_t cascadeResolution = 2048;
        ShadowFilter shadowFilter = ShadowFilter::Pcf3x3;
        float shadowNormalOffset = 1.5f;   // in shadow-map texels

        bool localShadows = true;
        uint32_t maxShadowedLights = 8;    // nearest to the camera win atlas tiles

        bool imageBasedLighting = true;
        float ambientIntensity = 1.0f;     // sky diffuse
        float reflectionIntensity = 1.0f;  // sky specular
    };
}
