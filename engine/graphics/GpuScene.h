#pragma once

#include "graphics/Material.h"
#include "graphics/d3d12/FrameStructuredBuffer.h"

#include <DirectXCollision.h>
#include <DirectXMath.h>

#include <array>
#include <cstdint>
#include <span>
#include <unordered_map>
#include <vector>

namespace atmos
{
    class CommandContext;
    class Frustum;
    class GraphicsDevice;
    class Mesh;
    struct RenderScene;

    // Mirrors DrawData in shaders/Scene/SceneData.hlsli. One entry per (instance, submesh).
    struct DrawData
    {
        DirectX::XMFLOAT4X4 world;
        DirectX::XMFLOAT4X4 normalMatrix;  // inverse-transpose of world
        uint32_t materialIndex;
        uint32_t padding[3];
    };

    static_assert(sizeof(DrawData) == 144);

    // Mirrors MeshRootConstants in shaders/Scene/SceneData.hlsli.
    struct MeshRootConstants
    {
        uint32_t drawIndex;
        uint32_t drawBufferIndex;
        uint32_t materialBufferIndex;
        uint32_t positionBufferIndex;
        uint32_t attributeBufferIndex;
        uint32_t viewBufferIndex;
        uint32_t viewIndex;
    };

    // Draws grouped by pipeline state. The first DepthBucketCount write depth and cast shadows.
    enum class DrawBucket : uint32_t
    {
        Opaque,
        OpaqueDoubleSided,
        Masked,
        MaskedDoubleSided,
        Transparent,
        Count,
    };

    inline constexpr uint32_t DepthBucketCount = 4;

    // Visible draws of one view, as indices into the frame's DrawData.
    struct DrawList
    {
        std::array<std::vector<uint32_t>, static_cast<size_t>(DrawBucket::Count)> buckets;

        void Clear();
        uint32_t GetDrawCount() const;
        const std::vector<uint32_t>& operator[](DrawBucket bucket) const { return buckets[static_cast<size_t>(bucket)]; }
    };

    struct GpuSceneStats
    {
        uint32_t instances = 0;
        uint32_t draws = 0;
        uint32_t materials = 0;
    };

    // The renderer's per-frame copy of the scene: DrawData and materials for every submesh,
    // written once per frame and shared by all views. Each view culls its own DrawList.
    class GpuScene
    {
    public:
        static constexpr uint32_t MaxDraws = 16384;
        static constexpr uint32_t MaxMaterials = 4096;

        explicit GpuScene(GraphicsDevice& device);

        // frameIndex: the FrameScheduler slot whose fence guards the written buffers.
        void Update(uint32_t frameIndex, const RenderScene& scene);

        void Cull(const Frustum& frustum, DrawList& out, bool includeTransparent) const;

        // Draws of the given entities (sorted ids) inside the frustum, transparent ones included.
        void CullEntities(const Frustum& frustum, std::span<const uint32_t> sortedEntities, DrawList& out) const;
        void SortTransparentBackToFront(DrawList& list, const DirectX::XMFLOAT3& eye) const;

        // The caller sets the pipeline.
        void RecordDraws(CommandContext& commands, const DrawList& list, DrawBucket bucket, uint32_t viewBufferIndex,
                         uint32_t viewIndex) const;

        uint32_t GetMaterialBufferIndex() const { return materials_.GetSrvIndex(frameIndex_); }
        uint32_t GetDrawBufferIndex() const { return drawData_.GetSrvIndex(frameIndex_); }
        const GpuSceneStats& GetStats() const { return stats_; }

    private:
        struct DrawRecord
        {
            const Mesh* mesh;
            uint32_t submeshIndex;
            DrawBucket bucket;
            uint32_t entity;
            DirectX::BoundingBox worldBounds;
        };

        uint32_t GetMaterialSlot(const Material& material);

        FrameStructuredBuffer<DrawData> drawData_;
        FrameStructuredBuffer<MaterialGpuData> materials_;
        std::vector<DrawRecord> draws_;
        std::unordered_map<const Material*, uint32_t> materialSlots_;
        GpuSceneStats stats_;
        uint32_t frameIndex_ = 0;
        bool overflowReported_ = false;
    };
}
