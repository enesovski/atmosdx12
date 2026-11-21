#pragma once

#include "panels/Panel.h"

#include "scene/Scene.h"

#include <functional>
#include <vector>

namespace atmos::editor
{
    // Scene tree (Unity Hierarchy / Unreal Outliner): select, rename (F2), reparent by drag and drop,
    // toggle active, and create/duplicate/delete from the context menu. "Sky & Sun" is pinned on top.
    class HierarchyPanel final : public Panel
    {
    public:
        HierarchyPanel();

    protected:
        void DrawContent(EditorContext& context) override;

    private:
        void DrawEnvironmentRow(EditorContext& context);
        void DrawEntity(EditorContext& context, EntityId id);
        void DrawFilteredList(EditorContext& context);
        void DrawRowContents(EditorContext& context, EntityId id);
        void HandleRowClick(EditorContext& context, EntityId id);
        void HandleRowDragDrop(EditorContext& context, EntityId id);
        void DrawEntityContextMenu(EditorContext& context, EntityId id);
        void DrawCreateMenu(EditorContext& context, EntityId parent);
        void DrawWindowDropTarget(EditorContext& context);

        char filter_[128] = {};
        std::vector<std::vector<EntityId>> children_;
        std::vector<EntityId> visibleOrder_;   // rows in display order this frame
        std::vector<EntityId> previousOrder_;  // last frame's rows, for Shift-click ranges
        EntityId rangeAnchor_ = InvalidEntity;
        EntityId renaming_ = InvalidEntity;
        bool startRename_ = false;
        EntityId scrollTo_ = InvalidEntity;
        EntityId lastPrimary_ = InvalidEntity;

        // Structural edits renumber entities, so they run after the tree is drawn.
        std::vector<std::function<void(EditorContext&)>> deferred_;
    };
}
