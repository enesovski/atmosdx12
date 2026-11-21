#include "graphics/GpuScene.h"
#include "graphics/Frustum.h"
#include "graphics/Mesh.h"
#include "graphics/RenderScene.h"
#include "graphics/d3d12/CommandContext.h"
#include "core/Log.h"

#include <algorithm>

using namespace DirectX;

namespace atmos
{
    namespace
    {
        DrawBucket GetBucket(const MaterialData& material)
        {
            switch (material.alphaMode)
            {
                case AlphaMode::Blend:
                    return DrawBucket::Transparent;
                case AlphaMode::Mask:
                    return material.doubleSided ? DrawBucket::MaskedDoubleSided : DrawBucket::Masked;
                default:
                    return material.doubleSided ? DrawBucket::OpaqueDoubleSided : DrawBucket::Opaque;
            }
        }
    }

    void DrawList::Clear()
    {
        for (std::vector<uint32_t>& bucket : buckets)
            bucket.clear();
    }

    uint32_t DrawList::GetDrawCount() const
    {
        size_t count = 0;

        for (const std::vector<uint32_t>& bucket : buckets)
            count += bucket.size();

        return static_cast<uint32_t>(count);
    }

    GpuScene::GpuScene(GraphicsDevice& device)
        : drawData_(device, MaxDraws, L"Draw Data"), materials_(device, MaxMaterials, L"Material Data")
    {
        draws_.reserve(MaxDraws);
    }

    void GpuScene::Update(uint32_t frameIndex, const RenderScene& scene)
    {
        frameIndex_ = frameIndex;
        stats_ = { .instances = static_cast<uint32_t>(scene.meshInstances.size()) };
        draws_.clear();
        materialSlots_.clear();

        DrawData* drawData = drawData_.GetMappedData(frameIndex);

        for (const MeshInstance& instance : scene.meshInstances)
        {
            const XMMATRIX world = XMLoadFloat4x4(&instance.world);
            XMFLOAT4X4 normalMatrix;
            XMStoreFloat4x4(&normalMatrix, XMMatrixTranspose(XMMatrixInverse(nullptr, world)));

            const std::span<const Submesh> submeshes = instance.mesh->GetSubmeshes();

            for (uint32_t s = 0; s < submeshes.size(); ++s)
            {
                if (draws_.size() == MaxDraws || submeshes[s].materialIndex >= instance.materials.size())
                {
                    if (!overflowReported_)
                        log::Warning("GpuScene: draw dropped (limit {} or missing material)", MaxDraws);

                    overflowReported_ = true;
                    continue;
                }

                const Material& material = *instance.materials[submeshes[s].materialIndex];
                drawData[draws_.size()] = { instance.world, normalMatrix, GetMaterialSlot(material), {} };

                DrawRecord& record =
                    draws_.emplace_back(DrawRecord{ instance.mesh, s, GetBucket(material.data), instance.entity, {} });
                instance.mesh->GetSubmeshBounds(s).Transform(record.worldBounds, world);
            }
        }

        stats_.draws = static_cast<uint32_t>(draws_.size());
        stats_.materials = static_cast<uint32_t>(materialSlots_.size());
    }

    uint32_t GpuScene::GetMaterialSlot(const Material& material)
    {
        const auto [entry, inserted] = materialSlots_.try_emplace(&material, static_cast<uint32_t>(materialSlots_.size()));

        if (inserted)
        {
            // Overflowing materials share slot 0 rather than writing past the buffer.
            if (entry->second >= MaxMaterials)
            {
                entry->second = 0;
                return 0;
            }

            materials_.GetMappedData(frameIndex_)[entry->second] = material.ToGpuData();
        }

        return entry->second;
    }

    void GpuScene::Cull(const Frustum& frustum, DrawList& out, bool includeTransparent) const
    {
        out.Clear();

        for (uint32_t i = 0; i < draws_.size(); ++i)
        {
            const DrawRecord& draw = draws_[i];

            if (draw.bucket == DrawBucket::Transparent && !includeTransparent)
                continue;

            if (frustum.Intersects(draw.worldBounds))
                out.buckets[static_cast<size_t>(draw.bucket)].push_back(i);
        }
    }

    void GpuScene::CullEntities(const Frustum& frustum, std::span<const uint32_t> sortedEntities, DrawList& out) const
    {
        out.Clear();

        if (sortedEntities.empty())
            return;

        for (uint32_t i = 0; i < draws_.size(); ++i)
        {
            const DrawRecord& draw = draws_[i];

            if (std::ranges::binary_search(sortedEntities, draw.entity) && frustum.Intersects(draw.worldBounds))
                out.buckets[static_cast<size_t>(draw.bucket)].push_back(i);
        }
    }

    void GpuScene::SortTransparentBackToFront(DrawList& list, const XMFLOAT3& eye) const
    {
        const XMVECTOR eyePosition = XMLoadFloat3(&eye);

        auto distanceSquared = [&](uint32_t drawIndex) {
            return XMVectorGetX(XMVector3LengthSq(XMLoadFloat3(&draws_[drawIndex].worldBounds.Center) - eyePosition));
        };

        std::ranges::sort(list.buckets[static_cast<size_t>(DrawBucket::Transparent)],
                          [&](uint32_t a, uint32_t b) { return distanceSquared(a) > distanceSquared(b); });
    }

    void GpuScene::RecordDraws(CommandContext& commands, const DrawList& list, DrawBucket bucket, uint32_t viewBufferIndex,
                               uint32_t viewIndex) const
    {
        const uint32_t drawBufferIndex = GetDrawBufferIndex();
        const uint32_t materialBufferIndex = GetMaterialBufferIndex();
        const Mesh* boundMesh = nullptr;

        for (uint32_t drawIndex : list[bucket])
        {
            const DrawRecord& draw = draws_[drawIndex];

            if (draw.mesh != boundMesh)
            {
                commands.SetIndexBuffer(draw.mesh->GetIndexBuffer());
                boundMesh = draw.mesh;
            }

            commands.SetRootConstants(MeshRootConstants{
                drawIndex,
                drawBufferIndex,
                materialBufferIndex,
                draw.mesh->GetPositionBuffer().GetSrvIndex(),
                draw.mesh->GetAttributeBuffer().GetSrvIndex(),
                viewBufferIndex,
                viewIndex,
            });

            const Submesh& submesh = draw.mesh->GetSubmeshes()[draw.submeshIndex];
            commands.DrawIndexed(submesh.indexCount, submesh.indexOffset);
        }
    }
}
