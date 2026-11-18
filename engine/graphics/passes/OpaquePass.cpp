#include "graphics/passes/OpaquePass.h"
#include "graphics/GpuScene.h"
#include "graphics/RenderScene.h"
#include "graphics/d3d12/CommandContext.h"
#include "graphics/d3d12/GraphicsConfig.h"
#include "graphics/d3d12/ShaderCompiler.h"

namespace atmos
{
    namespace
    {
        // Mirrors OpaquePassConstants in Mesh.hlsl.
        struct OpaquePassConstants
        {
            DirectX::XMFLOAT3 sunDirection;
            float sunIntensity;
            DirectX::XMFLOAT3 sunColor;
            float padding0;
            DirectX::XMFLOAT3 ambientColor;
            float padding1;
        };

        PipelineState CreatePipeline(const GraphicsDevice& device, const ShaderCompiler& compiler, CullMode cullMode)
        {
            const auto vertexShader = compiler.Compile(L"Mesh.hlsl", L"VSMain", ShaderStage::Vertex);
            const auto pixelShader = compiler.Compile(L"Mesh.hlsl", L"PSMain", ShaderStage::Pixel);

            return PipelineState::CreateGraphics(device, {
                .debugName = cullMode == CullMode::None ? L"Opaque Double-Sided" : L"Opaque",
                .vertexShader = vertexShader.Get(),
                .pixelShader = pixelShader.Get(),
                .renderTargetFormats = { GraphicsConfig::HdrColorFormat },
                .depthFormat = GraphicsConfig::DepthFormat,
                .cullMode = cullMode,
                .depthMode = DepthMode::ReadOnly,
                .depthFunc = D3D12_COMPARISON_FUNC_EQUAL,
            });
        }
    }

    OpaquePass::OpaquePass(const GraphicsDevice& device, const ShaderCompiler& compiler)
        : singleSidedPipeline_(CreatePipeline(device, compiler, CullMode::Back)),
          doubleSidedPipeline_(CreatePipeline(device, compiler, CullMode::None))
    {
    }

    void OpaquePass::AddToGraph(RenderGraph& graph, const GpuScene& scene, const SkyEnvironment& sky,
                                RGTexture depth, RGTexture sceneColor) const
    {
        const OpaquePassConstants constants{
            .sunDirection = sky.GetSunDirection(),
            .sunIntensity = sky.sunIntensity,
            .sunColor = { 1.0f, 0.95f, 0.85f },
            .ambientColor = { 0.12f, 0.15f, 0.22f },
        };

        graph.AddPass(L"Opaque",
            [&](RGBuilder& builder) {
                builder.ReadDepth(depth);
                builder.WriteColor(sceneColor, LoadOp::Clear);
            },
            [this, &scene, constants](RGContext& context) {
                CommandContext& commands = context.commands;

                commands.SetPipeline(singleSidedPipeline_);
                commands.SetConstantBuffer(RootSlot::PassConstants, constants);
                scene.RecordDraws(commands, DrawBucket::Opaque);
                scene.RecordDraws(commands, DrawBucket::Masked);

                // Root arguments persist across pipeline changes (same root signature).
                commands.SetPipeline(doubleSidedPipeline_);
                scene.RecordDraws(commands, DrawBucket::OpaqueDoubleSided);
                scene.RecordDraws(commands, DrawBucket::MaskedDoubleSided);
            });
    }
}
