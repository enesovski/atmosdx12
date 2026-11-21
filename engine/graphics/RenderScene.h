#pragma once

#include "graphics/RenderSettings.h"

#include <DirectXMath.h>

#include <cmath>
#include <numbers>
#include <span>
#include <vector>

namespace atmos
{
    class Mesh;

    struct SkyEnvironment
    {
        // Azimuth turns from +Z toward +X; elevation is above the horizon.
        float sunAzimuthDegrees = 30.0f;
        float sunElevationDegrees = 26.1f;
        float sunIntensity = 1.0f;

        DirectX::XMFLOAT3 GetSunDirection() const
        {
            constexpr float degreesToRadians = std::numbers::pi_v<float> / 180.0f;
            const float azimuth = sunAzimuthDegrees * degreesToRadians;
            const float elevation = sunElevationDegrees * degreesToRadians;
            const float horizontal = std::cos(elevation);

            return { horizontal * std::sin(azimuth), std::sin(elevation), horizontal * std::cos(azimuth) };
        }
    };

    struct PostProcessSettings
    {
        float exposure = 1.0f;
    };

    struct Material;

    // Owned by the asset manager. Each submesh uses materials[submesh.materialIndex].
    struct MeshInstance
    {
        const Mesh* mesh = nullptr;
        std::span<const Material* const> materials;
        DirectX::XMFLOAT4X4 world{};
    };

    enum class RenderLightType : uint32_t
    {
        Point,
        Spot,
    };

    struct RenderLight
    {
        RenderLightType type = RenderLightType::Point;
        DirectX::XMFLOAT3 position{};
        DirectX::XMFLOAT3 direction{ 0.0f, 0.0f, 1.0f };
        DirectX::XMFLOAT3 color{ 1.0f, 1.0f, 1.0f };
        float intensity = 0.0f;
        float range = 0.0f;
        float innerConeDegrees = 0.0f;
        float outerConeDegrees = 0.0f;
        bool castsShadows = false;
    };

    // Renderer-facing world description, rebuilt every frame by runtime/RenderSceneBuilder.
    struct RenderScene
    {
        SkyEnvironment sky;
        PostProcessSettings postProcess;
        RenderSettings settings;
        std::vector<MeshInstance> meshInstances;
        std::vector<RenderLight> lights;
    };
}
