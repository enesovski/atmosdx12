#include "Application.h"

#include "graphics/Renderer.h"
#include "graphics/RenderView.h"

namespace
{
    constexpr float MouseSensitivity = 0.15f;
    constexpr float ZoomSensitivity = 3.0f;

    const atmos::WindowDesc MainWindow{ L"AtmosDX12", 1280, 720 };
}

Application::Application(HINSTANCE instance, const atmos::GraphicsDeviceOptions& deviceOptions)
    : window_(instance, MainWindow, input_),
      renderer_(std::make_unique<atmos::Renderer>(window_.GetHandle(), MainWindow.width,
                                                  MainWindow.height, deviceOptions))
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

        if (const auto size = window_.ConsumeResize())
            renderer_->Resize(size->width, size->height);

        UpdateCamera();
        RenderFrame();

        // Show after the first frame so the window never flashes uninitialized content.
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
    const atmos::MouseDelta look = input_.ConsumeLookDelta();
    camera_.Rotate(look.x * MouseSensitivity, -look.y * MouseSensitivity);
    camera_.AdjustFov(-input_.ConsumeWheelSteps() * ZoomSensitivity);
}

void Application::RenderFrame()
{
    const atmos::WindowSize size = window_.GetClientSize();
    renderer_->Render(scene_, atmos::RenderView::FromCamera(camera_, size.width, size.height));
}
