#include "graphics/passes/LightClusteringPass.h"
#include "graphics/d3d12/CommandContext.h"
#include "graphics/d3d12/ShaderCompiler.h"

namespace atmos
{
    namespace
    {
        // Mirrors ClusteringConstants in Passes/LightClustering.hlsl.
        struct ClusteringConstants
        {
            uint32_t lightCountsUav;
            uint32_t lightIndicesUav;
        };
    }

    LightClusteringPass::LightClusteringPass(const GraphicsDevice& device, const ShaderCompiler& compiler)
        : pipeline_(PipelineState::CreateCompute(device, {
              .debugName = L"Light Clustering",
              .computeShader = compiler.Compile(L"Passes/LightClustering.hlsl", L"CSMain", ShaderStage::Compute).Get(),
          }))
    {
    }

    void LightClusteringPass::AddToGraph(RenderGraph& graph, const LightingConstants& constants,
                                         LightingResources& resources) const
    {
        resources.clusterLightCounts = graph.CreateBuffer({
            .size = LightGrid::ClusterCount * sizeof(uint32_t),
            .stride = sizeof(uint32_t),
            .debugName = L"Cluster Light Counts",
        });

        resources.clusterLightIndices = graph.CreateBuffer({
            .size = LightGrid::ClusterCount * LightGrid::MaxLightsPerCluster * sizeof(uint32_t),
            .stride = sizeof(uint32_t),
            .debugName = L"Cluster Light Indices",
        });

        const RGBuffer counts = resources.clusterLightCounts;
        const RGBuffer indices = resources.clusterLightIndices;

        graph.AddPass(L"Light Clustering",
            [&](RGBuilder& builder) {
                builder.ReadWriteBuffer(counts);
                builder.ReadWriteBuffer(indices);
            },
            [this, constants, counts, indices](RGContext& context) {
                CommandContext& commands = context.commands;
                commands.SetPipeline(pipeline_);
                commands.SetConstantBuffer(RootSlot::PassConstants, constants);
                commands.SetRootConstants(ClusteringConstants{ context.GetBuffer(counts).GetUavIndex(),
                                                               context.GetBuffer(indices).GetUavIndex() });
                commands.Dispatch(LightGrid::ClusterCount);
            });
    }
}
