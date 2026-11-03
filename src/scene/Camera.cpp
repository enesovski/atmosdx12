
#include "Camera.h"

#include <algorithm>
#include <cmath>
#include <numbers>

using namespace DirectX;

namespace
{
    constexpr float DegreesToRadians = std::numbers::pi_v<float> / 180.0f;
}

void Camera::Rotate(float yawDeltaDegrees, float pitchDeltaDegrees)
{
    yawDegrees_ += yawDeltaDegrees;
    pitchDegrees_ = std::clamp(pitchDegrees_ + pitchDeltaDegrees, -89.0f, 89.0f);
}

void Camera::AdjustFov(float deltaDegrees)
{
    fovDegrees_ = std::clamp(fovDegrees_ + deltaDegrees, 35.0f, 95.0f);
}

CameraBasis Camera::GetBasis() const
{
    float yaw = yawDegrees_ * DegreesToRadians;
    float pitch = pitchDegrees_ * DegreesToRadians;

    float cosPitch = std::cos(pitch);

    XMVECTOR forward = XMVectorSet(
        cosPitch * std::sin(yaw),
        std::sin(pitch),
        cosPitch * std::cos(yaw),
        0.0f
    );

    forward = XMVector3Normalize(forward);

    XMVECTOR worldUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    XMVECTOR right = XMVector3Normalize(XMVector3Cross(worldUp, forward));
    XMVECTOR up = XMVector3Cross(forward, right);

    CameraBasis basis{};

    XMStoreFloat3(&basis.forward, forward);
    XMStoreFloat3(&basis.right, right);
    XMStoreFloat3(&basis.up, up);

    basis.tanHalfFovY = std::tan(fovDegrees_ * DegreesToRadians * 0.5f);

    return basis;
}
