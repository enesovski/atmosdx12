#include "graphics/Mesh.h"
#include "graphics/d3d12/UploadContext.h"
#include "core/Assert.h"
#include "core/StringUtils.h"

namespace atmos
{
    namespace
    {
        template <typename T>
        Buffer CreateStaticBuffer(GraphicsDevice& device, UploadContext& uploadContext,
                                  const std::vector<T>& data, const std::wstring& name)
        {
            const uint64_t size = data.size() * sizeof(T);

            Buffer buffer(device, { .size = size, .debugName = name });
            uploadContext.UploadBuffer(buffer, data.data(), size);
            return buffer;
        }
    }

    Mesh::Mesh(GraphicsDevice& device, UploadContext& uploadContext, const MeshData& data)
        : submeshes_(data.submeshes), name_(data.name)
    {
        ATMOS_ASSERT(!data.positions.empty() && !data.indices.empty(), "Mesh has no geometry");
        ATMOS_ASSERT(data.positions.size() == data.attributes.size(), "Every vertex needs attributes");

        const std::wstring name = ToWide(data.name);

        // Raw (ByteAddressBuffer) views: the shader computes byte offsets itself.
        positions_ = CreateStaticBuffer(device, uploadContext, data.positions, name + L" Positions");
        attributes_ = CreateStaticBuffer(device, uploadContext, data.attributes, name + L" Attributes");
        indices_ = CreateStaticBuffer(device, uploadContext, data.indices, name + L" Indices");

        DirectX::BoundingBox::CreateFromPoints(localBounds_, data.positions.size(), data.positions.data(),
                                               sizeof(DirectX::XMFLOAT3));
    }
}
