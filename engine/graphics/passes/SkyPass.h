#pragma once

#include "graphics/d3d12/PipelineState.h"
#include "graphics/rendergraph/RenderGraph.h"

namespace atmos
{
    class GraphicsDevice;
    class ShaderCompiler;
    struct SkyEnvironment;

    // Fullscreen procedural sky with sun disk and glow. Drawn after opaque geometry on the
    // reverse-Z far plane, so the depth test leaves only pixels no geometry covered.
    class SkyPass
    {
    public:
        SkyPass(const GraphicsDevice& device, const ShaderCompiler& compiler);

        void AddToGraph(RenderGraph& graph, const SkyEnvironment& sky, RGTexture depth, RGTexture sceneColor) const;

    private:
        PipelineState pipeline_;
    };
}
