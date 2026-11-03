#pragma once

#include <DirectXMath.h>

struct FrameData
{
    DirectX::XMFLOAT3 cameraRight;
    DirectX::XMFLOAT3 cameraUp;
    DirectX::XMFLOAT3 cameraForward;

    float tanHalfFovY = 0.0f;
    float elapsedSeconds = 0.0f;
};
