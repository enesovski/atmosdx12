
#include "Renderer.h"
#include "RenderView.h"

#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace
{
    std::filesystem::path GetShaderPath(const wchar_t* filename)
    {
        wchar_t path[MAX_PATH]{};

        DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);

        if (length == 0 || length >= MAX_PATH)
            throw std::runtime_error("Failed to get executable path");

        return std::filesystem::path(path).parent_path() / "shaders" / filename;
    }

    void Transition(
        ID3D12GraphicsCommandList* commandList,
        ID3D12Resource* resource,
        D3D12_RESOURCE_STATES before,
        D3D12_RESOURCE_STATES after)
    {
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = resource;
        barrier.Transition.Subresource =
            D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = before;
        barrier.Transition.StateAfter = after;

        commandList->ResourceBarrier(1, &barrier);
    }
}

Renderer::Renderer(HWND window, int width, int height)
    : swapChain_(
          window,
          context_.GetFactory(),
          context_.GetDevice(),
          context_.GetCommandQueue(),
          width,
          height),
      frameScheduler_(
          context_.GetDevice(),
          context_.GetCommandQueue()),
      hdrTarget_(
          context_.GetDevice(),
          width,
          height,
          GraphicsConfig::HdrColorFormat),
      skyPass_(
          context_.GetDevice(),
          GetShaderPath(L"Sky.hlsl"),
          GraphicsConfig::HdrColorFormat),
      tonemapPass_(
          context_.GetDevice(),
          GetShaderPath(L"Tonemap.hlsl"),
          GraphicsConfig::BackBufferFormat)
{
}

Renderer::~Renderer()
{
    try
    {
        frameScheduler_.Flush();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error while waiting for GPU shutdown: "
                  << e.what() << '\n';
    }
}

void Renderer::Render(
    const RenderView& view,
    const SkySettings& skySettings)
{
    const UINT frameIndex = frameScheduler_.BeginFrame();
    const UINT backBufferIndex = swapChain_.GetBackBufferIndex();

    ID3D12GraphicsCommandList* commandList =
        frameScheduler_.GetCommandList();

    ID3D12Resource* backBuffer =
        swapChain_.GetBackBuffer(backBufferIndex);

    const D3D12_VIEWPORT viewport{
        0.0f,
        0.0f,
        static_cast<float>(view.viewportWidth),
        static_cast<float>(view.viewportHeight),
        0.0f,
        1.0f
    };

    const D3D12_RECT scissorRect{
        0, 0,
        view.viewportWidth,
        view.viewportHeight
    };

    commandList->RSSetViewports(1, &viewport);
    commandList->RSSetScissorRects(1, &scissorRect);

    // Pass 1: Render sky into the HDR texture.
    Transition(
        commandList,
        hdrTarget_.GetResource(),
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_RENDER_TARGET
    );

    const D3D12_CPU_DESCRIPTOR_HANDLE hdrRTV =
        hdrTarget_.GetRTV();

    constexpr float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };

    commandList->ClearRenderTargetView(
        hdrRTV, clearColor, 0, nullptr);

    commandList->OMSetRenderTargets(
        1, &hdrRTV, FALSE, nullptr);

    skyPass_.Record(
        commandList, frameIndex, view, skySettings);

    // The HDR texture is now read by the tonemap shader.
    Transition(
        commandList,
        hdrTarget_.GetResource(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
    );

    // Pass 2: Tone map HDR into the swap chain back buffer.
    Transition(
        commandList,
        backBuffer,
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET
    );

    const D3D12_CPU_DESCRIPTOR_HANDLE backBufferRTV =
        swapChain_.GetRenderTargetView(backBufferIndex);

    commandList->ClearRenderTargetView(
        backBufferRTV, clearColor, 0, nullptr);

    commandList->OMSetRenderTargets(
        1, &backBufferRTV, FALSE, nullptr);

    tonemapPass_.Record(commandList, hdrTarget_);

    // Prepare the back buffer for presentation.
    Transition(
        commandList,
        backBuffer,
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT
    );

    frameScheduler_.Submit();
    swapChain_.Present();
    frameScheduler_.EndFrame();
}
