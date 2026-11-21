#include "graphics/rendergraph/RenderGraph.h"
#include "graphics/rendergraph/TransientResourcePool.h"
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

    Buffer& RGContext::GetBuffer(RGBuffer handle) const
    {
        Buffer* buffer = graph_.resources_[handle.index].buffer;
        ATMOS_ASSERT(buffer, "Buffer was not declared by this pass");
        return *buffer;
    }

    RGTexture RGBuilder::ReadTexture(RGTexture texture)
    {
        graph_.AddAccess(passIndex_, texture.index, RenderGraph::Access::ShaderRead, LoadOp::Load, All);
        return texture;
    }

    RGTexture RGBuilder::WriteColor(RGTexture texture, LoadOp loadOp)
    {
        graph_.AddAccess(passIndex_, texture.index, RenderGraph::Access::ColorWrite, loadOp, All);
        return texture;
    }

    RGTexture RGBuilder::WriteDepth(RGTexture texture, LoadOp loadOp, uint32_t slice)
    {
        graph_.AddAccess(passIndex_, texture.index, RenderGraph::Access::DepthWrite, loadOp, slice);
        return texture;
    }

    RGTexture RGBuilder::ReadDepth(RGTexture texture)
    {
        graph_.AddAccess(passIndex_, texture.index, RenderGraph::Access::DepthRead, LoadOp::Load, All);
        return texture;
    }

    RGTexture RGBuilder::ReadWriteTexture(RGTexture texture, uint32_t mip)
    {
        graph_.AddAccess(passIndex_, texture.index, RenderGraph::Access::ReadWrite, LoadOp::Load, mip);
        return texture;
    }

    RGBuffer RGBuilder::ReadBuffer(RGBuffer buffer)
    {
        graph_.AddAccess(passIndex_, buffer.index, RenderGraph::Access::ShaderRead, LoadOp::Load, All);
        return buffer;
    }

    RGBuffer RGBuilder::ReadWriteBuffer(RGBuffer buffer)
    {
        graph_.AddAccess(passIndex_, buffer.index, RenderGraph::Access::ReadWrite, LoadOp::Load, All);
        return buffer;
    }

    void RGBuilder::SetSideEffects()
    {
        graph_.passes_[passIndex_].hasSideEffects = true;
    }

    // --- RenderGraph -------------------------------------------------------------------

    RenderGraph::RenderGraph(TransientResourcePool& pool)
        : pool_(pool)
    {
    }

    RGTexture RenderGraph::CreateTexture(const TextureDesc& desc)
    {
        resources_.push_back({ .textureDesc = desc });
        return { static_cast<uint32_t>(resources_.size() - 1) };
    }

    RGBuffer RenderGraph::CreateBuffer(const BufferDesc& desc)
    {
        resources_.push_back({ .isBuffer = true, .bufferDesc = desc });
        return { static_cast<uint32_t>(resources_.size() - 1) };
    }

    RGTexture RenderGraph::ImportTexture(Texture& texture, D3D12_RESOURCE_STATES finalState)
    {
        resources_.push_back({ .textureDesc = texture.GetDesc(), .texture = &texture, .imported = true,
                               .finalState = finalState });
        return { static_cast<uint32_t>(resources_.size() - 1) };
    }

    RGBuffer RenderGraph::ImportBuffer(Buffer& buffer)
    {
        resources_.push_back({ .isBuffer = true, .bufferDesc = buffer.GetDesc(), .buffer = &buffer, .imported = true });
        return { static_cast<uint32_t>(resources_.size() - 1) };
    }

    void RenderGraph::AddAccess(size_t passIndex, uint32_t resource, Access access, LoadOp loadOp, uint32_t subresource)
    {
        ATMOS_ASSERT(resource < resources_.size(), "Resource handle is not from this graph");
        passes_[passIndex].accesses.push_back({ resource, access, loadOp, subresource });
    }

    bool RenderGraph::Writes(const PassAccess& access)
    {
        return access.access == Access::ColorWrite || access.access == Access::DepthWrite ||
               access.access == Access::ReadWrite;
    }

    bool RenderGraph::OverwritesWholeResource(const PassAccess& access)
    {
        return (access.access == Access::ColorWrite || access.access == Access::DepthWrite) &&
               access.loadOp != LoadOp::Load && access.subresource == RGBuilder::All;
    }

    bool RenderGraph::ReadsPreviousContents(const PassAccess& access)
    {
        return !Writes(access) || access.access == Access::ReadWrite || !OverwritesWholeResource(access);
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
            if (resource.imported && !resource.isBuffer)
                commands.Transition(*resource.texture, resource.finalState);
        }
    }

    // Walks passes backwards. A resource is "needed" if a later live pass reads it (or it is
    // imported). A pass lives if it has side effects or writes something needed. A whole-resource
    // overwrite ends the need for earlier writers.
    void RenderGraph::CullPasses()
    {
        std::vector<bool> needed(resources_.size(), false);

        for (size_t i = 0; i < resources_.size(); ++i)
            needed[i] = resources_[i].imported;

        for (auto pass = passes_.rbegin(); pass != passes_.rend(); ++pass)
        {
            const bool writesNeeded = std::ranges::any_of(pass->accesses, [&](const PassAccess& access) {
                return Writes(access) && needed[access.resource];
            });

            pass->culled = !pass->hasSideEffects && !writesNeeded;

            if (pass->culled)
                continue;

            for (const PassAccess& access : pass->accesses)
            {
                if (OverwritesWholeResource(access))
                    needed[access.resource] = false;
            }

            for (const PassAccess& access : pass->accesses)
            {
                if (ReadsPreviousContents(access))
                    needed[access.resource] = true;
            }
        }
    }

    void RenderGraph::DeriveTransientUsage()
    {
        for (Resource& resource : resources_)
        {
            if (resource.imported)
                continue;

            resource.textureDesc.usage = TextureUsage::None;
            resource.bufferDesc.usage = BufferUsage::None;
        }

        for (const Pass& pass : passes_)
        {
            if (pass.culled)
                continue;

            for (const PassAccess& access : pass.accesses)
            {
                Resource& resource = resources_[access.resource];

                if (resource.imported)
                    continue;

                if (resource.isBuffer)
                {
                    const BufferUsage usage = access.access == Access::ReadWrite ? BufferUsage::UnorderedAccess
                                                                                 : BufferUsage::ShaderResource;
                    resource.bufferDesc.usage = resource.bufferDesc.usage | usage;
                    continue;
                }

                TextureUsage& usage = resource.textureDesc.usage;

                switch (access.access)
                {
                    case Access::ShaderRead: usage = usage | TextureUsage::ShaderResource; break;
                    case Access::ColorWrite: usage = usage | TextureUsage::RenderTarget; break;
                    case Access::DepthWrite:
                    case Access::DepthRead:  usage = usage | TextureUsage::DepthStencil; break;
                    case Access::ReadWrite:  usage = usage | TextureUsage::UnorderedAccess; break;
                }
            }
        }
    }

    RenderGraph::Resource& RenderGraph::Resolve(uint32_t index)
    {
        Resource& resource = resources_[index];

        if (resource.isBuffer && !resource.buffer)
            resource.buffer = &pool_.AcquireBuffer(resource.bufferDesc);
        else if (!resource.isBuffer && !resource.texture)
            resource.texture = &pool_.AcquireTexture(resource.textureDesc);

        return resource;
    }

    void RenderGraph::ExecutePass(Pass& pass, CommandContext& commands)
    {
        struct RequiredState
        {
            uint32_t resource;
            D3D12_RESOURCE_STATES state;
            bool uav;
        };

        std::vector<RequiredState> requiredStates;
        std::vector<Texture*> colorTargets;
        Texture* depthTarget = nullptr;
        uint32_t depthSlice = 0;
        bool readOnlyDepth = true;

        for (const PassAccess& access : pass.accesses)
        {
            Resource& resource = Resolve(access.resource);
            D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;

            switch (access.access)
            {
                case Access::ShaderRead: state = ShaderReadState; break;
                case Access::ReadWrite:  state = D3D12_RESOURCE_STATE_UNORDERED_ACCESS; break;
                case Access::ColorWrite:
                    state = D3D12_RESOURCE_STATE_RENDER_TARGET;
                    colorTargets.push_back(resource.texture);
                    break;
                case Access::DepthWrite:
                    state = D3D12_RESOURCE_STATE_DEPTH_WRITE;
                    depthTarget = resource.texture;
                    depthSlice = access.subresource == RGBuilder::All ? 0 : access.subresource;
                    readOnlyDepth = false;
                    break;
                case Access::DepthRead:
                    state = D3D12_RESOURCE_STATE_DEPTH_READ;
                    depthTarget = resource.texture;
                    break;
            }

            auto existing = std::ranges::find(requiredStates, access.resource, &RequiredState::resource);

            if (existing != requiredStates.end())
            {
                existing->state |= state;
                existing->uav |= access.access == Access::ReadWrite;
            }
            else
            {
                requiredStates.push_back({ access.resource, state, access.access == Access::ReadWrite });
            }
        }

        for (const RequiredState& required : requiredStates)
        {
            Resource& resource = resources_[required.resource];

            if (resource.isBuffer)
            {
                // Buffers decay to COMMON between frames; the first use this frame starts from there.
                if (!resource.usedThisFrame)
                    commands.MarkDecayedToCommon(*resource.buffer);

                if (required.uav && resource.lastAccessWasUav)
                    commands.UavBarrier(resource.buffer->GetResource());

                commands.Transition(*resource.buffer, required.state);
            }
            else
            {
                if (required.uav && resource.lastAccessWasUav)
                    commands.UavBarrier(resource.texture->GetResource());

                commands.Transition(*resource.texture, required.state);
            }

            resource.lastAccessWasUav = required.uav;
            resource.usedThisFrame = true;
        }

        const ScopedGpuEvent event(commands, pass.name.c_str());

        if (!colorTargets.empty() || depthTarget)
            commands.SetRenderTargets(colorTargets, depthTarget, readOnlyDepth, depthSlice);

        for (const PassAccess& access : pass.accesses)
        {
            if (access.loadOp != LoadOp::Clear)
                continue;

            Texture& texture = *resources_[access.resource].texture;

            if (access.access == Access::ColorWrite)
                commands.ClearRenderTarget(texture);
            else if (access.access == Access::DepthWrite)
                commands.ClearDepth(texture, access.subresource == RGBuilder::All ? 0 : access.subresource);
        }

        RGContext context(commands, *this);
        pass.execute(context);
    }

    const std::wstring& RenderGraph::GetName(const Resource& resource) const
    {
        return resource.isBuffer ? resource.bufferDesc.debugName : resource.textureDesc.debugName;
    }

    std::vector<RenderPassInfo> RenderGraph::GetPassInfos() const
    {
        std::vector<RenderPassInfo> infos;
        infos.reserve(passes_.size());

        for (const Pass& pass : passes_)
            infos.push_back({ pass.name, pass.culled });

        return infos;
    }

    void RenderGraph::LogSummary() const
    {
        static constexpr const char* AccessNames[] = { "read", "color", "depth-write", "depth-read", "uav" };
        static constexpr const char* LoadOpNames[] = { "load", "clear", "dont-care" };

        log::Info("Render graph: {} passes, {} resources", passes_.size(), resources_.size());

        for (const Pass& pass : passes_)
        {
            std::string accesses;

            for (const PassAccess& access : pass.accesses)
            {
                accesses += std::format(" {}({}", ToUtf8(GetName(resources_[access.resource])),
                                        AccessNames[static_cast<int>(access.access)]);

                if (access.access == Access::ColorWrite || access.access == Access::DepthWrite)
                    accesses += std::format(", {}", LoadOpNames[static_cast<int>(access.loadOp)]);

                if (access.subresource != RGBuilder::All)
                    accesses += std::format(", sub {}", access.subresource);

                accesses += ")";
            }

            log::Info("  [{}] {}:{}", pass.culled ? "culled" : "live", ToUtf8(pass.name), accesses);
        }

        for (const Resource& resource : resources_)
        {
            if (resource.isBuffer)
            {
                log::Info("  buffer {} {} B {}", ToUtf8(GetName(resource)), resource.bufferDesc.size,
                          resource.imported ? "imported" : "transient");
            }
            else
            {
                log::Info("  texture {} {}x{}x{} {}", ToUtf8(GetName(resource)), resource.textureDesc.width,
                          resource.textureDesc.height, resource.textureDesc.arraySize,
                          resource.imported ? "imported" : "transient");
            }
        }
    }
}
