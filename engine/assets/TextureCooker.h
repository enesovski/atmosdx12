#pragma once

#include "assets/TextureData.h"

#include <filesystem>

namespace atmos
{
    // How a source image is interpreted; decides the color space and compression.
    enum class TextureKind
    {
        Color,      // sRGB, BC1 (BC3 with alpha)
        Linear,     // data channels (roughness, metallic, occlusion), BC1
        NormalMap,  // tangent-space XY, BC5 (Z rebuilt in the shader)
    };

    // Returns the GPU-ready (mipmapped, block-compressed) texture for an image file.
    // The first call cooks it into `cacheDirectory` as DDS; later calls load that file as long
    // as it is newer than the source. Thread-safe: may run on any worker thread. Throws on failure.
    TextureData LoadCookedTexture(const std::filesystem::path& source, TextureKind kind,
                                  const std::filesystem::path& cacheDirectory);
}
