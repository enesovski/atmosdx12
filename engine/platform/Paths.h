#pragma once

#include <filesystem>

namespace atmos
{
    // Runtime files (shaders, DLLs) are resolved from here, not the working directory.
    std::filesystem::path GetExecutableDirectory();
}
