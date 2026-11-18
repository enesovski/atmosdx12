#include "assets/PrimitiveMeshes.h"

using namespace DirectX;

namespace atmos
{
    namespace
    {
        // Appends a quad facing `normal`. `up` is the quad's up direction as seen from the
        // front; corners are emitted clockwise as seen from the front (engine front faces).
        void AddQuad(MeshData& mesh, XMVECTOR center, XMVECTOR normal, XMVECTOR up, float halfSize)
        {
            // Left-handed: the viewer looks along -normal, and right = up x forward.
            const XMVECTOR right = XMVector3Cross(up, -normal) * halfSize;
            const XMVECTOR scaledUp = up * halfSize;

            const XMVECTOR corners[4] = {
                center - right - scaledUp,  // bottom-left
                center - right + scaledUp,  // top-left
                center + right + scaledUp,  // top-right
                center + right - scaledUp,  // bottom-right
            };
            const XMFLOAT2 uvs[4] = { { 0, 1 }, { 0, 0 }, { 1, 0 }, { 1, 1 } };

            const uint32_t base = static_cast<uint32_t>(mesh.positions.size());

            for (int i = 0; i < 4; ++i)
            {
                XMFLOAT3 position;
                VertexAttributes attributes{};
                XMStoreFloat3(&position, corners[i]);
                XMStoreFloat3(&attributes.normal, normal);
                // Tangent follows +u (right); with w = 1, cross(N, T) = -up follows +v (down).
                XMStoreFloat4(&attributes.tangent, XMVectorSetW(XMVector3Normalize(right), 1.0f));
                attributes.uv = uvs[i];

                mesh.positions.push_back(position);
                mesh.attributes.push_back(attributes);
            }

            mesh.indices.insert(mesh.indices.end(), { base, base + 1, base + 2, base, base + 2, base + 3 });
        }

        void FinishSingleSubmesh(MeshData& mesh)
        {
            mesh.submeshes.push_back({ 0, static_cast<uint32_t>(mesh.indices.size()), 0 });
        }
    }

    MeshData CreateCubeMeshData()
    {
        MeshData mesh;
        mesh.name = "Cube";

        const XMVECTOR up = XMVectorSet(0, 1, 0, 0);
        const XMVECTOR forward = XMVectorSet(0, 0, 1, 0);

        const XMVECTOR faces[6][2] = {
            { XMVectorSet(1, 0, 0, 0), up },  { XMVectorSet(-1, 0, 0, 0), up },
            { XMVectorSet(0, 0, 1, 0), up },  { XMVectorSet(0, 0, -1, 0), up },
            { XMVectorSet(0, 1, 0, 0), forward }, { XMVectorSet(0, -1, 0, 0), forward },
        };

        for (const auto& face : faces)
            AddQuad(mesh, face[0] * 0.5f, face[0], face[1], 0.5f);

        FinishSingleSubmesh(mesh);
        return mesh;
    }

    MeshData CreatePlaneMeshData(float size)
    {
        MeshData mesh;
        mesh.name = "Plane";

        AddQuad(mesh, XMVectorZero(), XMVectorSet(0, 1, 0, 0), XMVectorSet(0, 0, 1, 0), size * 0.5f);

        FinishSingleSubmesh(mesh);
        return mesh;
    }
}
