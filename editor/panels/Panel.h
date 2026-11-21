#pragma once

#include "style/Icons.h"

#include <imgui.h>

#include <string>

namespace atmos::editor
{
    struct EditorContext;

    // A dockable editor window. The window id is stable ("Title###Title"), so the saved layout
    // survives icon or label changes; closing only hides it (Window menu reopens it).
    class Panel
    {
    public:
        Panel(std::string title, const IconGlyph& icon);
        virtual ~Panel() = default;

        Panel(const Panel&) = delete;
        Panel& operator=(const Panel&) = delete;

        void Draw(EditorContext& context);

        const std::string& GetTitle() const { return title_; }
        const IconGlyph& GetIcon() const { return icon_; }
        bool IsOpen() const { return open_; }
        void SetOpen(bool open) { open_ = open; }

        // Brings the panel to front on the next frame (e.g. the console after an error).
        void Focus() { open_ = true; focusRequested_ = true; }

    protected:
        virtual void DrawContent(EditorContext& context) = 0;
        virtual ImGuiWindowFlags GetWindowFlags() const { return ImGuiWindowFlags_None; }
        virtual bool HasPadding() const { return true; }

        // True while the panel's window is visible (docked tab selected or floating).
        bool IsVisible() const { return visible_; }

    private:
        std::string title_;
        IconGlyph icon_;
        std::string windowName_;
        bool open_ = true;
        bool visible_ = false;
        bool focusRequested_ = false;
    };
}
