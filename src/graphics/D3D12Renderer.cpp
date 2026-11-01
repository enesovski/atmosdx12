
#include "D3D12Renderer.h"
#include "passes/SkyPass.h"

#include <filesystem>
#include <memory>
#include <stdexcept>

using Microsoft::WRL::ComPtr;

namespace
{
    void Check(HRESULT result, const char* message)
    {
        if (FAILED(result))
            throw std::runtime_error(message);
    }

    std::filesystem::path GetShaderPath()
    {
        wchar_t path[MAX_PATH]{};

        DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);

        if (length == 0 || length >= MAX_PATH)
            throw std::runtime_error("Failed to get executable path");

        return std::filesystem::path(path).parent_path()
            / "shaders" / "Sky.hlsl";
    }
}

D3D12Renderer::D3D12Renderer(HWND window, IDXGIFactory6* factory, ID3D12Device* device, int width, int height)
    : device_(device)
{
    CreateCommandObjects();
    CreateSwapChain(window, factory, width, height);
    CreateRenderTargets();

    skyPass_ = std::make_unique<SkyPass>(
        device_.Get(),
        GetShaderPath(),
        DXGI_FORMAT_R8G8B8A8_UNORM,
        width,
        height
    );

    viewport_.TopLeftX = 0.0f;
    viewport_.TopLeftY = 0.0f;
    viewport_.Width = static_cast<float>(width);
    viewport_.Height = static_cast<float>(height);
    viewport_.MinDepth = 0.0f;
    viewport_.MaxDepth = 1.0f;

    scissorRect_ = {
        0, 0,
        static_cast<LONG>(width),
        static_cast<LONG>(height)
    };

    Check(device_->CreateFence(
        0,
        D3D12_FENCE_FLAG_NONE,
        IID_PPV_ARGS(&fence_)
    ), "Failed to create fence");

    fenceEvent_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);

    if (!fenceEvent_)
        throw std::runtime_error("Failed to create fence event");
}

D3D12Renderer::~D3D12Renderer()
{
    if (fenceEvent_)
    {
        try
        {
            WaitForGPU();
        }
        catch (...)
        {
            // Avoid throwing during GPU shutdown.
        }

        CloseHandle(fenceEvent_);
    }
}

void D3D12Renderer::CreateCommandObjects()
{
    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    Check(device_->CreateCommandQueue(
        &queueDesc,
        IID_PPV_ARGS(&commandQueue_)
    ), "Failed to create command queue");

    Check(device_->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(&commandAllocator_)
    ), "Failed to create command allocator");

    Check(device_->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        commandAllocator_.Get(),
        nullptr,
        IID_PPV_ARGS(&commandList_)
    ), "Failed to create command list");

    Check(commandList_->Close(), "Failed to close initial command list");
}

void D3D12Renderer::CreateSwapChain(HWND window, IDXGIFactory6* factory, int width, int height)
{
    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.Width = static_cast<UINT>(width);
    desc.Height = static_cast<UINT>(height);
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = FrameCount;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    ComPtr<IDXGISwapChain1> swapChain;

    Check(factory->CreateSwapChainForHwnd(
        commandQueue_.Get(),
        window,
        &desc,
        nullptr,
        nullptr,
        &swapChain
    ), "Failed to create swap chain");

    Check(swapChain.As(&swapChain_), "Failed to query swap chain 3");

    Check(factory->MakeWindowAssociation(
        window,
        DXGI_MWA_NO_ALT_ENTER
    ), "Failed to disable DXGI Alt+Enter handling");
}

void D3D12Renderer::CreateRenderTargets()
{
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
    heapDesc.NumDescriptors = FrameCount;
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

    Check(device_->CreateDescriptorHeap(
        &heapDesc,
        IID_PPV_ARGS(&rtvHeap_)
    ), "Failed to create RTV heap");

    rtvDescriptorSize_ = device_->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV
    );

    D3D12_CPU_DESCRIPTOR_HANDLE handle =
        rtvHeap_->GetCPUDescriptorHandleForHeapStart();

    for (UINT i = 0; i < FrameCount; ++i)
    {
        Check(swapChain_->GetBuffer(
            i,
            IID_PPV_ARGS(&renderTargets_[i])
        ), "Failed to acquire back buffer");

        device_->CreateRenderTargetView(
            renderTargets_[i].Get(),
            nullptr,
            handle
        );

        handle.ptr += rtvDescriptorSize_;
    }
}

void D3D12Renderer::Render(const Camera& camera)
{
    const float elapsedSeconds = clock_.GetElapsedSeconds();
    const UINT frameIndex = swapChain_->GetCurrentBackBufferIndex();

    Check(commandAllocator_->Reset(), "Failed to reset command allocator");

    Check(commandList_->Reset(
        commandAllocator_.Get(),
        nullptr
    ), "Failed to reset command list");

    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = renderTargets_[frameIndex].Get();
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

    // Transition the back buffer before rendering.
    commandList_->ResourceBarrier(1, &barrier);

    D3D12_CPU_DESCRIPTOR_HANDLE rtv =
        rtvHeap_->GetCPUDescriptorHandleForHeapStart();

    rtv.ptr += static_cast<SIZE_T>(frameIndex) * rtvDescriptorSize_;

    constexpr float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };

    commandList_->ClearRenderTargetView(rtv, clearColor, 0, nullptr);

    commandList_->RSSetViewports(1, &viewport_);
    commandList_->RSSetScissorRects(1, &scissorRect_);
    commandList_->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

    skyPass_->Render(commandList_.Get(), camera, elapsedSeconds);

    // Make the completed back buffer available for presentation.
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;

    commandList_->ResourceBarrier(1, &barrier);

    Check(commandList_->Close(), "Failed to close command list");

    ID3D12CommandList* lists[] = { commandList_.Get() };
    commandQueue_->ExecuteCommandLists(1, lists);

    Check(swapChain_->Present(1, 0), "Failed to present frame");

    WaitForGPU();
}

void D3D12Renderer::WaitForGPU()
{
    const UINT64 value = ++fenceValue_;

    Check(commandQueue_->Signal(
        fence_.Get(),
        value
    ), "Failed to signal fence");

    if (fence_->GetCompletedValue() < value)
    {
        Check(fence_->SetEventOnCompletion(
            value,
            fenceEvent_
        ), "Failed to register fence event");

        if (WaitForSingleObject(fenceEvent_, INFINITE) != WAIT_OBJECT_0)
            throw std::runtime_error("Failed while waiting for GPU");
    }
}
