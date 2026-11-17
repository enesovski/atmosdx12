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
}
