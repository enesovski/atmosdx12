#include "assets/TextureCooker.h"
#include "assets/AssetCache.h"
#include "core/StringUtils.h"

#include <DirectXTex.h>
#include <objbase.h>

#include <cstring>
#include <format>
#include <stdexcept>
#include <thread>

namespace atmos
{
    namespace
    {
        // Bump when the cooking settings change so old cache files are ignored.
        constexpr std::string_view CookerVersion = "tex-v2";

        // WIC needs COM on whichever worker thread decodes images.
        struct ComThreadScope
        {
            ComThreadScope() : initialized(SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED))) {}
            ~ComThreadScope()
            {
                if (initialized)
                    CoUninitialize();
            }

            bool initialized;
        };

        void Check(HRESULT result, const char* what, const std::filesystem::path& source)
        {
            if (FAILED(result))
                throw std::runtime_error(std::string(what) + ": " + ToUtf8(source.wstring()));
        }

        std::string_view GetVariant(TextureKind kind)
        {
            switch (kind)
            {
                case TextureKind::Color:     return "color";
                case TextureKind::Linear:    return "linear";
                case TextureKind::NormalMap: return "normal";
            }

            return "";
        }

        // BC1/BC3/BC5 compress in milliseconds on the CPU (Unreal's default formats too).
        // BC7 looks better but needs a GPU compressor to cook in reasonable time.
        DXGI_FORMAT GetCompressedFormat(TextureKind kind, bool hasAlpha)
        {
            switch (kind)
            {
                case TextureKind::Color:     return hasAlpha ? DXGI_FORMAT_BC3_UNORM_SRGB : DXGI_FORMAT_BC1_UNORM_SRGB;
                case TextureKind::NormalMap: return DXGI_FORMAT_BC5_UNORM;
                default:                     return DXGI_FORMAT_BC1_UNORM;
            }
        }

        void Cook(const std::filesystem::path& source, TextureKind kind, const std::filesystem::path& cooked)
        {
            using namespace DirectX;

            ScratchImage decoded;
            Check(LoadFromWICFile(source.c_str(), WIC_FLAGS_IGNORE_SRGB, nullptr, decoded), "Failed to decode image",
                  source);

            // Normalize to RGBA8 and tag the color space; mips of sRGB data are filtered in linear.
            const DXGI_FORMAT baseFormat = kind == TextureKind::Color ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
                                                                       : DXGI_FORMAT_R8G8B8A8_UNORM;
            ScratchImage rgba;

            if (decoded.GetMetadata().format != DXGI_FORMAT_R8G8B8A8_UNORM)
            {
                Check(Convert(*decoded.GetImage(0, 0, 0), DXGI_FORMAT_R8G8B8A8_UNORM, TEX_FILTER_DEFAULT,
                              TEX_THRESHOLD_DEFAULT, rgba),
                      "Failed to convert image", source);
            }
            else
            {
                rgba = std::move(decoded);
            }

            rgba.OverrideFormat(baseFormat);

            ScratchImage mipChain;
            Check(GenerateMipMaps(*rgba.GetImage(0, 0, 0), TEX_FILTER_DEFAULT, 0, mipChain),
                  "Failed to generate mips", source);

            // Block compression needs a top level that is a multiple of 4 texels.
            const TexMetadata& metadata = mipChain.GetMetadata();
            const ScratchImage* result = &mipChain;
            ScratchImage compressed;

            if (metadata.width % 4 == 0 && metadata.height % 4 == 0)
            {
                const bool hasAlpha = !mipChain.IsAlphaAllOpaque();
                Check(Compress(mipChain.GetImages(), mipChain.GetImageCount(), metadata, GetCompressedFormat(kind, hasAlpha),
                               TEX_COMPRESS_DEFAULT, TEX_THRESHOLD_DEFAULT, compressed),
                      "Failed to compress image", source);
                result = &compressed;
            }

            std::filesystem::path temporary = cooked;
            temporary += std::format(L".{}.tmp", std::hash<std::thread::id>{}(std::this_thread::get_id()));

            Check(SaveToDDSFile(result->GetImages(), result->GetImageCount(), result->GetMetadata(), DDS_FLAGS_NONE,
                                temporary.c_str()),
                  "Failed to write cooked texture", cooked);
            CommitCookedFile(temporary, cooked);
        }

        TextureData LoadDds(const std::filesystem::path& cooked)
        {
            using namespace DirectX;

            TexMetadata metadata;
            ScratchImage image;
            Check(LoadFromDDSFile(cooked.c_str(), DDS_FLAGS_NONE, &metadata, image), "Failed to read cooked texture",
                  cooked);

            TextureData texture;
            texture.width = static_cast<uint32_t>(metadata.width);
            texture.height = static_cast<uint32_t>(metadata.height);
            texture.format = metadata.format;
            texture.pixels.resize(image.GetPixelsSize());
            std::memcpy(texture.pixels.data(), image.GetPixels(), image.GetPixelsSize());

            for (size_t mip = 0; mip < metadata.mipLevels; ++mip)
            {
                const Image* level = image.GetImage(mip, 0, 0);
                texture.mips.push_back({ static_cast<uint64_t>(level->pixels - image.GetPixels()),
                                         level->rowPitch, level->slicePitch });
            }

            return texture;
        }
    }

    TextureData LoadCookedTexture(const std::filesystem::path& source, TextureKind kind,
                                  const std::filesystem::path& cacheDirectory)
    {
        thread_local ComThreadScope com;

        const std::string variant = std::string(CookerVersion) + "-" + std::string(GetVariant(kind));
        const std::filesystem::path cooked = GetCookedPath(cacheDirectory, source, variant, ".dds");

        if (!IsCookedFileValid(cooked, source))
            Cook(source, kind, cooked);

        return LoadDds(cooked);
    }
}
