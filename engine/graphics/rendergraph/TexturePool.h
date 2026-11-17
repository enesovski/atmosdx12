#pragma once

#include "graphics/d3d12/Texture.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace atmos
{
    class GraphicsDevice;

    // Reuses textures across frames for the render graph's transient resources.
    // A texture is handed out at most once per frame; ones unused for a few frames are
    // destroyed (through the deferred release queue), which is how resize cleans up.
    class TexturePool
    {
    public:
        explicit TexturePool(GraphicsDevice& device);

        TexturePool(const TexturePool&) = delete;
        TexturePool& operator=(const TexturePool&) = delete;

        Texture& Acquire(const TextureDesc& desc);
        void EndFrame();

        size_t GetTextureCount() const { return entries_.size(); }

    private:
        struct Entry
        {
            std::unique_ptr<Texture> texture;
            uint64_t lastUsedFrame = 0;
            bool inUse = false;
        };

        GraphicsDevice& device_;
        std::vector<Entry> entries_;
        uint64_t frame_ = 0;
    };
}
