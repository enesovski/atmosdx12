
#include "D3D12Renderer.h"

#include <d3dcompiler.h>
#include <filesystem>
#include <stdexcept>
#include <string>

using Microsoft::WRL::ComPtr;

namespace
{
    void Check(HRESULT result, const char* error)
    {
        if (FAILED(result))
            throw std::runtime_error(error);
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

    ComPtr<ID3DBlob> CompileShader(const char* entryPoint, const char* target)
    {
        ComPtr<ID3DBlob> shader;
        ComPtr<ID3DBlob> errors;

        const std::wstring path = GetShaderPath().wstring();

        UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;

#if defined(_DEBUG)
        flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

        HRESULT result = D3DCompileFromFile(
            path.c_str(),
            nullptr,
            D3D_COMPILE_STANDARD_FILE_INCLUDE,
            entryPoint,
            target,
            flags,
            0,
            &shader,
            &errors
        );

        if (FAILED(result))
        {
            std::string message = "Failed to compile shader: ";

            if (errors)
            {
                message.append(
                    static_cast<const char*>(errors->GetBufferPointer()),
                    errors->GetBufferSize()
                );
            }
            else
            {
                message += "Sky.hlsl could not be loaded or compiled";
            }

            throw std::runtime_error(message);
        }

        return shader;
    }
}

D3D12Renderer::D3D12Renderer(HWND window, IDXGIFactory6* factory, ID3D12Device* device, int width, int height)
    : device_(device)
{
    CreateCommandObjects();
    CreateSwapChain(window, factory, width, height);
    CreateRenderTargets();
    CreatePipeline();

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
        0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_)
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
        }

        CloseHandle(fenceEvent_);
    }
}

void D3D12Renderer::CreateCommandObjects()
{
    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    Check(device_->CreateCommandQueue(
        &queueDesc, IID_PPV_ARGS(&commandQueue_)
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
        window, DXGI_MWA_NO_ALT_ENTER
    ), "Failed to disable DXGI Alt+Enter handling");
}

void D3D12Renderer::CreateRenderTargets()
{
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
    heapDesc.NumDescriptors = FrameCount;
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

    Check(device_->CreateDescriptorHeap(
        &heapDesc, IID_PPV_ARGS(&rtvHeap_)
    ), "Failed to create RTV heap");

    rtvDescriptorSize_ = device_->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV
    );

    D3D12_CPU_DESCRIPTOR_HANDLE handle =
        rtvHeap_->GetCPUDescriptorHandleForHeapStart();

    for (UINT i = 0; i < FrameCount; ++i)
    {
        Check(swapChain_->GetBuffer(
            i, IID_PPV_ARGS(&renderTargets_[i])
        ), "Failed to acquire back buffer");

        device_->CreateRenderTargetView(
            renderTargets_[i].Get(),
            nullptr,
            handle
        );

        handle.ptr += rtvDescriptorSize_;
    }
}

void D3D12Renderer::CreatePipeline()
{
    ComPtr<ID3DBlob> vertexShader = CompileShader("VSMain", "vs_5_1");
    ComPtr<ID3DBlob> pixelShader = CompileShader("PSMain", "ps_5_1");

    // This shader doesn't use textures, constant buffers or other resources.
    D3D12_ROOT_SIGNATURE_DESC rootDesc{};
    rootDesc.Flags =
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> errors;

    Check(D3D12SerializeRootSignature(
        &rootDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &signature,
        &errors
    ), "Failed to serialize root signature");

    Check(device_->CreateRootSignature(
        0,
        signature->GetBufferPointer(),
        signature->GetBufferSize(),
        IID_PPV_ARGS(&rootSignature_)
    ), "Failed to create root signature");

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.pRootSignature = rootSignature_.Get();

    psoDesc.VS = {
        vertexShader->GetBufferPointer(),
        vertexShader->GetBufferSize()
    };

    psoDesc.PS = {
        pixelShader->GetBufferPointer(),
        pixelShader->GetBufferSize()
    };

    psoDesc.BlendState.AlphaToCoverageEnable = FALSE;
    psoDesc.BlendState.IndependentBlendEnable = FALSE;

    D3D12_RENDER_TARGET_BLEND_DESC blendDesc{};
    blendDesc.BlendEnable = FALSE;
    blendDesc.LogicOpEnable = FALSE;
    blendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    psoDesc.BlendState.RenderTarget[0] = blendDesc;

    psoDesc.SampleMask = UINT_MAX;

    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    psoDesc.RasterizerState.FrontCounterClockwise = FALSE;
    psoDesc.RasterizerState.DepthClipEnable = TRUE;

    psoDesc.DepthStencilState.DepthEnable = FALSE;
    psoDesc.DepthStencilState.StencilEnable = FALSE;

    psoDesc.InputLayout = { nullptr, 0 };
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;

    Check(device_->CreateGraphicsPipelineState(
        &psoDesc, IID_PPV_ARGS(&pipelineState_)
    ), "Failed to create graphics pipeline state");
}

void D3D12Renderer::Render()
{
    const UINT frameIndex = swapChain_->GetCurrentBackBufferIndex();

    Check(commandAllocator_->Reset(), "Failed to reset command allocator");

    Check(commandList_->Reset(
        commandAllocator_.Get(),
        pipelineState_.Get()
    ), "Failed to reset command list");

    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = renderTargets_[frameIndex].Get();
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

    commandList_->ResourceBarrier(1, &barrier);

    D3D12_CPU_DESCRIPTOR_HANDLE rtv =
        rtvHeap_->GetCPUDescriptorHandleForHeapStart();

    rtv.ptr += static_cast<SIZE_T>(frameIndex) * rtvDescriptorSize_;

    constexpr float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };

    commandList_->ClearRenderTargetView(rtv, clearColor, 0, nullptr);

    commandList_->RSSetViewports(1, &viewport_);
    commandList_->RSSetScissorRects(1, &scissorRect_);

    commandList_->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

    commandList_->SetGraphicsRootSignature(rootSignature_.Get());
    commandList_->SetPipelineState(pipelineState_.Get());

    commandList_->IASetPrimitiveTopology(
        D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST
    );

    commandList_->DrawInstanced(3, 1, 0, 0);

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

    Check(commandQueue_->Signal(fence_.Get(), value), "Failed to signal fence");

    if (fence_->GetCompletedValue() < value)
    {
        Check(fence_->SetEventOnCompletion(
            value, fenceEvent_
        ), "Failed to register fence event");

        if (WaitForSingleObject(fenceEvent_, INFINITE) != WAIT_OBJECT_0)
            throw std::runtime_error("Failed while waiting for GPU");
    }
}
