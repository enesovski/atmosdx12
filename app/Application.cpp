#include "Application.h"

#include "assets/GltfLoader.h"
#include "assets/PrimitiveMeshes.h"
#include "graphics/Mesh.h"
#include "graphics/Renderer.h"
#include "graphics/RenderView.h"

using namespace DirectX;

namespace
{
    const atmos::WindowDesc MainWindow{ L"AtmosDX12", 1280, 720 };

    XMFLOAT4X4 MakeWorld(XMFLOAT3 position, XMFLOAT3 scale, float yawDegrees = 0.0f)
    {
        XMFLOAT4X4 world;
        XMStoreFloat4x4(&world, XMMatrixScaling(scale.x, scale.y, scale.z) *
                                    XMMatrixRotationY(XMConvertToRadians(yawDegrees)) *
                                    XMMatrixTranslation(position.x, position.y, position.z));
        return world;
    }
}

Application::Application(HINSTANCE instance, const AppOptions& options)
    : window_(instance, MainWindow, input_),
      renderer_(std::make_unique<atmos::Renderer>(window_.GetHandle(), MainWindow.width, MainWindow.height,
                                                  options.device))
{
    camera_.SetPosition({ 0.0f, 2.0f, -8.0f });
    camera_.Rotate(0.0f, -10.0f);

    if (options.scenePath.empty())
        LoadDefaultScene();
    else
        LoadGltfScene(options.scenePath);

    if (options.dumpFrame)
        renderer_->RequestFrameDump();
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

        cameraController_.Update(camera_, input_, frameTimer_.Tick());
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

void Application::LoadGltfScene(const std::filesystem::path& path)
{
    const atmos::ModelData model = atmos::LoadGltfModel(path);
    meshes_ = renderer_->CreateMeshes(model.meshes);

    for (const atmos::ModelInstance& instance : model.instances)
    {
        if (const atmos::Mesh* mesh = meshes_[instance.meshIndex].get())
            scene_.meshInstances.push_back({ mesh, instance.world });
    }
}

// A ground plane and a few cubes, so the engine has something to draw without assets.
void Application::LoadDefaultScene()
{
    const atmos::MeshData meshData[] = {
        atmos::CreatePlaneMeshData(40.0f, { 0.45f, 0.45f, 0.42f, 1.0f }),
        atmos::CreateCubeMeshData({ 0.80f, 0.20f, 0.15f, 1.0f }),
        atmos::CreateCubeMeshData({ 0.15f, 0.55f, 0.80f, 1.0f }),
        atmos::CreateCubeMeshData({ 0.85f, 0.75f, 0.25f, 1.0f }),
    };

    meshes_ = renderer_->CreateMeshes(meshData);

    scene_.meshInstances = {
        { meshes_[0].get(), MakeWorld({ 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f }) },
        { meshes_[1].get(), MakeWorld({ -3.0f, 1.0f, 2.0f }, { 2.0f, 2.0f, 2.0f }, 20.0f) },
        { meshes_[2].get(), MakeWorld({ 0.5f, 0.75f, 4.0f }, { 1.5f, 1.5f, 1.5f }, -35.0f) },
        { meshes_[3].get(), MakeWorld({ 3.5f, 2.0f, 1.0f }, { 1.0f, 4.0f, 1.0f }, 10.0f) },
    };
}

void Application::RenderFrame()
{
    const atmos::WindowSize size = window_.GetClientSize();
    renderer_->Render(scene_, atmos::RenderView::FromCamera(camera_, size.width, size.height));
}
