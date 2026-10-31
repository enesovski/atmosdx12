#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wrl/client.h>
#include <d3d12.h>
#include <dxgi1_6.h>

#include <iostream>
#include <stdexcept>

#include "Application.h"

using Microsoft::WRL::ComPtr;

void CheckHRESULT(HRESULT result, const char* message)
{
    if (FAILED(result))
        throw std::runtime_error(message);
}

int main()
{
    try
    {
#if defined(_DEBUG)
        ComPtr<ID3D12Debug> debug;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))
            debug->EnableDebugLayer();
#endif

        ComPtr<IDXGIFactory6> factory;
        CheckHRESULT(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory)), "Failed to create DXGI factory");

        ComPtr<IDXGIAdapter1> adapter;
        for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i)
        {
            DXGI_ADAPTER_DESC1 desc{};
            adapter->GetDesc1(&desc);
            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            {
                adapter.Reset();
                continue;
            }
            if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr)))
                break;
            adapter.Reset();
        }
        if (!adapter)
            throw std::runtime_error("No compatible hardware Direct3D 12 adapter found");

        ComPtr<ID3D12Device> device;
        CheckHRESULT(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)),
                     "Failed to create Direct3D 12 device");

        DXGI_ADAPTER_DESC1 desc{};
        adapter->GetDesc1(&desc);
        std::wcout << L"Selected GPU: " << desc.Description << L'\n';
        std::cout << "Direct3D 12 device initialized successfully.\n";

        Application app(GetModuleHandleW(nullptr), 1280, 720);
        return app.Run();        

    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }
}
