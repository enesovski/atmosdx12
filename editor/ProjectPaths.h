#pragma once

#include <filesystem>

namespace atmos::editor
{
    // Where the editor finds content and keeps per-user state. The project root is the directory
    // that contains `scenes/` (the repository), found from the working directory or above the exe.
    struct ProjectPaths
    {
        std::filesystem::path root;
        std::filesystem::path scenes;     // root/scenes
        std::filesystem::path assets;     // root/assets
        std::filesystem::path saved;      // root/Saved/Editor (git-ignored, like Unreal's Saved/)
        std::filesystem::path layoutFile; // saved/Layout.ini
        std::filesystem::path prefsFile;  // saved/EditorPrefs.json

        static ProjectPaths Find();
    };
}
