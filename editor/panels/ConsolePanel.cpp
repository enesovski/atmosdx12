#include "panels/ConsolePanel.h"
#include "EditorContext.h"
#include "style/EditorFonts.h"
#include "style/EditorTheme.h"
#include "widgets/Widgets.h"

#include <algorithm>
#include <cctype>
#include <format>
#include <string_view>
#include <vector>

namespace atmos::editor
{
    namespace
    {
        bool ContainsIgnoreCase(std::string_view text, std::string_view pattern)
        {
            const auto found = std::ranges::search(text, pattern, [](char a, char b) {
                return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
            });
            return !found.empty();
        }

        // A toggle that shows an icon and a count ("! 3"), like Unity's console filters.
        void LevelToggle(const char* id, const IconGlyph& icon, size_t count, bool& enabled, const ImVec4& color)
        {
            const ThemeColors& colors = GetThemeColors();
            const std::string label = std::format("{} {}##{}", std::string(icon), count, id);

            ImGui::PushStyleColor(ImGuiCol_Button, enabled ? colors.accentDim : colors.panelRaised);
            ImGui::PushStyleColor(ImGuiCol_Text, enabled ? color : colors.textDim);

            if (ImGui::Button(label.c_str()))
                enabled = !enabled;

            ImGui::PopStyleColor(2);
        }
    }

    ConsolePanel::ConsolePanel() : Panel("Console", Icon::Console)
    {
    }

    void ConsolePanel::DrawContent(EditorContext& context)
    {
        ConsoleLog& console = context.console;
        const ThemeColors& colors = GetThemeColors();
        console.MarkSeen();

        if (widgets::IconButton("clear", Icon::Delete, "Clear"))
            console.Clear();

        ImGui::SameLine();
        LevelToggle("info", Icon::Info, console.GetCount(log::Level::Info), showInfo_, colors.text);
        ImGui::SameLine();
        LevelToggle("warn", Icon::Warning, console.GetCount(log::Level::Warning), showWarnings_, colors.warning);
        ImGui::SameLine();
        LevelToggle("error", Icon::Error, console.GetCount(log::Level::Error), showErrors_, colors.error);
        ImGui::SameLine();

        if (widgets::IconButton("scroll", Icon::ChevronDown, "Auto-scroll to new messages", autoScroll_))
            autoScroll_ = !autoScroll_;

        ImGui::SameLine();
        widgets::SearchField("filter", filter_, sizeof(filter_), "Filter messages");
        ImGui::Separator();

        std::vector<const ConsoleEntry*> visible;

        for (const ConsoleEntry& entry : console.GetEntries())
        {
            const bool levelShown = (entry.level == log::Level::Info && showInfo_) ||
                                    (entry.level == log::Level::Warning && showWarnings_) ||
                                    (entry.level == log::Level::Error && showErrors_);

            if (levelShown && (!filter_[0] || ContainsIgnoreCase(entry.message, filter_)))
                visible.push_back(&entry);
        }

        ImGui::BeginChild("Messages", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);
        ImGui::PushFont(GetFont(FontRole::Mono));

        if (visible.empty())
            widgets::TextDim(console.GetEntries().empty() ? "No messages." : "No messages match the filter.");

        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(visible.size()));

        while (clipper.Step())
        {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
            {
                const ConsoleEntry& entry = *visible[i];
                const bool isDebugLayer = entry.message.starts_with("D3D12:");
                const ImVec4 color = entry.level == log::Level::Error   ? colors.error
                                   : entry.level == log::Level::Warning ? colors.warning
                                   : isDebugLayer                       ? colors.accent
                                                                        : colors.text;

                ImGui::PushID(i);
                widgets::TextDim("%8.2f", entry.time);
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Text, color);
                ImGui::TextUnformatted(entry.message.c_str());
                ImGui::PopStyleColor();

                if (ImGui::BeginPopupContextItem("LineMenu"))
                {
                    if (ImGui::MenuItem("Copy"))
                        ImGui::SetClipboardText(entry.message.c_str());

                    ImGui::EndPopup();
                }

                ImGui::PopID();
            }
        }

        // Stick to the bottom only while the user has not scrolled up.
        if (autoScroll_ && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - ImGui::GetTextLineHeight())
            ImGui::SetScrollHereY(1.0f);

        ImGui::PopFont();
        ImGui::EndChild();
    }
}
