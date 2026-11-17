#pragma once

namespace atmos
{
    class Camera;
    class Input;
}

// Free-flying editor-style camera: hold the right mouse button to look,
// WASD to move, Q/E down/up, Shift to move faster, mouse wheel for FOV.
class FlyCameraController
{
public:
    void Update(atmos::Camera& camera, atmos::Input& input, float deltaSeconds) const;

private:
    float moveSpeed_ = 5.0f;          // meters per second
    float fastMultiplier_ = 4.0f;
    float lookSensitivity_ = 0.15f;   // degrees per pixel
    float zoomSensitivity_ = 3.0f;    // degrees per wheel step
};
