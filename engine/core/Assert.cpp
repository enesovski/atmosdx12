#include "core/Assert.h"
#include "core/Log.h"

#include <windows.h>

#include <cstdlib>

namespace atmos::detail
{
    void AssertFailed(const char* expression, const char* message, const char* file, int line)
    {
        log::Error("Assertion failed: {} ({}) at {}:{}", message, expression, file, line);

        if (IsDebuggerPresent())
            __debugbreak();

        std::abort();
    }
}
