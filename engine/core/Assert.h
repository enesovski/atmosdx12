#pragma once

namespace atmos::detail
{
    [[noreturn]] void AssertFailed(const char* expression, const char* message,
                                   const char* file, int line);
}

// Programmer errors only; environment failures throw exceptions instead.
#if defined(_DEBUG)
#define ATMOS_ASSERT(condition, message)                                                  \
    do                                                                                    \
    {                                                                                     \
        if (!(condition))                                                                 \
            ::atmos::detail::AssertFailed(#condition, message, __FILE__, __LINE__);       \
    } while (false)
#else
#define ATMOS_ASSERT(condition, message) ((void)0)
#endif
