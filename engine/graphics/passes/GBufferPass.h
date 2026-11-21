#pragma once

#include "graphics/d3d12/PipelineState.h"
#include "graphics/rendergraph/RenderGraph.h"

#include <cstdint>

namespace atmos
{
    class GpuScene;
    class GraphicsDevice;
    class ShaderCompiler;
    struct DrawList;

    struct GBufferTargets
    {
        RGTexture a;  // base color, occlusion
        RGTexture b;  // octahedral normal, shading model
        RGTexture c;  // metallic, roughness
    };

    // Writes material properties for deferred lighting (layout in shaders/Scene/GBuffer.hlsli) and
    // emissive straight into scene color. Uses EQUAL depth against the prepass.
    class GBufferPass
    {
    public:
        static constexpr DXGI_FORMAT FormatA = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        static constexpr DXGI_FORMAT FormatB = DXGI_FORMAT_R10G10B10A2_UNORM;
        static constexpr DXGI_FORMAT FormatC = DXGI_FORMAT_R8G8B8A8_UNORM;

        GBufferPass(const GraphicsDevice& device, const ShaderCompiler& compiler);

        GBufferTargets AddToGraph(RenderGraph& graph, const GpuScene& scene, const DrawList& drawList,
                                  uint32_t viewBufferIndex, RGTexture depth, RGTexture sceneColor, uint32_t width,
                                  uint32_t height) const;

    private:
        PipelineState singleSidedPipeline_;
        PipelineState doubleSidedPipeline_;
    };
}
