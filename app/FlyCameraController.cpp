#include "FlyCameraController.h"

#include "platform/Input.h"
#include "scene/Camera.h"

#include <windows.h>

#include <algorithm>
#include <cmath>

using namespace DirectX;

namespace
{
    float Axis(const atmos::Input& input, uint32_t positiveKey, uint32_t negativeKey)
    {
        return (input.IsKeyDown(positiveKey) ? 1.0f : 0.0f) - (input.IsKeyDown(negativeKey) ? 1.0f : 0.0f);
    }
}

bool FlyCameraController::Update(atmos::Camera& camera, atmos::Input& input, float deltaSeconds, bool pointerOverUi)
{
    using atmos::MouseButton;

    // A flight starts with a right-button press in the viewport and lasts while the button is held.
    if (input.ConsumeMouseButtonPress(MouseButton::Right) && !pointerOverUi)
        flying_ = true;

    if (!input.IsMouseButtonDown(MouseButton::Right))
        flying_ = false;

    const atmos::MouseDelta look = input.ConsumeMouseDelta();
    const float wheelSteps = input.ConsumeWheelSteps();

    if (!flying_)
    {
        if (!pointerOverUi)
            camera.AdjustFov(-wheelSteps * zoomSensitivity_);

        return false;
    }

    camera.Rotate(look.x * lookSensitivity_, -look.y * lookSensitivity_);
    moveSpeed_ = std::clamp(moveSpeed_ * std::pow(1.25f, wheelSteps), 0.25f, 200.0f);
    Move(camera, input, deltaSeconds);
    return true;
}

void FlyCameraController::Move(atmos::Camera& camera, const atmos::Input& input, float deltaSeconds) const
{
    const XMFLOAT3 forward = camera.GetForward();
    const XMFLOAT3 right = camera.GetRight();

    const XMVECTOR direction =
        XMLoadFloat3(&forward) * Axis(input, 'W', 'S') +
        XMLoadFloat3(&right) * Axis(input, 'D', 'A') +
        XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f) * Axis(input, 'E', 'Q');

    if (XMVector3Equal(direction, XMVectorZero()))
        return;

    const float speed = moveSpeed_ * (input.IsKeyDown(VK_SHIFT) ? fastMultiplier_ : 1.0f);
    const XMFLOAT3& position = camera.GetPosition();

    XMFLOAT3 newPosition;
    XMStoreFloat3(&newPosition, XMLoadFloat3(&position) + XMVector3Normalize(direction) * speed * deltaSeconds);
    camera.SetPosition(newPosition);
}
