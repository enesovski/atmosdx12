#pragma once

#include "graphics/d3d12/PipelineState.h"
#include "graphics/rendergraph/RenderGraph.h"

namespace atmos
{
    class GpuScene;
    class GraphicsDevice;
    class ShaderCompiler;
    struct SkyEnvironment;

    // Shades visible meshes into scene color: material textures, normal mapping and a temporary
    // lambert + sun model. Depth comes from DepthPrepass and is tested with EQUAL, so masked
    // materials need no alpha test here and nothing is shaded twice.
    // Replaced by the deferred GBuffer + lighting passes in Phase 3.
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
