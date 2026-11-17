#include "graphics/passes/SkyPass.h"
#include "graphics/RenderScene.h"
#include "graphics/d3d12/CommandContext.h"
#include "graphics/d3d12/GraphicsConfig.h"
#include "graphics/d3d12/ShaderCompiler.h"

namespace atmos
{
    namespace
    {
        // Mirrors SkyConstants in Sky.hlsl.
        struct SkyConstants
        {
            DirectX::XMFLOAT3 sunDirection;
            float sunIntensity;
        };

        PipelineState CreatePipeline(const GraphicsDevice& device, const ShaderCompiler& compiler)
        {
            const auto vertexShader = compiler.Compile(L"Sky.hlsl", L"VSMain", ShaderStage::Vertex);
            const auto pixelShader = compiler.Compile(L"Sky.hlsl", L"PSMain", ShaderStage::Pixel);

            // Sky depth is 0 (far plane); GREATER_EQUAL passes only where depth is still cleared to 0.
            return PipelineState::CreateGraphics(device, {
                .debugName = L"Sky",
                .vertexShader = vertexShader.Get(),
                .pixelShader = pixelShader.Get(),
                .renderTargetFormats = { GraphicsConfig::HdrColorFormat },
                .depthFormat = GraphicsConfig::DepthFormat,
                .depthMode = DepthMode::ReadOnly,
            });
        }
    }

    SkyPass::SkyPass(const GraphicsDevice& device, const ShaderCompiler& compiler)
        : pipeline_(CreatePipeline(device, compiler))
    {
    }

    void SkyPass::AddToGraph(RenderGraph& graph, const SkyEnvironment& sky, RGTexture depth,
                             RGTexture sceneColor) const
    {
        const SkyConstants constants{ sky.GetSunDirection(), sky.sunIntensity };

        graph.AddPass(L"Sky",
            [&](RGBuilder& builder) {
                builder.ReadDepth(depth);
                builder.WriteColor(sceneColor, LoadOp::Load);
            },
            [this, constants](RGContext& context) {
                context.commands.SetPipeline(pipeline_);
                context.commands.SetRootConstants(constants);
                context.commands.DrawFullscreenTriangle();
            });
    }
}
