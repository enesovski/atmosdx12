#pragma once

#include "graphics/d3d12/CommandContext.h"
#include "graphics/d3d12/FrameScheduler.h"
#include "graphics/d3d12/GraphicsDevice.h"
#include "graphics/d3d12/ShaderCompiler.h"
#include "graphics/d3d12/SwapChain.h"
#include "graphics/d3d12/Texture.h"
#include "graphics/d3d12/UploadAllocator.h"
#include "graphics/passes/SkyPass.h"
#include "graphics/passes/TonemapPass.h"

#include <cstdint>

namespace atmos
{
    struct RenderScene;
    struct RenderView;
    struct ViewConstants;

    // Owns the GPU frame: runs the frame loop and wires render targets between passes.
    // Passes decide how to draw; the renderer decides what they read and write.
    class Renderer
    {
    public:
        Renderer(HWND window, uint32_t width, uint32_t height, const GraphicsDeviceOptions& options = {});
        ~Renderer();

        Renderer(const Renderer&) = delete;
        Renderer& operator=(const Renderer&) = delete;

        void Resize(uint32_t width, uint32_t height);
        void Render(const RenderScene& scene, const RenderView& view);

    private:
        void RecordFrame(const RenderScene& scene, const RenderView& view);
        void CreateRenderTargets();
        ViewConstants BuildViewConstants(const RenderView& view) const;

        // Declaration order is construction order; destruction runs after Flush().
        GraphicsDevice device_;
        SwapChain swapChain_;
        FrameScheduler frameScheduler_;
        UploadAllocator uploadAllocator_;
        CommandContext commandContext_;
        ShaderCompiler shaderCompiler_;

        Texture hdrTarget_;

        SkyPass skyPass_;
        TonemapPass tonemapPass_;

        uint64_t frameNumber_ = 0;
    };
}
