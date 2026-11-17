#pragma once

#include "core/Build.h"

namespace atmos
{
    struct GraphicsDeviceOptions
    {
        bool enableDebugLayer = IsDebugBuild;
        bool enableGpuValidation = false;   // Slow; implies the debug layer.
        bool enableDred = IsDebugBuild;     // Breadcrumbs + page faults on device removal.
    };
}
