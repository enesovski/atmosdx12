#pragma once

#include "graphics/d3d12/PipelineState.h"
#include "graphics/rendergraph/RenderGraph.h"

namespace atmos
{
    class GpuScene;
    class GraphicsDevice;
    class ShaderCompiler;
    struct SkyEnvironment;

    // Shades visible meshes into scene color with a temporary lambert + sun model.
    // Depth comes from DepthPrepass and is tested with EQUAL, so there is no overdraw.
    // Replaced by the deferred GBuffer + lighting passes in Phase 3.
    class OpaquePass
    {
    public:
        OpaquePass(const GraphicsDevice& device, const ShaderCompiler& compiler);

        void AddToGraph(RenderGraph& graph, const GpuScene& scene, const SkyEnvironment& sky,
                        RGTexture depth, RGTexture sceneColor) const;

    private:
        PipelineState pipeline_;
    };
}
