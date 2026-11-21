#include "tools/CameraController.h"

#include "scene/Camera.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>

using namespace DirectX;

namespace atmos::editor
{
    namespace
    {
        constexpr float FastMultiplier = 4.0f;
        constexpr float FocusDuration = 0.25f;  // seconds
        constexpr float PanSpeed = 0.0015f;     // fraction of the pivot distance per mouse count

        float Axis(ImGuiKey positive, ImGuiKey negative)
        {
            return (ImGui::IsKeyDown(positive) ? 1.0f : 0.0f) - (ImGui::IsKeyDown(negative) ? 1.0f : 0.0f);
        }

        XMVECTOR GetUp(const Camera& camera)
        {
            const XMFLOAT3 forward = camera.GetForward();
            const XMFLOAT3 right = camera.GetRight();
            return XMVector3Cross(XMLoadFloat3(&forward), XMLoadFloat3(&right));
        }
    }

    bool CameraController::Update(Camera& camera, const CameraInput& input, float deltaSeconds)
    {
        const ImGuiIO& io = ImGui::GetIO();

        if (mode_ == Mode::None && input.hovered)
        {
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
                mode_ = Mode::Fly;
            else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && io.KeyAlt)
                mode_ = Mode::Orbit;
            else if (ImGui::IsMouseClicked(ImGuiMouseButton_Middle))
                mode_ = Mode::Pan;

            if (mode_ != Mode::None)
                BeginDrag(camera, input);
        }

        const bool released = (mode_ == Mode::Fly && !ImGui::IsMouseDown(ImGuiMouseButton_Right)) ||
                              (mode_ == Mode::Orbit && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) ||
                              (mode_ == Mode::Pan && !ImGui::IsMouseDown(ImGuiMouseButton_Middle));

        if (released)
            mode_ = Mode::None;

        switch (mode_)
        {
            case Mode::Fly:
                SetMoveSpeed(moveSpeed_ * std::pow(1.25f, input.wheel));
                Fly(camera, input, deltaSeconds);
                break;
            case Mode::Orbit: Orbit(camera, input); break;
            case Mode::Pan:   Pan(camera, input); break;
            case Mode::None:
                if (input.hovered && input.wheel != 0.0f)
                    Dolly(camera, input.wheel);
                break;
        }

        Animate(camera, deltaSeconds);
        return mode_ != Mode::None;
    }

    void CameraController::BeginDrag(const Camera& camera, const CameraInput&)
    {
        animating_ = false;

        // Keep the pivot where it was if it is still in front of the camera; else re-seed it.
        const XMFLOAT3 position = camera.GetPosition();
        const XMFLOAT3 forward = camera.GetForward();
        const XMVECTOR toPivot = XMLoadFloat3(&pivot_) - XMLoadFloat3(&position);
        const float alongForward = XMVectorGetX(XMVector3Dot(toPivot, XMLoadFloat3(&forward)));

        if (alongForward > 0.1f)
        {
            pivotDistance_ = XMVectorGetX(XMVector3Length(toPivot));
        }
        else
        {
            XMStoreFloat3(&pivot_, XMLoadFloat3(&position) + XMLoadFloat3(&forward) * pivotDistance_);
        }
    }

    void CameraController::Fly(Camera& camera, const CameraInput& input, float deltaSeconds) const
    {
        const float pitchSign = invertY ? 1.0f : -1.0f;
        camera.Rotate(input.rawDeltaX * lookSensitivity, pitchSign * input.rawDeltaY * lookSensitivity);

        if (!input.keyboard)
            return;

        const XMFLOAT3 forward = camera.GetForward();
        const XMFLOAT3 right = camera.GetRight();
        const XMVECTOR direction = XMLoadFloat3(&forward) * Axis(ImGuiKey_W, ImGuiKey_S) +
                                   XMLoadFloat3(&right) * Axis(ImGuiKey_D, ImGuiKey_A) +
                                   XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f) * Axis(ImGuiKey_E, ImGuiKey_Q);

        if (XMVector3Equal(direction, XMVectorZero()))
            return;

        const float speed = moveSpeed_ * (ImGui::GetIO().KeyShift ? FastMultiplier : 1.0f);
        const XMFLOAT3& position = camera.GetPosition();

        XMFLOAT3 newPosition;
        XMStoreFloat3(&newPosition, XMLoadFloat3(&position) + XMVector3Normalize(direction) * speed * deltaSeconds);
        camera.SetPosition(newPosition);
    }

    void CameraController::Orbit(Camera& camera, const CameraInput& input)
    {
        const float pitchSign = invertY ? 1.0f : -1.0f;
        camera.Rotate(input.rawDeltaX * lookSensitivity, pitchSign * input.rawDeltaY * lookSensitivity);

        const XMFLOAT3 forward = camera.GetForward();
        XMFLOAT3 position;
        XMStoreFloat3(&position, XMLoadFloat3(&pivot_) - XMLoadFloat3(&forward) * pivotDistance_);
        camera.SetPosition(position);
    }

    void CameraController::Pan(Camera& camera, const CameraInput& input)
    {
        const XMFLOAT3 right = camera.GetRight();
        const float scale = std::max(pivotDistance_, 1.0f) * PanSpeed;
        const XMVECTOR offset = XMLoadFloat3(&right) * (-input.rawDeltaX * scale) + GetUp(camera) * (input.rawDeltaY * scale);

        const XMFLOAT3& position = camera.GetPosition();
        XMFLOAT3 newPosition;
        XMStoreFloat3(&newPosition, XMLoadFloat3(&position) + offset);
        XMStoreFloat3(&pivot_, XMLoadFloat3(&pivot_) + offset);
        camera.SetPosition(newPosition);
    }

    void CameraController::Dolly(Camera& camera, float steps)
    {
        animating_ = false;

        // Each step covers a fraction of the distance to the pivot, so zooming feels even at any scale.
        const float step = std::max(pivotDistance_ * 0.15f, 0.25f) * steps;
        const XMFLOAT3 forward = camera.GetForward();
        const XMFLOAT3& position = camera.GetPosition();

        XMFLOAT3 newPosition;
        XMStoreFloat3(&newPosition, XMLoadFloat3(&position) + XMLoadFloat3(&forward) * step);
        camera.SetPosition(newPosition);
        pivotDistance_ = std::max(pivotDistance_ - step, 0.5f);
        XMStoreFloat3(&pivot_, XMLoadFloat3(&newPosition) + XMLoadFloat3(&forward) * pivotDistance_);
    }

    void CameraController::Focus(const Camera& camera, const XMFLOAT3& center, float radius)
    {
        const float halfFov = XMConvertToRadians(camera.GetFovDegrees()) * 0.5f;
        const float distance = std::max(radius, 0.25f) / std::sin(halfFov) * 1.1f;
        const XMFLOAT3 forward = camera.GetForward();

        animationFrom_ = camera.GetPosition();
        XMStoreFloat3(&animationTo_, XMLoadFloat3(&center) - XMLoadFloat3(&forward) * distance);
        animationTime_ = 0.0f;
        animating_ = true;
        pivot_ = center;
        pivotDistance_ = distance;
    }

    void CameraController::SetMoveSpeed(float speed)
    {
        moveSpeed_ = std::clamp(speed, 0.25f, 200.0f);
    }

    void CameraController::Animate(Camera& camera, float deltaSeconds)
    {
        if (!animating_)
            return;

        animationTime_ += deltaSeconds;
        const float t = std::clamp(animationTime_ / FocusDuration, 0.0f, 1.0f);
        const float eased = t * t * (3.0f - 2.0f * t);

        XMFLOAT3 position;
        XMStoreFloat3(&position, XMVectorLerp(XMLoadFloat3(&animationFrom_), XMLoadFloat3(&animationTo_), eased));
        camera.SetPosition(position);
        animating_ = t < 1.0f;
    }
}
