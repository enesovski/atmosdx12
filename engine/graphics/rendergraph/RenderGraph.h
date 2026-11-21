#pragma once

#include "graphics/d3d12/Texture.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace atmos
{
    class CommandContext;
    class RenderGraph;
    class TexturePool;

    struct RenderPassInfo
    {
        std::wstring name;
        bool culled = false;
    };

    // Index of a texture in one frame's graph.
    struct RGTexture
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

        CommandContext& commands;

    private:
        const RenderGraph& graph_;
    };

    // Declares how a pass uses resources. Only valid inside the setup callback.
    class RGBuilder
    {
    public:
        RGTexture ReadTexture(RGTexture texture);                       // SRV in any shader stage
        RGTexture WriteColor(RGTexture texture, LoadOp loadOp);         // render target
        RGTexture WriteDepth(RGTexture texture, LoadOp loadOp);         // depth test + write
        RGTexture ReadDepth(RGTexture texture);                         // depth test only
        RGTexture ReadWriteTexture(RGTexture texture);                  // UAV
        void SetSideEffects();                                          // never culled

    private:
        friend class RenderGraph;
        RGBuilder(RenderGraph& graph, size_t passIndex) : graph_(graph), passIndex_(passIndex) {}

        RenderGraph& graph_;
        size_t passIndex_;
    };

    // Per-frame graph: passes declare accesses in setup and record commands in execute.
    // Execute() culls unused passes, allocates transients, then transitions and binds targets.
    class RenderGraph
    {
    public:
        explicit RenderGraph(TexturePool& texturePool);

        RenderGraph(const RenderGraph&) = delete;
        RenderGraph& operator=(const RenderGraph&) = delete;

        // Transient texture; usage flags are derived from how passes access it.
        RGTexture CreateTexture(const TextureDesc& desc);

        // External texture (e.g. back buffer). It is left in `finalState` after Execute.
        RGTexture ImportTexture(Texture& texture, D3D12_RESOURCE_STATES finalState);

        template <typename Setup, typename Execute>
        void AddPass(std::wstring name, Setup&& setup, Execute&& execute)
        {
            passes_.push_back({ .name = std::move(name) });
            RGBuilder builder(*this, passes_.size() - 1);
            setup(builder);
            passes_.back().execute = std::forward<Execute>(execute);
        }

        void Execute(CommandContext& commands);

        // Logs passes, their accesses and textures.
        void LogSummary() const;

        // Valid after Execute().
        std::vector<RenderPassInfo> GetPassInfos() const;

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
            RGTexture texture;
            Access access;
            LoadOp loadOp;
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
            TextureDesc desc;
            Texture* texture = nullptr;  // Set on import or first use.
            bool imported = false;
            D3D12_RESOURCE_STATES finalState = D3D12_RESOURCE_STATE_COMMON;
        };

        void AddAccess(size_t passIndex, RGTexture texture, Access access, LoadOp loadOp);
        void CullPasses();
        void DeriveTransientUsage();
        Texture& ResolveTexture(RGTexture handle);
        void ExecutePass(Pass& pass, CommandContext& commands);

        static bool ReadsPreviousContents(const PassAccess& access);
        static bool Writes(const PassAccess& access);

        TexturePool& texturePool_;
        std::vector<Resource> resources_;
        std::vector<Pass> passes_;
    };
}
