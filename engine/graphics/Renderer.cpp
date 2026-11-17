#include "graphics/Renderer.h"
#include "graphics/Frustum.h"
#include "graphics/Mesh.h"
#include "graphics/RenderScene.h"
#include "graphics/RenderView.h"
#include "graphics/ShaderInterop.h"
#include "graphics/rendergraph/RenderGraph.h"
#include "core/Log.h"
#include "platform/Paths.h"

using namespace DirectX;

namespace atmos
{
    namespace
    {
        ViewConstants BuildViewConstants(const RenderView& view, FXMMATRIX viewProjection, uint32_t width,
                                         uint32_t height, uint64_t frameNumber)
        {
            ViewConstants constants{};
            constants.view = view.viewMatrix;
            constants.projection = view.projectionMatrix;
            XMStoreFloat4x4(&constants.viewProjection, viewProjection);
            XMStoreFloat4x4(&constants.inverseViewProjection, XMMatrixInverse(nullptr, viewProjection));
            constants.cameraPosition = view.position;
            constants.frameNumber = static_cast<uint32_t>(frameNumber);
            constants.viewportSize = { static_cast<float>(width), static_cast<float>(height) };
            constants.inverseViewportSize = { 1.0f / width, 1.0f / height };
            return constants;
        }
    }

    Renderer::Renderer(HWND window, uint32_t width, uint32_t height, const GraphicsDeviceOptions& options)
        : device_(options),
          swapChain_(device_, window, width, height),
          frameScheduler_(device_),
          uploadAllocator_(device_.GetNativeDevice(), GraphicsConfig::UploadBytesPerFrame,
                           GraphicsConfig::FramesInFlight),
          commandContext_(device_, uploadAllocator_),
          uploadContext_(device_),
          shaderCompiler_(GetExecutableDirectory() / "shaders"),
          texturePool_(device_),
          gpuScene_(device_),
          depthPrepass_(device_, shaderCompiler_),
          opaquePass_(device_, shaderCompiler_),
          skyPass_(device_, shaderCompiler_),
          tonemapPass_(device_, shaderCompiler_, GraphicsConfig::BackBufferFormat)
    {
        device_.LogDebugMessages();
    }

    Renderer::~Renderer()
    {
        try
        {
            frameScheduler_.Flush();
        }
        catch (const std::exception& e)
        {
            log::Error("Failed to wait for the GPU during shutdown: {}", e.what());
        }
    }

    void Renderer::Resize(uint32_t width, uint32_t height)
    {
        if (width == 0 || height == 0 || (width == swapChain_.GetWidth() && height == swapChain_.GetHeight()))
            return;

        // Transient targets follow on their own: the graph requests the new size next frame.
        frameScheduler_.Flush();
        swapChain_.Resize(width, height);
    }

    std::vector<std::unique_ptr<Mesh>> Renderer::CreateMeshes(std::span<const MeshData> meshes)
    {
        std::vector<std::unique_ptr<Mesh>> result;
        result.reserve(meshes.size());

        for (const MeshData& data : meshes)
        {
            if (data.positions.empty() || data.indices.empty())
                result.push_back(nullptr);
            else
                result.push_back(std::make_unique<Mesh>(device_, uploadContext_, data));
        }

        uploadContext_.Flush();
        device_.LogDebugMessages();
        return result;
    }

    void Renderer::Render(const RenderScene& scene, const RenderView& view)
    {
        try
        {
            const uint32_t frameIndex = frameScheduler_.BeginFrame();
            uploadAllocator_.BeginFrame(frameIndex);
            commandContext_.Begin(frameScheduler_.GetCommandList());

            const XMMATRIX viewProjection = XMLoadFloat4x4(&view.viewMatrix) * XMLoadFloat4x4(&view.projectionMatrix);
            commandContext_.SetGlobalConstantBuffer(
                RootSlot::ViewConstants,
                BuildViewConstants(view, viewProjection, swapChain_.GetWidth(), swapChain_.GetHeight(), frameNumber_));

            gpuScene_.Update(frameIndex, scene, Frustum::FromViewProjection(viewProjection));

            RenderGraph graph(texturePool_);
            BuildFrameGraph(graph, scene);
            graph.Execute(commandContext_);

            if (dumpNextFrame_)
            {
                const GpuSceneStats& stats = gpuScene_.GetStats();
                graph.LogSummary();
                log::Info("Scene: {} instances, {} culled, {} draws, {} pooled textures", stats.instances,
                          stats.culledInstances, stats.draws, texturePool_.GetTextureCount());
                dumpNextFrame_ = false;
            }

            commandContext_.End();
            frameScheduler_.Submit();
            swapChain_.Present();
            frameScheduler_.EndFrame();
            texturePool_.EndFrame();

            device_.LogDebugMessages();
            ++frameNumber_;
        }
        catch (const DxException& e)
        {
            device_.LogDebugMessages();

            if (e.IsDeviceLost())
                device_.LogDeviceRemovedInfo();

            throw;
        }
    }

    void Renderer::BuildFrameGraph(RenderGraph& graph, const RenderScene& scene)
    {
        const uint32_t width = swapChain_.GetWidth();
        const uint32_t height = swapChain_.GetHeight();

        const RGTexture backBuffer = graph.ImportTexture(swapChain_.GetCurrentBackBuffer(),
                                                         D3D12_RESOURCE_STATE_PRESENT);

        const RGTexture depth = graph.CreateTexture({
            .width = width,
            .height = height,
            .format = GraphicsConfig::DepthFormat,
            .debugName = L"Scene Depth",
        });

        const RGTexture sceneColor = graph.CreateTexture({
            .width = width,
            .height = height,
            .format = GraphicsConfig::HdrColorFormat,
            .debugName = L"Scene Color",
        });

        depthPrepass_.AddToGraph(graph, gpuScene_, depth);
        opaquePass_.AddToGraph(graph, gpuScene_, scene.sky, depth, sceneColor);
        skyPass_.AddToGraph(graph, scene.sky, depth, sceneColor);
        tonemapPass_.AddToGraph(graph, scene.postProcess, sceneColor, backBuffer);
    }
}
