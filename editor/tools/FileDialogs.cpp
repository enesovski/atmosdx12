#include "tools/FileDialogs.h"

#include <commdlg.h>
#include <shellapi.h>

#include <string>

namespace atmos::editor
{
    namespace
    {
        // Filter strings are pairs of NUL-terminated strings ending in a double NUL.
        const wchar_t* GetFilter(FileKind kind)
        {
            return kind == FileKind::Scene ? L"Scenes (*.json)\0*.json\0All files\0*.*\0"
                                           : L"glTF models (*.gltf;*.glb)\0*.gltf;*.glb\0All files\0*.*\0";
        }

        OPENFILENAMEW MakeDialog(HWND owner, FileKind kind, const std::wstring& folder, wchar_t* buffer, DWORD size)
        {
            OPENFILENAMEW dialog{};
            dialog.lStructSize = sizeof(dialog);
            dialog.hwndOwner = owner;
            dialog.lpstrFilter = GetFilter(kind);
            dialog.lpstrFile = buffer;
            dialog.nMaxFile = size;
            dialog.lpstrInitialDir = folder.empty() ? nullptr : folder.c_str();
            dialog.lpstrDefExt = kind == FileKind::Scene ? L"json" : L"gltf";
            return dialog;
        }
    }

    std::optional<std::filesystem::path> ShowOpenDialog(HWND owner, FileKind kind, const std::filesystem::path& folder)
    {
        wchar_t buffer[MAX_PATH]{};
        const std::wstring initialFolder = folder.wstring();
        OPENFILENAMEW dialog = MakeDialog(owner, kind, initialFolder, buffer, MAX_PATH);
        dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

        if (!GetOpenFileNameW(&dialog))
            return std::nullopt;

        return std::filesystem::path(buffer);
    }

    std::optional<std::filesystem::path> ShowSaveDialog(HWND owner, FileKind kind, const std::filesystem::path& folder,
                                                        const std::wstring& defaultName)
    {
        wchar_t buffer[MAX_PATH]{};
        defaultName.copy(buffer, MAX_PATH - 1);
        const std::wstring initialFolder = folder.wstring();
        OPENFILENAMEW dialog = MakeDialog(owner, kind, initialFolder, buffer, MAX_PATH);
        dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

        if (!GetSaveFileNameW(&dialog))
            return std::nullopt;

        return std::filesystem::path(buffer);
    }

    void RevealInExplorer(const std::filesystem::path& path)
    {
        if (std::filesystem::is_directory(path))
        {
            ShellExecuteW(nullptr, L"explore", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            return;
        }

        const std::wstring arguments = L"/select,\"" + path.wstring() + L"\"";
        ShellExecuteW(nullptr, L"open", L"explorer.exe", arguments.c_str(), nullptr, SW_SHOWNORMAL);
    }
}
