#include "graphics/passes/DepthPrepass.h"

namespace atmos
{
    DepthPrepass::DepthPrepass(const GraphicsDevice& device, const ShaderCompiler& compiler)
        : pipelines_(device, compiler, { .name = L"Depth Prepass" })
    {
    }

    void DepthPrepass::AddToGraph(RenderGraph& graph, const GpuScene& scene, const DrawList& drawList,
                                  uint32_t viewBufferIndex, RGTexture depth) const
    {
        graph.AddPass(L"Depth Prepass",
            [&](RGBuilder& builder) {
                builder.WriteDepth(depth, LoadOp::Clear);
            },
            [this, &scene, &drawList, viewBufferIndex](RGContext& context) {
                pipelines_.RecordDraws(context.commands, scene, drawList, viewBufferIndex, 0);
            });
    }
}
