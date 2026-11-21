#include "graphics/passes/DeferredLightingPass.h"
#include "graphics/d3d12/CommandContext.h"
#include "graphics/d3d12/ShaderCompiler.h"

namespace atmos
{
    namespace
    {
        constexpr uint32_t GroupSize = 8;

        // Mirrors DeferredLightingConstants in Passes/DeferredLighting.hlsl.
        struct DeferredLightingConstants
        {
            uint32_t depthIndex;
            uint32_t gbufferAIndex;
            uint32_t gbufferBIndex;
            uint32_t gbufferCIndex;
            uint32_t sceneColorUav;
        };
    }

    DeferredLightingPass::DeferredLightingPass(const GraphicsDevice& device, const ShaderCompiler& compiler)
        : pipeline_(PipelineState::CreateCompute(device, {
              .debugName = L"Deferred Lighting",
              .computeShader = compiler.Compile(L"Passes/DeferredLighting.hlsl", L"CSMain", ShaderStage::Compute).Get(),
          }))
    {
    }

    void DeferredLightingPass::AddToGraph(RenderGraph& graph, const LightingConstants& constants,
                                          const LightingResources& lighting, RGTexture depth,
                                          const GBufferTargets& gbuffer, RGTexture sceneColor, uint32_t width,
                                          uint32_t height) const
    {
        graph.AddPass(L"Deferred Lighting",
            [&](RGBuilder& builder) {
                builder.ReadTexture(depth);
                builder.ReadTexture(gbuffer.a);
                builder.ReadTexture(gbuffer.b);
                builder.ReadTexture(gbuffer.c);
                builder.ReadWriteTexture(sceneColor);
                DeclareLightingReads(builder, lighting);
            },
            [this, constants, lighting, depth, gbuffer, sceneColor, width, height](RGContext& context) {
                CommandContext& commands = context.commands;
                commands.SetPipeline(pipeline_);
                commands.SetConstantBuffer(RootSlot::PassConstants, ResolveLightingConstants(context, lighting, constants));
                commands.SetRootConstants(DeferredLightingConstants{
                    context.GetTexture(depth).GetSrvIndex(),
                    context.GetTexture(gbuffer.a).GetSrvIndex(),
                    context.GetTexture(gbuffer.b).GetSrvIndex(),
                    context.GetTexture(gbuffer.c).GetSrvIndex(),
                    context.GetTexture(sceneColor).GetUavIndex(),
                });
                commands.Dispatch((width + GroupSize - 1) / GroupSize, (height + GroupSize - 1) / GroupSize);
            });
    }
}
