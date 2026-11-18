#include "core/Log.h"

#include <windows.h>

#include <cstdio>
#include <mutex>
#include <string>

namespace atmos::log
{
    namespace
    {
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
        static std::mutex mutex;
        const std::lock_guard lock(mutex);

        std::string line = GetPrefix(level);
        line.append(message);
        line.push_back('\n');

        // Flush so redirected output is not lost if the process ends abruptly.
        std::FILE* stream = level == Level::Info ? stdout : stderr;
        std::fputs(line.c_str(), stream);
        std::fflush(stream);
        OutputDebugStringA(line.c_str());
    }
}
