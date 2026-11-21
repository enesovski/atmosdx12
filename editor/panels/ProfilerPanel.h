#pragma once

#include "panels/Panel.h"

#include "graphics/d3d12/GraphicsDevice.h"

#include <string>
#include <unordered_map>

namespace atmos::editor
{
    // Frame-time graph, per-pass GPU timings and renderer counters.
    class ProfilerPanel final : public Panel
    {
    public:
        ProfilerPanel();

    protected:
        void DrawContent(EditorContext& context) override;

    private:
        void DrawSummary(EditorContext& context);
        void DrawFrameGraph(EditorContext& context);
        void DrawPassTable(EditorContext& context);
        void DrawCounters(EditorContext& context);

        std::unordered_map<std::wstring, float> smoothedPassMs_;
        VideoMemoryInfo videoMemory_;
        float videoMemoryAge_ = 1e9f;
    };
}
