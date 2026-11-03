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
      renderer_(std::make_unique<Renderer>(window_.GetHandle(), width, height))
{
    windowShown = false;
}

Application::~Application() = default;

int Application::Run()
{
    int exitCode = 0;

    while (window_.ProcessMessages(exitCode))
    {
        if (window_.IsMinimized())
        {
            WaitMessage();
            continue;
        }

        UpdateCamera();

        CameraBasis basis = camera_.GetBasis();
        FrameData frameData{};
        frameData.cameraRight = basis.right;
        frameData.cameraUp = basis.up;
        frameData.cameraForward = basis.forward;
        frameData.tanHalfFovY = basis.tanHalfFovY;
        frameData.elapsedSeconds = clock_.GetElapsedSeconds();

        renderer_->Render(frameData);

        if (!windowShown)
        {
            window_.Show();
            windowShown = true;
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
