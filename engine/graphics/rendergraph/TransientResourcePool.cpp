#include "graphics/rendergraph/TransientResourcePool.h"

#include <algorithm>

namespace atmos
{
    namespace
    {
        constexpr uint64_t FramesBeforeEviction = 3;

        // Debug names are ignored: any resource with the same shape can be reused.
        bool IsCompatible(const TextureDesc& a, const TextureDesc& b)
        {
            return a.width == b.width && a.height == b.height && a.mipLevels == b.mipLevels &&
                   a.arraySize == b.arraySize && a.cube == b.cube && a.format == b.format && a.usage == b.usage &&
                   a.clearColor == b.clearColor && a.clearDepth == b.clearDepth;
        }

        bool IsCompatible(const BufferDesc& a, const BufferDesc& b)
        {
            return a.size == b.size && a.stride == b.stride && a.usage == b.usage && a.memory == b.memory;
        }

        template <typename Resource, typename Desc, typename Entries>
        Resource& Acquire(Entries& entries, GraphicsDevice& device, const Desc& desc, uint64_t frame)
        {
            auto found = std::ranges::find_if(entries, [&](const auto& entry) {
                return !entry.inUse && IsCompatible(entry.resource->GetDesc(), desc);
            });

            if (found == entries.end())
            {
                entries.push_back({ std::make_unique<Resource>(device, desc) });
                found = entries.end() - 1;
            }

            found->inUse = true;
            found->lastUsedFrame = frame;
            return *found->resource;
        }

        template <typename Entries>
        void Recycle(Entries& entries, uint64_t frame)
        {
            std::erase_if(entries, [frame](const auto& entry) { return frame - entry.lastUsedFrame > FramesBeforeEviction; });

            for (auto& entry : entries)
                entry.inUse = false;
        }
    }

    TransientResourcePool::TransientResourcePool(GraphicsDevice& device)
        : device_(device)
    {
    }

    Texture& TransientResourcePool::AcquireTexture(const TextureDesc& desc)
    {
        return Acquire<Texture>(textures_, device_, desc, frame_);
    }

    Buffer& TransientResourcePool::AcquireBuffer(const BufferDesc& desc)
    {
        return Acquire<Buffer>(buffers_, device_, desc, frame_);
    }

    void TransientResourcePool::EndFrame()
    {
        ++frame_;
        Recycle(textures_, frame_);
        Recycle(buffers_, frame_);
    }
}
