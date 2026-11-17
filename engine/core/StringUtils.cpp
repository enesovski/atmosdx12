#include "core/StringUtils.h"

#include <windows.h>

namespace atmos
{
    std::string ToUtf8(std::wstring_view text)
    {
        if (text.empty())
            return {};

        const int sourceLength = static_cast<int>(text.size());
        const int length = WideCharToMultiByte(CP_UTF8, 0, text.data(), sourceLength,
                                               nullptr, 0, nullptr, nullptr);

        std::string result(static_cast<size_t>(length), '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.data(), sourceLength,
                            result.data(), length, nullptr, nullptr);

        return result;
    }

    std::wstring ToWide(std::string_view utf8)
    {
        if (utf8.empty())
            return {};

        const int sourceLength = static_cast<int>(utf8.size());
        const int length = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), sourceLength, nullptr, 0);

        std::wstring result(static_cast<size_t>(length), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, utf8.data(), sourceLength, result.data(), length);

        return result;
    }
}
