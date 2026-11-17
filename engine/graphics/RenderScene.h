#pragma once

#include <DirectXMath.h>

#include <cmath>
#include <numbers>

namespace atmos
{
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

    // The renderer-facing description of the world. It will grow render proxies
    // (meshes, lights, materials) as the engine gains geometry.
    struct RenderScene
    {
        SkyEnvironment sky;
        PostProcessSettings postProcess;
    };
}
