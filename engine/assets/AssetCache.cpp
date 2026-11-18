#include "assets/AssetCache.h"

#include <cstdint>
#include <format>
#include <system_error>

namespace atmos
{
    namespace
    {
        // FNV-1a: stable across runs and builds, unlike std::hash.
        uint64_t HashBytes(const void* data, size_t size, uint64_t hash = 14695981039346656037ull)
        {
            const auto* bytes = static_cast<const unsigned char*>(data);

            for (size_t i = 0; i < size; ++i)
            {
                hash ^= bytes[i];
                hash *= 1099511628211ull;
            }

            return hash;
        }
    }

    std::filesystem::path GetCookedPath(const std::filesystem::path& cacheDirectory,
                                        const std::filesystem::path& source, std::string_view variant,
                                        std::string_view extension)
    {
        const std::wstring key = std::filesystem::absolute(source).lexically_normal().wstring();
        uint64_t hash = HashBytes(key.data(), key.size() * sizeof(wchar_t));
        hash = HashBytes(variant.data(), variant.size(), hash);

        return cacheDirectory / std::format("{}_{:016x}{}", source.stem().string(), hash, extension);
    }

    bool IsCookedFileValid(const std::filesystem::path& cooked, const std::filesystem::path& source)
    {
        std::error_code error;
        const auto cookedTime = std::filesystem::last_write_time(cooked, error);

        if (error)
            return false;

        const auto sourceTime = std::filesystem::last_write_time(source, error);
        return !error && cookedTime >= sourceTime;
    }

    void CommitCookedFile(const std::filesystem::path& temporary, const std::filesystem::path& cooked)
    {
        std::filesystem::rename(temporary, cooked);
    }
}
