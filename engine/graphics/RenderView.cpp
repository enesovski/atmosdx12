#include "graphics/RenderView.h"
#include "scene/Camera.h"

using namespace DirectX;

namespace atmos
{
    RenderView RenderView::FromCamera(const Camera& camera, uint32_t width, uint32_t height)
    {
        const float aspectRatio = height > 0 ? static_cast<float>(width) / static_cast<float>(height) : 1.0f;

        RenderView view;
        view.position = camera.GetPosition();
        view.width = width;
        view.height = height;
        XMStoreFloat4x4(&view.viewMatrix, camera.GetViewMatrix());
        XMStoreFloat4x4(&view.projectionMatrix, camera.GetProjectionMatrix(aspectRatio));

        return view;
    }
}
