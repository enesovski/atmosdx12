#include "graphics/passes/GBufferPass.h"
#include "graphics/GpuScene.h"
#include "graphics/d3d12/CommandContext.h"
#include "graphics/d3d12/GraphicsConfig.h"
#include "graphics/d3d12/ShaderCompiler.h"

namespace atmos
{
    namespace
    {
        PipelineState CreatePipeline(const GraphicsDevice& device, const ShaderCompiler& compiler, CullMode cullMode)
        {
            const auto vertexShader = compiler.Compile(L"Passes/GBuffer.hlsl", L"VSMain", ShaderStage::Vertex);
            const auto pixelShader = compiler.Compile(L"Passes/GBuffer.hlsl", L"PSMain", ShaderStage::Pixel);

            return PipelineState::CreateGraphics(device, {
                .debugName = cullMode == CullMode::None ? L"GBuffer Double-Sided" : L"GBuffer",
                .vertexShader = vertexShader.Get(),
                .pixelShader = pixelShader.Get(),
                .renderTargetFormats = { GBufferPass::FormatA, GBufferPass::FormatB, GBufferPass::FormatC,
                                         GraphicsConfig::HdrColorFormat },
                .depthFormat = GraphicsConfig::DepthFormat,
                .cullMode = cullMode,
                .depthMode = DepthMode::ReadOnly,
                .depthFunc = D3D12_COMPARISON_FUNC_EQUAL,
            });
        }
    }

    GBufferPass::GBufferPass(const GraphicsDevice& device, const ShaderCompiler& compiler)
        : singleSidedPipeline_(CreatePipeline(device, compiler, CullMode::Back)),
          doubleSidedPipeline_(CreatePipeline(device, compiler, CullMode::None))
    {
    }

    GBufferTargets GBufferPass::AddToGraph(RenderGraph& graph, const GpuScene& scene, const DrawList& drawList,
                                           uint32_t viewBufferIndex, RGTexture depth, RGTexture sceneColor,
                                           uint32_t width, uint32_t height) const
    {
        auto createTarget = [&](DXGI_FORMAT format, const wchar_t* name) {
            return graph.CreateTexture({ .width = width, .height = height, .format = format, .debugName = name });
        };

        const GBufferTargets targets{
            createTarget(FormatA, L"GBuffer A"),
            createTarget(FormatB, L"GBuffer B"),
            createTarget(FormatC, L"GBuffer C"),
        };

        graph.AddPass(L"GBuffer",
            [&](RGBuilder& builder) {
                builder.ReadDepth(depth);
                builder.WriteColor(targets.a, LoadOp::DontCare);
                builder.WriteColor(targets.b, LoadOp::DontCare);
                builder.WriteColor(targets.c, LoadOp::DontCare);
                builder.WriteColor(sceneColor, LoadOp::Clear);
            },
            [this, &scene, &drawList, viewBufferIndex](RGContext& context) {
                CommandContext& commands = context.commands;

                commands.SetPipeline(singleSidedPipeline_);
                scene.RecordDraws(commands, drawList, DrawBucket::Opaque, viewBufferIndex, 0);
                scene.RecordDraws(commands, drawList, DrawBucket::Masked, viewBufferIndex, 0);

                commands.SetPipeline(doubleSidedPipeline_);
                scene.RecordDraws(commands, drawList, DrawBucket::OpaqueDoubleSided, viewBufferIndex, 0);
                scene.RecordDraws(commands, drawList, DrawBucket::MaskedDoubleSided, viewBufferIndex, 0);
            });

        return targets;
    }
}
