#include "graphics/passes/TonemapPass.h"
#include "graphics/d3d12/CommandContext.h"
#include "graphics/d3d12/ShaderCompiler.h"
#include "graphics/d3d12/Texture.h"
#include "graphics/RenderScene.h"

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

    void TonemapPass::Record(CommandContext& context, Texture& hdrInput, Texture& output,
                             const PostProcessSettings& settings) const
    {
        const ScopedGpuEvent event(context, L"Tonemap");

        context.Transition(hdrInput, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        context.SetRenderTarget(output);
        context.SetPipeline(pipeline_);
        context.SetRootConstants(TonemapConstants{ hdrInput.GetSrvIndex(), settings.exposure });
        context.DrawFullscreenTriangle();
    }
}
