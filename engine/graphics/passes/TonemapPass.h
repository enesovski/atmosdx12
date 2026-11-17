#pragma once

#include "graphics/d3d12/PipelineState.h"
#include "graphics/rendergraph/RenderGraph.h"

namespace atmos
{
    class GraphicsDevice;
    class ShaderCompiler;
    struct PostProcessSettings;

    // Exposure + tone curve + sRGB encode from the HDR scene color to a display target.
    class TonemapPass
    {
    public:
        TonemapPass(const GraphicsDevice& device, const ShaderCompiler& compiler, DXGI_FORMAT outputFormat);

        void AddToGraph(RenderGraph& graph, const PostProcessSettings& settings, RGTexture sceneColor,
                        RGTexture output) const;

    private:
        PipelineState pipeline_;
    };
}
