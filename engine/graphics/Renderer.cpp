#include "graphics/Renderer.h"
#include "graphics/RenderScene.h"
#include "graphics/RenderView.h"
#include "graphics/ShaderInterop.h"
#include "core/Log.h"
#include "platform/Paths.h"

using namespace DirectX;

namespace atmos
{
    Renderer::Renderer(HWND window, uint32_t width, uint32_t height, const GraphicsDeviceOptions& options)
        : device_(options),
          swapChain_(device_, window, width, height),
          frameScheduler_(device_),
          uploadAllocator_(device_.GetNativeDevice(), GraphicsConfig::UploadBytesPerFrame,
                           GraphicsConfig::FramesInFlight),
          commandContext_(device_, uploadAllocator_),
          shaderCompiler_(GetExecutableDirectory() / "shaders"),
          skyPass_(device_, shaderCompiler_, GraphicsConfig::HdrColorFormat),
          tonemapPass_(device_, shaderCompiler_, GraphicsConfig::BackBufferFormat)
    {
        CreateRenderTargets();
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

        // Size-dependent resources may still be in use by in-flight frames.
        frameScheduler_.Flush();
        swapChain_.Resize(width, height);
        CreateRenderTargets();
    }

    void Renderer::Render(const RenderScene& scene, const RenderView& view)
    {
        try
        {
            const uint32_t frameIndex = frameScheduler_.BeginFrame();
            uploadAllocator_.BeginFrame(frameIndex);
            commandContext_.Begin(frameScheduler_.GetCommandList());

            RecordFrame(scene, view);

            commandContext_.End();
            frameScheduler_.Submit();
            swapChain_.Present();
            frameScheduler_.EndFrame();

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

    void Renderer::RecordFrame(const RenderScene& scene, const RenderView& view)
    {
        CommandContext& context = commandContext_;
        Texture& backBuffer = swapChain_.GetCurrentBackBuffer();

        context.SetGlobalConstantBuffer(RootSlot::ViewConstants, BuildViewConstants(view));

        skyPass_.Record(context, hdrTarget_, scene.sky);
        tonemapPass_.Record(context, hdrTarget_, backBuffer, scene.postProcess);

        context.Transition(backBuffer, D3D12_RESOURCE_STATE_PRESENT);
    }

    void Renderer::CreateRenderTargets()
    {
        hdrTarget_ = Texture(device_, {
            .width = swapChain_.GetWidth(),
            .height = swapChain_.GetHeight(),
            .format = GraphicsConfig::HdrColorFormat,
            .usage = TextureUsage::RenderTarget | TextureUsage::ShaderResource,
            .debugName = L"HDR Scene Color",
        });
    }

    ViewConstants Renderer::BuildViewConstants(const RenderView& view) const
    {
        const XMMATRIX viewMatrix = XMLoadFloat4x4(&view.viewMatrix);
        const XMMATRIX projectionMatrix = XMLoadFloat4x4(&view.projectionMatrix);
        const XMMATRIX viewProjection = viewMatrix * projectionMatrix;

        const float width = static_cast<float>(swapChain_.GetWidth());
        const float height = static_cast<float>(swapChain_.GetHeight());

        ViewConstants constants{};
        XMStoreFloat4x4(&constants.view, viewMatrix);
        XMStoreFloat4x4(&constants.projection, projectionMatrix);
        XMStoreFloat4x4(&constants.viewProjection, viewProjection);
        XMStoreFloat4x4(&constants.inverseViewProjection, XMMatrixInverse(nullptr, viewProjection));
        constants.cameraPosition = view.position;
        constants.frameNumber = static_cast<uint32_t>(frameNumber_);
        constants.viewportSize = { width, height };
        constants.inverseViewportSize = { 1.0f / width, 1.0f / height };

        return constants;
    }
}
