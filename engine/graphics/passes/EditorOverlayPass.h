#pragma once

#include "graphics/d3d12/PipelineState.h"
#include "graphics/rendergraph/RenderGraph.h"

#include <dxgiformat.h>

namespace atmos
{
    class GpuScene;
    class GraphicsDevice;
    class ShaderCompiler;
    struct DrawList;
    struct ViewportOverlays;

    // Editor visualization on top of the tonemapped image (Unreal's editor primitives):
    //   Wireframe          camera draws as lines, depth-tested against the scene
    //   Selection Mask     selected draws into an R8 mask, not depth-tested
    //   Selection Outline  fullscreen edge detection of the mask, blended onto the output
    // Adds no passes when nothing is enabled.
    class EditorOverlayPass
    {
    public:
        EditorOverlayPass(const GraphicsDevice& device, const ShaderCompiler& compiler, DXGI_FORMAT outputFormat);

        void AddToGraph(RenderGraph& graph, const GpuScene& scene, const ViewportOverlays& overlays,
                        const DrawList& cameraDraws, const DrawList& selectionDraws, uint32_t viewBufferIndex,
                        RGTexture depth, RGTexture output, uint32_t width, uint32_t height) const;

    private:
        void AddWireframePass(RenderGraph& graph, const GpuScene& scene, const DrawList& draws, uint32_t viewBufferIndex,
                              RGTexture depth, RGTexture output) const;
        void AddOutlinePasses(RenderGraph& graph, const GpuScene& scene, const ViewportOverlays& overlays,
                              const DrawList& draws, uint32_t viewBufferIndex, RGTexture output, uint32_t width,
                              uint32_t height) const;

        PipelineState wireframePipeline_;
        PipelineState maskPipeline_;
        PipelineState maskedMaskPipeline_;
        PipelineState outlinePipeline_;
    };
}
