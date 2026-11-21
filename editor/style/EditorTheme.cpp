#include "style/EditorTheme.h"

namespace atmos::editor
{
    namespace
    {
        constexpr ImVec4 Hex(uint32_t rgb, float alpha = 1.0f)
        {
            return ImVec4(((rgb >> 16) & 0xFF) / 255.0f, ((rgb >> 8) & 0xFF) / 255.0f, (rgb & 0xFF) / 255.0f, alpha);
        }

        // Neutral greys like Unreal 5, one blue accent like Unity.
        constexpr ThemeColors DarkColors{
            .background = Hex(0x151515),
            .panel = Hex(0x1F1F1F),
            .panelRaised = Hex(0x262626),
            .field = Hex(0x121212),
            .fieldHover = Hex(0x2B2B2B),
            .border = Hex(0x0B0B0B),
            .text = Hex(0xD9D9D9),
            .textDim = Hex(0x8C8C8C),
            .accent = Hex(0x3B8EEA),
            .accentDim = Hex(0x24476B),
            .warning = Hex(0xE5A50A),
            .error = Hex(0xE5484D),
            .success = Hex(0x46A758),
            .axisX = Hex(0xE5484D),
            .axisY = Hex(0x6BBF59),
            .axisZ = Hex(0x4C8DF6),
        };

        constexpr ThemeColors DarkerColors{
            .background = Hex(0x0C0C0C),
            .panel = Hex(0x161616),
            .panelRaised = Hex(0x1C1C1C),
            .field = Hex(0x0A0A0A),
            .fieldHover = Hex(0x232323),
            .border = Hex(0x050505),
            .text = Hex(0xD0D0D0),
            .textDim = Hex(0x808080),
            .accent = Hex(0x3B8EEA),
            .accentDim = Hex(0x1F3D5C),
            .warning = Hex(0xE5A50A),
            .error = Hex(0xE5484D),
            .success = Hex(0x46A758),
            .axisX = Hex(0xE5484D),
            .axisY = Hex(0x6BBF59),
            .axisZ = Hex(0x4C8DF6),
        };

        const ThemeColors* activeColors = &DarkColors;

        ImVec4 WithAlpha(ImVec4 color, float alpha)
        {
            color.w = alpha;
            return color;
        }

        ImVec4 Lighten(const ImVec4& color, float amount)
        {
            return ImVec4(color.x + amount, color.y + amount, color.z + amount, color.w);
        }

        void ApplyColors(const ThemeColors& c, ImGuiStyle& style)
        {
            ImVec4* colors = style.Colors;

            colors[ImGuiCol_Text] = c.text;
            colors[ImGuiCol_TextDisabled] = c.textDim;
            colors[ImGuiCol_WindowBg] = c.panel;
            colors[ImGuiCol_ChildBg] = WithAlpha(c.panel, 0.0f);
            colors[ImGuiCol_PopupBg] = c.panelRaised;
            colors[ImGuiCol_Border] = c.border;
            colors[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);

            colors[ImGuiCol_FrameBg] = c.field;
            colors[ImGuiCol_FrameBgHovered] = c.fieldHover;
            colors[ImGuiCol_FrameBgActive] = Lighten(c.fieldHover, 0.03f);

            colors[ImGuiCol_TitleBg] = c.panelRaised;
            colors[ImGuiCol_TitleBgActive] = c.panelRaised;
            colors[ImGuiCol_TitleBgCollapsed] = c.panelRaised;
            colors[ImGuiCol_MenuBarBg] = c.panelRaised;

            colors[ImGuiCol_ScrollbarBg] = WithAlpha(c.panel, 0.0f);
            colors[ImGuiCol_ScrollbarGrab] = Lighten(c.panelRaised, 0.10f);
            colors[ImGuiCol_ScrollbarGrabHovered] = Lighten(c.panelRaised, 0.16f);
            colors[ImGuiCol_ScrollbarGrabActive] = Lighten(c.panelRaised, 0.22f);

            colors[ImGuiCol_CheckMark] = c.accent;
            colors[ImGuiCol_SliderGrab] = Lighten(c.panelRaised, 0.25f);
            colors[ImGuiCol_SliderGrabActive] = c.accent;

            colors[ImGuiCol_Button] = Lighten(c.panelRaised, 0.04f);
            colors[ImGuiCol_ButtonHovered] = Lighten(c.panelRaised, 0.10f);
            colors[ImGuiCol_ButtonActive] = c.accentDim;

            colors[ImGuiCol_Header] = c.accentDim;
            colors[ImGuiCol_HeaderHovered] = Lighten(c.panelRaised, 0.06f);
            colors[ImGuiCol_HeaderActive] = c.accentDim;

            colors[ImGuiCol_Separator] = c.border;
            colors[ImGuiCol_SeparatorHovered] = c.accentDim;
            colors[ImGuiCol_SeparatorActive] = c.accent;
            colors[ImGuiCol_ResizeGrip] = ImVec4(0, 0, 0, 0);
            colors[ImGuiCol_ResizeGripHovered] = c.accentDim;
            colors[ImGuiCol_ResizeGripActive] = c.accent;

            // Unreal-style tabs: the active tab is raised, the rest blend into the bar.
            colors[ImGuiCol_Tab] = c.panel;
            colors[ImGuiCol_TabHovered] = Lighten(c.panelRaised, 0.06f);
            colors[ImGuiCol_TabSelected] = c.panelRaised;
            colors[ImGuiCol_TabSelectedOverline] = c.accent;
            colors[ImGuiCol_TabDimmed] = c.panel;
            colors[ImGuiCol_TabDimmedSelected] = c.panelRaised;
            colors[ImGuiCol_TabDimmedSelectedOverline] = WithAlpha(c.accent, 0.0f);

            colors[ImGuiCol_DockingPreview] = WithAlpha(c.accent, 0.5f);
            colors[ImGuiCol_DockingEmptyBg] = c.background;

            colors[ImGuiCol_PlotLines] = c.accent;
            colors[ImGuiCol_PlotLinesHovered] = c.warning;
            colors[ImGuiCol_PlotHistogram] = c.accent;
            colors[ImGuiCol_PlotHistogramHovered] = c.warning;

            colors[ImGuiCol_TableHeaderBg] = c.panelRaised;
            colors[ImGuiCol_TableBorderStrong] = c.border;
            colors[ImGuiCol_TableBorderLight] = Lighten(c.border, 0.06f);
            colors[ImGuiCol_TableRowBg] = ImVec4(0, 0, 0, 0);
            colors[ImGuiCol_TableRowBgAlt] = ImVec4(1, 1, 1, 0.02f);

            colors[ImGuiCol_TextSelectedBg] = c.accentDim;
            colors[ImGuiCol_DragDropTarget] = c.accent;
            colors[ImGuiCol_NavCursor] = c.accent;
            colors[ImGuiCol_NavWindowingHighlight] = c.accent;
            colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0, 0, 0, 0.5f);
            colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0, 0, 0, 0.55f);
        }

        void ApplyMetrics(ImGuiStyle& style)
        {
            style.WindowPadding = ImVec2(8, 8);
            style.FramePadding = ImVec2(6, 4);
            style.CellPadding = ImVec2(6, 3);
            style.ItemSpacing = ImVec2(6, 4);
            style.ItemInnerSpacing = ImVec2(4, 4);
            style.IndentSpacing = 14;
            style.ScrollbarSize = 12;
            style.GrabMinSize = 10;

            style.WindowBorderSize = 1;
            style.ChildBorderSize = 1;
            style.PopupBorderSize = 1;
            style.FrameBorderSize = 0;
            style.TabBorderSize = 0;
            style.TabBarBorderSize = 1;
            style.TabBarOverlineSize = 2;
            style.DockingSeparatorSize = 2;

            style.WindowRounding = 4;
            style.ChildRounding = 4;
            style.FrameRounding = 3;
            style.PopupRounding = 4;
            style.ScrollbarRounding = 6;
            style.GrabRounding = 2;
            style.TabRounding = 3;

            style.WindowTitleAlign = ImVec2(0.0f, 0.5f);
            style.WindowMenuButtonPosition = ImGuiDir_None;
            style.SeparatorTextBorderSize = 1;
            style.SeparatorTextPadding = ImVec2(0, 3);
        }
    }

    const ThemeColors& GetThemeColors()
    {
        return *activeColors;
    }

    void ApplyTheme(Theme theme, float scale)
    {
        activeColors = theme == Theme::Darker ? &DarkerColors : &DarkColors;

        ImGuiStyle& style = ImGui::GetStyle();
        style = ImGuiStyle();
        ApplyMetrics(style);
        ApplyColors(*activeColors, style);
        style.ScaleAllSizes(scale);
    }

    ImU32 ToU32(const ImVec4& color, float alphaScale)
    {
        return ImGui::ColorConvertFloat4ToU32(ImVec4(color.x, color.y, color.z, color.w * alphaScale));
    }
}
