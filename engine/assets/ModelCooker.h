#pragma once

#include "assets/ModelData.h"

#include <filesystem>

namespace atmos
{
    // Reads the cooked .amodel, re-importing the glTF when it is missing or stale.
    // Thread-safe. Throws on failure.
    ModelData LoadCookedModel(const std::filesystem::path& source, const std::filesystem::path& cacheDirectory);
}
