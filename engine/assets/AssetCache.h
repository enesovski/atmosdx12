#pragma once

#include <filesystem>
#include <string_view>

namespace atmos
{
    // Path of the cooked file for `source` in `cacheDirectory`. The name hashes the source path
    // and `variant` (cooker version and settings), so changing either cooks a new file.
    std::filesystem::path GetCookedPath(const std::filesystem::path& cacheDirectory,
                                        const std::filesystem::path& source, std::string_view variant,
                                        std::string_view extension);

    // A cooked file is valid if it exists and is not older than its source.
    bool IsCookedFileValid(const std::filesystem::path& cooked, const std::filesystem::path& source);

    // Moves a finished temporary file into place so readers never see a partial file.
    void CommitCookedFile(const std::filesystem::path& temporary, const std::filesystem::path& cooked);
}
