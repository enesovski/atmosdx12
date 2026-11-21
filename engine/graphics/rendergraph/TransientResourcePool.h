#pragma once

#include "graphics/d3d12/Buffer.h"
#include "graphics/d3d12/Texture.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace atmos
{
    class GraphicsDevice;

    // Reuses the render graph's transient textures and buffers across frames. A resource is handed
    // out at most once per frame; ones unused for a few frames are released (e.g. after a resize).
    class TransientResourcePool
    {
    public:
        explicit TransientResourcePool(GraphicsDevice& device);

        TransientResourcePool(const TransientResourcePool&) = delete;
        TransientResourcePool& operator=(const TransientResourcePool&) = delete;

        Texture& AcquireTexture(const TextureDesc& desc);
        Buffer& AcquireBuffer(const BufferDesc& desc);
        void EndFrame();

        size_t GetTextureCount() const { return textures_.size(); }
        size_t GetBufferCount() const { return buffers_.size(); }

    private:
        template <typename T>
        struct Entry
        {
            std::unique_ptr<T> resource;
            uint64_t lastUsedFrame = 0;
            bool inUse = false;
        };

        GraphicsDevice& device_;
        std::vector<Entry<Texture>> textures_;
        std::vector<Entry<Buffer>> buffers_;
        uint64_t frame_ = 0;
    };
}
