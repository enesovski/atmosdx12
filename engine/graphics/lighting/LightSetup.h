#pragma once

#include "graphics/Frustum.h"
#include "graphics/ShaderInterop.h"
#include "graphics/lighting/LightingTypes.h"

#include <cstdint>
#include <vector>

namespace atmos
{
    struct RenderScene;
    struct RenderView;

    // One shadow-map render: a cascade slice or an atlas tile.
    struct ShadowView
    {
        uint32_t viewIndex;   // into LightSetup::GetViews()
        Frustum frustum;
        bool isCascade;
        uint32_t cascade;     // slice in the cascade array
        uint32_t tileX;       // atlas pixel offset
        uint32_t tileY;
    };

    // Per-frame light and shadow setup (Unreal's ShadowSetup + light gathering):
    //  - converts scene lights to GPU lights
    //  - fits stable sun cascades to the camera frustum
    //  - assigns shadow-atlas tiles to the shadow-casting local lights nearest the camera
    // Views: 0 = camera, then cascades, then atlas tiles (6 per point light, 1 per spot light).
    class LightSetup
    {
    public:
        static constexpr uint32_t CascadeCount = 4;
        static constexpr uint32_t CascadeResolution = 2048;
        static constexpr uint32_t AtlasResolution = 4096;
        static constexpr uint32_t AtlasTileSize = 512;
        static constexpr uint32_t MaxViews = 1 + CascadeCount + (AtlasResolution / AtlasTileSize) * (AtlasResolution / AtlasTileSize);
        static constexpr uint32_t MaxLights = 1024;

        void Prepare(const RenderScene& scene, const RenderView& view);

        const std::vector<LightGpuData>& GetLights() const { return lights_; }
        const std::vector<ViewData>& GetViews() const { return views_; }
        const std::vector<ShadowView>& GetShadowViews() const { return shadowViews_; }
        bool HasSunShadows() const { return cascadeCount_ > 0; }
        bool HasLocalShadows() const { return localShadowCount_ > 0; }

        // Sun, cascade and grid parameters. Bindless indices are filled in by the caller.
        LightingConstants BuildConstants(const RenderScene& scene, uint32_t width, uint32_t height) const;

    private:
        void SetupSun(const RenderScene& scene, const RenderView& view);
        void SetupLocalLights(const RenderScene& scene, const RenderView& view);

        std::vector<LightGpuData> lights_;
        std::vector<ViewData> views_;
        std::vector<ShadowView> shadowViews_;
        DirectX::XMFLOAT4 cascadeSplits_{};
        uint32_t cascadeCount_ = 0;
        uint32_t localShadowCount_ = 0;
        DirectX::XMFLOAT3 sunDirection_{};
        DirectX::XMFLOAT3 sunColor_{};
        float sunIntensity_ = 0.0f;
    };
}
