#include "graphics/passes/DepthPrepass.h"
#include "graphics/GpuScene.h"
#include "graphics/d3d12/CommandContext.h"
#include "graphics/d3d12/GraphicsConfig.h"
#include "graphics/d3d12/ShaderCompiler.h"

namespace atmos
{
    namespace
    {
        PipelineState CreatePipeline(const GraphicsDevice& device, const ShaderCompiler& compiler)
        {
            const auto vertexShader = compiler.Compile(L"Mesh.hlsl", L"VSDepth", ShaderStage::Vertex);

            return PipelineState::CreateGraphics(device, {
                .debugName = L"Depth Prepass",
                .vertexShader = vertexShader.Get(),
                .depthFormat = GraphicsConfig::DepthFormat,
                .cullMode = CullMode::Back,
                .depthMode = DepthMode::ReadWrite,
            });
        }
    }

    DepthPrepass::DepthPrepass(const GraphicsDevice& device, const ShaderCompiler& compiler)
        : pipeline_(CreatePipeline(device, compiler))
    {
    }

    void DepthPrepass::AddToGraph(RenderGraph& graph, const GpuScene& scene, RGTexture depth) const
    {
        graph.AddPass(L"Depth Prepass",
            [&](RGBuilder& builder) {
                builder.WriteDepth(depth, LoadOp::Clear);
            },
            [this, &scene](RGContext& context) {
                context.commands.SetPipeline(pipeline_);
                scene.RecordDraws(context.commands);
            });
    }
}
