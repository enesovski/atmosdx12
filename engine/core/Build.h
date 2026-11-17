#pragma once

namespace atmos
{
#if defined(_DEBUG)
    inline constexpr bool IsDebugBuild = true;
#else
    inline constexpr bool IsDebugBuild = false;
#endif
}
