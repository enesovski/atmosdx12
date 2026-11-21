#pragma once

#include <imgui.h>

namespace atmos::editor
{
    enum class FontRole
    {
        Regular,  // labels and values
        Bold,     // section headers, selected items
        Small,    // status bar, units, hints
        Mono,     // console, render graph, numeric tables
        Icons,    // large icons (viewport light icons, empty states)
        Count,
    };

    // Builds the font atlas at `scale` (monitor DPI x user scale) and `baseSize` pixels at 100%.
    // Uses the Windows UI fonts (Segoe UI, Cascadia Mono, Segoe Fluent Icons) with ImGui's built-in
    // font as the fallback. Call before the first frame, or with the GPU idle and the backend's
    // device objects invalidated.
    void BuildFonts(float scale, float baseSize);

    ImFont* GetFont(FontRole role);

    // Pushes a font for the lifetime of the object.
    class ScopedFont
    {
    public:
        explicit ScopedFont(FontRole role) { ImGui::PushFont(GetFont(role)); }
        ~ScopedFont() { ImGui::PopFont(); }

        ScopedFont(const ScopedFont&) = delete;
        ScopedFont& operator=(const ScopedFont&) = delete;
    };
}
