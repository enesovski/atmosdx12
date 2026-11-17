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

    // Handle to a texture inside one frame's graph. Cheap to copy and capture.
    struct RGTexture
    {
        static constexpr uint32_t InvalidIndex = UINT32_MAX;
        uint32_t index = InvalidIndex;

        bool IsValid() const { return index != InvalidIndex; }
    };

    // What happens to a render target's previous contents when a pass binds it.
    enum class LoadOp
    {
        Load,      // Keep: the pass also depends on earlier writers.
        Clear,     // Clear to the texture's clear value.
        DontCare,  // The pass overwrites every pixel.
    };

    // Given to execute callbacks: the command context plus access to the real textures.
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

    // Per-frame render graph (Frostbite FrameGraph / Unity RenderGraph style).
    // Passes declare their reads and writes in a setup callback and record commands in an
    // execute callback. Execute() culls passes whose results are unused, creates transient
    // textures from the pool, inserts transitions, binds render targets, applies load ops and
    // runs the surviving passes in declaration order.
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

        // Logs passes (live or culled), their accesses and the transient textures.
        void LogSummary() const;

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
