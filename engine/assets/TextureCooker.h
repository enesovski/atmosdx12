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

    // Reads the cooked (mipmapped, block-compressed) DDS, cooking it first when missing or stale.
    // Thread-safe. Throws on failure.
    TextureData LoadCookedTexture(const std::filesystem::path& source, TextureKind kind,
                                  const std::filesystem::path& cacheDirectory);
}
