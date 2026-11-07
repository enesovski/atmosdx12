#pragma once

#include <d3d12.h>
#include <DirectXMath.h>

#include "../d3d12/GraphicsPipeline.h"
#include "../d3d12/UploadBuffer.h"

#include <cstddef>
#include <filesystem>

struct RenderView;
struct SkySettings;

class SkyPass
{
public:
    SkyPass(ID3D12Device* device, const std::filesystem::path& shaderPath,
            DXGI_FORMAT renderTargetFormat);

    SkyPass(const SkyPass&) = delete;
    SkyPass& operator=(const SkyPass&) = delete;

    void Record(ID3D12GraphicsCommandList* commandList, UINT frameIndex,
                const RenderView& view, const SkySettings& skySettings);

private:
    struct SkyConstants
    {
        DirectX::XMFLOAT4 cameraRight;
        DirectX::XMFLOAT4 cameraUp;
        DirectX::XMFLOAT4 cameraForward;
        DirectX::XMFLOAT4 viewParams;
        DirectX::XMFLOAT4 sunParams;
    };

    static_assert(sizeof(SkyConstants) == 80);
    static_assert(offsetof(SkyConstants, cameraRight) == 0);
    static_assert(offsetof(SkyConstants, cameraUp) == 16);
    static_assert(offsetof(SkyConstants, cameraForward) == 32);
    static_assert(offsetof(SkyConstants, viewParams) == 48);
    static_assert(offsetof(SkyConstants, sunParams) == 64);

    GraphicsPipeline pipeline_;
    UploadBuffer constantBuffer_;
};
