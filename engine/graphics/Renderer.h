#pragma once

#include "graphics/SceneRenderer.h"
#include "graphics/d3d12/CommandContext.h"
#include "graphics/d3d12/FrameScheduler.h"
#include "graphics/d3d12/GpuProfiler.h"
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
        std::vector<RenderPassInfo> passes;  // gpuMilliseconds lags FramesInFlight frames
        std::vector<RenderResourceInfo> resources;
        SceneRenderStats scene;
        size_t pooledTextures = 0;
        size_t pooledBuffers = 0;
        float gpuMilliseconds = 0.0f;
        uint32_t renderWidth = 0;
        uint32_t renderHeight = 0;
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

        // `target` (optional) receives the scene instead of the back buffer, at the target's size (an editor
        // viewport); it ends up readable by pixel shaders. `overlay` (optional) is drawn last into the back
        // buffer: on top of the scene, or over the whole window when the scene went into `target`.
        void Render(const RenderScene& scene, const RenderView& view, IOverlayRenderer* overlay = nullptr,
                    Texture* target = nullptr);

        void WaitIdle();

        GraphicsDevice& GetDevice() { return device_; }
        const GraphicsDevice& GetDevice() const { return device_; }
        const RenderFrameStats& GetFrameStats() const { return frameStats_; }
        void RequestSkyLightRecapture() { sceneRenderer_->RequestSkyLightRecapture(); }

        // Logs the next frame's graph and statistics.
        void RequestFrameDump() { dumpNextFrame_ = true; }

    private:
        void AddOverlayPass(RenderGraph& graph, RGTexture backBuffer, RGTexture sceneTarget,
                            IOverlayRenderer* overlay) const;
        void UpdateFrameStats(const RenderGraph& graph, uint32_t width, uint32_t height);
        void LogFrameDump(const RenderGraph& graph) const;

        // Declaration order is construction order; destruction runs after Flush().
        GraphicsDevice device_;
        SwapChain swapChain_;
        FrameScheduler frameScheduler_;
        UploadAllocator uploadAllocator_;
        CommandContext commandContext_;
        ShaderCompiler shaderCompiler_;
        TransientResourcePool transientPool_;
        GpuProfiler gpuProfiler_;
        std::unique_ptr<SceneRenderer> sceneRenderer_;

        RenderFrameStats frameStats_;
        uint64_t frameNumber_ = 0;
        bool dumpNextFrame_ = false;
    };
}
