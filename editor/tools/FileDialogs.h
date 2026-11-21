#pragma once

#include <windows.h>

#include <filesystem>
#include <optional>

namespace atmos::editor
{
    enum class FileKind
    {
        Scene,  // .json
        Model,  // .gltf / .glb
    };

    // Native Windows file dialogs (modal). Nothing when the user cancels.
    std::optional<std::filesystem::path> ShowOpenDialog(HWND owner, FileKind kind, const std::filesystem::path& folder);
    std::optional<std::filesystem::path> ShowSaveDialog(HWND owner, FileKind kind, const std::filesystem::path& folder,
                                                        const std::wstring& defaultName);

    // Opens Explorer with the file selected (or the folder opened).
    void RevealInExplorer(const std::filesystem::path& path);
}
