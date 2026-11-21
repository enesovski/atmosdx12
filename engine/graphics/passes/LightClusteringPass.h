#pragma once

#include "graphics/d3d12/PipelineState.h"
#include "graphics/lighting/LightingTypes.h"
#include "graphics/rendergraph/RenderGraph.h"

namespace atmos
{
    class GraphicsDevice;
    class ShaderCompiler;

    // Builds the clustered light grid in compute: per cluster, the indices of the local lights
    // whose range overlaps it. Writes the cluster buffers into `resources`.
    class LightClusteringPass
    {
    public:
        LightClusteringPass(const GraphicsDevice& device, const ShaderCompiler& compiler);

        void AddToGraph(RenderGraph& graph, const LightingConstants& constants, LightingResources& resources) const;

    private:
        PipelineState pipeline_;
    };
}
