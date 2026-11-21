#pragma once

#include "graphics/d3d12/PipelineState.h"
#include "graphics/lighting/LightingTypes.h"
#include "graphics/passes/GBufferPass.h"
#include "graphics/rendergraph/RenderGraph.h"

namespace atmos
{
    // Lights every GBuffer pixel in compute (sun + cascaded shadows, clustered local lights with
    // atlas shadows, sky lighting) and adds the result into scene color.
    class DeferredLightingPass
    {
    public:
        DeferredLightingPass(const GraphicsDevice& device, const ShaderCompiler& compiler);

        void AddToGraph(RenderGraph& graph, const LightingConstants& constants, const LightingResources& lighting,
                        RGTexture depth, const GBufferTargets& gbuffer, RGTexture sceneColor, uint32_t width,
                        uint32_t height) const;

    private:
        PipelineState pipeline_;
    };
}
