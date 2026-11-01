
#pragma once

#include <d3d12.h>
#include <DirectXMath.h>
#include <wrl/client.h>

#include <cstdint>
#include <filesystem>
#include <memory>

class Camera;
class GraphicsPipeline;

class SkyPass
{
public:
    SkyPass(ID3D12Device* device, const std::filesystem::path& shaderPath,
            DXGI_FORMAT renderTargetFormat, int width, int height);

    ~SkyPass();

    void Render(ID3D12GraphicsCommandList* commandList, const Camera& camera, float elapsedSeconds);

private:
    struct SkyConstants
    {
        DirectX::XMFLOAT4 cameraRight;
        DirectX::XMFLOAT4 cameraUp;
        DirectX::XMFLOAT4 cameraForward;
        DirectX::XMFLOAT4 params;
    };

    static_assert(sizeof(SkyConstants) == 64);

    void CreatePipeline(ID3D12Device* device, const std::filesystem::path& shaderPath,
                        DXGI_FORMAT renderTargetFormat);

    void CreateConstantBuffer(ID3D12Device* device);
    void UpdateConstants(const Camera& camera, float elapsedSeconds);

    std::unique_ptr<GraphicsPipeline> pipeline_;

    Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;
    std::uint8_t* mappedData_ = nullptr;

    int width_;
    int height_;
};
