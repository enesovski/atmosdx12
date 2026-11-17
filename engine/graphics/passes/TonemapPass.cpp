#include "graphics/passes/TonemapPass.h"
#include "graphics/RenderScene.h"
#include "graphics/d3d12/CommandContext.h"
#include "graphics/d3d12/ShaderCompiler.h"

#include <cstdint>

namespace atmos
{
    namespace
    {
        // Mirrors TonemapConstants in Tonemap.hlsl.
        struct TonemapConstants
        {
            uint32_t hdrTextureIndex;
            float exposure;
        };

        PipelineState CreatePipeline(const GraphicsDevice& device, const ShaderCompiler& compiler,
                                     DXGI_FORMAT outputFormat)
        {
            const auto vertexShader = compiler.Compile(L"Tonemap.hlsl", L"VSMain", ShaderStage::Vertex);
            const auto pixelShader = compiler.Compile(L"Tonemap.hlsl", L"PSMain", ShaderStage::Pixel);

            return PipelineState::CreateGraphics(device, {
                .debugName = L"Tonemap",
                .vertexShader = vertexShader.Get(),
                .pixelShader = pixelShader.Get(),
                .renderTargetFormats = { outputFormat },
            });
        }
    }

    TonemapPass::TonemapPass(const GraphicsDevice& device, const ShaderCompiler& compiler,
                             DXGI_FORMAT outputFormat)
        : pipeline_(CreatePipeline(device, compiler, outputFormat))
    {
    }

    void TonemapPass::AddToGraph(RenderGraph& graph, const PostProcessSettings& settings, RGTexture sceneColor,
                                 RGTexture output) const
    {
        const float exposure = settings.exposure;

        graph.AddPass(L"Tonemap",
            [&](RGBuilder& builder) {
                builder.ReadTexture(sceneColor);
                builder.WriteColor(output, LoadOp::DontCare);
            },
            [this, sceneColor, exposure](RGContext& context) {
                const uint32_t hdrIndex = context.GetTexture(sceneColor).GetSrvIndex();

                context.commands.SetPipeline(pipeline_);
                context.commands.SetRootConstants(TonemapConstants{ hdrIndex, exposure });
                context.commands.DrawFullscreenTriangle();
            });
    }
}
