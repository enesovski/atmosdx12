#pragma once

#include "scene/RenderSettings.h"

#include <span>

namespace atmos::editor
{
    // One entry of the viewport's view-mode menu (Unreal's view modes). Later phases register their
    // modes here, so the menu only lists what the renderer can actually show. Alt+1..9 picks the Nth.
    struct ViewMode
    {
        const char* name;
        const char* group;  // menu section
        DebugView debugView;
    };

    std::span<const ViewMode> GetViewModes();
    const ViewMode& GetViewMode(DebugView debugView);
}
