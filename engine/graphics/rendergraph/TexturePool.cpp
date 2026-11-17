#include "graphics/rendergraph/TexturePool.h"

#include <algorithm>

namespace atmos
{
    namespace
    {
        constexpr uint64_t FramesBeforeEviction = 3;

        // Debug names are ignored: any texture with the same shape can be reused.
        bool IsCompatible(const TextureDesc& a, const TextureDesc& b)
        {
            return a.width == b.width && a.height == b.height && a.format == b.format &&
                   a.usage == b.usage && a.clearColor == b.clearColor && a.clearDepth == b.clearDepth;
        }
    }

    TexturePool::TexturePool(GraphicsDevice& device)
        : device_(device)
    {
    }

    Texture& TexturePool::Acquire(const TextureDesc& desc)
    {
        for (Entry& entry : entries_)
        {
            if (!entry.inUse && IsCompatible(entry.texture->GetDesc(), desc))
            {
                entry.inUse = true;
                entry.lastUsedFrame = frame_;
                return *entry.texture;
            }
        }

        Entry& entry = entries_.emplace_back();
        entry.texture = std::make_unique<Texture>(device_, desc);
        entry.inUse = true;
        entry.lastUsedFrame = frame_;
        return *entry.texture;
    }

    void TexturePool::EndFrame()
    {
        ++frame_;

        std::erase_if(entries_, [this](const Entry& entry) {
            return frame_ - entry.lastUsedFrame > FramesBeforeEviction;
        });

        for (Entry& entry : entries_)
            entry.inUse = false;
    }
}
