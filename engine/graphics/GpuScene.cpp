#include "graphics/GpuScene.h"
#include "graphics/Frustum.h"
#include "graphics/Material.h"
#include "graphics/Mesh.h"
#include "graphics/RenderScene.h"
#include "graphics/d3d12/CommandContext.h"
#include "core/Log.h"

#include <format>

using namespace DirectX;

namespace atmos
{
    namespace
    {
        DrawBucket GetBucket(const MaterialData& material)
        {
            const bool masked = material.alphaMode != AlphaMode::Opaque;

            if (masked)
                return material.doubleSided ? DrawBucket::MaskedDoubleSided : DrawBucket::Masked;

            return material.doubleSided ? DrawBucket::OpaqueDoubleSided : DrawBucket::Opaque;
        }
    }

    GpuScene::GpuScene(GraphicsDevice& device)
    {
        for (uint32_t i = 0; i < GraphicsConfig::FramesInFlight; ++i)
        {
            drawBuffers_[i] = Buffer(device, {
                .size = MaxDraws * sizeof(DrawData),
                .stride = sizeof(DrawData),
                .memory = BufferMemory::Upload,
                .debugName = std::format(L"Draw Data {}", i),
            });

            materialBuffers_[i] = Buffer(device, {
                .size = MaxMaterials * sizeof(MaterialGpuData),
                .stride = sizeof(MaterialGpuData),
                .memory = BufferMemory::Upload,
                .debugName = std::format(L"Material Data {}", i),
            });
        }
    }

    void GpuScene::Update(uint32_t frameIndex, const RenderScene& scene, const Frustum& frustum)
    {
        frameIndex_ = frameIndex;
        stats_ = { .instances = static_cast<uint32_t>(scene.meshInstances.size()) };
        materialSlots_.clear();

        for (std::vector<MeshDraw>& bucket : buckets_)
            bucket.clear();

        auto* drawData = reinterpret_cast<DrawData*>(drawBuffers_[frameIndex].GetMappedData());

        for (const MeshInstance& instance : scene.meshInstances)
        {
            const XMMATRIX world = XMLoadFloat4x4(&instance.world);

            BoundingBox worldBounds;
            instance.mesh->GetLocalBounds().Transform(worldBounds, world);

            if (!frustum.Intersects(worldBounds))
            {
                ++stats_.culledInstances;
                continue;
            }

            XMFLOAT4X4 normalMatrix;
            XMStoreFloat4x4(&normalMatrix, XMMatrixTranspose(XMMatrixInverse(nullptr, world)));

            const std::span<const Submesh> submeshes = instance.mesh->GetSubmeshes();

            for (uint32_t s = 0; s < submeshes.size(); ++s)
            {
                if (stats_.draws == MaxDraws || submeshes[s].materialIndex >= instance.materials.size())
                {
                    if (!overflowReported_)
                        log::Warning("GpuScene: draw dropped (draw limit {} or missing material)", MaxDraws);

                    overflowReported_ = true;
                    continue;
                }

                const Material& material = *instance.materials[submeshes[s].materialIndex];
                const uint32_t drawIndex = stats_.draws++;

                drawData[drawIndex] = { instance.world, normalMatrix, GetMaterialSlot(material), {} };
                buckets_[static_cast<size_t>(GetBucket(material.data))].push_back({ instance.mesh, s, drawIndex });
            }
        }

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

            auto* materials = reinterpret_cast<MaterialGpuData*>(materialBuffers_[frameIndex_].GetMappedData());
            materials[entry->second] = material.ToGpuData();
        }

        return entry->second;
    }

    void GpuScene::RecordDraws(CommandContext& commands, DrawBucket bucket) const
    {
        const uint32_t drawBufferIndex = drawBuffers_[frameIndex_].GetSrvIndex();
        const uint32_t materialBufferIndex = materialBuffers_[frameIndex_].GetSrvIndex();
        const Mesh* boundMesh = nullptr;

        for (const MeshDraw& draw : buckets_[static_cast<size_t>(bucket)])
        {
            if (draw.mesh != boundMesh)
            {
                commands.SetIndexBuffer(draw.mesh->GetIndexBuffer());
                boundMesh = draw.mesh;
            }

            commands.SetRootConstants(MeshRootConstants{
                draw.drawIndex,
                drawBufferIndex,
                materialBufferIndex,
                draw.mesh->GetPositionBuffer().GetSrvIndex(),
                draw.mesh->GetAttributeBuffer().GetSrvIndex(),
            });

            const Submesh& submesh = draw.mesh->GetSubmeshes()[draw.submeshIndex];
            commands.DrawIndexed(submesh.indexCount, submesh.indexOffset);
        }
    }
}
