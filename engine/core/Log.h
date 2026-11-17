#pragma once

#include <format>
#include <string_view>
#include <utility>

namespace atmos::log
{
    enum class Level
    {
        Info,
        Warning,
        Error
    };

    // Thread-safe; writes to the console and the debugger output.
    void Write(Level level, std::string_view message);

    template <typename... Args>
    void Info(std::format_string<Args...> format, Args&&... args)
    {
        Write(Level::Info, std::format(format, std::forward<Args>(args)...));
    }

    template <typename... Args>
    void Warning(std::format_string<Args...> format, Args&&... args)
    {
        Write(Level::Warning, std::format(format, std::forward<Args>(args)...));
    }

    template <typename... Args>
    void Error(std::format_string<Args...> format, Args&&... args)
    {
        Write(Level::Error, std::format(format, std::forward<Args>(args)...));
    }
}
