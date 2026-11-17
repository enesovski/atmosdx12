#include "graphics/passes/SkyPass.h"
#include "graphics/d3d12/CommandContext.h"
#include "graphics/d3d12/ShaderCompiler.h"
#include "graphics/RenderScene.h"

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

        PipelineState CreatePipeline(const GraphicsDevice& device, const ShaderCompiler& compiler,
                                     DXGI_FORMAT outputFormat)
        {
            const auto vertexShader = compiler.Compile(L"Sky.hlsl", L"VSMain", ShaderStage::Vertex);
            const auto pixelShader = compiler.Compile(L"Sky.hlsl", L"PSMain", ShaderStage::Pixel);

            return PipelineState::CreateGraphics(device, {
                .debugName = L"Sky",
                .vertexShader = vertexShader.Get(),
                .pixelShader = pixelShader.Get(),
                .renderTargetFormats = { outputFormat },
            });
        }
    }

    SkyPass::SkyPass(const GraphicsDevice& device, const ShaderCompiler& compiler, DXGI_FORMAT outputFormat)
        : pipeline_(CreatePipeline(device, compiler, outputFormat))
    {
    }

    void SkyPass::Record(CommandContext& context, Texture& output, const SkyEnvironment& sky) const
    {
        const ScopedGpuEvent event(context, L"Sky");

        // The sky covers every pixel, so the target needs no clear.
        context.SetRenderTarget(output);
        context.SetPipeline(pipeline_);
        context.SetRootConstants(SkyConstants{ sky.GetSunDirection(), sky.sunIntensity });
        context.DrawFullscreenTriangle();
    }
}
