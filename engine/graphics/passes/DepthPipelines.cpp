#include "graphics/passes/DepthPipelines.h"
#include "graphics/d3d12/CommandContext.h"
#include "graphics/d3d12/GraphicsConfig.h"
#include "graphics/d3d12/ShaderCompiler.h"

namespace atmos
{
    namespace
    {
        std::array<PipelineState, DepthBucketCount> CreatePipelines(const GraphicsDevice& device,
                                                                    const ShaderCompiler& compiler,
                                                                    const DepthPipelineSettings& settings)
        {
            const auto opaqueVs = compiler.Compile(L"Passes/DepthOnly.hlsl", L"VSDepth", ShaderStage::Vertex);
            const auto maskedVs = compiler.Compile(L"Passes/DepthOnly.hlsl", L"VSDepthMasked", ShaderStage::Vertex);
            const auto maskedPs = compiler.Compile(L"Passes/DepthOnly.hlsl", L"PSDepthMasked", ShaderStage::Pixel);
            const CullMode singleSidedCull = settings.cullBackFaces ? CullMode::Back : CullMode::None;

            auto create = [&](const wchar_t* suffix, IDxcBlob* vs, IDxcBlob* ps, CullMode cullMode) {
                return PipelineState::CreateGraphics(device, {
                    .debugName = settings.name + suffix,
                    .vertexShader = vs,
                    .pixelShader = ps,
                    .depthFormat = GraphicsConfig::DepthFormat,
                    .cullMode = cullMode,
                    .depthMode = DepthMode::ReadWrite,
                    .depthBias = settings.depthBias,
                    .slopeScaledDepthBias = settings.slopeScaledDepthBias,
                    .depthClip = settings.depthClip,
                });
            };

            // Order matches DrawBucket.
            return {
                create(L"", opaqueVs.Get(), nullptr, singleSidedCull),
                create(L" Double-Sided", opaqueVs.Get(), nullptr, CullMode::None),
                create(L" Masked", maskedVs.Get(), maskedPs.Get(), singleSidedCull),
                create(L" Masked Double-Sided", maskedVs.Get(), maskedPs.Get(), CullMode::None),
            };
        }
    }

    DepthPipelines::DepthPipelines(const GraphicsDevice& device, const ShaderCompiler& compiler,
                                   const DepthPipelineSettings& settings)
        : pipelines_(CreatePipelines(device, compiler, settings))
    {
    }

    void DepthPipelines::RecordDraws(CommandContext& commands, const GpuScene& scene, const DrawList& list,
                                     uint32_t viewBufferIndex, uint32_t viewIndex) const
    {
        for (uint32_t bucket = 0; bucket < DepthBucketCount; ++bucket)
        {
            if (list.buckets[bucket].empty())
                continue;

            commands.SetPipeline(pipelines_[bucket]);
            scene.RecordDraws(commands, list, static_cast<DrawBucket>(bucket), viewBufferIndex, viewIndex);
        }
    }
}
