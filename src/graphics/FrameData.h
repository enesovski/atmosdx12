#pragma once

#include <DirectXMath.h>

struct FrameData
{
    DirectX::XMFLOAT3 cameraRight{};
    DirectX::XMFLOAT3 cameraUp{};
    DirectX::XMFLOAT3 cameraForward{};
    DirectX::XMFLOAT3 sunDirection{};
    DirectX::XMFLOAT3 moonDirection{};

    float tanHalfFovY = 0.0f;
    float elapsedSeconds = 0.0f;
    float timeOfDay = 8.0f;
    float sunIntensity = 1.0f;
    float moonIntensity = 1.0f;
    float starIntensity = 1.0f;
};