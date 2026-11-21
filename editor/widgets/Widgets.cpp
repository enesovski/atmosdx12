#include "widgets/Widgets.h"
#include "style/EditorFonts.h"
#include "style/EditorTheme.h"

#include <algorithm>
#include <cstdarg>
#include <cstring>
#include <string>

namespace atmos::editor::widgets
{
    bool IconButton(const char* id, const IconGlyph& icon, const char* tooltip, bool selected)
    {
        const ThemeColors& colors = GetThemeColors();
        const float size = ImGui::GetFrameHeight();

        ImGui::PushID(id);

        if (selected)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, colors.accentDim);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, colors.accentDim);
        }

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
        const bool pressed = ImGui::Button(icon, ImVec2(size, size));
        ImGui::PopStyleVar();

        if (selected)
            ImGui::PopStyleColor(2);

        if (tooltip)
            Tooltip(tooltip);

        ImGui::PopID();
        return pressed;
    }

    void DropdownButton(const char* label, const IconGlyph& icon, const char* popupId, const char* tooltip)
    {
        const std::string text = std::string(icon) + "  " + label + "  " + std::string(Icon::ChevronDown);

        if (ImGui::Button(text.c_str()))
            ImGui::OpenPopup(popupId);

        if (tooltip)
            Tooltip(tooltip);
    }

    SectionState SectionHeader(const char* label, const IconGlyph& icon, bool* enabled, const char* contextMenu,
                               bool defaultOpen)
    {
        const ThemeColors& colors = GetThemeColors();
        SectionState state;

        ImGui::PushStyleColor(ImGuiCol_Header, colors.panelRaised);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(colors.panelRaised.x + 0.05f, colors.panelRaised.y + 0.05f,
                                                              colors.panelRaised.z + 0.05f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, colors.panelRaised);

        const std::string text = std::string(icon) + "  " + label + "###" + label;
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowOverlap |
                                   ImGuiTreeNodeFlags_FramePadding;

        if (defaultOpen)
            flags |= ImGuiTreeNodeFlags_DefaultOpen;

        {
            const ScopedFont bold(FontRole::Bold);
            state.open = ImGui::CollapsingHeader(text.c_str(), flags);
        }

        if (contextMenu)
            ImGui::OpenPopupOnItemClick(contextMenu, ImGuiPopupFlags_MouseButtonRight);

        ImGui::PopStyleColor(3);

        if (enabled)
        {
            const float checkboxWidth = ImGui::GetFrameHeight();
            ImGui::SameLine(ImGui::GetContentRegionMax().x - checkboxWidth);
            ImGui::PushID(label);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x, 2.0f));
            state.toggled = ImGui::Checkbox("##enabled", enabled);
            ImGui::PopStyleVar();
            Tooltip(*enabled ? "Enabled" : "Disabled");
            ImGui::PopID();
        }

        return state;
    }

    bool InputString(const char* id, std::string& value, float width, bool focusNow)
    {
        // One field is edited at a time, so one buffer is enough. It is refilled from `value` only
        // while its field is inactive, so the edit survives until it is committed.
        static char buffer[256];
        static ImGuiID editingId = 0;

        const ImGuiID itemId = ImGui::GetID(id);

        if (editingId != itemId)
        {
            const size_t length = std::min(value.size(), sizeof(buffer) - 1);
            value.copy(buffer, length);
            buffer[length] = '\0';
        }

        if (focusNow)
            ImGui::SetKeyboardFocusHere();

        ImGui::SetNextItemWidth(width);
        ImGui::InputText(id, buffer, sizeof(buffer), ImGuiInputTextFlags_AutoSelectAll);

        if (ImGui::IsItemActive())
            editingId = itemId;
        else if (editingId == itemId)
            editingId = 0;

        if (ImGui::IsItemDeactivatedAfterEdit() && buffer[0] != '\0' && value != buffer)
        {
            value = buffer;
            return true;
        }

        return false;
    }

    bool SearchField(const char* id, char* buffer, size_t bufferSize, const char* hint)
    {
        const ThemeColors& colors = GetThemeColors();
        const float clearWidth = buffer[0] ? ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.x : 0.0f;
        const std::string hintText = std::string(Icon::Search) + "  " + hint;

        ImGui::PushID(id);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - clearWidth);
        bool changed = ImGui::InputTextWithHint("##search", hintText.c_str(), buffer, bufferSize);

        if (buffer[0])
        {
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, colors.textDim);

            if (IconButton("clear", Icon::Close, "Clear search"))
            {
                buffer[0] = '\0';
                changed = true;
            }

            ImGui::PopStyleColor();
        }

        ImGui::PopID();
        return changed;
    }

    void TextDim(const char* format, ...)
    {
        va_list args;
        va_start(args, format);
        ImGui::PushStyleColor(ImGuiCol_Text, GetThemeColors().textDim);
        ImGui::TextV(format, args);
        ImGui::PopStyleColor();
        va_end(args);
    }

    void EmptyState(const IconGlyph& icon, const char* message)
    {
        const ImVec2 available = ImGui::GetContentRegionAvail();
        ImGui::PushStyleColor(ImGuiCol_Text, GetThemeColors().textDim);

        {
            const ScopedFont large(FontRole::Icons);
            const ImVec2 iconSize = ImGui::CalcTextSize(icon);
            ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + (available.x - iconSize.x) * 0.5f,
                                       ImGui::GetCursorPosY() + available.y * 0.3f));
            ImGui::TextUnformatted(icon);
        }

        const ImVec2 textSize = ImGui::CalcTextSize(message);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (available.x - textSize.x) * 0.5f);
        ImGui::TextUnformatted(message);
        ImGui::PopStyleColor();
    }

    void Tooltip(const char* text)
    {
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("%s", text);
    }
}
