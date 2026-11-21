#pragma once

#include "graphics/passes/DepthPipelines.h"
#include "graphics/rendergraph/RenderGraph.h"

namespace atmos
{
    // Writes camera depth for visible opaque and masked geometry, so later passes shade each pixel once.
    class DepthPrepass
    {
    public:
        DepthPrepass(const GraphicsDevice& device, const ShaderCompiler& compiler);

        void AddToGraph(RenderGraph& graph, const GpuScene& scene, const DrawList& drawList, uint32_t viewBufferIndex,
                        RGTexture depth) const;

    private:
        DepthPipelines pipelines_;
    };
}
