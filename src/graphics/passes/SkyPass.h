
#pragma once

#include "../GraphicsPipeline.h"

#include <d3d12.h>
#include <wrl/client.h>

#include <filesystem>
#include <memory>
#include <cstdint>

class SkyPass
{
public:
    SkyPass(ID3D12Device* device, const std::filesystem::path& shaderPath,
            DXGI_FORMAT renderTargetFormat, int width, int height);

    ~SkyPass();

    void Render(ID3D12GraphicsCommandList* commandList, float elapsedSeconds);

private:
    struct SkyConstants
    {
        float time = 0.0f;
        float resolution[2]{};
        float padding = 0.0f;
    };

    static_assert(sizeof(SkyConstants) == 16);

    void CreatePipeline(ID3D12Device* device, const std::filesystem::path& shaderPath,
                        DXGI_FORMAT renderTargetFormat);

    void CreateConstantBuffer(ID3D12Device* device);
    void UpdateConstants(float elapsedSeconds);

    std::unique_ptr<GraphicsPipeline> pipeline_;

    Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;
    std::uint8_t* mappedData_ = nullptr;

    int width_;
    int height_;
};
