#pragma once

#include <cstdint>
#include <format>
#include <functional>
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

    // Thread-safe; writes to the console, the debugger output and every registered sink.
    void Write(Level level, std::string_view message);

    // Sinks see every message after it is written (e.g. an editor console). They are called under
    // the log lock from any thread, so they must be quick and must not log themselves.
    using Sink = std::function<void(Level, std::string_view)>;
    using SinkId = uint32_t;

    SinkId AddSink(Sink sink);
    void RemoveSink(SinkId id);

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
