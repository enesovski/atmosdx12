#include "shell/EditorBars.h"
#include "EditorContext.h"
#include "style/EditorFonts.h"
#include "style/EditorTheme.h"
#include "widgets/Widgets.h"

#include "core/StringUtils.h"
#include "graphics/Renderer.h"
#include "runtime/AssetManager.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <format>
#include <string>

namespace atmos::editor
{
    namespace
    {
        void VerticalSeparator()
        {
            ImGui::SameLine();
            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
            ImGui::SameLine();
        }

        void DrawSnapPopup(GizmoSettings& gizmo)
        {
            if (!ImGui::BeginPopup("SnapMenu"))
                return;

            ImGui::SeparatorText("Snap increments");
            ImGui::PushItemWidth(ImGui::GetFontSize() * 8.0f);
            ImGui::DragFloat("Move", &gizmo.moveSnap, 0.01f, 0.01f, 10.0f, "%.2f m");
            ImGui::DragFloat("Rotate", &gizmo.rotateSnap, 0.5f, 1.0f, 90.0f, "%.0f deg");
            ImGui::DragFloat("Scale", &gizmo.scaleSnap, 0.01f, 0.01f, 1.0f, "%.2f");
            ImGui::PopItemWidth();
            ImGui::EndPopup();
        }
    }

    void DrawToolbar(EditorContext& context)
    {
        const ImGuiStyle& style = ImGui::GetStyle();
        const float height = ImGui::GetFrameHeight() + style.WindowPadding.y * 2.0f;
        const ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings;

        ImGui::PushStyleColor(ImGuiCol_WindowBg, GetThemeColors().panelRaised);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

        if (ImGui::BeginViewportSideBar("##Toolbar", ImGui::GetMainViewport(), ImGuiDir_Up, height, flags))
        {
            SceneDocument& document = context.document;
            GizmoSettings& gizmo = context.gizmo;

            if (widgets::IconButton("save", Icon::Save, "Save scene (Ctrl+S)"))
                context.requests.saveScene = true;

            VerticalSeparator();

            ImGui::BeginDisabled(!document.GetUndoStack().CanUndo());
            if (widgets::IconButton("undo", Icon::Undo, "Undo (Ctrl+Z)"))
                document.Undo();
            ImGui::EndDisabled();

            ImGui::SameLine();
            ImGui::BeginDisabled(!document.GetUndoStack().CanRedo());
            if (widgets::IconButton("redo", Icon::Redo, "Redo (Ctrl+Y)"))
                document.Redo();
            ImGui::EndDisabled();

            VerticalSeparator();

            struct ToolButton
            {
                GizmoTool tool;
                const IconGlyph* icon;
                const char* tooltip;
            };

            const ToolButton tools[] = {
                { GizmoTool::Select, &Icon::Select, "Select (Q)" },
                { GizmoTool::Move, &Icon::Move, "Move (W)" },
                { GizmoTool::Rotate, &Icon::Rotate, "Rotate (E)" },
                { GizmoTool::Scale, &Icon::Scale, "Scale (R)" },
            };

            for (const ToolButton& tool : tools)
            {
                if (widgets::IconButton(tool.tooltip, *tool.icon, tool.tooltip, gizmo.tool == tool.tool))
                    gizmo.tool = tool.tool;

                ImGui::SameLine();
            }

            VerticalSeparator();

            const bool world = gizmo.space == GizmoSpace::World;

            if (widgets::IconButton("space", world ? Icon::Globe : Icon::Model,
                                    world ? "World space (click for local)" : "Local space (click for world)"))
                gizmo.space = world ? GizmoSpace::Local : GizmoSpace::World;

            ImGui::SameLine();

            if (widgets::IconButton("snap", Icon::Grid, "Snap to increments (right-click to set them)", gizmo.snap))
                gizmo.snap = !gizmo.snap;

            if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
                ImGui::OpenPopup("SnapMenu");

            DrawSnapPopup(gizmo);

            // Fullscreen toggle on the right.
            ImGui::SameLine(ImGui::GetContentRegionMax().x - ImGui::GetFrameHeight());

            if (widgets::IconButton("fullscreen", Icon::Fullscreen, "Fullscreen viewport (F11)"))
                context.requests.toggleFullscreen = true;
        }

        ImGui::End();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    }

    bool DrawStatusBar(EditorContext& context)
    {
        const ThemeColors& colors = GetThemeColors();
        const ScopedFont smallFont(FontRole::Small);
        const float height = ImGui::GetFrameHeight();
        bool openConsole = false;

        ImGui::PushStyleColor(ImGuiCol_WindowBg, colors.panelRaised);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(ImGui::GetStyle().WindowPadding.x, 2.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

        if (ImGui::BeginViewportSideBar("##StatusBar", ImGui::GetMainViewport(), ImGuiDir_Down, height,
                                        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings))
        {
            // Last message, colored by level; errors stay red until the console is opened.
            const ConsoleLog& console = context.console;
            ImGui::AlignTextToFramePadding();

            if (console.GetUnseenErrors() > 0)
            {
                const std::string label = std::format("{} {}", std::string(Icon::Error), console.GetUnseenErrors());
                ImGui::PushStyleColor(ImGuiCol_Text, colors.error);
                openConsole = ImGui::SmallButton(label.c_str());
                ImGui::PopStyleColor();
                widgets::Tooltip("New errors: open the console");
                ImGui::SameLine();
            }

            if (!console.GetEntries().empty())
            {
                const ConsoleEntry& last = console.GetEntries().back();
                const ImVec4 color = last.level == log::Level::Error   ? colors.error
                                   : last.level == log::Level::Warning ? colors.warning
                                                                       : colors.textDim;
                ImGui::PushStyleColor(ImGuiCol_Text, color);
                ImGui::TextUnformatted(last.message.c_str());
                ImGui::PopStyleColor();
            }
            else
            {
                widgets::TextDim("Ready");
            }

            // Right side: asset progress, GPU, frame time.
            const AssetStats assets = context.assets.GetStats();
            const uint32_t pending = assets.modelsLoading + assets.texturesLoading;
            const uint32_t textureTotal = assets.texturesReady + assets.texturesLoading + assets.texturesFailed;
            const RenderFrameStats& stats = context.renderer.GetFrameStats();
            const float cpuMs = context.deltaSeconds * 1000.0f;

            const std::string gpu = ToUtf8(context.renderer.GetDevice().GetAdapterName());
            const std::string timing = std::format("{:.0f} FPS   GPU {:.2f} ms", cpuMs > 0.0f ? 1000.0f / cpuMs : 0.0f,
                                                   stats.gpuMilliseconds);
            const std::string loading = pending > 0 ? std::format("Loading {} / {}", assets.texturesReady, textureTotal)
                                                    : std::string();

            const float spacing = ImGui::GetFontSize() * 1.5f;
            const float progressWidth = pending > 0 ? ImGui::GetFontSize() * 6.0f + spacing : 0.0f;
            const float rightWidth = ImGui::CalcTextSize(timing.c_str()).x + ImGui::CalcTextSize(gpu.c_str()).x +
                                     ImGui::CalcTextSize(loading.c_str()).x + progressWidth + spacing * 2.0f;

            ImGui::SameLine(std::max(ImGui::GetCursorPosX() + spacing, ImGui::GetContentRegionMax().x - rightWidth));

            if (pending > 0)
            {
                widgets::TextDim("%s", loading.c_str());
                ImGui::SameLine();
                ImGui::ProgressBar(textureTotal > 0 ? static_cast<float>(assets.texturesReady) / textureTotal : 0.0f,
                                   ImVec2(ImGui::GetFontSize() * 6.0f, ImGui::GetFontSize() * 0.6f), "");
                ImGui::SameLine(0.0f, spacing);
            }

            widgets::TextDim("%s  %s", Icon::Gpu.utf8, gpu.c_str());
            ImGui::SameLine(0.0f, spacing);
            ImGui::TextUnformatted(timing.c_str());
        }

        ImGui::End();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor();
        return openConsole;
    }
}
