#pragma once

#include "graphics/SceneRenderer.h"
#include "graphics/d3d12/CommandContext.h"
#include "graphics/d3d12/FrameScheduler.h"
#include "graphics/d3d12/GpuTimer.h"
#include "graphics/d3d12/GraphicsDevice.h"
#include "graphics/d3d12/ShaderCompiler.h"
#include "graphics/d3d12/SwapChain.h"
#include "graphics/d3d12/UploadAllocator.h"
#include "graphics/rendergraph/RenderGraph.h"
#include "graphics/rendergraph/TransientResourcePool.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace atmos
{
    class IOverlayRenderer;
    struct RenderScene;
    struct RenderView;

    struct RenderFrameStats
    {
        std::vector<RenderPassInfo> passes;
        SceneRenderStats scene;
        size_t pooledTextures = 0;
        size_t pooledBuffers = 0;
        float gpuMilliseconds = 0.0f;
    };

    // Owns the GPU and the frame loop: frame slots, swap chain, transient pool and the render graph.
    // What is drawn is decided by SceneRenderer.
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

        void WaitIdle();

        GraphicsDevice& GetDevice() { return device_; }
        const RenderFrameStats& GetFrameStats() const { return frameStats_; }

        // Logs the next frame's graph and statistics.
        void RequestFrameDump() { dumpNextFrame_ = true; }

    private:
        void AddOverlayPass(RenderGraph& graph, RGTexture backBuffer, IOverlayRenderer* overlay) const;
        void LogFrameDump(const RenderGraph& graph) const;

        // Declaration order is construction order; destruction runs after Flush().
        GraphicsDevice device_;
        SwapChain swapChain_;
        FrameScheduler frameScheduler_;
        UploadAllocator uploadAllocator_;
        CommandContext commandContext_;
        ShaderCompiler shaderCompiler_;
        TransientResourcePool transientPool_;
        GpuTimer gpuTimer_;
        std::unique_ptr<SceneRenderer> sceneRenderer_;

        RenderFrameStats frameStats_;
        uint64_t frameNumber_ = 0;
        bool dumpNextFrame_ = false;
    };
}
