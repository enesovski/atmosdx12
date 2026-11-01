
#include "SkyPass.h"

#include <array>
#include <cstring>
#include <stdexcept>

namespace
{
    constexpr UINT64 ConstantBufferAlignment = 256;

    UINT64 AlignConstantBufferSize(UINT64 size)
    {
        return (size + ConstantBufferAlignment - 1) &
               ~(ConstantBufferAlignment - 1);
    }

    void Check(HRESULT result, const char* message)
    {
        if (FAILED(result))
            throw std::runtime_error(message);
    }
}

SkyPass::SkyPass(ID3D12Device* device, const std::filesystem::path& shaderPath,
                 DXGI_FORMAT renderTargetFormat, int width, int height)
    : width_(width), height_(height)
{
    CreatePipeline(device, shaderPath, renderTargetFormat);
    CreateConstantBuffer(device);
}

SkyPass::~SkyPass()
{
    if (mappedData_)
    {
        constantBuffer_->Unmap(0, nullptr);
        mappedData_ = nullptr;
    }
}

void SkyPass::CreatePipeline(ID3D12Device* device, const std::filesystem::path& shaderPath,
                             DXGI_FORMAT renderTargetFormat)
{
    D3D12_ROOT_PARAMETER constantBufferParameter{};
    constantBufferParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    constantBufferParameter.Descriptor.ShaderRegister = 0;
    constantBufferParameter.Descriptor.RegisterSpace = 0;
    constantBufferParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    std::array<D3D12_ROOT_PARAMETER, 1> rootParameters{
        constantBufferParameter
    };

    pipeline_ = std::make_unique<GraphicsPipeline>(
        device,
        shaderPath,
        "VSMain",
        "PSMain",
        renderTargetFormat,
        rootParameters
    );
}

void SkyPass::CreateConstantBuffer(ID3D12Device* device)
{
    D3D12_HEAP_PROPERTIES heapProperties{};
    heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC bufferDesc{};
    bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufferDesc.Width = AlignConstantBufferSize(sizeof(SkyConstants));
    bufferDesc.Height = 1;
    bufferDesc.DepthOrArraySize = 1;
    bufferDesc.MipLevels = 1;
    bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
    bufferDesc.SampleDesc.Count = 1;
    bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    Check(device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&constantBuffer_)
    ), "Failed to create sky constant buffer");

    D3D12_RANGE readRange{ 0, 0 };
    void* mapped = nullptr;

    Check(constantBuffer_->Map(
        0,
        &readRange,
        &mapped
    ), "Failed to map sky constant buffer");

    mappedData_ = static_cast<std::uint8_t*>(mapped);
}

void SkyPass::UpdateConstants(float elapsedSeconds)
{
    SkyConstants constants{};
    constants.time = elapsedSeconds;
    constants.resolution[0] = static_cast<float>(width_);
    constants.resolution[1] = static_cast<float>(height_);

    // The renderer waits for GPU completion before the next update.
    std::memcpy(mappedData_, &constants, sizeof(constants));
}

void SkyPass::Render(ID3D12GraphicsCommandList* commandList, float elapsedSeconds)
{
    UpdateConstants(elapsedSeconds);

    pipeline_->Bind(commandList);

    commandList->SetGraphicsRootConstantBufferView(
        0,
        constantBuffer_->GetGPUVirtualAddress()
    );

    commandList->IASetPrimitiveTopology(
        D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST
    );

    commandList->DrawInstanced(3, 1, 0, 0);
}
