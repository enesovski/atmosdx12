#include "ViewModes.h"

namespace atmos::editor
{
    namespace
    {
        constexpr ViewMode Modes[] = {
            { "Lit", "Lighting", DebugView::Lit },
            { "Lighting Only", "Lighting", DebugView::LightingOnly },
            { "Base Color", "Buffer Visualization", DebugView::BaseColor },
            { "World Normal", "Buffer Visualization", DebugView::Normals },
            { "Roughness", "Buffer Visualization", DebugView::Roughness },
            { "Metallic", "Buffer Visualization", DebugView::Metallic },
            { "Ambient Occlusion", "Buffer Visualization", DebugView::AmbientOcclusion },
            { "Shadow Cascades", "Lighting Debug", DebugView::ShadowCascades },
            { "Light Complexity", "Lighting Debug", DebugView::LightComplexity },
        };
    }

    std::span<const ViewMode> GetViewModes()
    {
        return Modes;
    }

    const ViewMode& GetViewMode(DebugView debugView)
    {
        for (const ViewMode& mode : Modes)
        {
            if (mode.debugView == debugView)
                return mode;
        }

        return Modes[0];
    }
}
