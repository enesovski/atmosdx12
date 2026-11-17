#include "FlyCameraController.h"

#include "platform/Input.h"
#include "scene/Camera.h"

#include <windows.h>

using namespace DirectX;

namespace
{
    float Axis(const atmos::Input& input, uint32_t positiveKey, uint32_t negativeKey)
    {
        return (input.IsKeyDown(positiveKey) ? 1.0f : 0.0f) - (input.IsKeyDown(negativeKey) ? 1.0f : 0.0f);
    }
}

void FlyCameraController::Update(atmos::Camera& camera, atmos::Input& input, float deltaSeconds) const
{
    const atmos::MouseDelta look = input.ConsumeLookDelta();
    camera.Rotate(look.x * lookSensitivity_, -look.y * lookSensitivity_);
    camera.AdjustFov(-input.ConsumeWheelSteps() * zoomSensitivity_);

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
