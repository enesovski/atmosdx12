#include "panels/HierarchyPanel.h"
#include "EditorContext.h"
#include "style/EditorFonts.h"
#include "style/EditorTheme.h"
#include "tools/FileDialogs.h"
#include "widgets/Widgets.h"

#include "core/StringUtils.h"

#include <algorithm>
#include <cctype>
#include <string>

namespace atmos::editor
{
    namespace
    {
        constexpr const char* EntityPayload = "ATMOS_ENTITY";
        constexpr const char* ModelPayload = "ATMOS_MODEL_PATH";

        const IconGlyph& GetEntityIcon(const Entity& entity)
        {
            if (entity.light)
                return entity.light->type == LightType::Spot ? Icon::SpotLight : Icon::PointLight;

            return entity.model ? Icon::Model : Icon::Entity;
        }

        bool ContainsIgnoreCase(const std::string& text, const char* pattern)
        {
            const std::string_view needle(pattern);
            const auto found = std::ranges::search(text, needle, [](char a, char b) {
                return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
            });
            return !found.empty();
        }
    }

    HierarchyPanel::HierarchyPanel() : Panel("Hierarchy", Icon::Hierarchy)
    {
    }

    void HierarchyPanel::DrawContent(EditorContext& context)
    {
        const Scene& scene = context.document.GetScene();

        if (widgets::IconButton("create", Icon::Add, "Create entity"))
            ImGui::OpenPopup("CreateMenu");

        if (ImGui::BeginPopup("CreateMenu"))
        {
            DrawCreateMenu(context, InvalidEntity);
            ImGui::EndPopup();
        }

        ImGui::SameLine();
        widgets::SearchField("filter", filter_, sizeof(filter_), "Search");
        ImGui::Separator();

        children_.assign(scene.GetEntityCount(), {});
        std::vector<EntityId> roots;

        for (EntityId id = 0; id < scene.GetEntityCount(); ++id)
        {
            const EntityId parent = scene.GetEntity(id).parent;
            (parent == InvalidEntity ? roots : children_[parent]).push_back(id);
        }

        // Bring rows selected elsewhere (e.g. clicked in the viewport) into view.
        const EntityId primary = context.document.GetSelection().GetPrimary();

        if (primary != lastPrimary_)
        {
            scrollTo_ = primary;
            lastPrimary_ = primary;
        }

        ImGui::BeginChild("Rows", ImVec2(0, 0), ImGuiChildFlags_None);
        previousOrder_ = std::move(visibleOrder_);
        visibleOrder_.clear();

        DrawEnvironmentRow(context);

        if (filter_[0])
        {
            DrawFilteredList(context);
        }
        else
        {
            for (EntityId root : roots)
                DrawEntity(context, root);
        }

        DrawWindowDropTarget(context);

        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && ImGui::IsKeyPressed(ImGuiKey_F2, false) &&
            primary != InvalidEntity)
        {
            renaming_ = primary;
            startRename_ = true;
        }

        ImGui::EndChild();

        for (const auto& action : deferred_)
            action(context);

        deferred_.clear();
    }

    void HierarchyPanel::DrawEnvironmentRow(EditorContext& context)
    {
        EditorSelection& selection = context.document.GetSelection();
        const std::string label = std::string(Icon::Sun) + "  Sky & Sun";

        if (ImGui::Selectable(label.c_str(), selection.IsEnvironmentSelected(), ImGuiSelectableFlags_SpanAllColumns))
            selection.SelectEnvironment();

        widgets::Tooltip("Sun direction and intensity, sky lighting, exposure");
        ImGui::Separator();
    }

    void HierarchyPanel::DrawEntity(EditorContext& context, EntityId id)
    {
        const Scene& scene = context.document.GetScene();
        const bool selected = context.document.GetSelection().IsSelected(id);
        const bool hasChildren = !children_[id].empty();

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanFullWidth |
                                   ImGuiTreeNodeFlags_AllowOverlap | ImGuiTreeNodeFlags_DefaultOpen |
                                   ImGuiTreeNodeFlags_FramePadding;

        if (selected)
            flags |= ImGuiTreeNodeFlags_Selected;

        if (!hasChildren)
            flags |= ImGuiTreeNodeFlags_Leaf;

        ImGui::PushID(static_cast<int>(id));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x, 2.0f));
        const bool open = ImGui::TreeNodeEx("##node", flags);
        ImGui::PopStyleVar();

        HandleRowClick(context, id);
        HandleRowDragDrop(context, id);

        if (ImGui::BeginPopupContextItem("EntityMenu"))
        {
            DrawEntityContextMenu(context, id);
            ImGui::EndPopup();
        }

        if (scrollTo_ == id)
        {
            if (!ImGui::IsItemVisible())
                ImGui::SetScrollHereY(0.5f);

            scrollTo_ = InvalidEntity;
        }

        visibleOrder_.push_back(id);
        DrawRowContents(context, id);

        if (open)
        {
            for (EntityId child : children_[id])
            {
                if (child < scene.GetEntityCount())
                    DrawEntity(context, child);
            }

            ImGui::TreePop();
        }

        ImGui::PopID();
    }

    void HierarchyPanel::DrawRowContents(EditorContext& context, EntityId id)
    {
        Scene& scene = context.document.GetScene();
        Entity& entity = scene.GetEntity(id);
        const ThemeColors& colors = GetThemeColors();
        const float eyeX = ImGui::GetContentRegionMax().x - ImGui::GetFrameHeight();

        ImGui::SameLine();

        if (renaming_ == id)
        {
            std::string name = entity.name;

            if (widgets::InputString("##rename", name, eyeX - ImGui::GetCursorPosX(), startRename_))
                context.document.Rename(id, name);

            if (!startRename_ && !ImGui::IsItemActive())
                renaming_ = InvalidEntity;

            startRename_ = false;
        }
        else
        {
            const bool active = entity.active;
            ImGui::PushStyleColor(ImGuiCol_Text, active ? colors.text : colors.textDim);
            ImGui::Text("%s  %s", GetEntityIcon(entity).utf8, entity.name.c_str());
            ImGui::PopStyleColor();
        }

        // Active toggle on the right edge, like Unreal's outliner eye.
        ImGui::SameLine(eyeX);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_Text, entity.active ? colors.textDim : ImVec4(colors.textDim.x, colors.textDim.y,
                                                                                      colors.textDim.z, 0.45f));

        if (widgets::IconButton("active", entity.active ? Icon::Eye : Icon::EyeOff, "Active (shown in the scene)"))
        {
            entity.active = !entity.active;
            context.document.MarkEdited(entity.active ? "Activate" : "Deactivate");
        }

        ImGui::PopStyleColor(2);
    }

    void HierarchyPanel::HandleRowClick(EditorContext& context, EntityId id)
    {
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            context.document.GetSelection().Select(id);
            context.requests.focusSelection = true;
            return;
        }

        if (!ImGui::IsItemClicked(ImGuiMouseButton_Left) || ImGui::IsItemToggledOpen())
            return;

        EditorSelection& selection = context.document.GetSelection();
        const ImGuiIO& io = ImGui::GetIO();

        if (io.KeyCtrl)
        {
            selection.Toggle(id);
            rangeAnchor_ = id;
        }
        else if (io.KeyShift && rangeAnchor_ != InvalidEntity)
        {
            // Range over the rows as they were laid out last frame.
            const auto anchor = std::ranges::find(previousOrder_, rangeAnchor_);
            const auto target = std::ranges::find(previousOrder_, id);

            if (anchor == previousOrder_.end() || target == previousOrder_.end())
            {
                selection.Add(id);
                return;
            }

            const auto [first, last] = std::minmax(anchor, target);
            selection.Clear();

            for (auto row = first; row <= last; ++row)
                selection.Add(*row);

            selection.Add(id);
        }
        else
        {
            selection.Select(id);
            rangeAnchor_ = id;
        }
    }

    void HierarchyPanel::HandleRowDragDrop(EditorContext& context, EntityId id)
    {
        if (ImGui::BeginDragDropSource())
        {
            ImGui::SetDragDropPayload(EntityPayload, &id, sizeof(id));
            ImGui::Text("%s  %s", GetEntityIcon(context.document.GetScene().GetEntity(id)).utf8,
                        context.document.GetScene().GetEntity(id).name.c_str());
            ImGui::EndDragDropSource();
        }

        if (!ImGui::BeginDragDropTarget())
            return;

        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(EntityPayload))
        {
            const EntityId dragged = *static_cast<const EntityId*>(payload->Data);
            deferred_.push_back([dragged, id](EditorContext& c) { c.document.Reparent(dragged, id); });
        }

        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(ModelPayload))
        {
            const std::string path(static_cast<const char*>(payload->Data), payload->DataSize);
            deferred_.push_back([path, id](EditorContext& c) {
                const EntityId model = c.document.AddModel(ToWide(path), {});
                c.document.Reparent(model, id);
            });
        }

        ImGui::EndDragDropTarget();
    }

    void HierarchyPanel::DrawEntityContextMenu(EditorContext& context, EntityId id)
    {
        EditorSelection& selection = context.document.GetSelection();

        // Right-clicking an unselected row acts on that row alone (Unity behavior).
        if (!selection.IsSelected(id))
            selection.Select(id);

        if (ImGui::BeginMenu("Create Child"))
        {
            DrawCreateMenu(context, id);
            ImGui::EndMenu();
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Rename", "F2"))
        {
            renaming_ = id;
            startRename_ = true;
        }

        if (ImGui::MenuItem("Duplicate", "Ctrl+D"))
            deferred_.push_back([](EditorContext& c) { c.document.DuplicateSelection(); });

        if (ImGui::MenuItem("Delete", "Del"))
            deferred_.push_back([](EditorContext& c) { c.document.DeleteSelection(); });

        if (ImGui::MenuItem("Focus", "F"))
            context.requests.focusSelection = true;

        if (context.document.GetScene().GetEntity(id).parent != InvalidEntity)
        {
            ImGui::Separator();

            if (ImGui::MenuItem("Unparent"))
                deferred_.push_back([id](EditorContext& c) { c.document.Reparent(id, InvalidEntity); });
        }
    }

    void HierarchyPanel::DrawCreateMenu(EditorContext& context, EntityId parent)
    {
        const DirectX::XMFLOAT3 position = GetSpawnPosition(context);

        auto create = [&](auto makeEntity) {
            deferred_.push_back([makeEntity, parent](EditorContext& c) {
                const EntityId id = makeEntity(c);

                if (parent != InvalidEntity)
                    c.document.Reparent(id, parent);
            });
        };

        if (ImGui::MenuItem((std::string(Icon::Entity) + "  Empty").c_str()))
            create([](EditorContext& c) { return c.document.CreateEntity("Entity"); });

        if (ImGui::MenuItem((std::string(Icon::PointLight) + "  Point Light").c_str()))
            create([position](EditorContext& c) { return c.document.CreatePointLight(position); });

        if (ImGui::MenuItem((std::string(Icon::SpotLight) + "  Spot Light").c_str()))
            create([position](EditorContext& c) { return c.document.CreateSpotLight(position); });

        ImGui::Separator();

        if (ImGui::MenuItem((std::string(Icon::Model) + "  Model from File...").c_str()))
        {
            if (const auto path = ShowOpenDialog(GetActiveWindow(), FileKind::Model, context.paths.assets))
                create([path = *path, position](EditorContext& c) { return c.document.AddModel(path, position); });
        }
    }

    void HierarchyPanel::DrawFilteredList(EditorContext& context)
    {
        const Scene& scene = context.document.GetScene();

        for (EntityId id = 0; id < scene.GetEntityCount(); ++id)
        {
            const Entity& entity = scene.GetEntity(id);

            if (!ContainsIgnoreCase(entity.name, filter_))
                continue;

            ImGui::PushID(static_cast<int>(id));
            const bool selected = context.document.GetSelection().IsSelected(id);
            ImGui::TreeNodeEx("##node", ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_SpanFullWidth |
                                            ImGuiTreeNodeFlags_AllowOverlap | ImGuiTreeNodeFlags_NoTreePushOnOpen |
                                            ImGuiTreeNodeFlags_FramePadding | (selected ? ImGuiTreeNodeFlags_Selected : 0));
            HandleRowClick(context, id);

            if (ImGui::BeginPopupContextItem("EntityMenu"))
            {
                DrawEntityContextMenu(context, id);
                ImGui::EndPopup();
            }

            visibleOrder_.push_back(id);
            DrawRowContents(context, id);
            ImGui::PopID();
        }
    }

    void HierarchyPanel::DrawWindowDropTarget(EditorContext& context)
    {
        // The empty space below the rows: drop to unparent, click to deselect, right-click to create.
        const ImVec2 available = ImGui::GetContentRegionAvail();
        ImGui::InvisibleButton("##empty", ImVec2(std::max(available.x, 1.0f), std::max(available.y, ImGui::GetFrameHeight())));

        if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
            context.document.GetSelection().Clear();

        if (ImGui::BeginPopupContextItem("EmptyMenu"))
        {
            DrawCreateMenu(context, InvalidEntity);
            ImGui::EndPopup();
        }

        if (!ImGui::BeginDragDropTarget())
            return;

        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(EntityPayload))
        {
            const EntityId dragged = *static_cast<const EntityId*>(payload->Data);
            deferred_.push_back([dragged](EditorContext& c) { c.document.Reparent(dragged, InvalidEntity); });
        }

        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(ModelPayload))
        {
            const std::string path(static_cast<const char*>(payload->Data), payload->DataSize);
            deferred_.push_back([path](EditorContext& c) { c.document.AddModel(ToWide(path), {}); });
        }

        ImGui::EndDragDropTarget();
    }
}
