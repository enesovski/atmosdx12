#include "tools/Picking.h"

#include "graphics/Mesh.h"
#include "graphics/RenderScene.h"
#include "scene/Camera.h"

#include <algorithm>
#include <cmath>
#include <limits>

using namespace DirectX;

namespace atmos::editor
{
    ViewportProjection::ViewportProjection(const Camera& camera, XMFLOAT2 rectMin, XMFLOAT2 rectSize)
        : position_(camera.GetPosition()),
          forward_(camera.GetForward()),
          right_(camera.GetRight()),
          tanHalfFovY_(std::tan(XMConvertToRadians(camera.GetFovDegrees()) * 0.5f)),
          rectMin_(rectMin),
          rectSize_({ std::max(rectSize.x, 1.0f), std::max(rectSize.y, 1.0f) })
    {
        XMStoreFloat3(&up_, XMVector3Cross(XMLoadFloat3(&forward_), XMLoadFloat3(&right_)));
        XMStoreFloat4x4(&viewProjection_, camera.GetViewMatrix() * camera.GetProjectionMatrix(GetAspect()));
    }

    Ray ViewportProjection::GetRay(XMFLOAT2 screenPoint) const
    {
        const float ndcX = (screenPoint.x - rectMin_.x) / rectSize_.x * 2.0f - 1.0f;
        const float ndcY = 1.0f - (screenPoint.y - rectMin_.y) / rectSize_.y * 2.0f;

        const XMVECTOR direction = XMLoadFloat3(&forward_) + XMLoadFloat3(&right_) * (ndcX * tanHalfFovY_ * GetAspect()) +
                                   XMLoadFloat3(&up_) * (ndcY * tanHalfFovY_);

        Ray ray{ position_, {} };
        XMStoreFloat3(&ray.direction, XMVector3Normalize(direction));
        return ray;
    }

    std::optional<XMFLOAT2> ViewportProjection::Project(const XMFLOAT3& point) const
    {
        XMFLOAT4 clip;
        XMStoreFloat4(&clip, XMVector4Transform(XMVectorSet(point.x, point.y, point.z, 1.0f), XMLoadFloat4x4(&viewProjection_)));

        if (clip.w < 0.09f)
            return std::nullopt;

        const float ndcX = clip.x / clip.w;
        const float ndcY = clip.y / clip.w;
        return XMFLOAT2{ rectMin_.x + (ndcX * 0.5f + 0.5f) * rectSize_.x, rectMin_.y + (0.5f - ndcY * 0.5f) * rectSize_.y };
    }

    bool ViewportProjection::ProjectSegment(XMFLOAT3 a, XMFLOAT3 b, XMFLOAT2& screenA, XMFLOAT2& screenB) const
    {
        constexpr float NearDepth = 0.1f;

        const XMVECTOR eye = XMLoadFloat3(&position_);
        const XMVECTOR forward = XMLoadFloat3(&forward_);
        const float depthA = XMVectorGetX(XMVector3Dot(XMLoadFloat3(&a) - eye, forward));
        const float depthB = XMVectorGetX(XMVector3Dot(XMLoadFloat3(&b) - eye, forward));

        if (depthA < NearDepth && depthB < NearDepth)
            return false;

        // Move the end behind the camera onto the near plane.
        if (depthA < NearDepth)
            XMStoreFloat3(&a, XMVectorLerp(XMLoadFloat3(&a), XMLoadFloat3(&b), (NearDepth - depthA) / (depthB - depthA)));
        else if (depthB < NearDepth)
            XMStoreFloat3(&b, XMVectorLerp(XMLoadFloat3(&b), XMLoadFloat3(&a), (NearDepth - depthB) / (depthA - depthB)));

        const std::optional<XMFLOAT2> projectedA = Project(a);
        const std::optional<XMFLOAT2> projectedB = Project(b);

        if (!projectedA || !projectedB)
            return false;

        screenA = *projectedA;
        screenB = *projectedB;
        return true;
    }

    std::optional<EntityId> PickMesh(const RenderScene& renderScene, const Ray& ray, float* hitDistance)
    {
        const XMVECTOR origin = XMLoadFloat3(&ray.origin);
        const XMVECTOR direction = XMLoadFloat3(&ray.direction);
        float nearest = std::numeric_limits<float>::max();
        std::optional<EntityId> picked;

        for (const MeshInstance& instance : renderScene.meshInstances)
        {
            if (instance.entity == NoEntity)
                continue;

            const XMMATRIX world = XMLoadFloat4x4(&instance.world);

            for (uint32_t s = 0; s < instance.mesh->GetSubmeshes().size(); ++s)
            {
                BoundingBox bounds;
                instance.mesh->GetSubmeshBounds(s).Transform(bounds, world);

                // Rays starting inside a box report 0; skip those so a camera inside a building can
                // still pick what it looks at.
                float distance = 0.0f;

                if (bounds.Intersects(origin, direction, distance) && distance > 0.0f && distance < nearest)
                {
                    nearest = distance;
                    picked = instance.entity;
                }
            }
        }

        if (picked && hitDistance)
            *hitDistance = nearest;

        return picked;
    }

    std::optional<BoundingSphere> ComputeBounds(const Scene& scene, const RenderScene& renderScene,
                                                std::span<const EntityId> entities)
    {
        auto isIncluded = [&](EntityId id) {
            for (EntityId root : entities)
            {
                if (scene.IsValid(id) && scene.IsDescendantOf(id, root))
                    return true;
            }

            return false;
        };

        std::optional<BoundingBox> total;
        auto merge = [&](const BoundingBox& box) {
            if (total)
                BoundingBox::CreateMerged(*total, *total, box);
            else
                total = box;
        };

        for (const MeshInstance& instance : renderScene.meshInstances)
        {
            if (!isIncluded(instance.entity))
                continue;

            BoundingBox bounds;
            instance.mesh->GetLocalBounds().Transform(bounds, XMLoadFloat4x4(&instance.world));
            merge(bounds);
        }

        const std::vector<XMFLOAT4X4> world = scene.ComputeWorldMatrices();

        for (EntityId id = 0; id < scene.GetEntityCount(); ++id)
        {
            if (!isIncluded(id))
                continue;

            // Entities without geometry (lights, empties) count as a one-meter box at their position.
            const XMFLOAT3 position(world[id]._41, world[id]._42, world[id]._43);
            merge(BoundingBox(position, XMFLOAT3(0.5f, 0.5f, 0.5f)));
        }

        if (!total)
            return std::nullopt;

        BoundingSphere sphere;
        BoundingSphere::CreateFromBoundingBox(sphere, *total);
        return sphere;
    }
}
