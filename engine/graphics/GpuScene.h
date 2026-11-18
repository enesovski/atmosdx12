#pragma once

#include "graphics/d3d12/Buffer.h"
#include "graphics/d3d12/GraphicsConfig.h"

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
    struct Material;
    struct RenderScene;

    // Mirrors DrawData in Mesh.hlsl. One entry per visible (instance, submesh).
    struct DrawData
    {
        DirectX::XMFLOAT4X4 world;
        DirectX::XMFLOAT4X4 normalMatrix;  // inverse-transpose of world
        uint32_t materialIndex;            // into this frame's material buffer
        uint32_t padding[3];
    };

    static_assert(sizeof(DrawData) == 144);

    // Mirrors MeshRootConstants in Mesh.hlsl.
    struct MeshRootConstants
    {
        uint32_t drawIndex;
        uint32_t drawBufferIndex;
        uint32_t materialBufferIndex;
        uint32_t positionBufferIndex;
        uint32_t attributeBufferIndex;
    };

    // Draws grouped by the pipeline state they need. Mask covers Blend until a transparent pass exists.
    enum class DrawBucket : uint32_t
    {
        Opaque,
        OpaqueDoubleSided,
        Masked,
        MaskedDoubleSided,
        Count,
    };

    struct MeshDraw
    {
        const Mesh* mesh;
        uint32_t submeshIndex;
        uint32_t drawIndex;
    };

    struct GpuSceneStats
    {
        uint32_t instances = 0;
        uint32_t culledInstances = 0;
        uint32_t draws = 0;
        uint32_t materials = 0;
    };

    // The renderer's per-frame view of the scene: frustum-culls mesh instances, writes their
    // DrawData and the materials they use into this frame slot's upload buffers, and keeps the
    // visible draws grouped by bucket. Mesh passes draw a bucket with RecordDraws().
    class GpuScene
    {
    public:
        static constexpr uint32_t MaxDraws = 16384;
        static constexpr uint32_t MaxMaterials = 4096;

        explicit GpuScene(GraphicsDevice& device);

        // frameIndex is the FrameScheduler slot; its fence protects the buffers being written.
        void Update(uint32_t frameIndex, const RenderScene& scene, const Frustum& frustum);

        // Binds per-draw root constants and index buffers, then draws. The caller sets the pipeline.
        void RecordDraws(CommandContext& commands, DrawBucket bucket) const;

        bool HasDraws(DrawBucket bucket) const { return !buckets_[static_cast<size_t>(bucket)].empty(); }
        const GpuSceneStats& GetStats() const { return stats_; }

    private:
        uint32_t GetMaterialSlot(const Material& material);

        std::array<Buffer, GraphicsConfig::FramesInFlight> drawBuffers_;
        std::array<Buffer, GraphicsConfig::FramesInFlight> materialBuffers_;
        std::array<std::vector<MeshDraw>, static_cast<size_t>(DrawBucket::Count)> buckets_;
        std::unordered_map<const Material*, uint32_t> materialSlots_;
        GpuSceneStats stats_;
        uint32_t frameIndex_ = 0;
        bool overflowReported_ = false;
    };
}
