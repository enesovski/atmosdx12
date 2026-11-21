#include "EditorLayout.h"

#include <imgui_internal.h>

namespace atmos::editor
{
    namespace
    {
        ImGuiID Split(ImGuiID& node, ImGuiDir direction, float ratio)
        {
            ImGuiID other = 0;
            const ImGuiID split = ImGui::DockBuilderSplitNode(node, direction, ratio, nullptr, &other);
            node = other;
            return split;
        }

        // Hierarchy | Viewport over a bottom tab row | Inspector + Render Settings.
        void BuildDefault(ImGuiID root)
        {
            ImGuiID center = root;
            const ImGuiID left = Split(center, ImGuiDir_Left, 0.18f);
            const ImGuiID right = Split(center, ImGuiDir_Right, 0.27f);
            const ImGuiID bottom = Split(center, ImGuiDir_Down, 0.27f);

            ImGui::DockBuilderDockWindow(PanelId::Hierarchy, left);
            ImGui::DockBuilderDockWindow(PanelId::Inspector, right);
            ImGui::DockBuilderDockWindow(PanelId::RenderSettings, right);
            ImGui::DockBuilderDockWindow(PanelId::ContentBrowser, bottom);
            ImGui::DockBuilderDockWindow(PanelId::Console, bottom);
            ImGui::DockBuilderDockWindow(PanelId::Profiler, bottom);
            ImGui::DockBuilderDockWindow(PanelId::RenderGraph, bottom);
            ImGui::DockBuilderDockWindow(PanelId::Viewport, center);
        }

        // Render Settings gets the full-height right column; the profiler sits under the viewport.
        void BuildLighting(ImGuiID root)
        {
            ImGuiID center = root;
            const ImGuiID left = Split(center, ImGuiDir_Left, 0.16f);
            ImGuiID right = Split(center, ImGuiDir_Right, 0.30f);
            const ImGuiID rightBottom = Split(right, ImGuiDir_Down, 0.35f);
            const ImGuiID bottom = Split(center, ImGuiDir_Down, 0.30f);

            ImGui::DockBuilderDockWindow(PanelId::Hierarchy, left);
            ImGui::DockBuilderDockWindow(PanelId::RenderSettings, right);
            ImGui::DockBuilderDockWindow(PanelId::Inspector, rightBottom);
            ImGui::DockBuilderDockWindow(PanelId::Profiler, bottom);
            ImGui::DockBuilderDockWindow(PanelId::Console, bottom);
            ImGui::DockBuilderDockWindow(PanelId::ContentBrowser, bottom);
            ImGui::DockBuilderDockWindow(PanelId::RenderGraph, bottom);
            ImGui::DockBuilderDockWindow(PanelId::Viewport, center);
        }

        // Profiler and Render Graph side by side under a smaller viewport.
        void BuildProfiling(ImGuiID root)
        {
            ImGuiID center = root;
            const ImGuiID left = Split(center, ImGuiDir_Left, 0.16f);
            const ImGuiID right = Split(center, ImGuiDir_Right, 0.25f);
            ImGuiID bottom = Split(center, ImGuiDir_Down, 0.45f);
            const ImGuiID bottomRight = Split(bottom, ImGuiDir_Right, 0.5f);

            ImGui::DockBuilderDockWindow(PanelId::Hierarchy, left);
            ImGui::DockBuilderDockWindow(PanelId::Inspector, right);
            ImGui::DockBuilderDockWindow(PanelId::RenderSettings, right);
            ImGui::DockBuilderDockWindow(PanelId::Profiler, bottom);
            ImGui::DockBuilderDockWindow(PanelId::Console, bottom);
            ImGui::DockBuilderDockWindow(PanelId::ContentBrowser, bottom);
            ImGui::DockBuilderDockWindow(PanelId::RenderGraph, bottomRight);
            ImGui::DockBuilderDockWindow(PanelId::Viewport, center);
        }
    }

    void BuildLayout(ImGuiID dockspace, ImVec2 size, LayoutPreset preset)
    {
        ImGui::DockBuilderRemoveNode(dockspace);
        ImGui::DockBuilderAddNode(dockspace, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspace, size);

        switch (preset)
        {
            case LayoutPreset::Lighting:  BuildLighting(dockspace); break;
            case LayoutPreset::Profiling: BuildProfiling(dockspace); break;
            default:                      BuildDefault(dockspace); break;
        }

        ImGui::DockBuilderFinish(dockspace);
    }
}
