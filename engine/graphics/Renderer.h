#pragma once

#include "graphics/GpuScene.h"
#include "graphics/d3d12/CommandContext.h"
#include "graphics/d3d12/FrameScheduler.h"
#include "graphics/d3d12/GraphicsDevice.h"
#include "graphics/d3d12/ShaderCompiler.h"
#include "graphics/d3d12/SwapChain.h"
#include "graphics/d3d12/UploadAllocator.h"
#include "graphics/passes/DepthPrepass.h"
#include "graphics/passes/OpaquePass.h"
#include "graphics/passes/SkyPass.h"
#include "graphics/passes/TonemapPass.h"
#include "graphics/rendergraph/RenderGraph.h"
#include "graphics/rendergraph/TexturePool.h"

#include <cstdint>
#include <vector>

namespace atmos
{
    class IOverlayRenderer;
    struct RenderScene;
    struct RenderView;

    struct RenderFrameStats
    {
        std::vector<RenderPassInfo> passes;
        GpuSceneStats scene;
        size_t pooledTextures = 0;
    };

    // The engine's rendering entry point. Owns the GPU objects, runs the frame loop and
    // builds a render graph every frame; passes describe their work, the graph schedules it.
    class Renderer
    {
    public:
        Renderer(HWND window, uint32_t width, uint32_t height, const GraphicsDeviceOptions& options = {});
        ~Renderer();

        Renderer(const Renderer&) = delete;
        Renderer& operator=(const Renderer&) = delete;

        void Resize(uint32_t width, uint32_t height);

        // `overlay` (optional) is drawn last, on top of the tonemapped image.
        void Render(const RenderScene& scene, const RenderView& view, IOverlayRenderer* overlay = nullptr);

        // Blocks until the GPU has finished all submitted frames.
        void WaitIdle();

        // For systems that create GPU resources (asset manager, debug UI).
        GraphicsDevice& GetDevice() { return device_; }

        // Statistics of the most recently rendered frame.
        const RenderFrameStats& GetFrameStats() const { return frameStats_; }

        // Logs the next frame's render graph and culling statistics.
        void RequestFrameDump() { dumpNextFrame_ = true; }

    private:
        void BuildFrameGraph(RenderGraph& graph, const RenderScene& scene, IOverlayRenderer* overlay);

        // Declaration order is construction order; destruction runs after Flush().
        GraphicsDevice device_;
        SwapChain swapChain_;
        FrameScheduler frameScheduler_;
        UploadAllocator uploadAllocator_;
        CommandContext commandContext_;
        ShaderCompiler shaderCompiler_;

        TexturePool texturePool_;
        GpuScene gpuScene_;

        DepthPrepass depthPrepass_;
        OpaquePass opaquePass_;
        SkyPass skyPass_;
        TonemapPass tonemapPass_;

        RenderFrameStats frameStats_;
        uint64_t frameNumber_ = 0;
        bool dumpNextFrame_ = false;
    };
}
