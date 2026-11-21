#pragma once

namespace atmos
{
    class Camera;
    class Input;
}

// Editor-style camera (Unity/Unreal viewport): hold the right mouse button to fly, with mouse
// look, WASD to move, Q/E down/up, Shift to go faster and the wheel to change speed.
// Without the right button, the wheel changes the field of view.
class FlyCameraController
{
public:
    // `pointerOverUi` blocks starting a flight and wheel zoom. Returns true while flying.
    bool Update(atmos::Camera& camera, atmos::Input& input, float deltaSeconds, bool pointerOverUi);

    bool IsFlying() const { return flying_; }
    float GetMoveSpeed() const { return moveSpeed_; }

private:
    void Move(atmos::Camera& camera, const atmos::Input& input, float deltaSeconds) const;

    bool flying_ = false;
    float moveSpeed_ = 5.0f;          // meters per second
    float fastMultiplier_ = 4.0f;
    float lookSensitivity_ = 0.1f;    // degrees per raw mouse count
    float zoomSensitivity_ = 3.0f;    // degrees per wheel step
};
