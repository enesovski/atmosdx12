#pragma once

#include "scene/Scene.h"

#include <DirectXCollision.h>
#include <DirectXMath.h>

#include <optional>
#include <span>
#include <vector>

namespace atmos
{
    class Camera;
    struct RenderScene;
}

namespace atmos::editor
{
    struct Ray
    {
        DirectX::XMFLOAT3 origin;
        DirectX::XMFLOAT3 direction;  // normalized
    };

    // Maps between the viewport image (pixels, origin top-left) and the world, for the camera's
    // perspective. Works with the reverse-Z infinite projection since only x/y and w are used.
    class ViewportProjection
    {
    public:
        ViewportProjection(const Camera& camera, DirectX::XMFLOAT2 rectMin, DirectX::XMFLOAT2 rectSize);

        Ray GetRay(DirectX::XMFLOAT2 screenPoint) const;

        // Screen position of a world point, or nothing when it is behind the camera.
        std::optional<DirectX::XMFLOAT2> Project(const DirectX::XMFLOAT3& point) const;

        // Screen positions of a world-space segment clipped to the near plane; false if fully behind.
        bool ProjectSegment(DirectX::XMFLOAT3 a, DirectX::XMFLOAT3 b, DirectX::XMFLOAT2& screenA,
                            DirectX::XMFLOAT2& screenB) const;

        float GetAspect() const { return rectSize_.x / rectSize_.y; }

    private:
        DirectX::XMFLOAT4X4 viewProjection_;
        DirectX::XMFLOAT3 position_;
        DirectX::XMFLOAT3 forward_;
        DirectX::XMFLOAT3 right_;
        DirectX::XMFLOAT3 up_;
        float tanHalfFovY_;
        DirectX::XMFLOAT2 rectMin_;
        DirectX::XMFLOAT2 rectSize_;
    };

    // Nearest entity whose rendered geometry's bounds the ray hits (whole-entity selection).
    std::optional<EntityId> PickMesh(const RenderScene& renderScene, const Ray& ray, float* hitDistance = nullptr);

    // Bounding sphere of the given entities and their descendants: rendered geometry, plus light
    // positions (with a small radius). Empty if nothing has a position yet.
    std::optional<DirectX::BoundingSphere> ComputeBounds(const Scene& scene, const RenderScene& renderScene,
                                                        std::span<const EntityId> entities);
}
