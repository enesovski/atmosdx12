#pragma once

namespace atmos::editor
{
    struct EditorContext;

    // The main window's side bars, around the dock space (Unreal's toolbar and status bar).

    // The strip under the menu bar: save, undo/redo, transform tools, gizmo space and snapping.
    void DrawToolbar(EditorContext& context);

    // The bottom strip: last log message, asset loading, GPU and frame time.
    // Returns true when the user clicked the error indicator (to open the console).
    bool DrawStatusBar(EditorContext& context);
}
