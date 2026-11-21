#include "core/Log.h"

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <mutex>
#include <string>
#include <vector>

namespace atmos::log
{
    namespace
    {
        struct SinkEntry
        {
            SinkId id;
            Sink sink;
        };

        struct LogState
        {
            std::mutex mutex;
            std::vector<SinkEntry> sinks;
            SinkId nextSinkId = 1;
        };

        LogState& GetState()
        {
            static LogState state;
            return state;
        }

        const char* GetPrefix(Level level)
        {
            switch (level)
            {
                case Level::Warning: return "[warn]  ";
                case Level::Error:   return "[error] ";
                default:             return "[info]  ";
            }
        }
    }

    void Write(Level level, std::string_view message)
    {
        LogState& state = GetState();
        const std::lock_guard lock(state.mutex);

        std::string line = GetPrefix(level);
        line.append(message);
        line.push_back('\n');

        // Flush so redirected output is not lost if the process ends abruptly.
        std::FILE* stream = level == Level::Info ? stdout : stderr;
        std::fputs(line.c_str(), stream);
        std::fflush(stream);
        OutputDebugStringA(line.c_str());

        for (const SinkEntry& entry : state.sinks)
            entry.sink(level, message);
    }

    SinkId AddSink(Sink sink)
    {
        LogState& state = GetState();
        const std::lock_guard lock(state.mutex);

        const SinkId id = state.nextSinkId++;
        state.sinks.push_back({ id, std::move(sink) });
        return id;
    }

    void RemoveSink(SinkId id)
    {
        LogState& state = GetState();
        const std::lock_guard lock(state.mutex);
        std::erase_if(state.sinks, [id](const SinkEntry& entry) { return entry.id == id; });
    }
}
