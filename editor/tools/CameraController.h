#pragma once

#include <DirectXMath.h>

namespace atmos
{
    class Camera;
}

namespace atmos::editor
{
    struct CameraInput
    {
        bool hovered = false;   // pointer over the viewport image, not over a viewport widget
        bool keyboard = false;  // keys may move the camera (no text field active)
        float rawDeltaX = 0.0f; // raw mouse counts this frame
        float rawDeltaY = 0.0f;
        float wheel = 0.0f;     // wheel steps this frame
    };

    // Unity/Unreal viewport navigation:
    //   RMB drag         fly: mouse look, WASD move, Q/E down/up, Shift faster, wheel = speed
    //   Alt + LMB drag   orbit around the pivot (the last focused point)
    //   MMB drag         pan
    //   wheel            dolly toward the pivot
    //   Focus()          frames a bounding sphere with a short animation
    class CameraController
    {
    public:
        // Returns true while a drag owns the mouse; the caller hides and pins the cursor.
        bool Update(Camera& camera, const CameraInput& input, float deltaSeconds);

        void Focus(const Camera& camera, const DirectX::XMFLOAT3& center, float radius);

        bool IsDragging() const { return mode_ != Mode::None; }
        float GetMoveSpeed() const { return moveSpeed_; }
        void SetMoveSpeed(float speed);

        float lookSensitivity = 0.1f;  // degrees per raw mouse count
        bool invertY = false;

    private:
        enum class Mode
        {
            None,
            Fly,
            Orbit,
            Pan,
        };

        void BeginDrag(const Camera& camera, const CameraInput& input);
        void Fly(Camera& camera, const CameraInput& input, float deltaSeconds) const;
        void Orbit(Camera& camera, const CameraInput& input);
        void Pan(Camera& camera, const CameraInput& input);
        void Dolly(Camera& camera, float steps);
        void Animate(Camera& camera, float deltaSeconds);

        Mode mode_ = Mode::None;
        float moveSpeed_ = 5.0f;  // meters per second
        DirectX::XMFLOAT3 pivot_{ 0.0f, 0.0f, 0.0f };
        float pivotDistance_ = 10.0f;

        bool animating_ = false;
        float animationTime_ = 0.0f;
        DirectX::XMFLOAT3 animationFrom_{};
        DirectX::XMFLOAT3 animationTo_{};
    };
}
