#pragma once

#include <dxgiformat.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace atmos
{
    // A CPU texture ready for upload: all mips of one 2D image in one allocation.
    struct TextureData
    {
        struct Mip
        {
            uint64_t offset = 0;      // into `pixels`
            uint64_t rowPitch = 0;    // bytes per row (per block row for BC formats)
            uint64_t slicePitch = 0;  // bytes for the whole mip
        };

        uint32_t width = 0;
        uint32_t height = 0;
        DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
        std::vector<Mip> mips;
        std::vector<std::byte> pixels;
    };
}
