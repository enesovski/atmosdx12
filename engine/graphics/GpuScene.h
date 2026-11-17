#pragma once

#include "graphics/d3d12/Buffer.h"
#include "graphics/d3d12/GraphicsConfig.h"

#include <DirectXMath.h>

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace atmos
{
    class CommandContext;
    class Frustum;
    class GraphicsDevice;
    class Mesh;
    struct RenderScene;

    // Mirrors DrawData in Mesh.hlsl. One entry per visible (instance, submesh).
    struct DrawData
    {
        DirectX::XMFLOAT4X4 world;
        DirectX::XMFLOAT4X4 normalMatrix;  // inverse-transpose of world
        DirectX::XMFLOAT4 baseColor;
    };

    static_assert(sizeof(DrawData) == 144);

    // Mirrors MeshRootConstants in Mesh.hlsl.
    struct MeshRootConstants
    {
        uint32_t drawIndex;
        uint32_t drawBufferIndex;
        uint32_t positionBufferIndex;
        uint32_t attributeBufferIndex;
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
    };

    // The renderer's per-frame view of the scene: frustum-culls mesh instances, writes their
    // DrawData into this frame slot's upload buffer and keeps the visible draw list.
    // Every mesh pass draws the same list with RecordDraws().
    class GpuScene
    {
    public:
        static constexpr uint32_t MaxDraws = 16384;

        explicit GpuScene(GraphicsDevice& device);

        // frameIndex is the FrameScheduler slot; its fence protects the buffer being written.
        void Update(uint32_t frameIndex, const RenderScene& scene, const Frustum& frustum);

        // Binds per-draw root constants and index buffers, then draws. The caller sets the pipeline.
        void RecordDraws(CommandContext& commands) const;

        std::span<const MeshDraw> GetDraws() const { return draws_; }
        const GpuSceneStats& GetStats() const { return stats_; }

    private:
        std::array<Buffer, GraphicsConfig::FramesInFlight> drawBuffers_;
        std::vector<MeshDraw> draws_;
        GpuSceneStats stats_;
        uint32_t frameIndex_ = 0;
        bool overflowReported_ = false;
    };
}
