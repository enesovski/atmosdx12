#include "graphics/passes/ShadowPass.h"
#include "graphics/d3d12/CommandContext.h"
#include "graphics/d3d12/GraphicsConfig.h"
#include "graphics/lighting/LightSetup.h"

#include <format>

namespace atmos
{
    namespace
    {
        // Reverse-Z: negative bias pushes the stored depth away from the light.
        constexpr int32_t ShadowDepthBias = -1000;
        constexpr float ShadowSlopeBias = -2.0f;
    }

    ShadowPass::ShadowPass(const GraphicsDevice& device, const ShaderCompiler& compiler)
        : cascadePipelines_(device, compiler, { .name = L"Shadow Cascade", .depthBias = ShadowDepthBias,
                                                .slopeScaledDepthBias = ShadowSlopeBias, .depthClip = false }),
          atlasPipelines_(device, compiler, { .name = L"Shadow Atlas", .depthBias = ShadowDepthBias,
                                              .slopeScaledDepthBias = ShadowSlopeBias })
    {
    }

    void ShadowPass::AddToGraph(RenderGraph& graph, const GpuScene& scene, std::span<const ShadowView> views,
                                std::span<const DrawList> drawLists, uint32_t viewBufferIndex, uint32_t cascadeResolution,
                                RGTexture& cascadeShadowMap, RGTexture& shadowAtlas) const
    {
        cascadeShadowMap = {};
        shadowAtlas = {};

        bool hasAtlasViews = false;

        for (size_t i = 0; i < views.size(); ++i)
        {
            const ShadowView& view = views[i];

            if (!view.isCascade)
            {
                hasAtlasViews = true;
                continue;
            }

            if (!cascadeShadowMap.IsValid())
            {
                cascadeShadowMap = graph.CreateTexture({
                    .width = cascadeResolution,
                    .height = cascadeResolution,
                    .arraySize = LightSetup::CascadeCount,
                    .format = GraphicsConfig::DepthFormat,
                    .debugName = L"Sun Shadow Cascades",
                });
            }

            const DrawList* drawList = &drawLists[i];
            const RGTexture target = cascadeShadowMap;

            graph.AddPass(std::format(L"Shadow Cascade {}", view.cascade),
                [&](RGBuilder& builder) {
                    builder.WriteDepth(target, LoadOp::Clear, view.cascade);
                },
                [this, &scene, drawList, viewBufferIndex, viewIndex = view.viewIndex](RGContext& context) {
                    cascadePipelines_.RecordDraws(context.commands, scene, *drawList, viewBufferIndex, viewIndex);
                });
        }

        if (!hasAtlasViews)
            return;

        shadowAtlas = graph.CreateTexture({
            .width = LightSetup::AtlasResolution,
            .height = LightSetup::AtlasResolution,
            .format = GraphicsConfig::DepthFormat,
            .debugName = L"Shadow Atlas",
        });

        graph.AddPass(L"Shadow Atlas",
            [&](RGBuilder& builder) {
                builder.WriteDepth(shadowAtlas, LoadOp::Clear);
            },
            [this, &scene, views, drawLists, viewBufferIndex](RGContext& context) {
                for (size_t i = 0; i < views.size(); ++i)
                {
                    if (views[i].isCascade)
                        continue;

                    context.commands.SetViewport(views[i].tileX, views[i].tileY, LightSetup::AtlasTileSize,
                                                 LightSetup::AtlasTileSize);
                    atlasPipelines_.RecordDraws(context.commands, scene, drawLists[i], viewBufferIndex, views[i].viewIndex);
                }
            });
    }
}
