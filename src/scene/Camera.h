
#pragma once

#include <DirectXMath.h>

struct CameraBasis
{
    DirectX::XMFLOAT3 forward;
    DirectX::XMFLOAT3 right;
    DirectX::XMFLOAT3 up;

    float tanHalfFovY = 0.0f;
};

class Camera
{
public:
    void Rotate(float yawDeltaDegrees, float pitchDeltaDegrees);
    void AdjustFov(float deltaDegrees);

    CameraBasis GetBasis() const;

private:
    float yawDegrees_ = 0.0f;
    float pitchDegrees_ = 0.0f;
    float fovDegrees_ = 65.0f;
};
