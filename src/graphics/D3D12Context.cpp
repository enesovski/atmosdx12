#include "D3D12Context.h"
#include "../core/DxError.h"

#include <iostream>
#include <stdexcept>

using Microsoft::WRL::ComPtr;

D3D12Context::D3D12Context()
{
    UINT factoryFlags = 0;

#if defined(_DEBUG)
    ComPtr<ID3D12Debug> debug;

    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))
    {
        debug->EnableDebugLayer();
        factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
    }
#endif

    CheckHRESULT(CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&factory_)),
                 "Failed to create DXGI factory");

    for (UINT i = 0; ; ++i)
    {
        ComPtr<IDXGIAdapter1> candidate;
        HRESULT result = factory_->EnumAdapters1(i, &candidate);

        if (result == DXGI_ERROR_NOT_FOUND)
            break;

        CheckHRESULT(result, "Failed to enumerate GPU adapters");

        DXGI_ADAPTER_DESC1 desc{};
        CheckHRESULT(candidate->GetDesc1(&desc), "Failed to get GPU description");

        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            continue;

        if (SUCCEEDED(D3D12CreateDevice(
            candidate.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr
        )))
        {
            adapter_ = candidate;
            break;
        }
    }

    if (!adapter_)
        throw std::runtime_error("No compatible hardware Direct3D 12 adapter found");

    CheckHRESULT(D3D12CreateDevice(
        adapter_.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device_)
    ), "Failed to create Direct3D 12 device");

    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    CheckHRESULT(device_->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue_)),
                 "Failed to create command queue");

    DXGI_ADAPTER_DESC1 desc{};
    CheckHRESULT(adapter_->GetDesc1(&desc), "Failed to get GPU description");

    std::wcout << L"Selected GPU: " << desc.Description << L'\n';
    std::cout << "Direct3D 12 device initialized successfully.\n";
}
