#pragma once

#include "graphics/d3d12/PipelineState.h"
#include "graphics/rendergraph/RenderGraph.h"

namespace atmos
{
    class GpuScene;
    class GraphicsDevice;
    class ShaderCompiler;

    // Writes scene depth for all visible meshes (positions only, no pixel shader).
    // Later passes test against it so every pixel is shaded once.
    class DepthPrepass
    {
    public:
        DepthPrepass(const GraphicsDevice& device, const ShaderCompiler& compiler);

        void AddToGraph(RenderGraph& graph, const GpuScene& scene, RGTexture depth) const;

    private:
        PipelineState pipeline_;
    };
}
