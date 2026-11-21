#include "graphics/Renderer.h"
#include "graphics/OverlayRenderer.h"
#include "graphics/RenderScene.h"
#include "graphics/RenderView.h"
#include "graphics/ShaderInterop.h"
#include "core/Log.h"
#include "platform/Paths.h"

#include <unordered_map>

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
          shaderCompiler_(GetExecutableDirectory() / "shaders"),
          transientPool_(device_),
          gpuProfiler_(device_),
          sceneRenderer_(std::make_unique<SceneRenderer>(device_, shaderCompiler_))
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

    void Renderer::WaitIdle()
    {
        frameScheduler_.Flush();
    }

    void Renderer::Render(const RenderScene& scene, const RenderView& view, IOverlayRenderer* overlay, Texture* target)
    {
        try
        {
            const uint32_t width = target ? target->GetWidth() : swapChain_.GetWidth();
            const uint32_t height = target ? target->GetHeight() : swapChain_.GetHeight();

            const uint32_t frameIndex = frameScheduler_.BeginFrame();
            uploadAllocator_.BeginFrame(frameIndex);
            commandContext_.Begin(frameScheduler_.GetCommandList());
            gpuProfiler_.BeginFrame(frameScheduler_.GetCommandList(), frameIndex);

            const XMMATRIX viewProjection = XMLoadFloat4x4(&view.viewMatrix) * XMLoadFloat4x4(&view.projectionMatrix);
            commandContext_.SetGlobalConstantBuffer(RootSlot::ViewConstants,
                                                    BuildViewConstants(view, viewProjection, width, height, frameNumber_));

            sceneRenderer_->Prepare(frameIndex, scene, view, width, height);

            RenderGraph graph(transientPool_);
            const RGTexture backBuffer = graph.ImportTexture(swapChain_.GetCurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT);
            const RGTexture sceneTarget =
                target ? graph.ImportTexture(*target, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE) : RGTexture{};

            sceneRenderer_->AddPasses(graph, scene, target ? sceneTarget : backBuffer);
            AddOverlayPass(graph, backBuffer, sceneTarget, overlay);
            graph.Execute(commandContext_, &gpuProfiler_);
            UpdateFrameStats(graph, width, height);

            if (std::exchange(dumpNextFrame_, false))
                LogFrameDump(graph);

            commandContext_.End();
            gpuProfiler_.EndFrame(frameScheduler_.GetCommandList());
            frameScheduler_.Submit();
            swapChain_.Present();
            frameScheduler_.EndFrame();
            transientPool_.EndFrame();

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

    void Renderer::UpdateFrameStats(const RenderGraph& graph, uint32_t width, uint32_t height)
    {
        std::unordered_map<std::wstring, float> passTimes;

        for (const GpuScopeTiming& scope : gpuProfiler_.GetLastScopes())
            passTimes[scope.name] = scope.milliseconds;

        frameStats_.passes = graph.GetPassInfos();

        for (RenderPassInfo& pass : frameStats_.passes)
        {
            const auto found = passTimes.find(pass.name);
            pass.gpuMilliseconds = pass.culled || found == passTimes.end() ? 0.0f : found->second;
        }

        frameStats_.resources = graph.GetResourceInfos();
        frameStats_.scene = sceneRenderer_->GetStats();
        frameStats_.pooledTextures = transientPool_.GetTextureCount();
        frameStats_.pooledBuffers = transientPool_.GetBufferCount();
        frameStats_.gpuMilliseconds = gpuProfiler_.GetLastFrameMilliseconds();
        frameStats_.renderWidth = width;
        frameStats_.renderHeight = height;
    }

    void Renderer::AddOverlayPass(RenderGraph& graph, RGTexture backBuffer, RGTexture sceneTarget,
                                  IOverlayRenderer* overlay) const
    {
        if (!overlay)
            return;

        graph.AddPass(L"Overlay",
            [&](RGBuilder& builder) {
                // An overlay that shows the scene texture covers the whole window, so it starts from clear.
                if (sceneTarget.IsValid())
                    builder.ReadTexture(sceneTarget);

                builder.WriteColor(backBuffer, sceneTarget.IsValid() ? LoadOp::Clear : LoadOp::Load);
            },
            [overlay](RGContext& context) {
                overlay->RecordOverlay(context.commands);
            });
    }

    void Renderer::LogFrameDump(const RenderGraph& graph) const
    {
        const SceneRenderStats& stats = frameStats_.scene;
        graph.LogSummary();
        log::Info("Scene: {} instances, {} draws ({} visible), {} materials", stats.scene.instances, stats.scene.draws,
                  stats.visibleDraws, stats.scene.materials);
        log::Info("Lights: {} ({} shadowed), {} shadow views with {} draws", stats.lights, stats.shadowedLights,
                  stats.shadowViews, stats.shadowDraws);
        log::Info("Pool: {} textures, {} buffers", frameStats_.pooledTextures, frameStats_.pooledBuffers);
    }
}
