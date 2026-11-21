#pragma once

#include "graphics/GpuScene.h"
#include "graphics/d3d12/PipelineState.h"
#include "graphics/rendergraph/RenderGraph.h"

#include <array>

namespace atmos
{
    class GraphicsDevice;
    class ShaderCompiler;

    // Writes depth for visible meshes; masked buckets alpha-test against base color.
    class DepthPrepass
    {
    public:
        DepthPrepass(const GraphicsDevice& device, const ShaderCompiler& compiler);

        void AddToGraph(RenderGraph& graph, const GpuScene& scene, RGTexture depth) const;

    private:
        std::array<PipelineState, static_cast<size_t>(DrawBucket::Count)> pipelines_;
    };
}
