#pragma once

#include <span>

namespace atmos::editor
{
    class Panel;
    struct EditorContext;

    // File, Edit, Create, Window, Render and Help menus. Actions that may need a prompt or a dialog
    // go through EditorContext::requests; the rest act directly.
    void DrawMainMenuBar(EditorContext& context, std::span<Panel* const> panels);
}
