
#include "RenderTexture.h"
#include "DxError.h"

#include <stdexcept>

using Microsoft::WRL::ComPtr;

RenderTexture::RenderTexture(
    ID3D12Device* device, int width, int height, DXGI_FORMAT format)
{
    if (width <= 0 || height <= 0)
        throw std::invalid_argument("Invalid render texture dimensions");

    D3D12_HEAP_PROPERTIES heapProperties{};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = static_cast<UINT64>(width);
    desc.Height = static_cast<UINT>(height);
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    D3D12_CLEAR_VALUE clearValue{};
    clearValue.Format = format;
    clearValue.Color[3] = 1.0f;

    CheckHRESULT(device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        &clearValue,
        IID_PPV_ARGS(&resource_)
    ), "Failed to create render texture");

    D3D12_DESCRIPTOR_HEAP_DESC rtvDesc{};
    rtvDesc.NumDescriptors = 1;
    rtvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

    CheckHRESULT(device->CreateDescriptorHeap(
        &rtvDesc, IID_PPV_ARGS(&rtvHeap_)
    ), "Failed to create render texture RTV heap");

    device->CreateRenderTargetView(resource_.Get(), nullptr, GetRTV());

    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc{};
    srvHeapDesc.NumDescriptors = 1;
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    CheckHRESULT(device->CreateDescriptorHeap(
        &srvHeapDesc, IID_PPV_ARGS(&srvHeap_)
    ), "Failed to create render texture SRV heap");

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels = 1;

    device->CreateShaderResourceView(
        resource_.Get(),
        &srvDesc,
        srvHeap_->GetCPUDescriptorHandleForHeapStart()
    );
}

D3D12_CPU_DESCRIPTOR_HANDLE RenderTexture::GetRTV() const
{
    return rtvHeap_->GetCPUDescriptorHandleForHeapStart();
}

D3D12_GPU_DESCRIPTOR_HANDLE RenderTexture::GetSRV() const
{
    return srvHeap_->GetGPUDescriptorHandleForHeapStart();
}
