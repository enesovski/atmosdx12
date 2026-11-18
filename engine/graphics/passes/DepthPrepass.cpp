#include "graphics/passes/DepthPrepass.h"
#include "graphics/d3d12/CommandContext.h"
#include "graphics/d3d12/GraphicsConfig.h"
#include "graphics/d3d12/ShaderCompiler.h"

namespace atmos
{
    namespace
    {
        std::array<PipelineState, static_cast<size_t>(DrawBucket::Count)> CreatePipelines(
            const GraphicsDevice& device, const ShaderCompiler& compiler)
        {
            const auto opaqueVs = compiler.Compile(L"Mesh.hlsl", L"VSDepth", ShaderStage::Vertex);
            const auto maskedVs = compiler.Compile(L"Mesh.hlsl", L"VSDepthMasked", ShaderStage::Vertex);
            const auto maskedPs = compiler.Compile(L"Mesh.hlsl", L"PSDepthMasked", ShaderStage::Pixel);

            auto create = [&](const wchar_t* name, IDxcBlob* vs, IDxcBlob* ps, CullMode cullMode) {
                return PipelineState::CreateGraphics(device, {
                    .debugName = name,
                    .vertexShader = vs,
                    .pixelShader = ps,
                    .depthFormat = GraphicsConfig::DepthFormat,
                    .cullMode = cullMode,
                    .depthMode = DepthMode::ReadWrite,
                });
            };

            // Order matches DrawBucket.
            return {
                create(L"Depth Prepass", opaqueVs.Get(), nullptr, CullMode::Back),
                create(L"Depth Prepass Double-Sided", opaqueVs.Get(), nullptr, CullMode::None),
                create(L"Depth Prepass Masked", maskedVs.Get(), maskedPs.Get(), CullMode::Back),
                create(L"Depth Prepass Masked Double-Sided", maskedVs.Get(), maskedPs.Get(), CullMode::None),
            };
        }
    }

    DepthPrepass::DepthPrepass(const GraphicsDevice& device, const ShaderCompiler& compiler)
        : pipelines_(CreatePipelines(device, compiler))
    {
    }

    void DepthPrepass::AddToGraph(RenderGraph& graph, const GpuScene& scene, RGTexture depth) const
    {
        graph.AddPass(L"Depth Prepass",
            [&](RGBuilder& builder) {
                builder.WriteDepth(depth, LoadOp::Clear);
            },
            [this, &scene](RGContext& context) {
                for (size_t bucket = 0; bucket < pipelines_.size(); ++bucket)
                {
                    if (!scene.HasDraws(static_cast<DrawBucket>(bucket)))
                        continue;

                    context.commands.SetPipeline(pipelines_[bucket]);
                    scene.RecordDraws(context.commands, static_cast<DrawBucket>(bucket));
                }
            });
    }
}
