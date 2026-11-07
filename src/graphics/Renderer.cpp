#include "Renderer.h"
#include "RenderView.h"

#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace
{
    std::filesystem::path GetShaderPath()
    {
        wchar_t path[MAX_PATH]{};

        DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);

        if (length == 0 || length >= MAX_PATH)
            throw std::runtime_error("Failed to get executable path");

        return std::filesystem::path(path).parent_path() / "shaders" / "Sky.hlsl";
    }
}

Renderer::Renderer(HWND window, int width, int height)
    : swapChain_(window, context_.GetFactory(), context_.GetDevice(),
                 context_.GetCommandQueue(), width, height),
      frameScheduler_(context_.GetDevice(), context_.GetCommandQueue()),
      skyPass_(context_.GetDevice(), GetShaderPath(), GraphicsConfig::BackBufferFormat)
{
}

Renderer::~Renderer()
{
    try
    {
        // Pass resources and back buffers must outlive all submitted GPU work.
        frameScheduler_.Flush();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error while waiting for GPU shutdown: " << e.what() << '\n';
    }
}

void Renderer::Render(const RenderView& view, const SkySettings& skySettings)
{
    const UINT frameIndex = frameScheduler_.BeginFrame();
    const UINT backBufferIndex = swapChain_.GetBackBufferIndex();
    ID3D12GraphicsCommandList* commandList = frameScheduler_.GetCommandList();

    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = swapChain_.GetBackBuffer(backBufferIndex);
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

    commandList->ResourceBarrier(1, &barrier);

    D3D12_CPU_DESCRIPTOR_HANDLE rtv = swapChain_.GetRenderTargetView(backBufferIndex);
    constexpr float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
    const D3D12_VIEWPORT viewport{
        0.0f, 0.0f, static_cast<float>(view.viewportWidth),
        static_cast<float>(view.viewportHeight), 0.0f, 1.0f
    };
    const D3D12_RECT scissorRect{ 0, 0, view.viewportWidth, view.viewportHeight };

    commandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
    commandList->RSSetViewports(1, &viewport);
    commandList->RSSetScissorRects(1, &scissorRect);
    commandList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

    skyPass_.Record(commandList, frameIndex, view, skySettings);

    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    commandList->ResourceBarrier(1, &barrier);

    frameScheduler_.Submit();
    swapChain_.Present();
    frameScheduler_.EndFrame();
}
