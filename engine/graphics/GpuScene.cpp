#include "graphics/GpuScene.h"
#include "graphics/Frustum.h"
#include "graphics/Mesh.h"
#include "graphics/RenderScene.h"
#include "graphics/d3d12/CommandContext.h"
#include "core/Log.h"

#include <format>

using namespace DirectX;

namespace atmos
{
    GpuScene::GpuScene(GraphicsDevice& device)
    {
        for (uint32_t i = 0; i < drawBuffers_.size(); ++i)
        {
            drawBuffers_[i] = Buffer(device, {
                .size = MaxDraws * sizeof(DrawData),
                .stride = sizeof(DrawData),
                .memory = BufferMemory::Upload,
                .debugName = std::format(L"Draw Data {}", i),
            });
        }

        draws_.reserve(MaxDraws);
    }

    void GpuScene::Update(uint32_t frameIndex, const RenderScene& scene, const Frustum& frustum)
    {
        frameIndex_ = frameIndex;
        draws_.clear();
        stats_ = { .instances = static_cast<uint32_t>(scene.meshInstances.size()) };

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
                if (draws_.size() == MaxDraws)
                {
                    if (!overflowReported_)
                        log::Warning("GpuScene: more than {} draws; the rest are dropped", MaxDraws);

                    overflowReported_ = true;
                    break;
                }

                const uint32_t drawIndex = static_cast<uint32_t>(draws_.size());
                drawData[drawIndex] = { instance.world, normalMatrix, submeshes[s].baseColor };
                draws_.push_back({ instance.mesh, s, drawIndex });
            }
        }

        stats_.draws = static_cast<uint32_t>(draws_.size());
    }

    void GpuScene::RecordDraws(CommandContext& commands) const
    {
        const uint32_t drawBufferIndex = drawBuffers_[frameIndex_].GetSrvIndex();
        const Mesh* boundMesh = nullptr;

        for (const MeshDraw& draw : draws_)
        {
            if (draw.mesh != boundMesh)
            {
                commands.SetIndexBuffer(draw.mesh->GetIndexBuffer());
                boundMesh = draw.mesh;
            }

            commands.SetRootConstants(MeshRootConstants{
                draw.drawIndex,
                drawBufferIndex,
                draw.mesh->GetPositionBuffer().GetSrvIndex(),
                draw.mesh->GetAttributeBuffer().GetSrvIndex(),
            });

            const Submesh& submesh = draw.mesh->GetSubmeshes()[draw.submeshIndex];
            commands.DrawIndexed(submesh.indexCount, submesh.indexOffset);
        }
    }
}
