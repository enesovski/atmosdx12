#pragma once

#include "graphics/GpuScene.h"
#include "graphics/d3d12/CommandContext.h"
#include "graphics/d3d12/FrameScheduler.h"
#include "graphics/d3d12/GraphicsDevice.h"
#include "graphics/d3d12/ShaderCompiler.h"
#include "graphics/d3d12/SwapChain.h"
#include "graphics/d3d12/UploadAllocator.h"
#include "graphics/d3d12/UploadContext.h"
#include "graphics/passes/DepthPrepass.h"
#include "graphics/passes/OpaquePass.h"
#include "graphics/passes/SkyPass.h"
#include "graphics/passes/TonemapPass.h"
#include "graphics/rendergraph/TexturePool.h"

#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace atmos
{
    class Mesh;
    struct MeshData;
    struct RenderScene;
    struct RenderView;

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
        void Render(const RenderScene& scene, const RenderView& view);

        // Uploads CPU meshes and waits for the copies. Empty entries yield nullptr.
        std::vector<std::unique_ptr<Mesh>> CreateMeshes(std::span<const MeshData> meshes);

        // Logs the next frame's render graph and culling statistics.
        void RequestFrameDump() { dumpNextFrame_ = true; }

    private:
        void BuildFrameGraph(RenderGraph& graph, const RenderScene& scene);

        // Declaration order is construction order; destruction runs after Flush().
        GraphicsDevice device_;
        SwapChain swapChain_;
        FrameScheduler frameScheduler_;
        UploadAllocator uploadAllocator_;
        CommandContext commandContext_;
        UploadContext uploadContext_;
        ShaderCompiler shaderCompiler_;

        TexturePool texturePool_;
        GpuScene gpuScene_;

        DepthPrepass depthPrepass_;
        OpaquePass opaquePass_;
        SkyPass skyPass_;
        TonemapPass tonemapPass_;

        uint64_t frameNumber_ = 0;
        bool dumpNextFrame_ = false;
    };
}
