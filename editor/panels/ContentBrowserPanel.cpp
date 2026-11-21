#include "panels/ContentBrowserPanel.h"
#include "EditorContext.h"
#include "style/EditorFonts.h"
#include "style/EditorTheme.h"
#include "tools/FileDialogs.h"
#include "widgets/Widgets.h"

#include "core/StringUtils.h"

#include <algorithm>
#include <cctype>

namespace atmos::editor
{
    namespace
    {
        constexpr const char* ModelPayload = "ATMOS_MODEL_PATH";

        std::string FitText(ImFont* font, const std::string& text, float width)
        {
            auto measure = [&](const std::string& value) {
                return font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.0f, value.c_str()).x;
            };

            if (measure(text) <= width)
                return text;

            std::string shortened = text;

            while (!shortened.empty() && measure(shortened + "...") > width)
                shortened.pop_back();

            return shortened + "...";
        }

        std::string ToLower(std::string text)
        {
            std::ranges::transform(text, text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return text;
        }
    }

    ContentBrowserPanel::ContentBrowserPanel() : Panel("Content Browser", Icon::Folder)
    {
    }

    const std::vector<ContentBrowserPanel::Item>& ContentBrowserPanel::GetItems(const std::filesystem::path& folder)
    {
        const auto cached = cache_.find(folder);

        if (cached != cache_.end())
            return cached->second;

        std::vector<Item> items;
        std::error_code error;

        for (const auto& entry : std::filesystem::directory_iterator(folder, error))
        {
            const std::filesystem::path& path = entry.path();
            const std::string extension = ToLower(ToUtf8(path.extension().wstring()));
            ItemKind kind = ItemKind::Other;

            if (entry.is_directory(error))
                kind = ItemKind::Folder;
            else if (extension == ".json")
                kind = ItemKind::Scene;
            else if (extension == ".gltf" || extension == ".glb")
                kind = ItemKind::Model;
            else if (extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".dds")
                kind = ItemKind::Image;

            items.push_back({ path, ToUtf8(path.filename().wstring()), kind });
        }

        // Folders first, then by name.
        std::ranges::sort(items, [](const Item& a, const Item& b) {
            if ((a.kind == ItemKind::Folder) != (b.kind == ItemKind::Folder))
                return a.kind == ItemKind::Folder;

            return ToLower(a.name) < ToLower(b.name);
        });

        return cache_.emplace(folder, std::move(items)).first->second;
    }

    void ContentBrowserPanel::DrawContent(EditorContext& context)
    {
        if (current_.empty())
            current_ = context.paths.scenes;

        const float treeWidth = std::max(ImGui::GetContentRegionAvail().x * 0.22f, ImGui::GetFontSize() * 10.0f);

        ImGui::BeginChild("Folders", ImVec2(treeWidth, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
        DrawFolderTree(context, context.paths.scenes, "Scenes");
        DrawFolderTree(context, context.paths.assets, "Assets");
        ImGui::EndChild();

        ImGui::SameLine();
        ImGui::BeginChild("Files", ImVec2(0, 0), ImGuiChildFlags_None);
        DrawBreadcrumbs(context);
        ImGui::Separator();
        DrawItems(context);
        ImGui::EndChild();
    }

    void ContentBrowserPanel::DrawFolderTree(EditorContext& context, const std::filesystem::path& folder, const char* label)
    {
        if (!std::filesystem::exists(folder))
            return;

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;

        if (folder == current_)
            flags |= ImGuiTreeNodeFlags_Selected;

        const bool hasSubfolders = std::ranges::any_of(GetItems(folder), [](const Item& item) { return item.kind == ItemKind::Folder; });

        if (!hasSubfolders)
            flags |= ImGuiTreeNodeFlags_Leaf;

        const std::string text = std::string(folder == current_ ? Icon::FolderOpen : Icon::Folder) + "  " + label;
        ImGui::PushID(ToUtf8(folder.wstring()).c_str());
        const bool open = ImGui::TreeNodeEx("##folder", flags, "%s", text.c_str());

        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
            current_ = folder;

        if (open)
        {
            // Copy: expanding a child can grow the cache while iterating.
            const std::vector<Item> items = GetItems(folder);

            for (const Item& item : items)
            {
                if (item.kind == ItemKind::Folder)
                    DrawFolderTree(context, item.path, item.name.c_str());
            }

            ImGui::TreePop();
        }

        ImGui::PopID();
    }

    void ContentBrowserPanel::DrawBreadcrumbs(EditorContext& context)
    {
        if (widgets::IconButton("refresh", Icon::Refresh, "Refresh"))
            cache_.clear();

        ImGui::SameLine();

        // Path segments below the project root, each clickable.
        std::error_code error;
        const std::filesystem::path relative = std::filesystem::relative(current_, context.paths.root, error);
        std::filesystem::path accumulated = context.paths.root;
        bool first = true;

        for (const std::filesystem::path& part : relative)
        {
            accumulated /= part;

            if (!first)
            {
                ImGui::SameLine(0.0f, 2.0f);
                widgets::TextDim("%s", Icon::ChevronRight.utf8);
                ImGui::SameLine(0.0f, 2.0f);
            }

            first = false;
            ImGui::PushID(ToUtf8(accumulated.wstring()).c_str());
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));

            if (ImGui::Button(ToUtf8(part.wstring()).c_str()))
                current_ = accumulated;

            ImGui::PopStyleColor();
            ImGui::PopID();
        }

        const float searchWidth = ImGui::GetFontSize() * 12.0f;
        ImGui::SameLine(std::max(ImGui::GetCursorPosX(), ImGui::GetContentRegionMax().x - searchWidth));
        ImGui::BeginGroup();
        ImGui::PushItemWidth(searchWidth);
        widgets::SearchField("filter", filter_, sizeof(filter_), "Search folder");
        ImGui::PopItemWidth();
        ImGui::EndGroup();
    }

    void ContentBrowserPanel::DrawItems(EditorContext& context)
    {
        const ThemeColors& colors = GetThemeColors();
        const float tileWidth = ImGui::GetFontSize() * 6.0f;
        const float tileHeight = ImGui::GetFontSize() * 5.2f;
        const int columns = std::max(1, static_cast<int>(ImGui::GetContentRegionAvail().x / (tileWidth + 8.0f)));
        const std::string filter = ToLower(filter_);

        // Copy: opening a folder replaces current_ and the listing while iterating.
        const std::vector<Item> items = GetItems(current_);
        int column = 0;

        if (items.empty())
            widgets::TextDim("This folder is empty.");

        for (const Item& item : items)
        {
            if (!filter.empty() && ToLower(item.name).find(filter) == std::string::npos)
                continue;

            if (column > 0)
                ImGui::SameLine();

            ImGui::PushID(item.name.c_str());
            ImGui::BeginGroup();
            const ImVec2 tileMin = ImGui::GetCursorScreenPos();

            if (ImGui::Selectable("##tile", selected_ == item.path, ImGuiSelectableFlags_AllowDoubleClick,
                                  ImVec2(tileWidth, tileHeight)))
            {
                selected_ = item.path;

                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                    Open(context, item);
            }

            if (item.kind == ItemKind::Model && ImGui::BeginDragDropSource())
            {
                const std::string path = ToUtf8(item.path.wstring());
                ImGui::SetDragDropPayload(ModelPayload, path.data(), path.size());
                ImGui::Text("%s  %s", Icon::Model.utf8, item.name.c_str());
                ImGui::EndDragDropSource();
            }

            if (ImGui::BeginPopupContextItem("ItemMenu"))
            {
                if (ImGui::MenuItem("Open"))
                    Open(context, item);

                if (ImGui::MenuItem("Show in Explorer"))
                    RevealInExplorer(item.path);

                ImGui::EndPopup();
            }

            widgets::Tooltip(item.name.c_str());

            // Icon and name drawn over the selectable.
            const IconGlyph& icon = item.kind == ItemKind::Folder ? Icon::Folder
                                  : item.kind == ItemKind::Scene  ? Icon::SceneFile
                                  : item.kind == ItemKind::Model  ? Icon::Model
                                  : item.kind == ItemKind::Image  ? Icon::Image
                                                                  : Icon::File;
            const ImVec4 iconColor = item.kind == ItemKind::Folder ? colors.warning
                                   : item.kind == ItemKind::Scene  ? colors.accent
                                   : item.kind == ItemKind::Model  ? colors.success
                                                                   : colors.textDim;

            ImDrawList* draw = ImGui::GetWindowDrawList();
            ImFont* iconFont = GetFont(FontRole::Icons);
            const float iconSize = iconFont->FontSize * 1.3f;
            const ImVec2 iconExtent = iconFont->CalcTextSizeA(iconSize, FLT_MAX, 0.0f, icon);
            draw->AddText(iconFont, iconSize, ImVec2(tileMin.x + (tileWidth - iconExtent.x) * 0.5f, tileMin.y + 6.0f),
                          ToU32(iconColor), icon);

            // One line, shortened with an ellipsis; the full name is in the tooltip.
            ImFont* font = GetFont(FontRole::Small);
            const std::string name = FitText(font, item.name, tileWidth - 8.0f);
            const ImVec2 nameSize = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.0f, name.c_str());
            draw->AddText(font, font->FontSize,
                          ImVec2(tileMin.x + (tileWidth - nameSize.x) * 0.5f, tileMin.y + tileHeight - nameSize.y - 6.0f),
                          ToU32(colors.text), name.c_str());

            ImGui::EndGroup();
            ImGui::PopID();
            column = (column + 1) % columns;
        }
    }

    void ContentBrowserPanel::Open(EditorContext& context, const Item& item)
    {
        switch (item.kind)
        {
            case ItemKind::Folder:
                current_ = item.path;
                break;
            case ItemKind::Scene:
                context.requests.openScene = item.path;
                break;
            case ItemKind::Model:
                context.document.AddModel(item.path, GetSpawnPosition(context));
                break;
            default:
                RevealInExplorer(item.path);
                break;
        }
    }
}
