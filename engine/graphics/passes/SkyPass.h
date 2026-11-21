#pragma once

#include "graphics/d3d12/PipelineState.h"
#include "graphics/rendergraph/RenderGraph.h"

namespace atmos
{
    class GraphicsDevice;
    class ShaderCompiler;
    struct SkyEnvironment;

    // Procedural sky on the far plane, drawn only where no geometry wrote depth.
    class SkyPass
    {
    public:
        SkyPass(const GraphicsDevice& device, const ShaderCompiler& compiler);

        void AddToGraph(RenderGraph& graph, const SkyEnvironment& sky, RGTexture depth, RGTexture sceneColor) const;

    private:
        PipelineState pipeline_;
    };
}
