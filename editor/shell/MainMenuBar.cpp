#include "shell/MainMenuBar.h"
#include "EditorContext.h"
#include "ViewModes.h"
#include "panels/Panel.h"
#include "style/EditorTheme.h"
#include "style/Icons.h"
#include "tools/FileDialogs.h"
#include "widgets/Widgets.h"

#include "core/StringUtils.h"
#include "graphics/Renderer.h"

#include <imgui.h>

#include <format>
#include <string>

namespace atmos::editor
{
    namespace
    {
        std::string WithIcon(const IconGlyph& icon, const char* label)
        {
            return std::string(icon) + "  " + label;
        }

        void DrawFileMenu(EditorContext& context)
        {
            if (!ImGui::BeginMenu("File"))
                return;

            if (ImGui::MenuItem(WithIcon(Icon::File, "New Scene").c_str(), "Ctrl+N"))
                context.requests.newScene = true;

            if (ImGui::MenuItem(WithIcon(Icon::OpenFile, "Open Scene...").c_str(), "Ctrl+O"))
                context.requests.openSceneDialog = true;

            if (ImGui::BeginMenu(WithIcon(Icon::FolderOpen, "Open Recent").c_str(), !context.prefs.recentScenes.empty()))
            {
                for (const std::filesystem::path& path : context.prefs.recentScenes)
                {
                    if (ImGui::MenuItem(ToUtf8(path.filename().wstring()).c_str()))
                        context.requests.openScene = path;

                    widgets::Tooltip(ToUtf8(path.wstring()).c_str());
                }

                ImGui::EndMenu();
            }

            ImGui::Separator();

            if (ImGui::MenuItem(WithIcon(Icon::Save, "Save").c_str(), "Ctrl+S"))
                context.requests.saveScene = true;

            if (ImGui::MenuItem(WithIcon(Icon::Save, "Save As...").c_str(), "Ctrl+Shift+S"))
                context.requests.saveSceneAs = true;

            ImGui::Separator();

            if (ImGui::MenuItem("Exit", "Alt+F4"))
                context.requests.exit = true;

            ImGui::EndMenu();
        }

        void DrawEditMenu(EditorContext& context)
        {
            if (!ImGui::BeginMenu("Edit"))
                return;

            SceneDocument& document = context.document;
            const UndoStack& undo = document.GetUndoStack();
            const std::string undoLabel = undo.CanUndo() ? "Undo " + undo.GetUndoLabel() : std::string("Undo");
            const std::string redoLabel = undo.CanRedo() ? "Redo " + undo.GetRedoLabel() : std::string("Redo");

            if (ImGui::MenuItem(WithIcon(Icon::Undo, undoLabel.c_str()).c_str(), "Ctrl+Z", false, undo.CanUndo()))
                document.Undo();

            if (ImGui::MenuItem(WithIcon(Icon::Redo, redoLabel.c_str()).c_str(), "Ctrl+Y", false, undo.CanRedo()))
                document.Redo();

            ImGui::Separator();
            const bool hasSelection = document.GetSelection().HasEntities();

            if (ImGui::MenuItem(WithIcon(Icon::Copy, "Duplicate").c_str(), "Ctrl+D", false, hasSelection))
                document.DuplicateSelection();

            if (ImGui::MenuItem(WithIcon(Icon::Delete, "Delete").c_str(), "Del", false, hasSelection))
                document.DeleteSelection();

            if (ImGui::MenuItem(WithIcon(Icon::Search, "Focus Selection").c_str(), "F", false, hasSelection))
                context.requests.focusSelection = true;

            ImGui::Separator();

            if (ImGui::MenuItem(WithIcon(Icon::Settings, "Preferences...").c_str()))
                context.requests.showPreferences = true;

            ImGui::EndMenu();
        }

        void DrawCreateMenu(EditorContext& context)
        {
            if (!ImGui::BeginMenu("Create"))
                return;

            SceneDocument& document = context.document;
            const DirectX::XMFLOAT3 position = GetSpawnPosition(context);

            if (ImGui::MenuItem(WithIcon(Icon::Entity, "Empty Entity").c_str()))
                document.CreateEntity("Entity");

            if (ImGui::MenuItem(WithIcon(Icon::PointLight, "Point Light").c_str()))
                document.CreatePointLight(position);

            if (ImGui::MenuItem(WithIcon(Icon::SpotLight, "Spot Light").c_str()))
                document.CreateSpotLight(position);

            ImGui::Separator();

            if (ImGui::MenuItem(WithIcon(Icon::Model, "Model from File...").c_str()))
            {
                if (const auto path = ShowOpenDialog(GetActiveWindow(), FileKind::Model, context.paths.assets))
                    document.AddModel(*path, position);
            }

            ImGui::Separator();

            if (ImGui::BeginMenu("Stress Test"))
            {
                for (const uint32_t count : { 16u, 64u, 256u })
                {
                    if (ImGui::MenuItem(std::format("{} Point Lights", count).c_str()))
                        document.SpawnTestLights(count);
                }

                ImGui::EndMenu();
            }

            ImGui::EndMenu();
        }

        void DrawWindowMenu(EditorContext& context, std::span<Panel* const> panels)
        {
            if (!ImGui::BeginMenu("Window"))
                return;

            // Like Unity's Window menu: picking a panel shows and focuses it; tabs close it.
            for (Panel* panel : panels)
            {
                if (ImGui::MenuItem(WithIcon(panel->GetIcon(), panel->GetTitle().c_str()).c_str(), nullptr, panel->IsOpen()))
                    panel->Focus();
            }

            ImGui::Separator();

            if (ImGui::BeginMenu("Layout"))
            {
                if (ImGui::MenuItem("Default"))
                    context.requests.layout = LayoutPreset::Default;

                if (ImGui::MenuItem("Lighting"))
                    context.requests.layout = LayoutPreset::Lighting;

                if (ImGui::MenuItem("Profiling"))
                    context.requests.layout = LayoutPreset::Profiling;

                ImGui::EndMenu();
            }

            if (ImGui::MenuItem("Reset Layout"))
                context.requests.layout = LayoutPreset::Default;

            if (ImGui::MenuItem(WithIcon(Icon::Fullscreen, "Fullscreen Viewport").c_str(), "F11", context.fullscreenViewport))
                context.requests.toggleFullscreen = true;

            ImGui::EndMenu();
        }

        void DrawRenderMenu(EditorContext& context)
        {
            if (!ImGui::BeginMenu("Render"))
                return;

            if (ImGui::BeginMenu(WithIcon(Icon::Image, "View Mode").c_str()))
            {
                int index = 0;

                for (const ViewMode& mode : GetViewModes())
                {
                    const std::string shortcut = ++index <= 9 ? std::format("Alt+{}", index) : std::string();

                    if (ImGui::MenuItem(mode.name, shortcut.c_str(), context.viewport.viewMode == mode.debugView))
                        context.viewport.viewMode = mode.debugView;
                }

                ImGui::EndMenu();
            }

            ImGui::MenuItem(WithIcon(Icon::Wireframe, "Wireframe Overlay").c_str(), nullptr, &context.viewport.wireframe);
            ImGui::Separator();

            if (ImGui::MenuItem(WithIcon(Icon::Refresh, "Recapture Sky Light").c_str()))
                context.renderer.RequestSkyLightRecapture();

            if (ImGui::MenuItem(WithIcon(Icon::RenderGraph, "Dump Frame Graph").c_str()))
                context.renderer.RequestFrameDump();

            ImGui::EndMenu();
        }

        void DrawHelpMenu()
        {
            bool showControls = false;

            if (ImGui::BeginMenu("Help"))
            {
                showControls = ImGui::MenuItem(WithIcon(Icon::Help, "Controls").c_str());
                ImGui::Separator();
                widgets::TextDim("AtmosDX12 editor: Direct3D 12, deferred + clustered");
                ImGui::EndMenu();
            }

            if (showControls)
                ImGui::OpenPopup("Controls");

            ImGui::SetNextWindowSize(ImVec2(ImGui::GetFontSize() * 28.0f, 0.0f), ImGuiCond_Appearing);

            if (!ImGui::BeginPopupModal("Controls", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
                return;

            constexpr const char* rows[][2] = {
                { "Hold RMB", "Fly: mouse look, WASD move, Q/E down/up, Shift faster, wheel speed" },
                { "Alt + LMB drag", "Orbit around the focused point" },
                { "MMB drag", "Pan" },
                { "Wheel", "Dolly toward the focused point" },
                { "F", "Focus the selection" },
                { "Q / W / E / R", "Select / Move / Rotate / Scale tool" },
                { "Ctrl+1..9 / 1..9", "Save / recall a camera bookmark (viewport)" },
                { "Alt+1..9", "View modes" },
                { "Ctrl+Z / Ctrl+Y", "Undo / redo" },
                { "Ctrl+D / Del / F2", "Duplicate / delete / rename" },
                { "Ctrl+S", "Save the scene" },
                { "F11 or F1", "Fullscreen viewport" },
            };

            if (ImGui::BeginTable("Controls", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV))
            {
                for (const auto& row : rows)
                {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted(row[0]);
                    ImGui::TableSetColumnIndex(1);
                    widgets::TextDim("%s", row[1]);
                }

                ImGui::EndTable();
            }

            ImGui::Spacing();

            if (ImGui::Button("Close", ImVec2(-FLT_MIN, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape))
                ImGui::CloseCurrentPopup();

            ImGui::EndPopup();
        }
    }

    void DrawMainMenuBar(EditorContext& context, std::span<Panel* const> panels)
    {
        if (!ImGui::BeginMainMenuBar())
            return;

        DrawFileMenu(context);
        DrawEditMenu(context);
        DrawCreateMenu(context);
        DrawWindowMenu(context, panels);
        DrawRenderMenu(context);
        DrawHelpMenu();

        // Scene name on the right, with a dot while there are unsaved changes.
        const SceneDocument& document = context.document;
        const std::string title = document.GetDisplayName() + (document.IsDirty() ? "  \xE2\x80\xA2" : "");
        const float width = ImGui::CalcTextSize(title.c_str()).x;
        ImGui::SameLine(ImGui::GetWindowWidth() - width - ImGui::GetStyle().WindowPadding.x * 2.0f);
        widgets::TextDim("%s", title.c_str());

        ImGui::EndMainMenuBar();
    }
}
