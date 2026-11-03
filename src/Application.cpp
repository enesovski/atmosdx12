#include "Application.h"
#include "graphics/FrameData.h"
#include "graphics/Renderer.h"

namespace
{
    constexpr float MouseSensitivity = 0.15f;
    constexpr float ZoomSensitivity = 3.0f;
}

Application::Application(HINSTANCE instance, int width, int height)
    : window_(instance, width, height, input_),
      ui_(window_.GetHandle(), timeManager_, skySettings_),
      renderer_(std::make_unique<Renderer>(window_.GetHandle(), width, height))
{
}

Application::~Application() = default;

int Application::Run()
{
    int exitCode = 0;
    float previousSeconds = clock_.GetElapsedSeconds();

    while (window_.ProcessMessages(exitCode))
    {
        if (window_.IsMinimized())
        {
            WaitMessage();
            continue;
        }

        const float elapsedSeconds = clock_.GetElapsedSeconds();
        timeManager_.Update(elapsedSeconds - previousSeconds);
        previousSeconds = elapsedSeconds;

        UpdateCamera();
        celestialManager_.Update(timeManager_.GetTimeOfDay());
        ui_.Refresh();

        const CameraBasis basis = camera_.GetBasis();
        const CelestialState& celestial = celestialManager_.GetState();

        FrameData frameData{};
        frameData.cameraRight = basis.right;
        frameData.cameraUp = basis.up;
        frameData.cameraForward = basis.forward;
        frameData.tanHalfFovY = basis.tanHalfFovY;
        frameData.elapsedSeconds = elapsedSeconds;
        frameData.timeOfDay = timeManager_.GetTimeOfDay();
        frameData.sunDirection = celestial.sunDirection;
        frameData.moonDirection = celestial.moonDirection;
        frameData.sunIntensity = skySettings_.sunIntensity;
        frameData.moonIntensity = skySettings_.moonIntensity;
        frameData.starIntensity = skySettings_.starIntensity;

        renderer_->Render(frameData);

        if (!windowShown_)
        {
            window_.Show();
            ui_.Show();
            windowShown_ = true;
        }
    }

    return exitCode;
}

void Application::UpdateCamera()
{
    MouseDelta look = input_.ConsumeLookDelta();
    camera_.Rotate(look.x * MouseSensitivity, -look.y * MouseSensitivity);

    float wheelSteps = input_.ConsumeWheelSteps();
    camera_.AdjustFov(-wheelSteps * ZoomSensitivity);
}