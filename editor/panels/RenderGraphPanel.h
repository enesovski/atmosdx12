#pragma once

#include "panels/Panel.h"

#include <string>

namespace atmos::editor
{
    // The last frame's render graph: live and culled passes with GPU time, what each pass reads
    // and writes, and every resource the graph created or imported.
    class RenderGraphPanel final : public Panel
    {
    public:
        RenderGraphPanel();

    protected:
        void DrawContent(EditorContext& context) override;

    private:
        std::wstring selectedPass_;
    };
}
