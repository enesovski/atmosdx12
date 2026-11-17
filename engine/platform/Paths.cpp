#include "platform/Paths.h"

#include <windows.h>

#include <stdexcept>
#include <string>

namespace atmos
{
    std::filesystem::path GetExecutableDirectory()
    {
        std::wstring path(MAX_PATH, L'\0');

        while (true)
        {
            const DWORD length = GetModuleFileNameW(nullptr, path.data(),
                                                    static_cast<DWORD>(path.size()));
            if (length == 0)
                throw std::runtime_error("Failed to get executable path");

            if (length < path.size())
            {
                path.resize(length);
                return std::filesystem::path(path).parent_path();
            }

            path.resize(path.size() * 2);
        }
    }
}
