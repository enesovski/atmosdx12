#pragma once

#include <DirectXMath.h>

#include <cmath>
#include <numbers>
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

    // One placed mesh. The mesh is owned elsewhere and must outlive the scene's use of it.
    struct MeshInstance
    {
        const Mesh* mesh = nullptr;
        DirectX::XMFLOAT4X4 world{};
    };

    // The renderer-facing description of the world: flat arrays of render proxies.
    // Phase 2 adds a gameplay Scene that fills this each frame.
    struct RenderScene
    {
        SkyEnvironment sky;
        PostProcessSettings postProcess;
        std::vector<MeshInstance> meshInstances;
    };
}
