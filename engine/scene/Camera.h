#pragma once

#include <DirectXMath.h>

namespace atmos
{
    // Left-handed, +Y up. Yaw 0 looks along +Z and turns toward +X.
    class Camera
    {
    public:
        void Rotate(float yawDeltaDegrees, float pitchDeltaDegrees);
        void AdjustFov(float deltaDegrees);

        void SetRotation(float yawDegrees, float pitchDegrees);
        void SetFov(float fovDegrees);
        float GetYawDegrees() const { return yawDegrees_; }
        float GetPitchDegrees() const { return pitchDegrees_; }
        float GetFovDegrees() const { return fovDegrees_; }

        void SetPosition(const DirectX::XMFLOAT3& position) { position_ = position; }
        const DirectX::XMFLOAT3& GetPosition() const { return position_; }
        DirectX::XMFLOAT3 GetForward() const;
        DirectX::XMFLOAT3 GetRight() const;

        DirectX::XMMATRIX GetViewMatrix() const;

        // Reverse-Z with an infinite far plane: depth 1 at the near plane, 0 at infinity.
        DirectX::XMMATRIX GetProjectionMatrix(float aspectRatio) const;

    private:
        DirectX::XMFLOAT3 position_{ 0.0f, 0.0f, 0.0f };
        float yawDegrees_ = 0.0f;
        float pitchDegrees_ = 0.0f;
        float fovDegrees_ = 65.0f;
        float nearPlane_ = 0.1f;
    };
}
