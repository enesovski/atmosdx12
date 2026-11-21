#pragma once

#include "EditorContext.h"

#include <imgui.h>

namespace atmos::editor
{
    // Stable window ids ("###Name"); the visible titles carry an icon in front.
    namespace PanelId
    {
        inline constexpr const char* Viewport = "###Viewport";
        inline constexpr const char* Hierarchy = "###Hierarchy";
        inline constexpr const char* Inspector = "###Inspector";
        inline constexpr const char* RenderSettings = "###Render Settings";
        inline constexpr const char* ContentBrowser = "###Content Browser";
        inline constexpr const char* Console = "###Console";
        inline constexpr const char* Profiler = "###Profiler";
        inline constexpr const char* RenderGraph = "###Render Graph";
    }

    // Rebuilds the dock tree of `dockspace` for a preset (docs/phase-4-plan.md, Default layout).
    // A fresh node shows the tab of the first of its windows submitted that frame (EditorApp's panel order).
    void BuildLayout(ImGuiID dockspace, ImVec2 size, LayoutPreset preset);
}
