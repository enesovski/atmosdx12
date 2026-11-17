#include "graphics/rendergraph/RenderGraph.h"
#include "graphics/rendergraph/TexturePool.h"
#include "graphics/d3d12/CommandContext.h"
#include "core/Assert.h"
#include "core/Log.h"
#include "core/StringUtils.h"

#include <algorithm>
#include <string>

namespace atmos
{
    namespace
    {
        constexpr D3D12_RESOURCE_STATES ShaderReadState =
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    }

    // --- RGContext / RGBuilder ---------------------------------------------------------

    Texture& RGContext::GetTexture(RGTexture handle) const
    {
        Texture* texture = graph_.resources_[handle.index].texture;
        ATMOS_ASSERT(texture, "Texture was not declared by this pass");
        return *texture;
    }

    RGTexture RGBuilder::ReadTexture(RGTexture texture)
    {
        graph_.AddAccess(passIndex_, texture, RenderGraph::Access::ShaderRead, LoadOp::Load);
        return texture;
    }

    RGTexture RGBuilder::WriteColor(RGTexture texture, LoadOp loadOp)
    {
        graph_.AddAccess(passIndex_, texture, RenderGraph::Access::ColorWrite, loadOp);
        return texture;
    }

    RGTexture RGBuilder::WriteDepth(RGTexture texture, LoadOp loadOp)
    {
        graph_.AddAccess(passIndex_, texture, RenderGraph::Access::DepthWrite, loadOp);
        return texture;
    }

    RGTexture RGBuilder::ReadDepth(RGTexture texture)
    {
        graph_.AddAccess(passIndex_, texture, RenderGraph::Access::DepthRead, LoadOp::Load);
        return texture;
    }

    RGTexture RGBuilder::ReadWriteTexture(RGTexture texture)
    {
        graph_.AddAccess(passIndex_, texture, RenderGraph::Access::ReadWrite, LoadOp::Load);
        return texture;
    }

    void RGBuilder::SetSideEffects()
    {
        graph_.passes_[passIndex_].hasSideEffects = true;
    }

    // --- RenderGraph -------------------------------------------------------------------

    RenderGraph::RenderGraph(TexturePool& texturePool)
        : texturePool_(texturePool)
    {
    }

    RGTexture RenderGraph::CreateTexture(const TextureDesc& desc)
    {
        resources_.push_back({ .desc = desc });
        return { static_cast<uint32_t>(resources_.size() - 1) };
    }

    RGTexture RenderGraph::ImportTexture(Texture& texture, D3D12_RESOURCE_STATES finalState)
    {
        resources_.push_back({ .desc = texture.GetDesc(), .texture = &texture, .imported = true,
                               .finalState = finalState });
        return { static_cast<uint32_t>(resources_.size() - 1) };
    }

    void RenderGraph::AddAccess(size_t passIndex, RGTexture texture, Access access, LoadOp loadOp)
    {
        ATMOS_ASSERT(texture.index < resources_.size(), "Texture handle is not from this graph");
        passes_[passIndex].accesses.push_back({ texture, access, loadOp });
    }

    bool RenderGraph::Writes(const PassAccess& access)
    {
        return access.access == Access::ColorWrite || access.access == Access::DepthWrite ||
               access.access == Access::ReadWrite;
    }

    bool RenderGraph::ReadsPreviousContents(const PassAccess& access)
    {
        switch (access.access)
        {
            case Access::ColorWrite:
            case Access::DepthWrite:
                return access.loadOp == LoadOp::Load;
            default:
                return true;
        }
    }

    void RenderGraph::Execute(CommandContext& commands)
    {
        CullPasses();
        DeriveTransientUsage();

        for (Pass& pass : passes_)
        {
            if (!pass.culled)
                ExecutePass(pass, commands);
        }

        for (Resource& resource : resources_)
        {
            if (resource.imported)
                commands.Transition(*resource.texture, resource.finalState);
        }
    }

    // Walks passes backwards. A resource is "needed" if a later live pass reads it (or it is
    // imported). A pass lives if it has side effects or writes something needed. A full
    // overwrite (Clear/DontCare) ends the need for earlier writers of that resource.
    void RenderGraph::CullPasses()
    {
        std::vector<bool> needed(resources_.size(), false);

        for (size_t i = 0; i < resources_.size(); ++i)
            needed[i] = resources_[i].imported;

        for (auto pass = passes_.rbegin(); pass != passes_.rend(); ++pass)
        {
            const bool writesNeeded = std::ranges::any_of(pass->accesses, [&](const PassAccess& access) {
                return Writes(access) && needed[access.texture.index];
            });

            pass->culled = !pass->hasSideEffects && !writesNeeded;

            if (pass->culled)
                continue;

            for (const PassAccess& access : pass->accesses)
            {
                if (Writes(access) && !ReadsPreviousContents(access))
                    needed[access.texture.index] = false;
            }

            for (const PassAccess& access : pass->accesses)
            {
                if (ReadsPreviousContents(access))
                    needed[access.texture.index] = true;
            }
        }
    }

    void RenderGraph::DeriveTransientUsage()
    {
        for (Resource& resource : resources_)
        {
            if (!resource.imported)
                resource.desc.usage = TextureUsage::None;
        }

        for (const Pass& pass : passes_)
        {
            if (pass.culled)
                continue;

            for (const PassAccess& access : pass.accesses)
            {
                Resource& resource = resources_[access.texture.index];

                if (resource.imported)
                    continue;

                switch (access.access)
                {
                    case Access::ShaderRead: resource.desc.usage = resource.desc.usage | TextureUsage::ShaderResource; break;
                    case Access::ColorWrite: resource.desc.usage = resource.desc.usage | TextureUsage::RenderTarget; break;
                    case Access::DepthWrite:
                    case Access::DepthRead:  resource.desc.usage = resource.desc.usage | TextureUsage::DepthStencil; break;
                    case Access::ReadWrite:  resource.desc.usage = resource.desc.usage | TextureUsage::UnorderedAccess; break;
                }
            }
        }
    }

    Texture& RenderGraph::ResolveTexture(RGTexture handle)
    {
        Resource& resource = resources_[handle.index];

        if (!resource.texture)
            resource.texture = &texturePool_.Acquire(resource.desc);

        return *resource.texture;
    }

    void RenderGraph::ExecutePass(Pass& pass, CommandContext& commands)
    {
        // 1. Required state per texture; several reads of one texture combine.
        struct RequiredState
        {
            Texture* texture;
            D3D12_RESOURCE_STATES state;
        };

        std::vector<RequiredState> requiredStates;
        std::vector<Texture*> colorTargets;
        Texture* depthTarget = nullptr;
        bool readOnlyDepth = true;

        for (const PassAccess& access : pass.accesses)
        {
            Texture& texture = ResolveTexture(access.texture);
            D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;

            switch (access.access)
            {
                case Access::ShaderRead: state = ShaderReadState; break;
                case Access::ColorWrite: state = D3D12_RESOURCE_STATE_RENDER_TARGET; colorTargets.push_back(&texture); break;
                case Access::DepthWrite: state = D3D12_RESOURCE_STATE_DEPTH_WRITE; depthTarget = &texture; readOnlyDepth = false; break;
                case Access::DepthRead:  state = D3D12_RESOURCE_STATE_DEPTH_READ; depthTarget = &texture; break;
                case Access::ReadWrite:  state = D3D12_RESOURCE_STATE_UNORDERED_ACCESS; break;
            }

            auto existing = std::ranges::find(requiredStates, &texture, &RequiredState::texture);

            if (existing != requiredStates.end())
                existing->state |= state;
            else
                requiredStates.push_back({ &texture, state });
        }

        // 2. Transitions, batched by the command context.
        for (const RequiredState& required : requiredStates)
            commands.Transition(*required.texture, required.state);

        const ScopedGpuEvent event(commands, pass.name.c_str());

        // 3. Render targets and load ops.
        if (!colorTargets.empty() || depthTarget)
            commands.SetRenderTargets(colorTargets, depthTarget, readOnlyDepth);

        for (const PassAccess& access : pass.accesses)
        {
            if (access.loadOp != LoadOp::Clear)
                continue;

            Texture& texture = ResolveTexture(access.texture);

            if (access.access == Access::ColorWrite)
                commands.ClearRenderTarget(texture);
            else if (access.access == Access::DepthWrite)
                commands.ClearDepth(texture);
        }

        // 4. The pass's own commands.
        RGContext context(commands, *this);
        pass.execute(context);
    }

    void RenderGraph::LogSummary() const
    {
        static constexpr const char* AccessNames[] = { "read", "color", "depth-write", "depth-read", "uav" };
        static constexpr const char* LoadOpNames[] = { "load", "clear", "dont-care" };

        log::Info("Render graph: {} passes, {} textures", passes_.size(), resources_.size());

        for (const Pass& pass : passes_)
        {
            std::string accesses;

            for (const PassAccess& access : pass.accesses)
            {
                const Resource& resource = resources_[access.texture.index];
                accesses += std::format(" {}({}", ToUtf8(resource.desc.debugName),
                                        AccessNames[static_cast<int>(access.access)]);

                if (access.access == Access::ColorWrite || access.access == Access::DepthWrite)
                    accesses += std::format(", {}", LoadOpNames[static_cast<int>(access.loadOp)]);

                accesses += ")";
            }

            log::Info("  [{}] {}:{}", pass.culled ? "culled" : "live", ToUtf8(pass.name), accesses);
        }

        for (const Resource& resource : resources_)
        {
            log::Info("  texture {} {}x{} {}", ToUtf8(resource.desc.debugName), resource.desc.width,
                      resource.desc.height, resource.imported ? "imported" : "transient");
        }
    }
}
