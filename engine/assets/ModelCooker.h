#pragma once

#include "assets/ModelData.h"

#include <filesystem>

namespace atmos
{
    // Returns a model's data, importing the source (glTF) only when its cooked binary in
    // `cacheDirectory` is missing or older than the source. The cooked format is a flat dump of
    // ModelData, so loading it is a few large reads instead of JSON parsing and conversion.
    // Thread-safe: may run on any worker thread. Throws on failure.
    ModelData LoadCookedModel(const std::filesystem::path& source, const std::filesystem::path& cacheDirectory);
}
