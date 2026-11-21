#pragma once

#include "graphics/d3d12/Buffer.h"
#include "graphics/d3d12/PipelineState.h"
#include "graphics/d3d12/Texture.h"
#include "graphics/lighting/LightingTypes.h"

#include <DirectXMath.h>

#include <optional>

namespace atmos
{
    class GraphicsDevice;
    class ShaderCompiler;
    struct SkyEnvironment;

    // Image-based lighting from the sky (Unreal's SkyLight with real-time capture). Persistent
    // resources, refreshed in compute only when the sun moves:
    //   sky cube map -> irradiance SH (diffuse) + GGX-prefiltered cube (specular)
    // plus a split-sum BRDF LUT computed once.
    class SkyLight
    {
    public:
        static constexpr uint32_t CaptureSize = 64;
        static constexpr uint32_t SpecularSize = 128;
        static constexpr uint32_t SpecularMipCount = 6;
        static constexpr uint32_t BrdfLutSize = 128;

        SkyLight(GraphicsDevice& device, const ShaderCompiler& compiler);

        SkyLightResources AddToGraph(RenderGraph& graph, const SkyEnvironment& sky);

        // Captures again next frame even if the sun has not moved.
        void RequestRecapture() { capturedSunDirection_.reset(); }

    private:
        Texture skyCube_;
        Texture specularCube_;
        Texture brdfLut_;
        Buffer irradianceSH_;

        PipelineState capturePipeline_;
        PipelineState projectPipeline_;
        PipelineState prefilterPipeline_;
        PipelineState brdfPipeline_;

        bool brdfLutReady_ = false;
        std::optional<DirectX::XMFLOAT3> capturedSunDirection_;
    };
}
