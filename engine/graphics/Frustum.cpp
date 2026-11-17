#include "graphics/Frustum.h"

using namespace DirectX;

namespace atmos
{
    Frustum Frustum::FromViewProjection(FXMMATRIX viewProjection)
    {
        // Gribb-Hartmann: with row vectors, clip = p * M, so clip.x is dot(p, column 0) and so on.
        // The transposed matrix has those columns as rows. A point is inside where -w <= x <= w,
        // -w <= y <= w and z <= w (reverse-Z near plane).
        const XMMATRIX columns = XMMatrixTranspose(viewProjection);

        const XMVECTOR planes[5] = {
            columns.r[3] + columns.r[0],  // left
            columns.r[3] - columns.r[0],  // right
            columns.r[3] + columns.r[1],  // bottom
            columns.r[3] - columns.r[1],  // top
            columns.r[3] - columns.r[2],  // near
        };

        Frustum frustum;

        for (size_t i = 0; i < frustum.planes_.size(); ++i)
            XMStoreFloat4(&frustum.planes_[i], XMPlaneNormalize(planes[i]));

        return frustum;
    }

    bool Frustum::Intersects(const BoundingBox& box) const
    {
        const XMVECTOR center = XMLoadFloat3(&box.Center);
        const XMVECTOR extents = XMLoadFloat3(&box.Extents);

        for (const XMFLOAT4& planeValue : planes_)
        {
            // Outside if even the corner furthest along the plane normal is behind it.
            const XMVECTOR plane = XMLoadFloat4(&planeValue);
            const float distance = XMVectorGetX(XMPlaneDotCoord(plane, center));
            const float radius = XMVectorGetX(XMVector3Dot(extents, XMVectorAbs(plane)));

            if (distance + radius < 0.0f)
                return false;
        }

        return true;
    }
}
