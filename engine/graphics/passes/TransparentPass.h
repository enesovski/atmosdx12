#pragma once

#include "graphics/d3d12/PipelineState.h"
#include "graphics/lighting/LightingTypes.h"
#include "graphics/rendergraph/RenderGraph.h"

namespace atmos
{
    class GpuScene;
    class GraphicsDevice;
    class ShaderCompiler;
    struct DrawList;

    // Forward-shaded alpha-blended materials, drawn back to front after the sky with the same
    // lighting as the deferred path. Depth is tested but not written.
    class TransparentPass
    {
    public:
        TransparentPass(const GraphicsDevice& device, const ShaderCompiler& compiler);

        void AddToGraph(RenderGraph& graph, const GpuScene& scene, const DrawList& drawList, uint32_t viewBufferIndex,
                        const LightingConstants& constants, const LightingResources& lighting, RGTexture depth,
                        RGTexture sceneColor) const;

    private:
        PipelineState pipeline_;
    };
}
