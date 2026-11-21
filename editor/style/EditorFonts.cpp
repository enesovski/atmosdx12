#include "style/EditorFonts.h"
#include "style/Icons.h"

#include "core/Log.h"
#include "core/StringUtils.h"

#include <windows.h>

#include <array>
#include <cmath>
#include <filesystem>
#include <initializer_list>
#include <string>
#include <vector>

namespace atmos::editor
{
    namespace
    {
        std::array<ImFont*, static_cast<size_t>(FontRole::Count)> fonts{};
        std::vector<ImWchar> iconRanges;

        // Latin-1 plus the punctuation, arrows and symbols the UI uses (dashes, ellipsis, degree, x).
        constexpr ImWchar TextRanges[] = { 0x0020, 0x00FF, 0x2010, 0x2030, 0x2190, 0x21FF, 0x2212, 0x2212, 0 };

        std::filesystem::path GetSystemFontDirectory()
        {
            wchar_t windows[MAX_PATH]{};
            GetWindowsDirectoryW(windows, MAX_PATH);
            return std::filesystem::path(windows) / "Fonts";
        }

        // The first file that exists, or an empty path.
        std::string FindFont(std::initializer_list<const wchar_t*> candidates)
        {
            const std::filesystem::path directory = GetSystemFontDirectory();

            for (const wchar_t* name : candidates)
            {
                if (std::filesystem::exists(directory / name))
                    return ToUtf8((directory / name).wstring());
            }

            return {};
        }

        const std::vector<ImWchar>& GetIconRanges()
        {
            if (iconRanges.empty())
            {
                for (uint16_t codepoint : Icon::Codepoints)
                {
                    iconRanges.push_back(codepoint);
                    iconRanges.push_back(codepoint);
                }

                iconRanges.push_back(0);
            }

            return iconRanges;
        }

        ImFont* AddFont(const std::string& file, float size, const std::string& iconFile, float iconScale)
        {
            ImGuiIO& io = ImGui::GetIO();

            ImFontConfig config;
            config.OversampleH = 2;
            config.OversampleV = 1;
            config.PixelSnapH = false;

            ImFont* font = file.empty() ? nullptr : io.Fonts->AddFontFromFileTTF(file.c_str(), size, &config, TextRanges);

            if (!font)
            {
                ImFontConfig fallback;
                fallback.SizePixels = size;
                font = io.Fonts->AddFontDefault(&fallback);
            }

            if (!iconFile.empty())
            {
                // Icons are monospaced and centered on the text's x-height.
                const float iconSize = size * iconScale;
                ImFontConfig icons;
                icons.MergeMode = true;
                icons.PixelSnapH = true;
                icons.GlyphMinAdvanceX = iconSize;
                icons.GlyphOffset = ImVec2(0.0f, (size - iconSize) * 0.5f + size * 0.04f);
                io.Fonts->AddFontFromFileTTF(iconFile.c_str(), iconSize, &icons, GetIconRanges().data());
            }

            return font;
        }
    }

    void BuildFonts(float scale, float baseSize)
    {
        const std::string regular = FindFont({ L"segoeui.ttf" });
        const std::string bold = FindFont({ L"seguisb.ttf", L"segoeuib.ttf", L"segoeui.ttf" });
        const std::string mono = FindFont({ L"CascadiaMono.ttf", L"consola.ttf" });
        const std::string icons = FindFont({ L"SegoeIcons.ttf", L"segmdl2.ttf" });

        if (regular.empty() || icons.empty())
            log::Warning("Editor: Segoe UI or Segoe icon font not found; using the built-in font");

        ImGuiIO& io = ImGui::GetIO();
        io.Fonts->Clear();

        const float size = std::round(baseSize * scale);
        fonts[static_cast<size_t>(FontRole::Regular)] = AddFont(regular, size, icons, 0.95f);
        fonts[static_cast<size_t>(FontRole::Bold)] = AddFont(bold, size, icons, 0.95f);
        fonts[static_cast<size_t>(FontRole::Small)] = AddFont(regular, std::round(size * 0.87f), icons, 0.95f);
        fonts[static_cast<size_t>(FontRole::Mono)] = AddFont(mono, std::round(size * 0.93f), icons, 0.95f);
        fonts[static_cast<size_t>(FontRole::Icons)] = AddFont(regular, std::round(size * 1.6f), icons, 1.0f);

        io.FontDefault = fonts[static_cast<size_t>(FontRole::Regular)];
        io.Fonts->Build();
    }

    ImFont* GetFont(FontRole role)
    {
        return fonts[static_cast<size_t>(role)];
    }
}
