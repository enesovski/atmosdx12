#include "ProjectPaths.h"

#include "platform/Paths.h"

namespace atmos::editor
{
    namespace
    {
        std::filesystem::path FindRoot()
        {
            if (std::filesystem::exists("scenes"))
                return std::filesystem::current_path();

            for (std::filesystem::path directory = GetExecutableDirectory(); directory.has_relative_path();
                 directory = directory.parent_path())
            {
                if (std::filesystem::exists(directory / "scenes"))
                    return directory;
            }

            // No project around: keep editor state next to the executable.
            return GetExecutableDirectory();
        }
    }

    ProjectPaths ProjectPaths::Find()
    {
        ProjectPaths paths;
        paths.root = FindRoot();
        paths.scenes = paths.root / "scenes";
        paths.assets = paths.root / "assets";
        paths.saved = paths.root / "Saved" / "Editor";
        paths.layoutFile = paths.saved / "Layout.ini";
        paths.prefsFile = paths.saved / "EditorPrefs.json";

        std::error_code error;
        std::filesystem::create_directories(paths.saved, error);
        return paths;
    }
}
