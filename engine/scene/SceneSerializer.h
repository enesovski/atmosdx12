#pragma once

#include "scene/Scene.h"

#include <filesystem>

namespace atmos
{
    // JSON scene files; model paths are stored relative to the file. Both throw on failure.
    Scene LoadScene(const std::filesystem::path& path);
    void SaveScene(const Scene& scene, const std::filesystem::path& path);
}
