#pragma once

#include "style/EditorTheme.h"

#include <array>
#include <filesystem>
#include <vector>

namespace atmos::editor
{
    // Per-user editor preferences (Saved/Editor/EditorPrefs.json). Scene data never goes here.
    struct EditorPrefs
    {
        static constexpr size_t MaxRecentScenes = 8;

        // Appearance
        float uiScale = 1.0f;  // multiplied with the monitor's DPI scale
        float fontSize = 15.0f;
        Theme theme = Theme::Dark;

        // Camera
        float lookSensitivity = 0.1f;  // degrees per raw mouse count
        float defaultMoveSpeed = 5.0f;
        bool invertY = false;

        // Viewport
        std::array<float, 4> outlineColor{ 1.0f, 0.55f, 0.1f, 1.0f };
        bool showStatsByDefault = true;

        // Files
        int autosaveMinutes = 0;  // 0 = off
        std::vector<std::filesystem::path> recentScenes;

        void AddRecentScene(const std::filesystem::path& path);

        // Missing or unreadable files leave the defaults.
        void Load(const std::filesystem::path& file);
        void Save(const std::filesystem::path& file) const;
    };
}
