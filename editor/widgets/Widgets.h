#pragma once

#include "style/Icons.h"

#include <imgui.h>

#include <cstddef>
#include <string>

namespace atmos::editor::widgets
{
    // Square icon-only button for toolbars, with a tooltip; `selected` shows it pressed.
    bool IconButton(const char* id, const IconGlyph& icon, const char* tooltip, bool selected = false);

    // Icon + text button that looks like a dropdown ("Lit v"); opens `popupId` when clicked.
    void DropdownButton(const char* label, const IconGlyph& icon, const char* popupId, const char* tooltip = nullptr);

    struct SectionState
    {
        bool open = false;
        bool toggled = false;  // the enable checkbox changed
    };

    // Full-width collapsible header (Unity component / Unreal category), with an optional enable
    // checkbox on the right. Right-clicking the header opens `contextMenu` (use ImGui::BeginPopup).
    SectionState SectionHeader(const char* label, const IconGlyph& icon, bool* enabled = nullptr,
                               const char* contextMenu = nullptr, bool defaultOpen = true);

    // Single-line text edit that applies on Enter or focus loss, so typing is not one undo step per key.
    // Returns true when `value` changed. Empty input is rejected.
    bool InputString(const char* id, std::string& value, float width, bool focusNow = false);

    // Text field with a search icon and a clear button. Returns true when the text changed.
    bool SearchField(const char* id, char* buffer, size_t bufferSize, const char* hint);

    // Dim text for secondary information.
    void TextDim(const char* format, ...);

    // Centered dim message for empty panels ("Nothing selected").
    void EmptyState(const IconGlyph& icon, const char* message);

    // Tooltip on the last item after a short hover delay.
    void Tooltip(const char* text);
}
