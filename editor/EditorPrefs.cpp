#include "EditorPrefs.h"

#include "core/Log.h"
#include "core/StringUtils.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>

using nlohmann::json;

namespace atmos::editor
{
    void EditorPrefs::AddRecentScene(const std::filesystem::path& path)
    {
        const std::filesystem::path absolute = std::filesystem::absolute(path).lexically_normal();
        std::erase(recentScenes, absolute);
        recentScenes.insert(recentScenes.begin(), absolute);

        if (recentScenes.size() > MaxRecentScenes)
            recentScenes.resize(MaxRecentScenes);
    }

    void EditorPrefs::Load(const std::filesystem::path& file)
    {
        std::ifstream stream(file);

        if (!stream)
            return;

        try
        {
            const json root = json::parse(stream);
            uiScale = std::clamp(root.value("uiScale", uiScale), 0.75f, 2.0f);
            fontSize = std::clamp(root.value("fontSize", fontSize), 13.0f, 18.0f);
            theme = root.value("theme", std::string("dark")) == "darker" ? Theme::Darker : Theme::Dark;
            lookSensitivity = root.value("lookSensitivity", lookSensitivity);
            defaultMoveSpeed = root.value("defaultMoveSpeed", defaultMoveSpeed);
            invertY = root.value("invertY", invertY);
            outlineColor = root.value("outlineColor", outlineColor);
            showStatsByDefault = root.value("showStatsByDefault", showStatsByDefault);
            autosaveMinutes = root.value("autosaveMinutes", autosaveMinutes);

            recentScenes.clear();

            for (const json& entry : root.value("recentScenes", json::array()))
                recentScenes.push_back(ToWide(entry.get<std::string>()));
        }
        catch (const std::exception& e)
        {
            log::Warning("Ignoring editor preferences ({})", e.what());
        }
    }

    void EditorPrefs::Save(const std::filesystem::path& file) const
    {
        json recent = json::array();

        for (const std::filesystem::path& path : recentScenes)
            recent.push_back(ToUtf8(path.wstring()));

        const json root = {
            { "uiScale", uiScale },
            { "fontSize", fontSize },
            { "theme", theme == Theme::Darker ? "darker" : "dark" },
            { "lookSensitivity", lookSensitivity },
            { "defaultMoveSpeed", defaultMoveSpeed },
            { "invertY", invertY },
            { "outlineColor", outlineColor },
            { "showStatsByDefault", showStatsByDefault },
            { "autosaveMinutes", autosaveMinutes },
            { "recentScenes", recent },
        };

        std::error_code error;
        std::filesystem::create_directories(file.parent_path(), error);
        std::ofstream stream(file);

        if (stream)
            stream << root.dump(2) << '\n';
    }
}
