#include "panels/PreferencesWindow.h"
#include "EditorContext.h"
#include "style/Icons.h"
#include "widgets/PropertyGrid.h"
#include "widgets/Widgets.h"

#include <imgui.h>

#include <string>

namespace atmos::editor
{
    void PreferencesWindow::Draw(EditorContext& context)
    {
        if (!open_)
            return;

        EditorPrefs& prefs = context.prefs;
        const EditorPrefs defaults;
        bool changed = false;

        ImGui::SetNextWindowSize(ImVec2(ImGui::GetFontSize() * 30.0f, ImGui::GetFontSize() * 28.0f), ImGuiCond_FirstUseEver);
        const std::string title = std::string(Icon::Settings) + "  Preferences###Preferences";

        if (ImGui::Begin(title.c_str(), &open_, ImGuiWindowFlags_NoDocking))
        {
            if (widgets::SectionHeader("Appearance", Icon::Image).open)
            {
                PropertyGrid grid("Appearance");
                float scalePercent = prefs.uiScale * 100.0f;

                if (grid.Slider("UI scale", scalePercent, 100.0f, 75.0f, 200.0f, "%.0f%%", false,
                                "Multiplies the monitor's own scaling"))
                {
                    prefs.uiScale = scalePercent / 100.0f;
                    changed = true;
                }

                changed |= grid.Slider("Font size", prefs.fontSize, defaults.fontSize, 13.0f, 18.0f, "%.0f px");

                constexpr const char* themes[] = { "Dark", "Darker" };
                auto theme = static_cast<uint32_t>(prefs.theme);

                if (grid.Enum("Theme", theme, themes, static_cast<uint32_t>(defaults.theme)))
                {
                    prefs.theme = static_cast<Theme>(theme);
                    changed = true;
                }
            }

            if (widgets::SectionHeader("Camera", Icon::Camera).open)
            {
                PropertyGrid grid("Camera");
                changed |= grid.Slider("Look sensitivity", prefs.lookSensitivity, defaults.lookSensitivity, 0.02f, 0.5f,
                                       "%.2f deg/count");
                changed |= grid.Slider("Default move speed", prefs.defaultMoveSpeed, defaults.defaultMoveSpeed, 0.5f,
                                       50.0f, "%.1f m/s", true);
                changed |= grid.Bool("Invert Y", prefs.invertY, defaults.invertY);
            }

            if (widgets::SectionHeader("Viewport", Icon::Viewport).open)
            {
                PropertyGrid grid("Viewport");
                DirectX::XMFLOAT3 outline(prefs.outlineColor[0], prefs.outlineColor[1], prefs.outlineColor[2]);
                const DirectX::XMFLOAT3 defaultOutline(defaults.outlineColor[0], defaults.outlineColor[1],
                                                       defaults.outlineColor[2]);

                if (grid.Color("Selection outline", outline, defaultOutline))
                {
                    prefs.outlineColor = { outline.x, outline.y, outline.z, 1.0f };
                    changed = true;
                }

                changed |= grid.Bool("Show stats", prefs.showStatsByDefault, defaults.showStatsByDefault,
                                     "Stats overlay on new viewports");
            }

            if (widgets::SectionHeader("Files", Icon::Save).open)
            {
                PropertyGrid grid("Files");
                auto minutes = static_cast<uint32_t>(prefs.autosaveMinutes);

                if (grid.UInt("Autosave (minutes)", minutes, 0, 0, 30,
                              "Writes <scene>.autosave.json while there are unsaved changes; 0 = off"))
                {
                    prefs.autosaveMinutes = static_cast<int>(minutes);
                    changed = true;
                }
            }
        }

        ImGui::End();

        // Applied live; written to disk once the drag or edit is finished.
        if (changed)
        {
            context.cameraController.lookSensitivity = prefs.lookSensitivity;
            context.cameraController.invertY = prefs.invertY;
            dirty_ = true;
        }

        if (dirty_ && !ImGui::IsAnyItemActive())
        {
            prefs.Save(context.paths.prefsFile);
            dirty_ = false;
        }
    }
}
