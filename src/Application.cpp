#include "Application.h"
#include "graphics/Renderer.h"

namespace
{
    constexpr float MouseSensitivity = 0.15f;
    constexpr float ZoomSensitivity = 3.0f;
}

Application::Application(HINSTANCE instance, int width, int height)
    : window_(instance, width, height, input_),
      renderView_{ {}, width, height },
      renderer_(std::make_unique<Renderer>(window_.GetHandle(), width, height))
{
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

        renderView_.camera = camera_.GetBasis();
        renderer_->Render(renderView_, skySettings_);

        if (!windowShown_)
        {
            window_.Show();
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
