#pragma once

#include "graphics/d3d12/Buffer.h"
#include "graphics/d3d12/Texture.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace atmos
{
    class CommandContext;
    class GpuProfiler;
    class RenderGraph;
    class TransientResourcePool;

    // What one pass did this frame, for stats and the editor's render-graph view.
    struct RenderPassInfo
    {
        std::wstring name;
        bool culled = false;
        std::vector<std::string> accesses;  // "Scene Depth (depth-write, clear)"
        float gpuMilliseconds = 0.0f;       // filled by the renderer from the profiler
    };

    struct RenderResourceInfo
    {
        std::wstring name;
        std::string description;  // "texture 1280x720x1 R16G16B16A16_FLOAT" / "buffer 13824 B"
        bool imported = false;
    };

    // Typed indices of resources in one frame's graph.
    struct RGTexture
    {
        static constexpr uint32_t InvalidIndex = UINT32_MAX;
        uint32_t index = InvalidIndex;

        bool IsValid() const { return index != InvalidIndex; }
    };

    struct RGBuffer
    {
        static constexpr uint32_t InvalidIndex = UINT32_MAX;
        uint32_t index = InvalidIndex;

        bool IsValid() const { return index != InvalidIndex; }
    };

    // What a pass does with a target's previous contents.
    enum class LoadOp
    {
        Load,      // Keep: the pass also depends on earlier writers.
        Clear,     // Clear to the texture's clear value.
        DontCare,  // The pass overwrites every pixel.
    };

    // Passed to execute callbacks.
    class RGContext
    {
    public:
        RGContext(CommandContext& commands, const RenderGraph& graph) : commands(commands), graph_(graph) {}

        Texture& GetTexture(RGTexture handle) const;
        Buffer& GetBuffer(RGBuffer handle) const;

        CommandContext& commands;

    private:
        const RenderGraph& graph_;
    };

    // Declares how a pass uses resources. Only valid inside the setup callback.
    // `subresource` is a depth slice (WriteDepth) or a mip (ReadWriteTexture); writing a single
    // subresource never counts as overwriting the whole resource.
    class RGBuilder
    {
    public:
        static constexpr uint32_t All = Texture::AllSubresources;

        RGTexture ReadTexture(RGTexture texture);
        RGTexture WriteColor(RGTexture texture, LoadOp loadOp);
        RGTexture WriteDepth(RGTexture texture, LoadOp loadOp, uint32_t slice = All);
        RGTexture ReadDepth(RGTexture texture);
        RGTexture ReadWriteTexture(RGTexture texture, uint32_t mip = All);
        RGBuffer ReadBuffer(RGBuffer buffer);
        RGBuffer ReadWriteBuffer(RGBuffer buffer);
        void SetSideEffects();

    private:
        friend class RenderGraph;
        RGBuilder(RenderGraph& graph, size_t passIndex) : graph_(graph), passIndex_(passIndex) {}

        RenderGraph& graph_;
        size_t passIndex_;
    };

    // Per-frame graph (Frostbite FrameGraph / Unity RenderGraph style): passes declare accesses in
    // setup and record commands in execute. Execute() culls passes whose results are unused, takes
    // transients from the pool, inserts transitions and UAV barriers, then binds and clears targets.
    class RenderGraph
    {
    public:
        explicit RenderGraph(TransientResourcePool& pool);

        RenderGraph(const RenderGraph&) = delete;
        RenderGraph& operator=(const RenderGraph&) = delete;

        // Transient resources; usage flags are derived from how passes access them.
        RGTexture CreateTexture(const TextureDesc& desc);
        RGBuffer CreateBuffer(const BufferDesc& desc);

        // Persistent resources (back buffer, IBL, history). Left in `finalState` after Execute.
        RGTexture ImportTexture(Texture& texture, D3D12_RESOURCE_STATES finalState);
        RGBuffer ImportBuffer(Buffer& buffer);

        template <typename Setup, typename Execute>
        void AddPass(std::wstring name, Setup&& setup, Execute&& execute)
        {
            passes_.push_back({ .name = std::move(name) });
            RGBuilder builder(*this, passes_.size() - 1);
            setup(builder);
            passes_.back().execute = std::forward<Execute>(execute);
        }

        // `profiler` (optional) times every live pass.
        void Execute(CommandContext& commands, GpuProfiler* profiler = nullptr);

        void LogSummary() const;

        // Valid after Execute().
        std::vector<RenderPassInfo> GetPassInfos() const;
        std::vector<RenderResourceInfo> GetResourceInfos() const;

    private:
        friend class RGBuilder;
        friend class RGContext;

        enum class Access
        {
            ShaderRead,
            ColorWrite,
            DepthWrite,
            DepthRead,
            ReadWrite,
        };

        struct PassAccess
        {
            uint32_t resource;
            Access access;
            LoadOp loadOp;
            uint32_t subresource;
        };

        struct Pass
        {
            std::wstring name;
            std::vector<PassAccess> accesses;
            std::function<void(RGContext&)> execute;
            bool hasSideEffects = false;
            bool culled = false;
        };

        struct Resource
        {
            bool isBuffer = false;
            TextureDesc textureDesc;
            BufferDesc bufferDesc;
            Texture* texture = nullptr;  // Set on import or first use.
            Buffer* buffer = nullptr;
            bool imported = false;
            D3D12_RESOURCE_STATES finalState = D3D12_RESOURCE_STATE_COMMON;
            bool lastAccessWasUav = false;
            bool usedThisFrame = false;
        };

        void AddAccess(size_t passIndex, uint32_t resource, Access access, LoadOp loadOp, uint32_t subresource);
        void CullPasses();
        void DeriveTransientUsage();
        Resource& Resolve(uint32_t resource);
        void ExecutePass(Pass& pass, CommandContext& commands, GpuProfiler* profiler);
        const std::wstring& GetName(const Resource& resource) const;
        std::vector<std::string> DescribeAccesses(const Pass& pass) const;
        static std::string DescribeResource(const Resource& resource);

        static bool Writes(const PassAccess& access);
        static bool OverwritesWholeResource(const PassAccess& access);
        static bool ReadsPreviousContents(const PassAccess& access);

        TransientResourcePool& pool_;
        std::vector<Resource> resources_;
        std::vector<Pass> passes_;
    };
}
