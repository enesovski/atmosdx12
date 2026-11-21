#pragma once

#include "graphics/passes/DepthPipelines.h"
#include "graphics/rendergraph/RenderGraph.h"

#include <span>
#include <vector>

namespace atmos
{
    struct ShadowView;

    // Renders shadow depth: one graph pass per sun cascade (array slice) and one pass for all
    // shadow-atlas tiles of local lights. Cascades disable depth clip so casters behind the
    // near plane are pancaked onto it instead of being lost.
    class ShadowPass
    {
    public:
        ShadowPass(const GraphicsDevice& device, const ShaderCompiler& compiler);

        // `drawLists` is parallel to `views`. Returns invalid handles for features with no views.
        void AddToGraph(RenderGraph& graph, const GpuScene& scene, std::span<const ShadowView> views,
                        std::span<const DrawList> drawLists, uint32_t viewBufferIndex, RGTexture& cascadeShadowMap,
                        RGTexture& shadowAtlas) const;

    private:
        DepthPipelines cascadePipelines_;
        DepthPipelines atlasPipelines_;
    };
}
