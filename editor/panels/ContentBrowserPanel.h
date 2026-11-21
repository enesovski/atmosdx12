#pragma once

#include "panels/Panel.h"

#include <filesystem>
#include <map>
#include <vector>

namespace atmos::editor
{
    // Project files: a folder tree (scenes/, assets/) and the selected folder's contents as tiles.
    // Double-click opens a scene; models drag into the viewport or hierarchy.
    class ContentBrowserPanel final : public Panel
    {
    public:
        ContentBrowserPanel();

    protected:
        void DrawContent(EditorContext& context) override;

    private:
        enum class ItemKind
        {
            Folder,
            Scene,
            Model,
            Image,
            Other,
        };

        struct Item
        {
            std::filesystem::path path;
            std::string name;
            ItemKind kind;
        };

        const std::vector<Item>& GetItems(const std::filesystem::path& folder);
        void DrawFolderTree(EditorContext& context, const std::filesystem::path& folder, const char* label);
        void DrawBreadcrumbs(EditorContext& context);
        void DrawItems(EditorContext& context);
        void Open(EditorContext& context, const Item& item);

        std::filesystem::path current_;
        std::filesystem::path selected_;
        std::map<std::filesystem::path, std::vector<Item>> cache_;  // folder listings, until Refresh
        char filter_[128] = {};
    };
}
