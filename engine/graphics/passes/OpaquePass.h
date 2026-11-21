#pragma once

#include "graphics/d3d12/PipelineState.h"
#include "graphics/rendergraph/RenderGraph.h"

namespace atmos
{
    class GpuScene;
    class GraphicsDevice;
    class ShaderCompiler;
    struct SkyEnvironment;

    // Temporary forward shading (textures, normal maps, lambert sun) with EQUAL depth, so nothing
    // is shaded twice. Replaced by deferred lighting in Phase 3.
    class OpaquePass
    {
    public:
        OpaquePass(const GraphicsDevice& device, const ShaderCompiler& compiler);

        void AddToGraph(RenderGraph& graph, const GpuScene& scene, const SkyEnvironment& sky,
                        RGTexture depth, RGTexture sceneColor) const;

    private:
        PipelineState singleSidedPipeline_;
        PipelineState doubleSidedPipeline_;
    };
}
