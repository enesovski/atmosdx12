#include "graphics/passes/TransparentPass.h"
#include "graphics/GpuScene.h"
#include "graphics/d3d12/CommandContext.h"
#include "graphics/d3d12/GraphicsConfig.h"
#include "graphics/d3d12/ShaderCompiler.h"

namespace atmos
{
    namespace
    {
        // Culling off: sorted per draw, so back faces of a transparent surface blend correctly enough.
        PipelineState CreatePipeline(const GraphicsDevice& device, const ShaderCompiler& compiler)
        {
            const auto vertexShader = compiler.Compile(L"Passes/Transparent.hlsl", L"VSMain", ShaderStage::Vertex);
            const auto pixelShader = compiler.Compile(L"Passes/Transparent.hlsl", L"PSMain", ShaderStage::Pixel);

            return PipelineState::CreateGraphics(device, {
                .debugName = L"Transparent",
                .vertexShader = vertexShader.Get(),
                .pixelShader = pixelShader.Get(),
                .renderTargetFormats = { GraphicsConfig::HdrColorFormat },
                .depthFormat = GraphicsConfig::DepthFormat,
                .blendMode = BlendMode::AlphaBlend,
                .cullMode = CullMode::None,
                .depthMode = DepthMode::ReadOnly,
            });
        }
    }

    TransparentPass::TransparentPass(const GraphicsDevice& device, const ShaderCompiler& compiler)
        : pipeline_(CreatePipeline(device, compiler))
    {
    }

    void TransparentPass::AddToGraph(RenderGraph& graph, const GpuScene& scene, const DrawList& drawList,
                                     uint32_t viewBufferIndex, const LightingConstants& constants,
                                     const LightingResources& lighting, RGTexture depth, RGTexture sceneColor) const
    {
        if (drawList[DrawBucket::Transparent].empty())
            return;

        graph.AddPass(L"Transparent",
            [&](RGBuilder& builder) {
                builder.ReadDepth(depth);
                builder.WriteColor(sceneColor, LoadOp::Load);
                DeclareLightingReads(builder, lighting);
            },
            [this, &scene, &drawList, viewBufferIndex, constants, lighting](RGContext& context) {
                CommandContext& commands = context.commands;
                commands.SetPipeline(pipeline_);
                commands.SetConstantBuffer(RootSlot::PassConstants, ResolveLightingConstants(context, lighting, constants));
                scene.RecordDraws(commands, drawList, DrawBucket::Transparent, viewBufferIndex, 0);
            });
    }
}
