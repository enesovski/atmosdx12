
#pragma once

#include <d3d12.h>
#include <wrl/client.h>

class RenderTexture
{
public:
    RenderTexture(ID3D12Device* device, int width, int height, DXGI_FORMAT format);

    RenderTexture(const RenderTexture&) = delete;
    RenderTexture& operator=(const RenderTexture&) = delete;

    ID3D12Resource* GetResource() const { return resource_.Get(); }
    D3D12_CPU_DESCRIPTOR_HANDLE GetRTV() const;
    D3D12_GPU_DESCRIPTOR_HANDLE GetSRV() const;
    ID3D12DescriptorHeap* GetSRVHeap() const { return srvHeap_.Get(); }

private:
    Microsoft::WRL::ComPtr<ID3D12Resource> resource_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvHeap_;
};
