#pragma once

#include <d3d12.h>
#include <DirectXMath.h>

#include "../UploadBuffer.h"

#include <filesystem>
#include <memory>

class GraphicsPipeline;
struct FrameData;

class SkyPass
{
public:
    SkyPass(ID3D12Device* device, const std::filesystem::path& shaderPath,
            DXGI_FORMAT renderTargetFormat, int width, int height);
    ~SkyPass();

    SkyPass(const SkyPass&) = delete;
    SkyPass& operator=(const SkyPass&) = delete;

    void Record(ID3D12GraphicsCommandList* commandList, UINT frameIndex, const FrameData& frameData);

private:
    struct SkyConstants
    {
        DirectX::XMFLOAT4 cameraRight;
        DirectX::XMFLOAT4 cameraUp;
        DirectX::XMFLOAT4 cameraForward;
        DirectX::XMFLOAT4 viewParams;
        DirectX::XMFLOAT4 sunParams;
        DirectX::XMFLOAT4 moonParams;
        DirectX::XMFLOAT4 nightParams;
    };

    static_assert(sizeof(SkyConstants) == 112);

    void CreatePipeline(ID3D12Device* device, const std::filesystem::path& shaderPath,
                        DXGI_FORMAT renderTargetFormat);
    void UpdateConstants(UINT frameIndex, const FrameData& frameData);

    std::unique_ptr<GraphicsPipeline> pipeline_;
    UploadBuffer constantBuffer_;

    int width_;
    int height_;
};