#pragma once

#include <cstdint>

// Icon glyphs from Segoe Fluent Icons (Windows 11), merged into every editor font. Segoe MDL2 Assets
// (Windows 10) shares most code points. Use as "%s" arguments or with std::string concatenation.
#define ATMOS_EDITOR_ICONS(X)          \
    X(Menu, 0xE700)                    \
    X(ChevronDown, 0xE70D)             \
    X(ChevronRight, 0xE76C)            \
    X(Add, 0xE710)                     \
    X(Close, 0xE711)                   \
    X(More, 0xE712)                    \
    X(Settings, 0xE713)                \
    X(Filter, 0xE71C)                  \
    X(Search, 0xE721)                  \
    X(Camera, 0xE722)                  \
    X(Refresh, 0xE72C)                 \
    X(Check, 0xE73E)                   \
    X(Delete, 0xE74D)                  \
    X(Save, 0xE74E)                    \
    X(Undo, 0xE7A7)                    \
    X(Redo, 0xE7A6)                    \
    X(Eye, 0xE7B3)                     \
    X(EyeOff, 0xED1A)                  \
    X(Select, 0xE8B0)                  \
    X(Move, 0xE7C2)                    \
    X(Rotate, 0xE7AD)                  \
    X(Scale, 0xE740)                   \
    X(Fullscreen, 0xE740)              \
    X(Folder, 0xE8B7)                  \
    X(FolderOpen, 0xE838)              \
    X(File, 0xE8A5)                    \
    X(OpenFile, 0xE8E5)                \
    X(SceneFile, 0xF156)               \
    X(Model, 0xF158)                   \
    X(PointLight, 0xE82F)              \
    X(SpotLight, 0xF157)               \
    X(Sun, 0xE706)                     \
    X(Exposure, 0xE793)                \
    X(Entity, 0xF16B)                  \
    X(Hierarchy, 0xEA40)               \
    X(Inspector, 0xE946)               \
    X(RenderSettings, 0xF8A6)          \
    X(Profiler, 0xEC4A)                \
    X(RenderGraph, 0xF0B9)             \
    X(Console, 0xE756)                 \
    X(Viewport, 0xE7F4)                \
    X(Info, 0xE946)                    \
    X(Warning, 0xE7BA)                 \
    X(Error, 0xE783)                   \
    X(Copy, 0xE8C8)                    \
    X(Rename, 0xE8AC)                  \
    X(Lock, 0xE72E)                    \
    X(Unlock, 0xE785)                  \
    X(Bookmark, 0xE734)                \
    X(Grid, 0xE8A9)                    \
    X(Gpu, 0xE950)                     \
    X(Image, 0xE91B)                   \
    X(Wireframe, 0xE809)               \
    X(Help, 0xE9CE)                    \
    X(Pause, 0xE769)                   \
    X(Play, 0xE768)                    \
    X(Globe, 0xE909)                   \
    X(Shadow, 0xE793)

namespace atmos::editor
{
    // A three-byte UTF-8 encoding of a code point in the Private Use Area.
    struct IconGlyph
    {
        char utf8[4];
        uint16_t codepoint;

        constexpr explicit IconGlyph(uint16_t code)
            : utf8{ static_cast<char>(0xE0 | (code >> 12)), static_cast<char>(0x80 | ((code >> 6) & 0x3F)),
                    static_cast<char>(0x80 | (code & 0x3F)), 0 },
              codepoint(code)
        {
        }

        constexpr operator const char*() const { return utf8; }
    };

    namespace Icon
    {
#define ATMOS_DECLARE_ICON(name, code) inline constexpr IconGlyph name{ code };
        ATMOS_EDITOR_ICONS(ATMOS_DECLARE_ICON)
#undef ATMOS_DECLARE_ICON

        // Every code point above, for the font atlas.
        inline constexpr uint16_t Codepoints[] = {
#define ATMOS_ICON_CODEPOINT(name, code) code,
            ATMOS_EDITOR_ICONS(ATMOS_ICON_CODEPOINT)
#undef ATMOS_ICON_CODEPOINT
        };
    }
}
