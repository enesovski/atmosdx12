#include "graphics/Mesh.h"
#include "graphics/d3d12/UploadQueue.h"
#include "core/Assert.h"
#include "core/StringUtils.h"

#include <cfloat>

namespace atmos
{
    namespace
    {
        template <typename T>
        Buffer CreateStaticBuffer(GraphicsDevice& device, UploadQueue& uploadQueue, const std::vector<T>& data,
                                  const std::wstring& name)
        {
            const uint64_t size = data.size() * sizeof(T);

            Buffer buffer(device, { .size = size, .debugName = name });
            uploadQueue.UploadBuffer(buffer, data.data(), size);
            return buffer;
        }
    }

    Mesh::Mesh(GraphicsDevice& device, UploadQueue& uploadQueue, const MeshData& data)
        : submeshes_(data.submeshes), name_(data.name)
    {
        ATMOS_ASSERT(!data.positions.empty() && !data.indices.empty(), "Mesh has no geometry");
        ATMOS_ASSERT(data.positions.size() == data.attributes.size(), "Every vertex needs attributes");

        const std::wstring name = ToWide(data.name);

        // Raw (ByteAddressBuffer) views: the shader computes byte offsets itself.
        positions_ = CreateStaticBuffer(device, uploadQueue, data.positions, name + L" Positions");
        attributes_ = CreateStaticBuffer(device, uploadQueue, data.attributes, name + L" Attributes");
        indices_ = CreateStaticBuffer(device, uploadQueue, data.indices, name + L" Indices");

        DirectX::BoundingBox::CreateFromPoints(localBounds_, data.positions.size(), data.positions.data(),
                                               sizeof(DirectX::XMFLOAT3));

        // Per-submesh bounds let views cull parts of large meshes (Sponza is one mesh).
        for (const Submesh& submesh : submeshes_)
        {
            DirectX::XMVECTOR min = DirectX::XMVectorReplicate(FLT_MAX);
            DirectX::XMVECTOR max = DirectX::XMVectorReplicate(-FLT_MAX);

            for (uint32_t i = submesh.indexOffset; i < submesh.indexOffset + submesh.indexCount; ++i)
            {
                const DirectX::XMVECTOR position = DirectX::XMLoadFloat3(&data.positions[data.indices[i]]);
                min = DirectX::XMVectorMin(min, position);
                max = DirectX::XMVectorMax(max, position);
            }

            DirectX::BoundingBox& bounds = submeshBounds_.emplace_back();
            DirectX::BoundingBox::CreateFromPoints(bounds, min, max);
        }
    }
}
