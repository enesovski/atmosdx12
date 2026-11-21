#include "graphics/passes/EditorOverlayPass.h"
#include "graphics/GpuScene.h"
#include "graphics/RenderScene.h"
#include "graphics/d3d12/CommandContext.h"
#include "graphics/d3d12/GraphicsConfig.h"
#include "graphics/d3d12/ShaderCompiler.h"

#include <DirectXMath.h>

namespace atmos
{
    namespace
    {
        constexpr DXGI_FORMAT MaskFormat = DXGI_FORMAT_R8_UNORM;

        // Reverse-Z: positive bias pulls lines toward the viewer, so they win against their own faces.
        constexpr int32_t WireframeDepthBias = 2000;
        constexpr float WireframeSlopeBias = 2.0f;

        // Mirrors OutlineConstants in Passes/SelectionOutline.hlsl.
        struct OutlineConstants
        {
            DirectX::XMFLOAT4 color;
            uint32_t maskIndex;
        };

        bool IsMaskedBucket(DrawBucket bucket)
        {
            return bucket == DrawBucket::Masked || bucket == DrawBucket::MaskedDoubleSided;
        }
    }

    EditorOverlayPass::EditorOverlayPass(const GraphicsDevice& device, const ShaderCompiler& compiler,
                                         DXGI_FORMAT outputFormat)
    {
        const auto meshVs = compiler.Compile(L"Passes/EditorMesh.hlsl", L"VSMesh", ShaderStage::Vertex);
        const auto maskedVs = compiler.Compile(L"Passes/EditorMesh.hlsl", L"VSMeshMasked", ShaderStage::Vertex);
        const auto wireframePs = compiler.Compile(L"Passes/EditorMesh.hlsl", L"PSWireframe", ShaderStage::Pixel);
        const auto maskPs = compiler.Compile(L"Passes/EditorMesh.hlsl", L"PSMask", ShaderStage::Pixel);
        const auto maskedPs = compiler.Compile(L"Passes/EditorMesh.hlsl", L"PSMaskMasked", ShaderStage::Pixel);
        const auto outlineVs = compiler.Compile(L"Passes/SelectionOutline.hlsl", L"VSMain", ShaderStage::Vertex);
        const auto outlinePs = compiler.Compile(L"Passes/SelectionOutline.hlsl", L"PSMain", ShaderStage::Pixel);

        wireframePipeline_ = PipelineState::CreateGraphics(device, {
            .debugName = L"Wireframe",
            .vertexShader = meshVs.Get(),
            .pixelShader = wireframePs.Get(),
            .renderTargetFormats = { outputFormat },
            .depthFormat = GraphicsConfig::DepthFormat,
            .blendMode = BlendMode::AlphaBlend,
            .depthMode = DepthMode::ReadOnly,
            .depthBias = WireframeDepthBias,
            .slopeScaledDepthBias = WireframeSlopeBias,
            .wireframe = true,
        });

        maskPipeline_ = PipelineState::CreateGraphics(device, {
            .debugName = L"Selection Mask",
            .vertexShader = meshVs.Get(),
            .pixelShader = maskPs.Get(),
            .renderTargetFormats = { MaskFormat },
        });

        maskedMaskPipeline_ = PipelineState::CreateGraphics(device, {
            .debugName = L"Selection Mask Masked",
            .vertexShader = maskedVs.Get(),
            .pixelShader = maskedPs.Get(),
            .renderTargetFormats = { MaskFormat },
        });

        outlinePipeline_ = PipelineState::CreateGraphics(device, {
            .debugName = L"Selection Outline",
            .vertexShader = outlineVs.Get(),
            .pixelShader = outlinePs.Get(),
            .renderTargetFormats = { outputFormat },
            .blendMode = BlendMode::AlphaBlend,
        });
    }

    void EditorOverlayPass::AddToGraph(RenderGraph& graph, const GpuScene& scene, const ViewportOverlays& overlays,
                                       const DrawList& cameraDraws, const DrawList& selectionDraws,
                                       uint32_t viewBufferIndex, RGTexture depth, RGTexture output, uint32_t width,
                                       uint32_t height) const
    {
        if (overlays.wireframe && cameraDraws.GetDrawCount() > 0)
            AddWireframePass(graph, scene, cameraDraws, viewBufferIndex, depth, output);

        if (selectionDraws.GetDrawCount() > 0)
            AddOutlinePasses(graph, scene, overlays, selectionDraws, viewBufferIndex, output, width, height);
    }

    void EditorOverlayPass::AddWireframePass(RenderGraph& graph, const GpuScene& scene, const DrawList& draws,
                                             uint32_t viewBufferIndex, RGTexture depth, RGTexture output) const
    {
        graph.AddPass(L"Wireframe",
            [&](RGBuilder& builder) {
                builder.ReadDepth(depth);
                builder.WriteColor(output, LoadOp::Load);
            },
            [this, &scene, &draws, viewBufferIndex](RGContext& context) {
                context.commands.SetPipeline(wireframePipeline_);

                for (uint32_t bucket = 0; bucket < static_cast<uint32_t>(DrawBucket::Count); ++bucket)
                    scene.RecordDraws(context.commands, draws, static_cast<DrawBucket>(bucket), viewBufferIndex, 0);
            });
    }

    void EditorOverlayPass::AddOutlinePasses(RenderGraph& graph, const GpuScene& scene, const ViewportOverlays& overlays,
                                             const DrawList& draws, uint32_t viewBufferIndex, RGTexture output,
                                             uint32_t width, uint32_t height) const
    {
        const RGTexture mask = graph.CreateTexture({
            .width = width,
            .height = height,
            .format = MaskFormat,
            .clearColor = { 0.0f, 0.0f, 0.0f, 0.0f },
            .debugName = L"Selection Mask",
        });

        graph.AddPass(L"Selection Mask",
            [&](RGBuilder& builder) {
                builder.WriteColor(mask, LoadOp::Clear);
            },
            [this, &scene, &draws, viewBufferIndex](RGContext& context) {
                for (uint32_t index = 0; index < static_cast<uint32_t>(DrawBucket::Count); ++index)
                {
                    const auto bucket = static_cast<DrawBucket>(index);

                    if (draws[bucket].empty())
                        continue;

                    context.commands.SetPipeline(IsMaskedBucket(bucket) ? maskedMaskPipeline_ : maskPipeline_);
                    scene.RecordDraws(context.commands, draws, bucket, viewBufferIndex, 0);
                }
            });

        const DirectX::XMFLOAT4 color = overlays.outlineColor;

        graph.AddPass(L"Selection Outline",
            [&](RGBuilder& builder) {
                builder.ReadTexture(mask);
                builder.WriteColor(output, LoadOp::Load);
            },
            [this, mask, color](RGContext& context) {
                context.commands.SetPipeline(outlinePipeline_);
                context.commands.SetRootConstants(OutlineConstants{ color, context.GetTexture(mask).GetSrvIndex() });
                context.commands.DrawFullscreenTriangle();
            });
    }
}
