#pragma once

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

    // One placed mesh. Mesh and materials are owned elsewhere (asset manager) and must outlive
    // the frame. Each submesh uses materials[submesh.materialIndex].
    struct MeshInstance
    {
        const Mesh* mesh = nullptr;
        std::span<const Material* const> materials;
        DirectX::XMFLOAT4X4 world{};
    };

    // The renderer-facing description of the world: flat arrays of render proxies, rebuilt
    // from the gameplay Scene every frame (see runtime/RenderSceneBuilder).
    struct RenderScene
    {
        SkyEnvironment sky;
        PostProcessSettings postProcess;
        std::vector<MeshInstance> meshInstances;
    };
}
