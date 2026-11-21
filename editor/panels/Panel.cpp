#include "panels/Panel.h"

#include <imgui_internal.h>

namespace atmos::editor
{
    Panel::Panel(std::string title, const IconGlyph& icon)
        : title_(std::move(title)), icon_(icon), windowName_(std::string(icon) + "  " + title_ + "###" + title_)
    {
    }

    void Panel::Draw(EditorContext& context)
    {
        if (!open_)
        {
            visible_ = false;
            return;
        }

        if (focusRequested_)
        {
            ImGui::SetNextWindowFocus();
            focusRequested_ = false;
        }

        if (!HasPadding())
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

        // Tabs carry their own close button; the dock node's extra one is redundant.
        ImGuiWindowClass windowClass;
        windowClass.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoCloseButton;
        ImGui::SetNextWindowClass(&windowClass);

        ImGui::SetNextWindowSize(ImVec2(480, 360), ImGuiCond_FirstUseEver);
        visible_ = ImGui::Begin(windowName_.c_str(), &open_, GetWindowFlags());

        if (!HasPadding())
            ImGui::PopStyleVar();

        if (visible_)
            DrawContent(context);

        ImGui::End();
    }
}
