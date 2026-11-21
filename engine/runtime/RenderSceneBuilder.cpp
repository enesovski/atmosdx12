#include "runtime/RenderSceneBuilder.h"
#include "runtime/AssetManager.h"
#include "graphics/RenderScene.h"
#include "scene/Scene.h"

using namespace DirectX;

namespace atmos
{
    namespace
    {
        void AddLight(const LightComponent& light, const XMFLOAT4X4& world, RenderScene& out)
        {
            const XMMATRIX matrix = XMLoadFloat4x4(&world);

            RenderLight& renderLight = out.lights.emplace_back();
            renderLight.type = light.type == LightType::Spot ? RenderLightType::Spot : RenderLightType::Point;
            XMStoreFloat3(&renderLight.position, matrix.r[3]);
            XMStoreFloat3(&renderLight.direction, XMVector3Normalize(matrix.r[2]));
            renderLight.color = light.color;
            renderLight.intensity = light.intensity;
            renderLight.range = light.range;
            renderLight.innerConeDegrees = light.innerConeDegrees;
            renderLight.outerConeDegrees = light.outerConeDegrees;
            renderLight.castsShadows = light.castsShadows;
        }
    }

    void RequestSceneAssets(Scene& scene, AssetManager& assets)
    {
        for (Entity& entity : scene.GetEntities())
        {
            if (entity.model && !entity.model->handle.IsValid())
                entity.model->handle = assets.LoadModel(entity.model->path);
        }
    }

    void BuildRenderScene(const Scene& scene, const AssetManager& assets, RenderScene& out)
    {
        out.sky.sunAzimuthDegrees = scene.environment.sunAzimuthDegrees;
        out.sky.sunElevationDegrees = scene.environment.sunElevationDegrees;
        out.sky.sunIntensity = scene.environment.sunIntensity;
        out.postProcess.exposure = scene.environment.exposure;
        out.settings = scene.renderSettings;
        out.meshInstances.clear();
        out.lights.clear();

        const std::vector<XMFLOAT4X4> worldMatrices = scene.ComputeWorldMatrices();
        const std::vector<bool> active = scene.ComputeActiveInHierarchy();
        const std::span<const Entity> entities = scene.GetEntities();

        for (size_t i = 0; i < entities.size(); ++i)
        {
            if (!active[i])
                continue;

            if (entities[i].light)
                AddLight(*entities[i].light, worldMatrices[i], out);

            if (!entities[i].model)
                continue;

            const LoadedModel* model = assets.GetModel(entities[i].model->handle);

            if (!model)
                continue;

            const XMMATRIX entityWorld = XMLoadFloat4x4(&worldMatrices[i]);

            for (const ModelInstance& instance : model->instances)
            {
                const Mesh* mesh = model->meshes[instance.meshIndex].get();

                if (!mesh)
                    continue;

                MeshInstance& meshInstance = out.meshInstances.emplace_back();
                meshInstance.mesh = mesh;
                meshInstance.materials = model->materialPointers;
                meshInstance.entity = static_cast<uint32_t>(i);
                XMStoreFloat4x4(&meshInstance.world, XMLoadFloat4x4(&instance.transform) * entityWorld);
            }
        }
    }
}
