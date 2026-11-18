#pragma once

#include "scene/Scene.h"

#include <filesystem>

namespace atmos
{
    // JSON scene files. Model paths are stored relative to the scene file and become absolute
    // when loaded, so a scene folder can move together with its assets. Both throw on failure.
    Scene LoadScene(const std::filesystem::path& path);
    void SaveScene(const Scene& scene, const std::filesystem::path& path);
}
