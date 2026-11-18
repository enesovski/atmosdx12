#pragma once

#include "graphics/GpuScene.h"
#include "graphics/d3d12/PipelineState.h"
#include "graphics/rendergraph/RenderGraph.h"

#include <array>

namespace atmos
{
    class GraphicsDevice;
    class ShaderCompiler;

    // Writes scene depth for all visible meshes. Opaque buckets use a position-only pipeline
    // with no pixel shader; masked buckets alpha-test against the base color texture.
    // Later passes test against this depth so every pixel is shaded once.
    class DepthPrepass
    {
    public:
        DepthPrepass(const GraphicsDevice& device, const ShaderCompiler& compiler);

        void AddToGraph(RenderGraph& graph, const GpuScene& scene, RGTexture depth) const;

    private:
        std::array<PipelineState, static_cast<size_t>(DrawBucket::Count)> pipelines_;
    };
}
