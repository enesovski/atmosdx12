#pragma once

#include "panels/Panel.h"

namespace atmos::editor
{
    // The engine log: level filters with counts, search, auto-scroll and copy.
    class ConsolePanel final : public Panel
    {
    public:
        ConsolePanel();

    protected:
        void DrawContent(EditorContext& context) override;

    private:
        bool showInfo_ = true;
        bool showWarnings_ = true;
        bool showErrors_ = true;
        bool autoScroll_ = true;
        char filter_[128] = {};
    };
}
