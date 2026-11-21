#include "scene/Camera.h"

#include <algorithm>
#include <cmath>
#include <numbers>

using namespace DirectX;

namespace atmos
{
    namespace
    {
        constexpr float DegreesToRadians = std::numbers::pi_v<float> / 180.0f;
    }

    void Camera::Rotate(float yawDeltaDegrees, float pitchDeltaDegrees)
    {
        SetRotation(yawDegrees_ + yawDeltaDegrees, pitchDegrees_ + pitchDeltaDegrees);
    }

    void Camera::AdjustFov(float deltaDegrees)
    {
        SetFov(fovDegrees_ + deltaDegrees);
    }

    void Camera::SetRotation(float yawDegrees, float pitchDegrees)
    {
        yawDegrees_ = std::remainder(yawDegrees, 360.0f);
        pitchDegrees_ = std::clamp(pitchDegrees, -89.0f, 89.0f);
    }

    void Camera::SetFov(float fovDegrees)
    {
        fovDegrees_ = std::clamp(fovDegrees, 35.0f, 95.0f);
    }

    void Camera::SetNearPlane(float nearPlane)
    {
        nearPlane_ = std::clamp(nearPlane, 0.01f, 10.0f);
    }

    XMFLOAT3 Camera::GetForward() const
    {
        const float yaw = yawDegrees_ * DegreesToRadians;
        const float pitch = pitchDegrees_ * DegreesToRadians;
        const float cosPitch = std::cos(pitch);

        return { cosPitch * std::sin(yaw), std::sin(pitch), cosPitch * std::cos(yaw) };
    }

    XMFLOAT3 Camera::GetRight() const
    {
        // cross(worldUp, forward), which never tilts because the camera has no roll.
        const float yaw = yawDegrees_ * DegreesToRadians;
        return { std::cos(yaw), 0.0f, -std::sin(yaw) };
    }

    XMMATRIX Camera::GetViewMatrix() const
    {
        const XMFLOAT3 forward = GetForward();

        return XMMatrixLookToLH(XMLoadFloat3(&position_), XMLoadFloat3(&forward),
                                XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
    }

    XMMATRIX Camera::GetProjectionMatrix(float aspectRatio) const
    {
        const float yScale = 1.0f / std::tan(fovDegrees_ * DegreesToRadians * 0.5f);
        const float xScale = yScale / aspectRatio;

        // Row-vector form: clip.z = near, clip.w = viewZ, so depth = near / viewZ.
        return XMMATRIX(
            xScale, 0.0f,   0.0f,       0.0f,
            0.0f,   yScale, 0.0f,       0.0f,
            0.0f,   0.0f,   0.0f,       1.0f,
            0.0f,   0.0f,   nearPlane_, 0.0f);
    }
}
