#pragma once

#include <imgui.h>

#include <cstdint>

namespace atmos::editor
{
    enum class Theme : uint32_t
    {
        Dark,
        Darker,
    };

    // Design tokens: every panel takes colors from here, never raw values (docs/phase-4-plan.md).
    struct ThemeColors
    {
        ImVec4 background;   // dock space, behind panels
        ImVec4 panel;        // window background
        ImVec4 panelRaised;  // headers, toolbars, menu bar, active tab
        ImVec4 field;        // inputs, sliders, combos
        ImVec4 fieldHover;
        ImVec4 border;
        ImVec4 text;
        ImVec4 textDim;      // property labels, units, disabled
        ImVec4 accent;       // selection, focus, active tab line, checks
        ImVec4 accentDim;    // selected rows
        ImVec4 warning;      // warnings, modified values
        ImVec4 error;
        ImVec4 success;
        ImVec4 axisX;
        ImVec4 axisY;
        ImVec4 axisZ;
    };

    const ThemeColors& GetThemeColors();

    // Rewrites ImGuiStyle from the tokens and metrics, scaled for the UI scale.
    void ApplyTheme(Theme theme, float scale);

    ImU32 ToU32(const ImVec4& color, float alphaScale = 1.0f);
}
