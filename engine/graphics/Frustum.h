#pragma once

#include <DirectXCollision.h>
#include <DirectXMath.h>

#include <array>

namespace atmos
{
    // View frustum for CPU culling: left, right, bottom, top and near planes.
    // The infinite reverse-Z projection has no far plane.
    class Frustum
    {
    public:
        static Frustum FromViewProjection(DirectX::FXMMATRIX viewProjection);

        bool Intersects(const DirectX::BoundingBox& box) const;

    private:
        std::array<DirectX::XMFLOAT4, 5> planes_{};  // xyz normal pointing inside, w distance
    };
}
